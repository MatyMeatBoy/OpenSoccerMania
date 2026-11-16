"""Version-locked compatibility patches with byte checks and original backups."""
from dataclasses import dataclass
from pathlib import Path
import hashlib
import json


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


def apply_patch(exe, patch):
    exe=Path(exe)
    original=exe.read_bytes()
    backup=exe.with_suffix('.exe.original')
    manifest=exe.with_suffix('.patch.json')
    if manifest.exists():
        info=json.loads(manifest.read_text(encoding='utf-8'))
        if info['patch']==patch.name and info['original_sha256']==patch.sha256 and hashlib.sha256(original).hexdigest()==info['patched_sha256']:
            return
        raise ValueError('A different patch or executable version is already present.')
    if hashlib.sha256(original).hexdigest()!=patch.sha256:
        raise ValueError('Unsupported executable version. No changes were made.')
    modified=bytearray(original)
    for offset,before,after in patch.edits:
        if len(before)!=len(after) or original[offset:offset+len(before)]!=before:
            raise ValueError('Expected original bytes do not match. No changes were made.')
        modified[offset:offset+len(after)]=after
    if backup.exists() and backup.read_bytes()!=original:
        raise ValueError('The original backup belongs to a different executable version.')
    backup.write_bytes(original)
    temp=exe.with_suffix('.patch.tmp')
    temp.write_bytes(modified)
    # Write the record before mutation, so a failed rename still has recovery data.
    manifest.write_text(json.dumps({'patch':patch.name,'original_sha256':patch.sha256,'patched_sha256':hashlib.sha256(modified).hexdigest()},indent=2),encoding='utf-8')
    temp.replace(exe)


def restore_patch(exe):
    exe=Path(exe); backup=exe.with_suffix('.exe.original');manifest=exe.with_suffix('.patch.json')
    info=json.loads(manifest.read_text(encoding='utf-8'))
    data=backup.read_bytes()
    if hashlib.sha256(data).hexdigest()!=info['original_sha256']:
        raise ValueError('Original backup checksum does not match.')
    if hashlib.sha256(exe.read_bytes()).hexdigest() not in [info['patched_sha256'],info['original_sha256']]:
        raise ValueError('The executable changed after patching. Refusing to overwrite it.')
    temporary=exe.with_suffix('.restore.tmp');temporary.write_bytes(data);temporary.replace(exe)
    manifest.unlink()
