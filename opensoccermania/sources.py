"""Discover user-owned game files and prepare isolated, reversible copies."""
from dataclasses import dataclass
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile
from contextlib import contextmanager


@dataclass
class Source:
    root: Path
    exe: Path
    data: Path
    languages: list[str]
    voices: list[str]
    sha256: str


def child(folder, name):
    return next((p for p in folder.iterdir() if p.name.casefold() == name.casefold()), None) if folder.is_dir() else None


def chain(folder, *names):
    for name in names:
        folder = child(folder, name)
        if folder is None:
            return None
    return folder


def discover(path):
    root = Path(path).resolve()
    if not root.is_dir():
        raise ValueError('Select an extracted disc or an installed game folder.')
    # Bounded layout detection avoids scanning an entire drive or unrelated games.
    candidates = [root, chain(root, 'Setup', 'English', 'program files', 'LEGO Media', 'Soccer Mania')]
    exe = next((child(p, 'Game.exe') for p in candidates if p and child(p, 'Game.exe')), None)
    if exe is None:
        raise ValueError('Game executable not found in this folder.')
    raw = exe.read_bytes()
    if raw[:2] != b'MZ':
        raise ValueError('The game executable is damaged or zero-filled. Choose another dump.')
    data = next((p for p in [chain(root, 'Mania', 'CD'), chain(exe.parent, 'Mania', 'CD'), child(root, 'CD'), root]
                 if p and chain(p, 'Messages') and chain(p, 'Sound')), None)
    if data is None:
        raise ValueError('Game data is missing (Messages and Sound). Select the full disc or installation.')
    messages = child(data, 'Messages')
    languages = sorted(p.stem.title() for p in messages.iterdir() if p.suffix.lower() == '.txt' and p.stat().st_size and any(p.read_bytes()))
    cutscenes = chain(data, 'Sound', 'CutScene')
    voices = sorted(p.name.upper() for p in cutscenes.iterdir() if p.is_dir()) if cutscenes else []
    return Source(root, exe, data, languages, voices, hashlib.sha256(raw).hexdigest())


def prepare(source, destination, progress=lambda message: None):
    destination = Path(destination).resolve()
    if destination.exists():
        raise FileExistsError('This library entry already exists. Select it from your library.')
    if destination.is_relative_to(source.root):
        raise ValueError('The private library must be outside the source folder.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix='.import-', dir=destination.parent))
    try:
        progress('Copying game data. Your originals are preserved.')
        # Never follow source symlinks into unrelated files.
        for folder in [source.data, source.exe.parent]:
            if any(p.is_symlink() or p.is_junction() if hasattr(p, 'is_junction') else p.is_symlink() for p in folder.rglob('*')):
                raise ValueError('Linked directories are not supported in game sources.')
        shutil.copytree(source.data, temporary / 'Mania' / 'CD')
        for p in source.exe.parent.iterdir():
            if p.is_file() and (p.suffix.lower() in ('.exe', '.dll', '.dat', '.conf') or p.name.lower() == 'messages'):
                shutil.copy2(p, temporary / p.name)
            elif p.is_dir() and p.name.lower() == 'messages':
                shutil.copytree(p, temporary / p.name)
        # Re-check the executable after copying, before making the entry visible.
        if hashlib.sha256((temporary / source.exe.name).read_bytes()).hexdigest() != source.sha256:
            raise ValueError('The executable changed during import. Please try again.')
        (temporary / 'opensoccermania-source.json').write_text(json.dumps({
            'source': str(source.root), 'sha256': source.sha256,
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
    path = Path(path).resolve()
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
