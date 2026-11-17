"""Create an immutable, inventoried source ZIP from an exact Git commit.

No worktree data, game files or unapproved graphics wrappers are read. Repeated packaging
with the same commit/version and Python/zlib toolchain produces identical ZIPs.
"""
import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile
import zipfile

TOP_FILES = {'.gitattributes', '.gitignore', 'CHANGELOG.md', 'LICENSE',
             'OpenSoccerMania.pyw', 'README.md', 'THIRD_PARTY.md', 'requirements.txt'}
TOP_DIRECTORIES = {'.github', 'assets', 'docs', 'licenses', 'native',
                   'opensoccermania', 'runtime', 'tests', 'third_party', 'tools'}
ALLOWED_ASSETS = {'assets/backgrounds/title-extension.png', 'assets/fonts/Nunito.ttf'}
DENIED_SUFFIXES = {'.exe', '.dll', '.iso', '.bin', '.cue', '.img', '.mdf', '.mds',
                   '.nrg', '.zip', '.7z', '.rar', '.ogg', '.wav', '.mp4', '.avi',
                   '.bik', '.bmp', '.log', '.sav', '.bak', '.pyc', '.pdb', '.obj',
                   '.o', '.a', '.lib', '.so', '.dylib', '.dat', '.conf'}
# Exact unmodified vendor release approved for this dedicated game launcher.
VENDOR_FILES = {
    'runtime/graphics/dgVoodoo/d3d8.dll': '25a0ceeb85fcc4675c6f964442eecc14fedd504effd39c931a627249ad4685e2',
    'runtime/graphics/dgVoodoo/dgVoodoo.conf': '45cc0eb8e6ed4f067aca1b2e4ac1268dfbd164bbbd06d264c53e8a60642c4dbf',
    'runtime/graphics/dgVoodoo/manifest.json': '44e4899cf4baf6d975f1f1e80ccf5c2914c97c1870082fd1dcf9c362f3396a56',
}
VENDOR_LICENSE = 'licenses/dgVoodoo.txt'


def validate_vendor_file(name, data):
    if name in VENDOR_FILES and hashlib.sha256(data).hexdigest() != VENDOR_FILES[name]:
        raise ValueError('Bundled graphics vendor hash mismatch: ' + name)


MAX_FILE_BYTES = 64 * 1024**2
MAX_TOTAL_BYTES = 256 * 1024**2
MAX_FILES = 10000


@dataclass(frozen=True)
class PackageResult:
    path: Path
    checksum_path: Path
    sha256: str
    source_commit: str
    members: int


def _git(repo, *arguments, data=None):
    process = subprocess.run(['git', '-C', str(repo), *arguments], input=data,
                             capture_output=True, timeout=120,
                             creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    if process.returncode:
        raise ValueError('Git could not read the requested source commit: ' + process.stderr.decode('utf-8', errors='replace').strip())
    return process.stdout


def _validate_member(name, mode):
    path = PurePosixPath(name)
    parts = name.split('/')
    if path.is_absolute() or any(part in ('', '.', '..') or '\\' in part or any(char in part for char in ':<>\"|?*') or part.endswith(('.', ' ')) or part.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))} or any(ord(char) < 32 for char in part) for part in parts):
        raise ValueError('Unsafe tracked source path: ' + name)
    if mode not in ('100644', '100755'):
        raise ValueError('Linked files, submodules and special files cannot be packaged: ' + name)
    if len(parts) == 1:
        if name not in TOP_FILES:
            raise ValueError('Unapproved root file in source release: ' + name)
    elif parts[0] not in TOP_DIRECTORIES:
        raise ValueError('Private or unapproved directory in source release: ' + name)
    if any(part.casefold() in ('local', '__pycache__', '.venv', '.git') for part in parts):
        raise ValueError('Private/cache directory in source release: ' + name)
    if any(part.casefold() in ('build', 'dist') for part in parts) and not name.startswith('third_party/minhook/build/'):
        raise ValueError('Build output cannot be packaged: ' + name)
    if parts[0] == 'assets' and name not in ALLOWED_ASSETS:
        raise ValueError('Asset requires explicit public redistribution approval: ' + name)
    if parts[0] == 'runtime' and name != 'runtime/SDL3.dll' and name not in VENDOR_FILES:
        raise ValueError('Unapproved runtime binary: ' + name)
    if path.suffix.lower() in DENIED_SUFFIXES and name != 'runtime/SDL3.dll' and name not in VENDOR_FILES:
        raise ValueError('Binary, game, wrapper or private artifact in source release: ' + name)
    if ('dgvoodoo' in name.casefold() and name not in VENDOR_FILES and name != VENDOR_LICENSE) or path.name.casefold().startswith(('osm-frame-', 'capture', 'screenshot')):
        raise ValueError('Wrapper or private capture in source release: ' + name)


def _tracked_files(repo, revision):
    raw = _git(repo, 'ls-tree', '-rlz', '--full-tree', revision)
    if len(raw) > 8 * 1024**2:
        raise ValueError('Source tree listing exceeds the release limit.')
    entries = []; total = 0; seen = set()
    for line in raw.split(b'\0'):
        if not line:
            continue
        header, encoded_name = line.split(b'\t', 1)
        mode, kind, blob, size = header.decode('ascii').split()
        name = encoded_name.decode('utf-8')
        _validate_member(name, mode)
        if kind != 'blob' or not size.isdigit():
            raise ValueError('Only ordinary source blobs can be packaged.')
        size = int(size); total += size
        if size > MAX_FILE_BYTES or total > MAX_TOTAL_BYTES or len(entries) >= MAX_FILES:
            raise ValueError('Source inventory exceeds release limits.')
        if name.casefold() in seen:
            raise ValueError('Case-colliding source paths cannot be packaged.')
        seen.add(name.casefold()); entries.append((name, mode, blob, size))
    names = {entry[0] for entry in entries}
    if 'runtime/SDL3.dll' in names and 'licenses/SDL3.txt' not in names:
        raise ValueError('The SDL3 redistributable requires its license.')
    if names.intersection(VENDOR_FILES) and not (set(VENDOR_FILES) | {VENDOR_LICENSE}) <= names:
        raise ValueError('Bundled graphics vendor requires all pinned files and its license.')
    if not {'README.md', 'LICENSE'} <= names:
        raise ValueError('Source release requires README.md and LICENSE.')
    return sorted(entries)


def _blobs(repo, entries):
    result = _git(repo, 'cat-file', '--batch', data=b''.join(blob.encode() + b'\n' for _, _, blob, _ in entries))
    offset = 0
    for name, mode, blob, size in entries:
        end = result.find(b'\n', offset)
        expected = f'{blob} blob {size}'.encode()
        if end < 0 or result[offset:end] != expected:
            raise ValueError('Git blob inventory changed during packaging.')
        offset = end + 1
        data = result[offset:offset + size]
        if len(data) != size or result[offset + size:offset + size + 1] != b'\n':
            raise ValueError('Truncated source blob.')
        offset += size + 1
        yield name, mode, data
    if offset != len(result):
        raise ValueError('Unexpected bytes after source inventory.')


def _safe_output(output):
    output = Path(output).absolute()
    if any(ord(char) < 32 for char in output.name):
        raise ValueError('Unsafe release artifact filename.')
    if output.suffix.lower() != '.zip':
        raise ValueError('The source artifact must have a .zip extension.')
    for ancestor in (output, *output.parents):
        try:
            info = ancestor.lstat()
        except FileNotFoundError:
            continue
        if ancestor.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Release output cannot use linked paths or reparse points.')
    return output


def package_release(repo, revision, output, version):
    """Build from an exact 40/64-digit commit SHA; refuse existing ZIP/sidecar."""
    if not re.fullmatch(r'(?:[0-9a-f]{40}|[0-9a-f]{64})', revision):
        raise ValueError('Provide the exact full source commit SHA, not a branch or tag.')
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?', version):
        raise ValueError('Invalid release version.')
    output = _safe_output(output); checksum = output.with_suffix(output.suffix + '.sha256')
    if output.exists() or checksum.exists() or checksum.is_symlink():
        raise FileExistsError('Release artifacts are immutable; select a new output path.')
    repo = Path(repo).resolve()
    actual = _git(repo, 'rev-parse', '--verify', revision + '^{commit}').decode().strip()
    if actual != revision:
        raise ValueError('The supplied SHA does not identify this exact source commit.')
    tree = _git(repo, 'rev-parse', revision + '^{tree}').decode().strip()
    timestamp = int(_git(repo, 'show', '-s', '--format=%ct', revision).decode().strip())
    date = datetime.fromtimestamp(max(315532800, min(timestamp, 4354819198)), timezone.utc)
    zip_time = (date.year, date.month, date.day, date.hour, date.minute, date.second // 2 * 2)
    entries = _tracked_files(repo, revision)
    prefix = f'OpenSoccerMania-{version}-source/'
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.source-release-', dir=output.parent) as temporary:
        staged = Path(temporary) / 'release.zip'
        inventory = []
        with zipfile.ZipFile(staged, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9, strict_timestamps=True) as archive:
            def write(name, mode, data):
                info = zipfile.ZipInfo(prefix + name, zip_time)
                info.create_system = 3
                info.external_attr = (int(mode, 8) | 0o100000) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
            for name, mode, data in _blobs(repo, entries):
                validate_vendor_file(name, data)
                write(name, mode, data)
                inventory.append({'path': name, 'mode': mode, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
            manifest = {'format': 1, 'version': version, 'source_commit': revision,
                        'source_tree': tree, 'source_timestamp': timestamp, 'files': inventory}
            write('SOURCE-MANIFEST.json', '100644', (json.dumps(manifest, sort_keys=True, indent=2) + '\n').encode('utf-8'))
        with staged.open('rb') as artifact:
            digest = hashlib.file_digest(artifact, 'sha256').hexdigest()
        staged_checksum = Path(temporary) / 'release.sha256'
        staged_checksum.write_text(digest + '  ' + output.name + '\n', encoding='utf-8')
        created = []
        try:
            for source, destination in ((staged, output), (staged_checksum, checksum)):
                os.link(source, destination)
                created.append(destination)
        except BaseException:
            for destination in reversed(created):
                destination.unlink()
            raise
    return PackageResult(output, checksum, digest, revision, len(entries) + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', default='.')
    parser.add_argument('--revision', required=True, help='Exact full source commit SHA')
    parser.add_argument('--version', required=True)
    parser.add_argument('--output', required=True, type=Path)
    options = parser.parse_args()
    result = package_release(options.repo, options.revision, options.output, options.version)
    print(json.dumps({'artifact': str(result.path), 'sha256_file': str(result.checksum_path),
                      'sha256': result.sha256, 'source_commit': result.source_commit,
                      'members': result.members}, indent=2))


if __name__ == '__main__':
    main()
