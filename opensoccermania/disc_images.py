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
WINDOWS_DEVICES = {'CON', 'PRN', 'AUX', 'NUL', 'CLOCK$', 'CONIN$', 'CONOUT$',
                   *(prefix + str(digit) for prefix in ('COM', 'LPT') for digit in range(10)),
                   *(prefix + chr(digit) for prefix in ('COM', 'LPT') for digit in (0xb9, 0xb2, 0xb3))}


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
    descriptor_sector: int = 16


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
                # Boot records may precede the primary volume descriptor.
                for sector in range(16, min(16 + 128, size // sector_size)):
                    stream.seek(sector * sector_size + offset)
                    header = stream.read(7)
                    if header[1:] != b'CD001\x01' or header[:1] == b'\xff':
                        break
                    if header[:1] == b'\x01':
                        return DiscImageProbe(path, True, 'ISO9660 data volume (' + mode + ').', data, sector_size, offset, sector)
        raise ValueError('No supported ISO9660 data volume found. Audio, multisession, compressed and proprietary variants need an external read-only conversion to ISO.')
    except (OSError, ValueError, UnicodeError) as error:
        return DiscImageProbe(original, False, str(error))


@contextmanager
def normalized_disc_image(path, *, deadline=None):
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
            source.seek(probe.descriptor_sector * 2352 + probe.data_offset + 80)
            lengths = source.read(8)
            count = int.from_bytes(lengths[:4], 'little')
            if count != int.from_bytes(lengths[4:], 'big') or count < 17 or count * 2352 > probe.data_path.stat().st_size:
                raise ValueError('Invalid or truncated ISO volume size in raw image.')
            source.seek(0)
            for _ in range(count):
                if deadline is not None and time.monotonic() > deadline:
                    raise ValueError('Raw disc conversion exceeded its time limit.')
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
        if not name or PureWindowsPath(name).is_absolute() or any(p in ('', '.', '..') or any(c in p for c in ':<>\"|?*') or p.endswith(('.', ' ')) or p.split('.')[0].rstrip(' .').upper() in WINDOWS_DEVICES or any(ord(c) < 32 for c in p) for p in parts):
            raise ValueError('Unsafe path in disc image.')
        if any(values.get(key) for key in ('Symbolic Link', 'Hard Link')) or 'L' in values.get('Attributes', '').split() or values.get('Mode', '').startswith('l'):
            raise ValueError('Linked files are unsupported in disc images.')
        key = name.casefold()
        if key in seen:
            raise ValueError('Case-colliding paths in disc image.')
        seen.add(key)
        is_dir = values.get('Folder') == '+' or 'D' in values.get('Attributes', '').split()
        try:
            size = int(values.get('Size') or '0')
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


ISO_BLOCK = 2048
MAX_DIRECTORY_BYTES = 16 * 1024**2
MAX_METADATA_BYTES = 64 * 1024**2
MAX_DIRECTORY_DEPTH = 64


def _iso_integer(data, offset, width):
    raw = data[offset:offset + width * 2]
    if len(raw) != width * 2 or int.from_bytes(raw[:width], 'little') != int.from_bytes(raw[width:], 'big'):
        raise ValueError('Inconsistent or truncated ISO9660 both-endian field.')
    return int.from_bytes(raw[:width], 'little')


def _iso_name(name):
    if any(ord(character) < 32 for character in name):
        raise ValueError('Control characters are unsafe in ISO9660 identifiers.')
    # Reuse the same Windows-safe path rules as the optional external reader.
    _parse_listing('Path = ' + name + '\nSize = 0\nFolder = -\n', 1)
    if '/' in name or '\\' in name:
        raise ValueError('An ISO9660 identifier must be one safe path component.')
    return name


def _iso_inventory(stream, image_bytes, max_bytes, deadline):
    """Read ISO9660/Joliet metadata before creating any output files.

    Directory records follow ECMA-119; Joliet SVD names use big-endian Unicode.
    Interleaved/multiextent/multivolume and Rock Ridge-only names fail explicitly.
    """
    def read_at(offset, size, bound=image_bytes):
        if time.monotonic() > deadline:
            raise ValueError('ISO9660 import exceeded its time limit.')
        if offset < 0 or size < 0 or offset + size > bound:
            raise ValueError('ISO9660 extent is outside the image/volume bounds.')
        stream.seek(offset)
        data = stream.read(size)
        if len(data) != size:
            raise ValueError('ISO9660 data is truncated.')
        return data

    primary = None; supplementary = []; terminated = False
    for sector in range(16, 16 + 128):
        descriptor = read_at(sector * ISO_BLOCK, ISO_BLOCK)
        if descriptor[1:7] != b'CD001\x01':
            raise ValueError('Invalid ISO9660 volume descriptor.')
        if descriptor[0] == 1 and primary is None:
            primary = descriptor
        elif descriptor[0] == 2 and descriptor[88:91] in (b'%/@', b'%/C', b'%/E'):
            supplementary.append(descriptor)
        elif descriptor[0] == 255:
            terminated = True; break
    if primary is None or not terminated:
        raise ValueError('ISO9660 primary descriptor or terminator is missing.')
    count = _iso_integer(primary, 80, 4)
    if count < 17 or count * ISO_BLOCK > image_bytes:
        raise ValueError('ISO9660 volume size exceeds the readable image.')
    volume_bytes = count * ISO_BLOCK
    chosen = max(supplementary, key=lambda descriptor: descriptor[90]) if supplementary else primary
    joliet = chosen is not primary
    if (_iso_integer(chosen, 80, 4) != count or _iso_integer(chosen, 128, 2) != ISO_BLOCK
            or _iso_integer(chosen, 120, 2) != 1 or _iso_integer(chosen, 124, 2) != 1):
        raise ValueError('Unsupported ISO9660 block size or multivolume layout.')

    def record(data):
        if len(data) < 34 or data[0] != len(data) or data[32] == 0 or 33 + data[32] > len(data):
            raise ValueError('Invalid ISO9660 directory record length.')
        if data[1] or data[26] or data[27] or data[25] & 0xe4 or _iso_integer(data, 28, 2) != 1:
            raise ValueError('Extended, associated, interleaved or multiextent ISO9660 records are unsupported.')
        extent = _iso_integer(data, 2, 4); size = _iso_integer(data, 10, 4)
        if extent * ISO_BLOCK + size > volume_bytes:
            raise ValueError('ISO9660 extent is outside the volume bounds.')
        name = data[33:33 + data[32]]
        system_use = data[33 + data[32] + (data[32] % 2 == 0):]
        # Do not silently reinterpret Rock Ridge links or truncated long names.
        while len(system_use) >= 4 and system_use[2] >= 4 and system_use[2] <= len(system_use) and system_use[3] == 1:
            if system_use[:2] in (b'SL', b'NM', b'CE', b'RE', b'CL', b'PL'):
                raise ValueError('Rock Ridge links/names are unsupported; use an ISO9660/Joliet data image.')
            system_use = system_use[system_use[2]:]
        directory = bool(data[25] & 2)
        if directory and (not size or size % ISO_BLOCK or size > MAX_DIRECTORY_BYTES):
            raise ValueError('Invalid or oversized ISO9660 directory extent.')
        return name, extent, size, directory

    root_length = chosen[156]
    root_name, root_extent, root_size, root_directory = record(chosen[156:156 + root_length])
    if root_name != b'\0' or not root_directory:
        raise ValueError('Invalid ISO9660 root directory.')
    pending = [('', root_extent, root_size, 0)]
    visited = set(); seen_names = set(); result = []; total = 0; metadata = 0
    while pending:
        parent, extent, length, depth = pending.pop()
        if depth > MAX_DIRECTORY_DEPTH or extent in visited:
            raise ValueError('ISO9660 directory cycle, alias or excessive depth.')
        visited.add(extent); metadata += length
        if metadata > MAX_METADATA_BYTES:
            raise ValueError('ISO9660 directory metadata exceeds the import limit.')
        data = read_at(extent * ISO_BLOCK, length, volume_bytes)
        position = 0
        while position < length:
            if time.monotonic() > deadline:
                raise ValueError('ISO9660 directory scan exceeded its time limit.')
            record_length = data[position]
            if record_length == 0:
                boundary = min((position // ISO_BLOCK + 1) * ISO_BLOCK, length)
                if any(data[position:boundary]):
                    raise ValueError('Nonzero ISO9660 directory padding is invalid.')
                position = boundary; continue
            if position % ISO_BLOCK + record_length > ISO_BLOCK or position + record_length > length:
                raise ValueError('ISO9660 directory record crosses a sector boundary.')
            name, file_extent, size, directory = record(data[position:position + record_length])
            position += record_length
            if name in (b'\0', b'\1'):
                if not directory:
                    raise ValueError('Invalid ISO9660 dot directory record.')
                continue
            try:
                decoded = name.decode('utf-16-be' if joliet else 'ascii')
            except UnicodeError as error:
                raise ValueError('Invalid ISO9660/Joliet filename encoding.') from error
            if not directory:
                decoded = re.sub(r';[0-9]+$', '', decoded)
                if decoded.endswith('.'):
                    decoded = decoded[:-1]
            decoded = _iso_name(decoded)
            relative = parent + '/' + decoded if parent else decoded
            if relative.casefold() in seen_names:
                raise ValueError('Case-colliding ISO9660 paths are unsupported.')
            seen_names.add(relative.casefold())
            if len(seen_names) > MAX_FILES:
                raise ValueError('ISO9660 file count exceeds the import limit.')
            if not directory:
                total += size
                if total > max_bytes:
                    raise ValueError('ISO9660 payload exceeds the import byte limit.')
            result.append((relative, file_extent, size, directory))
            if directory:
                pending.append((relative, file_extent, size, depth + 1))
    if not result:
        raise ValueError('ISO9660 volume contains no readable entries.')
    return result


def _extract_iso9660(image, tree, max_bytes, deadline):
    with image.open('rb') as stream:
        entries = _iso_inventory(stream, os.fstat(stream.fileno()).st_size, max_bytes, deadline)
        for name, extent, size, directory in entries:
            if time.monotonic() > deadline:
                raise ValueError('ISO9660 extraction exceeded its time limit.')
            output = tree.joinpath(*name.split('/'))
            if directory:
                output.mkdir(parents=True, exist_ok=True); continue
            output.parent.mkdir(parents=True, exist_ok=True)
            stream.seek(extent * ISO_BLOCK)
            with output.open('xb') as target:
                remaining = size
                while remaining:
                    if time.monotonic() > deadline:
                        raise ValueError('ISO9660 extraction exceeded its time limit.')
                    block = stream.read(min(1024**2, remaining))
                    if not block:
                        raise ValueError('ISO9660 file became truncated during extraction.')
                    target.write(block); remaining -= len(block)


@contextmanager
def open_disc_image(path, *, extractor=None, max_bytes=MAX_IMPORT_BYTES):
    """Yield a bounded private extracted tree; delete it even when import fails.

    ISO9660/Joliet extraction is built in and never searches for installed tools.
    An explicitly supplied 7-Zip executable remains an optional compatibility API.
    """
    tool = Path(extractor) if extractor is not None else None
    if tool is not None and not tool.is_file():
        raise ValueError('The explicitly supplied disc extractor does not exist.')
    if not 0 < max_bytes <= MAX_IMPORT_BYTES:
        raise ValueError('Invalid import byte limit.')
    deadline = time.monotonic() + 300
    with normalized_disc_image(path, deadline=deadline) as image, tempfile.TemporaryDirectory(prefix='opensoccermania-extract-') as temp:
        base = Path(temp); listing = base / 'listing.txt'; tree = base / 'disc'; tree.mkdir()
        if tool is None:
            _extract_iso9660(image, tree, max_bytes, deadline)
            yield tree
            return
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
