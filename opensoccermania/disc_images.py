"""Read-only, content-probed optical image imports; never execute disc contents.

Only ISO9660 data volumes are accepted. Raw conversion handles one MODE1 or
MODE2 Form 1 data track, not audio, encrypted images or arbitrary descriptors.
"""
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path, PureWindowsPath
import os
import re
import shutil
import subprocess
import tempfile
import time
import threading

MAX_IMAGE_BYTES = 4 * 1024**3
MAX_FILES = 100000
MAX_LIST_BYTES = 16 * 1024**2
MAX_IMPORT_BYTES = 4 * 1024**3


def _unlinked(path):
    path = Path(path).absolute()
    for part in (path, *path.parents):
        info = part.lstat()
        if part.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Linked paths and reparse points are not supported.')
    return path.resolve()


@dataclass(frozen=True)
class DiscImageProbe:
    path: Path
    supported: bool
    reason: str
    data_path: Path | None = None
    sector_size: int = 0
    data_offset: int = 0


def _cue_data(path):
    if path.stat().st_size > 65536:
        raise ValueError('CUE descriptor is too large.')
    lines = path.read_text(encoding='utf-8-sig').splitlines()
    files = []; tracks = []; indexes = []
    for line in lines:
        line = line.strip()
        if not line or line.upper().startswith(('REM ', 'TITLE ', 'PERFORMER ', 'CATALOG ')):
            continue
        match = re.fullmatch(r'FILE\s+"([^"\r\n]+)"\s+BINARY', line, re.I)
        if match:
            files.append(match[1]); continue
        match = re.fullmatch(r'TRACK\s+01\s+(MODE1/2048|MODE1/2352|MODE2/2352)', line, re.I)
        if match:
            tracks.append(match[1].upper()); continue
        if re.fullmatch(r'INDEX\s+01\s+00:00:00', line, re.I):
            indexes.append(line); continue
        raise ValueError('Only a single data track at INDEX 01 00:00:00 is supported in CUE images.')
    if len(files) != 1 or len(tracks) != 1 or len(indexes) != 1:
        raise ValueError('CUE requires exactly one binary file, data track and zero-start index.')
    name = files[0]
    if '/' in name or '\\' in name or ':' in name or name in ('.', '..'):
        raise ValueError('CUE binary must be a file beside its descriptor.')
    return _unlinked(path.parent / name), tracks[0]


def probe_disc_image(path):
    """Return support and a concrete reason, without mounting or changing inputs."""
    original = Path(path).absolute()
    try:
        path = _unlinked(original)
        if not path.is_file() or path.suffix.lower() not in ('.iso', '.img', '.bin', '.cue', '.mdf', '.mds', '.nrg'):
            raise ValueError('Choose an ISO, IMG, BIN/CUE, MDF/MDS or NRG disc image.')
        expected = None
        if path.suffix.lower() == '.cue':
            data, expected = _cue_data(path)
        elif path.suffix.lower() == '.mds':
            # MDS can describe sessions and tracks that cannot safely be inferred.
            raise ValueError('MDS session descriptors are unsupported; select the paired MDF for content probing.')
        else:
            data = path
        if path.suffix.lower() != '.cue':
            data = path
        size = data.stat().st_size
        if size < 17 * 2048 or size > MAX_IMAGE_BYTES:
            raise ValueError('Image is truncated or exceeds the 4 GiB import limit.')
        with data.open('rb') as stream:
            for sector_size, offset, mode in ((2048, 0, 'MODE1/2048'), (2352, 16, 'MODE1/2352'), (2352, 24, 'MODE2/2352')):
                if expected and expected != mode:
                    continue
                stream.seek(16 * sector_size + offset)
                if stream.read(7) == b'\x01CD001\x01':
                    return DiscImageProbe(path, True, 'ISO9660 data volume (' + mode + ').', data, sector_size, offset)
        raise ValueError('No supported ISO9660 data volume found. Audio, multisession, compressed and proprietary variants need an external read-only conversion to ISO.')
    except (OSError, ValueError, UnicodeError) as error:
        return DiscImageProbe(original, False, str(error))


@contextmanager
def normalized_disc_image(path):
    """Yield a cooked ISO path. Temporary raw-sector conversions are removed."""
    probe = probe_disc_image(path)
    if not probe.supported:
        raise ValueError(probe.reason)
    if probe.sector_size == 2048:
        yield probe.data_path
        return
    with tempfile.TemporaryDirectory(prefix='opensoccermania-disc-') as temp:
        normalized = Path(temp) / 'data.iso'
        with probe.data_path.open('rb') as source, normalized.open('xb') as target:
            # ISO volume space size bounds conversion and excludes trailing NRG metadata.
            source.seek(16 * 2352 + probe.data_offset + 80)
            lengths = source.read(8)
            count = int.from_bytes(lengths[:4], 'little')
            if count != int.from_bytes(lengths[4:], 'big') or count < 17 or count * 2352 > probe.data_path.stat().st_size:
                raise ValueError('Invalid or truncated ISO volume size in raw image.')
            source.seek(0)
            for _ in range(count):
                sector = source.read(2352)
                if len(sector) != 2352 or sector[:12] != b'\x00' + b'\xff' * 10 + b'\x00' or sector[15] != (1 if probe.data_offset == 16 else 2):
                    raise ValueError('Raw image contains a damaged or unsupported sector.')
                if probe.data_offset == 24 and (sector[16:20] != sector[20:24] or sector[18] & 0x20):
                    raise ValueError('MODE2 Form 2 or inconsistent subheaders are unsupported.')
                target.write(sector[probe.data_offset:probe.data_offset + 2048])
        yield normalized


def find_7zip():
    """Find the optional installed extractor; never install or download tools."""
    for candidate in (shutil.which('7zz'), shutil.which('7z'), os.path.join(os.environ.get('ProgramFiles', 'C:/Program Files'), '7-Zip', '7z.exe')):
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    return None


def _run_bounded(arguments, output, limit, timeout=120):
    """Bound extractor stdout on disk, time and stderr; no visible windows."""
    with tempfile.TemporaryFile() as errors, output.open('wb') as target:
        process = subprocess.Popen([str(p) for p in arguments], stdout=subprocess.PIPE, stderr=errors,
                                   stdin=subprocess.DEVNULL, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        stopped = threading.Event()
        exceeded = threading.Event()
        deadline = time.monotonic() + timeout

        def watchdog():
            while not stopped.wait(0.02):
                if time.monotonic() > deadline or os.fstat(errors.fileno()).st_size > 65536:
                    exceeded.set()
                    if process.poll() is None:
                        process.kill()
                    return

        worker = threading.Thread(target=watchdog, daemon=True)
        worker.start()
        try:
            written = 0
            while True:
                block = process.stdout.read(min(65536, limit - written + 1))
                if not block:
                    break
                written += len(block)
                if written > limit:
                    raise ValueError('Disc extractor exceeded the output limit.')
                target.write(block)
            process.wait()
            if process.returncode or exceeded.is_set():
                raise ValueError('Disc extractor failed or exceeded the time limit.')
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
            stopped.set()
            worker.join()
            process.stdout.close()


def _parse_listing(text, max_bytes):
    entries = []; total = 0; seen = set()
    for block in re.split(r'\r?\n\s*\r?\n', text.strip()):
        values = dict(line.split(' = ', 1) for line in block.splitlines() if ' = ' in line)
        if 'Path' not in values:
            continue
        name = values['Path'].replace('\\', '/')
        parts = name.split('/')
        if not name or PureWindowsPath(name).is_absolute() or any(p in ('', '.', '..') or any(c in p for c in ':<>\"|?*') or p.endswith(('.', ' ')) or p.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))} or any(ord(c) < 32 for c in p) for p in parts):
            raise ValueError('Unsafe path in disc image.')
        if any(key in values for key in ('Symbolic Link', 'Hard Link')) or 'L' in values.get('Attributes', '').split() or values.get('Mode', '').startswith('l'):
            raise ValueError('Linked files are unsupported in disc images.')
        key = name.casefold()
        if key in seen:
            raise ValueError('Case-colliding paths in disc image.')
        seen.add(key)
        is_dir = values.get('Folder') == '+' or 'D' in values.get('Attributes', '').split()
        try:
            size = int(values.get('Size', '0'))
        except ValueError:
            raise ValueError('Invalid disc file size.') from None
        if size < 0:
            raise ValueError('Invalid disc file size.')
        total += size
        if total > max_bytes or len(seen) > MAX_FILES:
            raise ValueError('Disc contents exceed import limits.')
        entries.append((name, size, is_dir))
    if not entries:
        raise ValueError('Disc contains no readable files.')
    return entries


@contextmanager
def open_disc_image(path, *, extractor=None, max_bytes=MAX_IMPORT_BYTES):
    """Yield a bounded private extracted tree; delete it even when import fails.

    Requires an installed 7-Zip. Each listed file is streamed to a validated path,
    so the external tool cannot create links or choose output filesystem paths.
    """
    tool = Path(extractor) if extractor else find_7zip()
    if tool is None or not tool.is_file():
        raise ValueError('Install 7-Zip or provide its executable path to import this image.')
    if not 0 < max_bytes <= MAX_IMPORT_BYTES:
        raise ValueError('Invalid import byte limit.')
    with normalized_disc_image(path) as image, tempfile.TemporaryDirectory(prefix='opensoccermania-extract-') as temp:
        base = Path(temp); listing = base / 'listing.txt'; tree = base / 'disc'; tree.mkdir()
        _run_bounded([tool, 'l', '-slt', '-ba', '-sccUTF-8', '--', image], listing, MAX_LIST_BYTES)
        entries = _parse_listing(listing.read_text(encoding='utf-8-sig'), max_bytes)
        for name, size, is_dir in entries:
            output = tree.joinpath(*name.split('/'))
            if is_dir:
                output.mkdir(parents=True, exist_ok=True)
                continue
            output.parent.mkdir(parents=True, exist_ok=True)
            _run_bounded([tool, 'x', '-so', '-y', '-spd', '--', image, name], output, size)
            if output.stat().st_size != size:
                raise ValueError('Disc file size changed during extraction.')
        yield tree
