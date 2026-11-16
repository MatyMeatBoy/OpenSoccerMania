"""Discover user-owned game files and prepare isolated, reversible copies."""
from dataclasses import dataclass
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile
import os
from .disc_images import _unlinked, probe_disc_image, open_disc_image
from contextlib import contextmanager


@dataclass
class Source:
    root: Path
    exe: Path
    data: Path
    languages: list[str]
    voices: list[str]
    sha256: str
    executable_root: Path | None = None

    @property
    def runnable(self):
        # A valid executable is still subject to the runtime's exact hash gate.
        return True


@dataclass
class AssetSource:
    root: Path
    data: Path
    languages: list[str]
    voices: list[str]

    @property
    def runnable(self):
        return False


def _assert_tree(folder):
    _unlinked(folder)
    def failed(error):
        raise error
    count = 0
    for base, directories, files in os.walk(folder, followlinks=False, onerror=failed):
        count += len(directories) + len(files)
        if count > 100000:
            raise ValueError('Game source exceeds the 100,000 file import limit.')
        for name in directories + files:
            entry = Path(base) / name
            info = entry.lstat()
            if entry.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
                raise ValueError('Linked paths and reparse points are not supported.')


def _hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def _data_details(root, exe_folder=None):
    data = next((p for p in [chain(root, 'Mania', 'CD'), chain(exe_folder, 'Mania', 'CD') if exe_folder else None, child(root, 'CD'), root]
                 if p and chain(p, 'Messages') and chain(p, 'Sound')), None)
    if data is None:
        raise ValueError('Game data is missing (Messages and Sound). Select the full disc or installation.')
    _assert_tree(data)
    messages = child(data, 'Messages')
    languages = []
    for p in messages.iterdir():
        if p.is_file() and p.suffix.lower() == '.txt' and p.stat().st_size:
            with p.open('rb') as stream:
                if any(any(block) for block in iter(lambda: stream.read(65536), b'')):
                    languages.append(p.stem.title())
    cutscenes = chain(data, 'Sound', 'CutScene')
    voices = sorted(p.name.upper() for p in cutscenes.iterdir() if p.is_dir()) if cutscenes else []
    return data, sorted(languages), voices


def discover_assets(path):
    """Inspect assets independently; this result cannot be launched or prepared alone."""
    root = _unlinked(path)
    if not root.is_dir():
        raise ValueError('Select an extracted disc or an installed game folder.')
    data, languages, voices = _data_details(root)
    return AssetSource(root, data, languages, voices)


def prepare_with_executable(assets, executable_source, destination, progress=lambda message: None):
    """Combine selected assets with an explicitly selected intact owner-owned copy.

    The donor executable and adjacent runtime files are copied into a new private
    library entry. Neither original is repaired or modified. Hook hash gates apply.
    """
    if not isinstance(assets, AssetSource) or not isinstance(executable_source, Source):
        raise ValueError('Select assets and an intact executable source explicitly.')
    combined = Source(assets.root, executable_source.exe, assets.data,
                      assets.languages, assets.voices, executable_source.sha256,
                      executable_source.root)
    return prepare(combined, destination, progress)


def child(folder, name):
    return next((p for p in folder.iterdir() if p.name.casefold() == name.casefold()), None) if folder.is_dir() else None


def chain(folder, *names):
    for name in names:
        folder = child(folder, name)
        if folder is None:
            return None
    return folder


def discover(path):
    root = _unlinked(path)
    if not root.is_dir():
        raise ValueError('Select an extracted disc or an installed game folder.')
    # Probe known layouts before walking only their selected asset/runtime trees.
    candidates = [root, chain(root, 'Setup', 'English', 'program files', 'LEGO Media', 'Soccer Mania')]
    exe = next((child(p, 'Game.exe') for p in candidates if p and child(p, 'Game.exe')), None)
    if exe is None or not exe.is_file():
        raise ValueError('Game executable not found in this folder.')
    _unlinked(exe)
    with exe.open('rb') as stream:
        if stream.read(2) != b'MZ':
            raise ValueError('The game executable is damaged or zero-filled. Choose another dump or explicitly select an intact executable source for private asset repair.')
    data, languages, voices = _data_details(root, exe.parent)
    return Source(root, exe, data, languages, voices, _hash(exe))


def prepare(source, destination, progress=lambda message: None):
    if not isinstance(source, Source):
        raise ValueError('Asset-only sources require an explicitly selected intact executable source.')
    requested = Path(destination).absolute()
    if requested.exists() or requested.is_symlink():
        raise FileExistsError('This library entry already exists. Select it from your library.')
    ancestor = requested.parent
    while not ancestor.exists():
        ancestor = ancestor.parent
    _unlinked(ancestor)
    destination = requested.resolve()
    if destination.exists():
        raise FileExistsError('This library entry already exists. Select it from your library.')
    if any(destination.is_relative_to(root.resolve()) for root in (source.root, source.executable_root or source.root)):
        raise ValueError('The private library must be outside the source folder.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.import-', dir=destination.parent))
    try:
        progress('Copying game data. Your originals are preserved.')
        # Never follow source symlinks into unrelated files.
        for folder in [source.data, source.exe.parent]:
            _assert_tree(folder)
        shutil.copytree(source.data, temporary / 'Mania' / 'CD')
        for p in source.exe.parent.iterdir():
            if p.is_file() and (p.suffix.lower() in ('.exe', '.dll', '.dat', '.conf') or p.name.lower() == 'messages'):
                shutil.copy2(p, temporary / p.name)
            elif p.is_dir() and p.name.lower() == 'messages':
                shutil.copytree(p, temporary / p.name)
        # Re-check the executable after copying, before making the entry visible.
        with (temporary / source.exe.name).open('rb') as stream:
            if stream.read(2) != b'MZ':
                raise ValueError('The copied executable is damaged or zero-filled.')
        if _hash(temporary / source.exe.name) != source.sha256:
            raise ValueError('The executable changed during import. Please try again.')
        (temporary / 'opensoccermania-source.json').write_text(json.dumps({
            'source': str(source.root), 'sha256': source.sha256,
            'executable_source': str(source.executable_root or source.root),
            'asset_repair': source.executable_root is not None,
            'languages': source.languages, 'voices': source.voices,
        }, indent=2), encoding='utf-8')
        temporary.rename(destination)
        return destination
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


@contextmanager
def mounted_iso(path):
    """Mount an ISO read-only and detach only if this call mounted it."""
    path = _unlinked(path)
    if path.suffix.lower() != '.iso' or not path.is_file():
        raise ValueError('Select a readable .iso image.')
    escaped = str(path).replace("'", "''")
    script = f"""$ErrorActionPreference='Stop'
$image=Get-DiskImage -ImagePath '{escaped}'
$owned=-not $image.Attached
if ($owned) {{ $image=Mount-DiskImage -ImagePath '{escaped}' -Access ReadOnly -PassThru }}
try {{
  $volume=$image | Get-Volume | Where-Object DriveLetter | Select-Object -First 1
  if (-not $volume) {{ throw 'The ISO has no readable volume.' }}
  @{{root=($volume.DriveLetter+':\\');owned=$owned}} | ConvertTo-Json -Compress
}} catch {{ if ($owned) {{ Dismount-DiskImage -ImagePath '{escaped}' | Out-Null }}; throw }}
"""
    flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
    result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script], capture_output=True, text=True, creationflags=flags)
    if result.returncode:
        raise ValueError('Could not mount the ISO: ' + result.stderr.strip())
    info = json.loads(result.stdout)
    try:
        yield Path(info['root'])
    finally:
        if info['owned']:
            result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', f"Dismount-DiskImage -ImagePath '{escaped}' -ErrorAction Stop"], capture_output=True, text=True, creationflags=flags)
            if result.returncode:
                raise RuntimeError('Import finished, but Windows could not detach the ISO. Eject it in Explorer.')
