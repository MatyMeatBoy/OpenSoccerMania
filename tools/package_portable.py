"""Inventory and package an already frozen Windows portable launcher.

This packages exact input bytes without running the launcher. Native DLLs and SDL
must match separately supplied build/runtime files. Provenance names the supplied
source commit; it does not independently establish binary build reproducibility.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
import zipfile
try:
    from .package_release import PackageResult, _git, _safe_output, VENDOR_FILES, VENDOR_LICENSE, validate_vendor_file, ALLOWED_ASSETS
except ImportError:
    from package_release import PackageResult, _git, _safe_output, VENDOR_FILES, VENDOR_LICENSE, validate_vendor_file, ALLOWED_ASSETS

REQUIRED_FILES = {
    'OpenSoccerMania.exe', 'assets/backgrounds/title-extension.png',
    'assets/fonts/Nunito.ttf', 'runtime/SDL3.dll', 'native/d3d8.dll',
    'native/dinput8.dll', 'LICENSE', 'THIRD_PARTY.md', 'README.md',
    'licenses/SDL3.txt', 'licenses/Nunito-OFL.txt',
    'licenses/NewSnapRecomp-MIT.txt', 'licenses/MinHook/LICENSE.txt',
    'licenses/Python/LICENSE.txt',
}
REQUIRED_FILES |= set(VENDOR_FILES) | {VENDOR_LICENSE}
REQUIRED_FILES |= ALLOWED_ASSETS
VENDOR_DIRECTORIES = {'runtime/graphics', 'runtime/graphics/dgVoodoo'}

MAX_FILES = 20000
MAX_FILE_BYTES = 128 * 1024**2
MAX_TOTAL_BYTES = 512 * 1024**2
PRIVATE_PARTS = {'local', 'build', 'dist', '.git', '.venv', '__pycache__',
                 'mania', 'cd', 'messages', 'sound', 'setup',
                 'electronicarts-legosoccermania', 'lego football mania', 'lego football mania dutch'}
PRIVATE_SUFFIXES = {'.iso', '.img', '.bin', '.cue', '.mdf', '.mds', '.nrg',
                    '.ogg', '.wav', '.avi', '.mp4', '.bik', '.bmp', '.sav', '.log', '.conf', '.original', '.7z', '.rar', '.dat', '.bmb', '.dpc'}
PUBLIC_DIRECTORIES = {'assets', 'licenses', 'native', 'runtime', 'PIL', '_tcl_data', '_tk_data', 'tcl8'}
ROOT_DLLS = {'libcrypto-3-x64.dll', 'libssl-3-x64.dll', 'libffi-8.dll',
             'tcl86t.dll', 'tk86t.dll', 'ucrtbase.dll', 'vcruntime140.dll', 'vcruntime140_1.dll'}
ROOT_PYDS = {'_bz2', '_ctypes', '_decimal', '_elementtree', '_hashlib', '_lzma',
             '_queue', '_socket', '_ssl', '_tkinter', '_uuid', 'pyexpat', 'select', 'unicodedata'}


def _plain(path):
    path = Path(path).absolute()
    for ancestor in (path, *path.parents):
        info = ancestor.lstat()
        if ancestor.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise ValueError('Portable inputs cannot contain links or reparse points.')
    return path.resolve()


def _member(name):
    parts = name.split('/')
    if any(part in ('', '.', '..') or any(character in part for character in '\\:<>"|?*') or part.endswith(('.', ' '))
           or any(ord(character) < 32 for character in part)
           or part.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))} for part in parts):
        raise ValueError('Unsafe portable member path: ' + name)
    lowered = name.casefold(); leaf = parts[-1].casefold()
    vendor = name in VENDOR_FILES
    approved_vendor_path = vendor or name in VENDOR_DIRECTORIES or name == VENDOR_LICENSE
    if any(part.casefold() in PRIVATE_PARTS for part in parts) or ('dgvoodoo' in lowered and not approved_vendor_path) or 'd3d8_backend' in lowered:
        raise ValueError('Private game, wrapper or build path in portable input: ' + name)
    if leaf.startswith(('game.exe', 'osm-frame-', 'capture', 'screenshot', 'opensoccermania-source.', 'opensoccermania-bridge.', 'opensoccermania-input.')):
        raise ValueError('Private game file in portable input: ' + name)
    if len(parts)>1 and parts[0] not in PUBLIC_DIRECTORIES and not re.fullmatch(r'(?:pillow|pyinstaller)-[0-9.]+\.dist-info',parts[0]):
        raise ValueError('Unapproved portable dependency directory: ' + name)
    if parts[0] == 'assets' and name not in {'assets', 'assets/backgrounds', 'assets/fonts', 'assets/branding'} | ALLOWED_ASSETS:
        raise ValueError('Unapproved redistributable asset: ' + name)
    if parts[0] == 'native' and name not in {'native', 'native/d3d8.dll', 'native/dinput8.dll'}:
        raise ValueError('Unapproved project-native binary: ' + name)
    if parts[0] == 'runtime' and name not in {'runtime', 'runtime/SDL3.dll'} and not approved_vendor_path:
        raise ValueError('Unapproved bundled runtime: ' + name)
    suffix = Path(name).suffix.lower()
    if suffix=='.dll' and not vendor and name not in {'native/d3d8.dll','native/dinput8.dll','runtime/SDL3.dll'}:
        if len(parts)!=1 or not (leaf in ROOT_DLLS or re.fullmatch(r'python3[0-9]{1,2}\.dll',leaf) or re.fullmatch(r'api-ms-win-(?:core|crt)-[a-z0-9-]+\.dll',leaf)):
            raise ValueError('Unapproved portable dependency DLL: ' + name)
    if suffix=='.pyd' and not (len(parts)==1 and Path(name).stem in ROOT_PYDS) and not re.fullmatch(r'PIL/_(?:avif|imaging|imagingcms|imagingft|imagingtk|webp)\.cp3[0-9]{1,2}-win_amd64\.pyd',name):
        raise ValueError('Unapproved portable Python extension: ' + name)
    if len(parts)==1 and suffix not in {'.exe','.dll','.pyd','.zip'} and name not in {'LICENSE','README.md','THIRD_PARTY.md'} and name not in PUBLIC_DIRECTORIES and not re.fullmatch(r'(?:pillow|pyinstaller)-[0-9.]+\.dist-info',name):
        raise ValueError('Unapproved portable root file: ' + name)
    if suffix == '.zip' and name != 'base_library.zip':
        raise ValueError('Only the frozen Python base library archive is permitted.')
    if (suffix in PRIVATE_SUFFIXES and not vendor) or (suffix == '.exe' and name != 'OpenSoccerMania.exe'):
        raise ValueError('Private or unapproved binary file in portable input: ' + name)
    if leaf in ('d3d8.dll', 'dinput8.dll') and name not in ('native/d3d8.dll', 'native/dinput8.dll') and not vendor:
        raise ValueError('Only the validated project-native bridge DLLs may be included.')
    if leaf == 'portable-manifest.json':
        raise ValueError('Portable manifest is generated by the packager.')


def _inventory(root):
    seen = set(); files = []; total = 0; count = 0
    def failed(error):
        raise error
    for base, directories, names in os.walk(root, followlinks=False, onerror=failed):
        count += len(directories) + len(names)
        if count > MAX_FILES:
            raise ValueError('Portable input exceeds the member limit.')
        for name in directories + names:
            path = Path(base) / name
            relative = path.relative_to(root).as_posix()
            # Each entry is checked before os.walk may descend into it.
            info = path.lstat()
            if path.is_symlink() or getattr(info, 'st_file_attributes', 0) & 0x400:
                raise ValueError('Portable inputs cannot contain links or reparse points.')
            _member(relative)
            if relative.casefold() in seen:
                raise ValueError('Case-colliding portable members are unsupported.')
            seen.add(relative.casefold())
            if path.is_file():
                size = info.st_size; total += size
                if size > MAX_FILE_BYTES or total > MAX_TOTAL_BYTES:
                    raise ValueError('Portable input exceeds the byte limit.')
                files.append((relative, path, size))
            elif not path.is_dir():
                raise ValueError('Portable input contains a special file.')
    missing = REQUIRED_FILES - {name for name, _, _ in files}
    if missing:
        raise ValueError('Portable input is missing required files: ' + ', '.join(sorted(missing)))
    return sorted(files)


def _read(path, limit=MAX_FILE_BYTES):
    path = _plain(path)
    if not path.is_file() or path.stat().st_size > limit:
        raise ValueError('Invalid or oversized portable binary input.')
    with path.open('rb') as source:
        data = source.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Portable file changed or exceeded its size limit.')
    return data


def package_portable(repo, revision, input_root, output, *, bridge, input_bridge, sdl):
    """Package frozen files, validating native/SDL hashes; refuse existing outputs."""
    if not re.fullmatch(r'(?:[0-9a-f]{40}|[0-9a-f]{64})', revision):
        raise ValueError('Provide the exact full source commit SHA.')
    output = _safe_output(output); checksum = output.with_suffix(output.suffix + '.sha256')
    if output.exists() or checksum.exists() or checksum.is_symlink():
        raise FileExistsError('Portable artifacts are immutable; select a new output path.')
    root = _plain(input_root)
    if not root.is_dir():
        raise ValueError('Choose the frozen portable launcher directory.')
    if output.is_relative_to(root):
        raise ValueError('Portable output must be outside its frozen input directory.')
    actual = _git(repo, 'rev-parse', '--verify', revision + '^{commit}').decode().strip()
    if actual != revision:
        raise ValueError('The supplied SHA does not identify this exact source commit.')
    tree = _git(repo, 'rev-parse', revision + '^{tree}').decode().strip()
    timestamp = int(_git(repo, 'show', '-s', '--format=%ct', revision).decode().strip())
    date = datetime.fromtimestamp(max(315532800, min(timestamp, 4354819198)), timezone.utc)
    zip_time = (date.year, date.month, date.day, date.hour, date.minute, date.second // 2 * 2)
    files = _inventory(root)
    if any(_plain(path).is_relative_to(root) for path in (bridge, input_bridge, sdl)):
        raise ValueError('Reference build/runtime binaries must be separate from the frozen tree.')
    for name in VENDOR_FILES:
        validate_vendor_file(name, _read(root / name))
    expected = {name: hashlib.sha256(_read(path)).hexdigest() for name, path in
                (('native/d3d8.dll', bridge), ('native/dinput8.dll', input_bridge), ('runtime/SDL3.dll', sdl))}
    for member, digest in expected.items():
        if hashlib.sha256(_read(root / member)).hexdigest() != digest:
            raise ValueError('Portable binary hash does not match the supplied build/runtime: ' + member)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.portable-release-', dir=output.parent) as temporary:
        staged = Path(temporary) / 'portable.zip'
        inventory = []
        with zipfile.ZipFile(staged, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            def write(name, data):
                info = zipfile.ZipInfo('OpenSoccerMania/' + name, zip_time)
                info.create_system = 3; info.external_attr = 0o100644 << 16
                archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
            for name, path, size in files:
                data = _read(path)
                validate_vendor_file(name, data)
                digest = hashlib.sha256(data).hexdigest()
                if len(data) != size or (name in expected and digest != expected[name]):
                    raise ValueError('Portable input changed during packaging: ' + name)
                write(name, data)
                inventory.append({'path': name, 'size': size, 'sha256': digest})
            manifest = {'format': 1, 'kind': 'windows-portable', 'source_commit': revision,
                        'source_tree': tree, 'source_timestamp': timestamp,
                        'validated_binaries': expected, 'files': inventory}
            write('PORTABLE-MANIFEST.json', (json.dumps(manifest, sort_keys=True, indent=2) + '\n').encode('utf-8'))
        with staged.open('rb') as artifact:
            digest = hashlib.file_digest(artifact, 'sha256').hexdigest()
        staged_checksum = Path(temporary) / 'portable.sha256'
        staged_checksum.write_text(digest + '  ' + output.name + '\n', encoding='utf-8')
        created = []
        try:
            for source, destination in ((staged, output), (staged_checksum, checksum)):
                os.link(source, destination); created.append(destination)
        except BaseException:
            for destination in reversed(created):
                destination.unlink()
            raise
    return PackageResult(output, checksum, digest, revision, len(files) + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', default='.')
    parser.add_argument('--revision', required=True)
    parser.add_argument('--input', required=True, type=Path, dest='input_root')
    parser.add_argument('--bridge', required=True, type=Path)
    parser.add_argument('--input-bridge', required=True, type=Path)
    parser.add_argument('--sdl', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    options = parser.parse_args()
    result = package_portable(options.repo, options.revision, options.input_root, options.output,
                              bridge=options.bridge, input_bridge=options.input_bridge, sdl=options.sdl)
    print(json.dumps({'artifact': str(result.path), 'sha256_file': str(result.checksum_path),
                      'sha256': result.sha256, 'source_commit': result.source_commit,
                      'members': result.members}, indent=2))


if __name__ == '__main__':
    main()
