"""Provision the dedicated bundled graphics backend into private game imports."""
from pathlib import Path
import hashlib
import os
import struct
import tempfile
from .disc_images import _unlinked

MAX_DLL_BYTES = 32 * 1024**2
MAX_CONF_BYTES = 1024**2
MAX_LIBRARY_ENTRIES = 200
BUNDLED_BACKEND_HASHES = {
    'd3d8.dll': '25a0ceeb85fcc4675c6f964442eecc14fedd504effd39c931a627249ad4685e2',
    'dgVoodoo.conf': '45cc0eb8e6ed4f067aca1b2e4ac1268dfbd164bbbd06d264c53e8a60642c4dbf',
}


def bundled_backend(root):
    folder = Path(root) / 'runtime/graphics/dgVoodoo'
    try:
        for name, digest in BUNDLED_BACKEND_HASHES.items():
            raw = _read_file(folder / name, MAX_DLL_BYTES if name.endswith('.dll') else MAX_CONF_BYTES)
            if hashlib.sha256(raw).hexdigest() != digest:
                raise ValueError('Bundled graphics dependency checksum mismatch.')
        _validate_backend(_read_file(folder / 'd3d8.dll', MAX_DLL_BYTES))
    except (OSError, ValueError) as error:
        raise ValueError('The OpenSoccerMania package is incomplete or damaged. Extract the complete portable ZIP again; no separate graphics download is required.') from error
    return folder / 'd3d8.dll', folder / 'dgVoodoo.conf'


def ensure_backend(entry, root):
    """Fill missing private graphics files from the pack, without component dialogs."""
    entry = _unlinked(entry)
    _read_file(_named(entry, 'opensoccermania-source.json'), MAX_CONF_BYTES)
    dll = _named(entry, 'd3d8.dll'); conf = _named(entry, 'dgVoodoo.conf')
    marker = _named(entry, 'opensoccermania-bridge.sha256')
    if dll.exists():
        actual = _named(entry, 'd3d8_backend.dll') if marker.exists() and _owned_bridge(entry) else dll
        try:
            _validate_backend(_read_file(actual, MAX_DLL_BYTES))
        except (OSError, ValueError) as error:
            raise ValueError('This private entry has an unrecognized graphics setup; existing files were preserved. Import a clean game ISO or folder to rebuild it automatically.') from error
        if conf.exists():
            _read_file(conf, MAX_CONF_BYTES)
            return False
    elif marker.exists() or _named(entry, 'd3d8_backend.dll').exists():
        raise ValueError('This private graphics installation is incomplete. Import a clean game ISO or folder to rebuild it automatically.')
    supplied = bundled_backend(root)
    if not dll.exists() and not conf.exists():
        return install_backend(entry, *supplied)
    # Complete a partial import with only missing files; never replace a file
    # already present in the private entry or touch the source installation.
    for target, source in ((dll, supplied[0]), (conf, supplied[1])):
        if target.exists():
            continue
        with tempfile.NamedTemporaryFile(prefix='.graphics-', dir=entry, delete=False) as stream:
            staged = Path(stream.name)
            stream.write(_read_file(source, MAX_DLL_BYTES if source.suffix == '.dll' else MAX_CONF_BYTES))
            stream.flush(); os.fsync(stream.fileno())
        try:
            os.link(staged, target)
        finally:
            staged.unlink(missing_ok=True)
    return True


def _named(folder, name):
    matches = [p for p in folder.iterdir() if p.name.casefold() == name.casefold()]
    if len(matches) > 1:
        raise ValueError('Case-colliding private library files are unsupported.')
    return matches[0] if matches else folder / name


def _read_file(path, limit):
    path = _unlinked(path)
    if not path.is_file() or not 0 < path.stat().st_size <= limit:
        raise ValueError('Graphics backend file is missing, empty or too large.')
    with path.open('rb') as source:
        result = source.read(limit + 1)
    if len(result) > limit:
        raise ValueError('Graphics backend changed or exceeded the size limit.')
    return result


def _validate_backend(raw):
    """Validate bounded PE32 i386 DLL exports without loading native code."""
    def number(offset, fmt):
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(raw):
            raise ValueError('Truncated graphics backend PE headers.')
        return struct.unpack_from(fmt, raw, offset)[0]
    if raw[:2] != b'MZ':
        raise ValueError('Graphics backend is not a PE DLL.')
    pe = number(0x3c, '<I')
    if raw[pe:pe + 4] != b'PE\0\0' or number(pe + 4, '<H') != 0x14c or not number(pe + 22, '<H') & 0x2000:
        raise ValueError('Graphics backend must be an x86 DLL.')
    sections = number(pe + 6, '<H'); optional_size = number(pe + 20, '<H'); optional = pe + 24
    if not 1 <= sections <= 96 or optional_size < 104 or number(optional, '<H') != 0x10b or number(optional + 92, '<I') < 1:
        raise ValueError('Graphics backend requires valid PE32 export headers.')
    section_table = optional + optional_size
    if section_table + sections * 40 > len(raw):
        raise ValueError('Truncated graphics backend section table.')
    spans = []
    mapped = []
    for index in range(sections):
        section = section_table + index * 40
        virtual = number(section + 12, '<I'); size = number(section + 16, '<I'); offset = number(section + 20, '<I')
        if offset + size > len(raw):
            raise ValueError('Truncated graphics backend section.')
        spans.append((virtual, offset, size))
        mapped.append((virtual, max(size, number(section + 8, '<I'))))
    def rva(address, length):
        matches = [(offset + address - virtual) for virtual, offset, size in spans if virtual <= address and address - virtual + length <= size]
        if len(matches) != 1 or length < 0:
            raise ValueError('Invalid graphics backend export address.')
        return matches[0]
    export_address = number(optional + 96, '<I'); export_size = number(optional + 100, '<I')
    if export_size < 40:
        raise ValueError('Graphics backend has no valid exports.')
    exports = rva(export_address, 40)
    functions_count = number(exports + 20, '<I'); count = number(exports + 24, '<I')
    if not 1 <= count <= 65536 or not 1 <= functions_count <= 65536:
        raise ValueError('Invalid graphics backend export count.')
    names = rva(number(exports + 32, '<I'), count * 4)
    ordinals = rva(number(exports + 36, '<I'), count * 2)
    functions = rva(number(exports + 28, '<I'), functions_count * 4)
    found = False
    for index in range(count):
        address = number(names + index * 4, '<I')
        name = rva(address, 1)
        end = raw.find(b'\0', name, min(name + 256, len(raw)))
        if end < 0:
            raise ValueError('Unterminated graphics backend export name.')
        if raw[name:end] == b'Direct3DCreate8':
            ordinal = number(ordinals + index * 2, '<H')
            if ordinal >= functions_count:
                raise ValueError('Invalid Direct3DCreate8 export ordinal.')
            function = number(functions + ordinal * 4, '<I')
            if export_address <= function < export_address + export_size:
                raise ValueError('Forwarded Direct3DCreate8 backends are unsupported.')
            if function == 0 or sum(start <= function < start + size for start, size in mapped) != 1:
                raise ValueError('Invalid Direct3DCreate8 function address.')
            found = True
    lowered = raw.lower()
    if not found or b'opensoccermania' in lowered or 'opensoccermania'.encode('utf-16le') in lowered:
        raise ValueError('Select a real graphics backend, not the OpenSoccerMania bridge.')
    # Conservative vendor evidence, not a security signature or DLL trust proof.
    if not any(value in lowered for value in (b'dgvoodoo', 'dgvoodoo'.encode('utf-16le'))):
        raise ValueError('Only recognized dgVoodoo graphics backends are supported.')


def _owned_bridge(entry):
    marker = _named(entry, 'opensoccermania-bridge.sha256')
    target = _named(entry, 'd3d8.dll')
    if not marker.exists() or not target.exists():
        return False
    expected = _read_file(marker, 128).decode('ascii').strip()
    return len(expected) == 64 and hashlib.sha256(_read_file(target, MAX_DLL_BYTES)).hexdigest() == expected


def find_library_backend(library_root):
    """Return (DLL, configuration) from at most 200 immediate private imports."""
    try:
        root = _unlinked(library_root)
    except FileNotFoundError:
        return None
    if not root.is_dir():
        return None
    # Limit enumeration itself; an oversized/unrelated library is not scanned.
    candidates = []
    with os.scandir(root) as entries:
        for item in entries:
            if len(candidates) >= MAX_LIBRARY_ENTRIES:
                return None
            candidates.append(Path(item.path))
    for entry in sorted(candidates, key=lambda path: path.name.casefold()):
        try:
            entry = _unlinked(entry)
            if not entry.is_dir() or entry.parent != root:
                continue
            _read_file(_named(entry, 'opensoccermania-source.json'), MAX_CONF_BYTES)
            conf = _named(entry, 'dgVoodoo.conf'); _read_file(conf, MAX_CONF_BYTES)
            marker = _named(entry, 'opensoccermania-bridge.sha256')
            if marker.exists():
                if not _owned_bridge(entry):
                    continue
                dll = _named(entry, 'd3d8_backend.dll')
            else:
                dll = _named(entry, 'd3d8.dll')
            _validate_backend(_read_file(dll, MAX_DLL_BYTES))
            return dll, conf
        except (OSError, ValueError, UnicodeError):
            continue
    return None


def install_backend(entry, dll, conf):
    """Copy a validated owner backend into a fresh private entry; never replace.

    Return False for an entry with any existing graphics DLL/configuration/marker.
    Failure rolls back only files newly created by this call. No native code runs.
    """
    entry = _unlinked(entry)
    if not entry.is_dir():
        raise ValueError('Choose an existing private library entry.')
    _read_file(_named(entry, 'opensoccermania-source.json'), MAX_CONF_BYTES)
    dll = _unlinked(dll); conf = _unlinked(conf)
    if dll.is_relative_to(entry) or conf.is_relative_to(entry):
        raise ValueError('Backend source must be separate from its destination.')
    raw = _read_file(dll, MAX_DLL_BYTES); _validate_backend(raw)
    configuration = _read_file(conf, MAX_CONF_BYTES)
    names = ('d3d8.dll', 'dgVoodoo.conf', 'd3d8_backend.dll', 'opensoccermania-bridge.sha256')
    for name in names:
        target = _named(entry, name)
        if target.exists() or target.is_symlink():
            _unlinked(target)
            return False
    created = []
    with tempfile.TemporaryDirectory(prefix='.backend-', dir=entry) as temporary:
        staged = Path(temporary)
        for name, data in (('d3d8.dll', raw), ('dgVoodoo.conf', configuration)):
            with (staged / name).open('xb') as output:
                output.write(data)
                output.flush()
                os.fsync(output.fileno())
        try:
            for name in ('d3d8.dll', 'dgVoodoo.conf'):
                target = entry / name
                # Same-volume hard-link publication is atomic and refuses to
                # overwrite existing files on Windows and POSIX alike.
                os.link(staged / name, target)
                created.append(target)
            return True
        except BaseException:
            for target in reversed(created):
                target.unlink()
            raise
