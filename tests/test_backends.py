import hashlib
import os
import subprocess
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from opensoccermania.backends import find_library_backend, install_backend, _validate_backend


def backend(machine=0x14c, proxy=False):
    raw = bytearray(1536)
    raw[:2] = b'MZ'; struct.pack_into('<I', raw, 0x3c, 0x80)
    raw[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', raw, 0x84, machine, 1)
    struct.pack_into('<HH', raw, 0x94, 224, 0x2002)
    struct.pack_into('<H', raw, 0x98, 0x10b)
    struct.pack_into('<I', raw, 0x98 + 92, 16)
    struct.pack_into('<II', raw, 0x98 + 96, 0x1000, 0x80)
    struct.pack_into('<IIII', raw, 0x178 + 8, 1024, 0x1000, 1024, 512)
    struct.pack_into('<IIIIII', raw, 512 + 16, 1, 1, 1, 0x1040, 0x1044, 0x1048)
    struct.pack_into('<I', raw, 576, 0x1100)
    struct.pack_into('<I', raw, 580, 0x1050)
    struct.pack_into('<H', raw, 584, 0)
    raw[592:608] = b'Direct3DCreate8\0'
    raw[768] = 0xc3
    identity = b'OpenSoccerMania dgVoodoo' if proxy else 'dgVoodoo'.encode('utf-16le')
    raw[900:900 + len(identity)] = identity
    return bytes(raw)


def private(root, name, own=False):
    entry = root / name; entry.mkdir()
    (entry / 'opensoccermania-source.json').write_text('{}')
    dll = entry / ('d3d8_backend.dll' if own else 'D3D8.dll')
    dll.write_bytes(backend())
    (entry / 'dgVoodoo.conf').write_text('[General]\n')
    if own:
        proxy = backend(proxy=True)
        (entry / 'D3D8.dll').write_bytes(proxy)
        (entry / 'opensoccermania-bridge.sha256').write_text(hashlib.sha256(proxy).hexdigest())
    return entry, dll, entry / 'dgVoodoo.conf'


class BackendTests(unittest.TestCase):
    def test_find_owned_backup_and_preserve_sources(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); entry, dll, conf = private(root, 'donor', own=True)
            self.assertEqual(find_library_backend(root), (dll.resolve(), conf.resolve()))
            destination = root / 'new'; destination.mkdir()
            (destination / 'opensoccermania-source.json').write_text('{}')
            before = dll.read_bytes(), conf.read_bytes()
            self.assertTrue(install_backend(destination, dll, conf))
            self.assertEqual((destination / 'd3d8.dll').read_bytes(), before[0])
            self.assertEqual((dll.read_bytes(), conf.read_bytes()), before)
            self.assertFalse(install_backend(destination, dll, conf))

    def test_reject_wrong_arch_truncation_proxy_and_missing_export(self):
        for raw in (backend(machine=0x8664), backend()[:550], backend(proxy=True), backend().replace(b'Direct3DCreate8', b'AbsentFunction')):
            with self.assertRaises(ValueError):
                _validate_backend(raw)

    def test_packed_backend_export_can_target_mapped_virtual_section(self):
        raw = bytearray(backend())
        struct.pack_into('<H', raw, 0x86, 2)
        section = 0x178 + 40
        struct.pack_into('<IIII', raw, section + 8, 4096, 0x2000, 0, 1536)
        struct.pack_into('<I', raw, section + 36, 0xe0000080)
        struct.pack_into('<I', raw, 576, 0x2100)
        _validate_backend(raw)

    def test_bridge_marker_mismatch_cannot_supply_backup(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); entry, dll, conf = private(root, 'donor', own=True)
            (entry / 'D3D8.dll').write_bytes(b'changed')
            self.assertIsNone(find_library_backend(root))

    def test_existing_foreign_config_is_never_changed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); _, dll, conf = private(root, 'donor')
            destination = root / 'new'; destination.mkdir()
            (destination / 'opensoccermania-source.json').write_text('{}')
            (destination / 'dgVoodoo.conf').write_text('keep foreign settings')
            self.assertFalse(install_backend(destination, dll, conf))
            self.assertEqual((destination / 'dgVoodoo.conf').read_text(), 'keep foreign settings')
            self.assertFalse((destination / 'd3d8.dll').exists())

    def test_failed_second_publication_rolls_back(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); _, dll, conf = private(root, 'donor')
            destination = root / 'new'; destination.mkdir()
            (destination / 'opensoccermania-source.json').write_text('{}')
            import os
            real_link = os.link
            calls = []
            def fail_second(source, target):
                calls.append(target)
                if len(calls) == 2:
                    raise OSError('injected publication failure')
                return real_link(source, target)
            with patch('opensoccermania.backends.os.link', side_effect=fail_second):
                with self.assertRaisesRegex(OSError, 'injected'):
                    install_backend(destination, dll, conf)
            self.assertEqual([p.name for p in destination.iterdir()], ['opensoccermania-source.json'])

    def test_existing_owned_bridge_and_backend_remain_unchanged(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); _, dll, conf = private(root, 'donor')
            entry, _, _ = private(root, 'existing', own=True)
            before = {p.name: p.read_bytes() for p in entry.iterdir()}
            self.assertFalse(install_backend(entry, dll, conf))
            self.assertEqual({p.name: p.read_bytes() for p in entry.iterdir()}, before)
            with self.assertRaisesRegex(ValueError, 'separate'):
                install_backend(dll.parent, dll, conf)
            original = root / 'original'; original.mkdir()
            with self.assertRaises((ValueError, OSError)):
                install_backend(original, dll, conf)
            self.assertEqual(list(original.iterdir()), [])

    @unittest.skipUnless(os.name == 'nt', 'Windows junction regression.')
    def test_library_does_not_follow_junction_to_outside_donor(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); library = root / 'library'; library.mkdir()
            outside, dll, conf = private(root, 'outside')
            link = library / 'donor'
            result = subprocess.run(['cmd.exe', '/c', 'mklink', '/J', str(link), str(outside)], capture_output=True,
                                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            if result.returncode:
                self.skipTest('Junction creation unavailable.')
            try:
                self.assertIsNone(find_library_backend(library))
                with self.assertRaisesRegex(ValueError, 'Linked'):
                    install_backend(link, dll, conf)
            finally:
                link.rmdir()

    def test_missing_library_has_no_backend(self):
        with tempfile.TemporaryDirectory() as temp:
            self.assertIsNone(find_library_backend(Path(temp) / 'not-created'))

    def test_library_enumeration_is_bounded(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for index in range(201):
                (root / str(index)).mkdir()
            self.assertIsNone(find_library_backend(root))


class LauncherBackendImportTests(unittest.TestCase):
    def launcher(self):
        from opensoccermania.launcher import Launcher
        from types import SimpleNamespace
        import queue
        launcher = Launcher.__new__(Launcher)
        launcher.status = SimpleNamespace(set=lambda value: None)
        launcher.queue = queue.Queue()
        launcher.run_work = lambda work: work()
        return launcher

    def test_import_reuses_private_owner_backend_before_ready(self):
        from tests.test_core import fixture
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); home = root / 'local'; library = home / 'library'; library.mkdir(parents=True)
            _, dll, conf = private(library, 'existing-owner-copy', own=True)
            source = fixture(root / 'original')
            before = (source / 'Game.exe').read_bytes(), dll.read_bytes(), conf.read_bytes()
            with patch('opensoccermania.launcher.HOME', home):
                self.launcher().import_path(source)
            imported = [p for p in library.iterdir() if p.name != 'existing-owner-copy'][0]
            self.assertEqual((imported / 'd3d8.dll').read_bytes(), before[1])
            self.assertEqual((source / 'Game.exe').read_bytes(), before[0])
            self.assertFalse((source / 'd3d8.dll').exists())
            self.assertEqual((dll.read_bytes(), conf.read_bytes()), before[1:])

    def test_explicit_backend_finishes_import_without_library_donor(self):
        from tests.test_core import fixture
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = fixture(root / 'original')
            _, dll, conf = private(root, 'owner-selected-wrapper')
            home = root / 'local'
            with patch('opensoccermania.launcher.HOME', home):
                self.launcher().import_path(source, backend=(dll, conf))
            entry = next((home / 'library').iterdir())
            self.assertEqual((entry / 'd3d8.dll').read_bytes(), dll.read_bytes())

    def test_import_never_replaces_incomplete_original_graphics_files(self):
        from tests.test_core import fixture
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = fixture(root / 'original')
            (source / 'D3D8.dll').write_bytes(b'foreign wrapper to preserve')
            home = root / 'local'
            with patch('opensoccermania.launcher.HOME', home):
                with self.assertRaisesRegex(ValueError, 'existing files were preserved'):
                    self.launcher().import_path(source)
            entry = next((home / 'library').iterdir())
            self.assertEqual((source / 'D3D8.dll').read_bytes(), b'foreign wrapper to preserve')
            self.assertEqual((entry / 'D3D8.dll').read_bytes(), b'foreign wrapper to preserve')

    def test_real_private_owner_backend_read_only_when_available(self):
        library = Path(__file__).resolve().parent.parent / 'local/library'
        if not library.is_dir():
            self.skipTest('Owner-supplied private inputs are not distributed.')
        pair = find_library_backend(library)
        if pair is None:
            self.skipTest('No local recognized owner-supplied backend.')
        dll, conf = pair
        before = hashlib.sha256(dll.read_bytes()).hexdigest(), hashlib.sha256(conf.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory() as temp:
            entry = Path(temp)
            (entry / 'opensoccermania-source.json').write_text('{}')
            self.assertTrue(install_backend(entry, dll, conf))
            self.assertEqual(hashlib.sha256((entry / 'd3d8.dll').read_bytes()).hexdigest(), before[0])
        self.assertEqual((hashlib.sha256(dll.read_bytes()).hexdigest(), hashlib.sha256(conf.read_bytes()).hexdigest()), before)

    def test_missing_backend_requests_owner_supplied_files(self):
        from tests.test_core import fixture
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = fixture(root / 'original')
            with patch('opensoccermania.launcher.HOME', root / 'local'):
                with self.assertRaisesRegex(ValueError, 'owner-supplied'):
                    self.launcher().import_path(source)



if __name__ == '__main__':
    unittest.main()
