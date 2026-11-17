"""Version-locked compatibility patches with byte checks and original backups."""
from dataclasses import dataclass
from pathlib import Path
import hashlib
import json
import os
import stat
import tempfile
from .disc_images import _unlinked


@dataclass
class Patch:
    name: str
    sha256: str
    edits: list[tuple[int, bytes, bytes]]


SUPPORTED_HASHES = [
    '58a98cf9fa332bbc0ec301e16d779c8dbe5497b69778c22e5c363f892fbcb19d',
    'b89ce11443b64b0da19e55f88bfe7993a3130e01137ee54517c14896f8cca07c',
]
SUPPORTED_RUNTIME_HASHES=SUPPORTED_HASHES+[
    '841cd6150e0298e00720387796d520b06e079920181cb1e30dec51d2b64dc68b',
    '067a46c21a1ce7faf1d761041bff62b4188a53f6a484ae1c95f3df4cf1f00ff4',
]


def portable_startup(sha256):
    if sha256 not in SUPPORTED_HASHES:
        raise ValueError('This executable version is not supported by the portable startup patch.')
    return Patch('portable-startup-v1', sha256, [
        # 0x4010e9: validate the legacy .dat authorisation sidecar.
        (0x10e9, bytes.fromhex('e8 e2 01 00 00'), bytes.fromhex('b0 01 90 90 90')),
        # 0x401142: start the registry-located *_Code.exe legacy helper.
        (0x1142, bytes.fromhex('e8 99 02 00 00'), bytes.fromhex('b0 01 90 90 90')),
    ])


MAX_EXECUTABLE_BYTES = 64 * 1024**2
MAX_RECORD_BYTES = 64 * 1024


def _exists(path):
    try:
        path.lstat()
        return True
    except FileNotFoundError:
        return False


def _read_private(path, entry, limit=MAX_EXECUTABLE_BYTES):
    path = _unlinked(path)
    info = path.lstat()
    if path.parent != entry or not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or info.st_size > limit:
        raise ValueError('Patch files must be ordinary, unlinked files inside the private entry.')
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Private patch file exceeds its size limit.')
    return data


def _target(exe):
    exe = _unlinked(exe); entry = exe.parent
    provenance = entry / 'opensoccermania-source.json'
    try:
        info = json.loads(_read_private(provenance, entry, MAX_RECORD_BYTES))
    except (OSError, ValueError, TypeError) as error:
        raise ValueError('Patching requires a valid private import entry; original game folders are never patched.') from error
    if (not isinstance(info, dict) or not isinstance(info.get('source'), str) or not info['source']
            or not isinstance(info.get('sha256'), str) or len(info['sha256']) != 64
            or any(character not in '0123456789abcdef' for character in info['sha256'])):
        raise ValueError('The private import provenance is incomplete.')
    for field in ('source', 'executable_source'):
        if info.get(field):
            if not isinstance(info[field], str):
                raise ValueError('The private import provenance is incomplete.')
            source = Path(info[field]).resolve()
            if entry.is_relative_to(source) or source.is_relative_to(entry):
                raise ValueError('The private executable must stay separate from its original source.')
    _read_private(exe, entry)
    return exe, entry, info['sha256']


def _digest(data):
    return hashlib.sha256(data).hexdigest()


def _modified(original, patch):
    if _digest(original) != patch.sha256:
        raise ValueError('Original backup checksum does not match the executable version.')
    if not isinstance(patch.edits, (list, tuple)) or len(patch.edits) > 1000:
        raise ValueError('Invalid patch edit count.')
    modified = bytearray(original); ranges = []
    for offset, before, after in patch.edits:
        if (type(offset) is not int or offset < 0 or not isinstance(before, bytes) or not isinstance(after, bytes)
                or not before or len(before) != len(after) or offset + len(before) > len(original)
                or original[offset:offset + len(before)] != before
                or any(offset < end and start < offset + len(before) for start, end in ranges)):
            raise ValueError('Expected original bytes or patch edit bounds do not match.')
        ranges.append((offset, offset + len(before)))
        modified[offset:offset + len(after)] = after
    return bytes(modified)


def _record(patch, modified):
    return {'format': 2, 'patch': patch.name, 'original_sha256': patch.sha256,
            'patched_sha256': _digest(modified),
            'edits': [{'offset': offset, 'before': before.hex(), 'after': after.hex()} for offset, before, after in patch.edits]}


def _validate_record(info, expected):
    if not isinstance(info, dict) or any(info.get(key) != expected[key] for key in ('patch', 'original_sha256', 'patched_sha256')):
        raise ValueError('The patch recovery record does not match the derived executable bytes.')
    if ('edits' in info and info['edits'] != expected['edits']) or info.get('format', 1) not in (1, 2):
        raise ValueError('The patch recovery record has different edits or an unsupported version.')


def _stage(entry, data, label):
    handle, name = tempfile.mkstemp(prefix='.' + label + '-', suffix='.tmp', dir=entry)
    path = Path(name)
    try:
        with os.fdopen(handle, 'wb') as stream:
            stream.write(data); stream.flush(); os.fsync(stream.fileno())
    except BaseException:
        path.unlink()
        raise
    return path


def _publish(path, data, entry, *, new_only=False):
    temporary = _stage(entry, data, path.stem)
    try:
        if new_only:
            if os.name == 'nt':
                os.rename(temporary, path)  # Windows rename refuses existing files.
            else:
                os.link(temporary, path)
        else:
            if _exists(path):
                _read_private(path, entry)
            os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def _access_error(exe):
    return PermissionError(f'Could not update the private executable {exe}. Close the game and check that this private library folder is writable, then retry. The original backup and recovery record are preserved; deleting the library or running as administrator is not required.')


def _replace_executable(exe, data, expected_current, entry):
    temporary = _stage(entry, data, exe.stem + '.patch')
    def unchanged():
        if _digest(_read_private(exe, entry)) != expected_current:
            raise ValueError('The private executable changed during patching. No replacement was made.')
    try:
        unchanged()
        try:
            os.replace(temporary, exe)
        except PermissionError as error:
            unchanged()
            info = exe.lstat()
            readonly = (getattr(info, 'st_file_attributes', 0) & 1) if os.name == 'nt' else not (info.st_mode & stat.S_IWUSR)
            if not readonly:
                raise _access_error(exe) from error
            # Only this proven private, singly-linked target may lose readonly.
            # Owner inputs, backups, links and ACL permissions are never altered.
            try:
                exe.chmod(info.st_mode | stat.S_IWUSR)
                unchanged()
                os.replace(temporary, exe)
            except PermissionError as retry_error:
                raise _access_error(exe) from retry_error
    finally:
        if temporary.exists():
            temporary.unlink()


def apply_patch(exe, patch):
    exe, entry, imported_hash = _target(exe)
    if imported_hash != patch.sha256:
        raise ValueError('Unsupported executable version for this private import.')
    current = _read_private(exe, entry); current_hash = _digest(current)
    backup = exe.with_suffix('.exe.original'); manifest = exe.with_suffix('.patch.json')
    if _exists(backup):
        original = _read_private(backup, entry)
        if _digest(original) != patch.sha256:
            raise ValueError('Original backup checksum does not match. No files were replaced.')
    elif current_hash == patch.sha256:
        original = current
    else:
        raise ValueError('Original backup is missing or the executable version changed.')
    modified = _modified(original, patch); expected = _record(patch, modified)
    if _exists(manifest):
        try:
            info = json.loads(_read_private(manifest, entry, MAX_RECORD_BYTES))
        except (ValueError, TypeError) as error:
            raise ValueError('The patch recovery record is damaged. No executable was replaced.') from error
        _validate_record(info, expected)
    if current_hash not in (patch.sha256, expected['patched_sha256']):
        raise ValueError('The executable changed after patching. Refusing to overwrite it.')
    encoded = (json.dumps(expected, indent=2) + '\n').encode('utf-8')
    if len(encoded) > MAX_RECORD_BYTES:
        raise ValueError('Patch recovery record exceeds its size limit.')
    if not _exists(backup):
        _publish(backup, original, entry, new_only=True)
    if _digest(_read_private(backup, entry)) != patch.sha256:
        raise ValueError('Original backup changed during patching. No executable was replaced.')
    if not _exists(manifest) or _read_private(manifest, entry, MAX_RECORD_BYTES) != encoded:
        _publish(manifest, encoded, entry)
    # Exact original + valid pending manifest is a resumable interrupted import.
    if current_hash == patch.sha256:
        _replace_executable(exe, modified, current_hash, entry)


def restore_patch(exe):
    exe, entry, imported_hash = _target(exe)
    backup = exe.with_suffix('.exe.original'); manifest = exe.with_suffix('.patch.json')
    if not _exists(backup):
        raise ValueError('Original backup is missing. No executable was replaced.')
    original = _read_private(backup, entry)
    if _digest(original) != imported_hash:
        raise ValueError('Original backup checksum does not match.')
    current_hash = _digest(_read_private(exe, entry))
    if not _exists(manifest):
        if current_hash == imported_hash:
            return  # Restoring an already restored, verified original is safe.
        raise ValueError('Patch recovery record is missing; retry the matching patch before restoring.')
    try:
        info = json.loads(_read_private(manifest, entry, MAX_RECORD_BYTES))
        if imported_hash in SUPPORTED_HASHES:
            patch = portable_startup(imported_hash)
        else:
            edits = info['edits']
            if not isinstance(edits, list) or len(edits) > 1000:
                raise ValueError('Invalid edit count.')
            patch = Patch(info['patch'], imported_hash,
                          [(edit['offset'], bytes.fromhex(edit['before']), bytes.fromhex(edit['after'])) for edit in edits])
    except (ValueError, TypeError, KeyError, AttributeError) as error:
        raise ValueError('Patch recovery record cannot establish the expected executable bytes; retry the matching patch before restoring.') from error
    modified = _modified(original, patch); expected = _record(patch, modified)
    _validate_record(info, expected)
    if current_hash not in (imported_hash, expected['patched_sha256']):
        raise ValueError('The executable changed after patching. Refusing to overwrite it.')
    if current_hash != imported_hash:
        _replace_executable(exe, original, current_hash, entry)
    # If deletion fails the original+record state remains valid for another retry.
    manifest.unlink()
