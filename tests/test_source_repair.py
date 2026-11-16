import tempfile
import unittest
import os
import subprocess
import stat
import hashlib
from dataclasses import replace
from unittest.mock import patch
from pathlib import Path
from opensoccermania.sources import discover, discover_assets, prepare, prepare_with_executable, _remove_private_staging
from tests.test_core import fixture


class RepairTests(unittest.TestCase):
    def test_asset_only_requires_explicit_intact_donor_and_preserves_both(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            broken = fixture(root / 'broken'); donor = fixture(root / 'donor')
            original = bytes(1024); (broken / 'Game.exe').write_bytes(original)
            assets = discover_assets(broken)
            self.assertFalse(assets.runnable)
            with self.assertRaises(ValueError):
                discover(broken)
            target = prepare_with_executable(assets, discover(donor), root / 'private')
            self.assertEqual((target / 'Game.exe').read_bytes(), (donor / 'Game.exe').read_bytes())
            self.assertEqual((broken / 'Game.exe').read_bytes(), original)
            self.assertTrue((target / 'Mania/CD/Messages/spanish.txt').exists())
            with self.assertRaises(ValueError):
                prepare_with_executable(assets, discover(donor), donor / 'private')

    def test_changed_donor_does_not_publish_or_leave_staging(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); broken = fixture(root / 'broken'); donor = fixture(root / 'donor')
            source = discover(donor)
            (donor / 'Game.exe').write_bytes(b'MZchanged')
            with self.assertRaisesRegex(ValueError, 'changed'):
                prepare_with_executable(discover_assets(broken), source, root / 'private')
            self.assertFalse((root / 'private').exists())
            self.assertFalse(list(root.glob('.import-*')))

    def test_cleanup_failure_retains_original_import_diagnostic(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); original = fixture(root / 'original')
            selected = replace(discover(original), sha256='0' * 64)
            with patch('opensoccermania.sources._remove_private_staging', side_effect=PermissionError('cleanup denied')):
                with self.assertRaisesRegex(RuntimeError, 'cleanup denied') as failure:
                    prepare(selected, root / 'library/private')
            self.assertIn('executable changed', str(failure.exception))
            self.assertIn('.import-', str(failure.exception))
            self.assertIsInstance(failure.exception.__cause__, PermissionError)
            self.assertFalse((root / 'library/private').exists())
            self.assertTrue(list((root / 'library').glob('.import-*')))

    def test_cleanup_refuses_unconfined_target(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve(); library = root / 'library'; library.mkdir()
            outside = root / '.import-outside'; outside.mkdir(); (outside / 'keep').write_text('original')
            with self.assertRaisesRegex(ValueError, 'outside'):
                _remove_private_staging(outside, library)
            self.assertEqual((outside / 'keep').read_text(), 'original')

    @unittest.skipUnless(os.name == 'nt', 'Windows 8.3 alias regression.')
    def test_cleanup_accepts_canonical_identity_of_windows_short_alias(self):
        import ctypes
        with tempfile.TemporaryDirectory() as temp:
            library = Path(temp).resolve() / 'private library with long name'; library.mkdir()
            stage = Path(tempfile.mkdtemp(prefix='.import-', dir=library))
            (stage / 'copied.txt').write_text('private copy')
            buffer = ctypes.create_unicode_buffer(32768)
            length = ctypes.windll.kernel32.GetShortPathNameW(str(stage), buffer, len(buffer))
            if not length or length >= len(buffer) or Path(buffer.value) == stage:
                self.skipTest('This filesystem does not provide distinct 8.3 aliases.')
            _remove_private_staging(Path(buffer.value), library)
            self.assertFalse(stage.exists())

    @unittest.skipUnless(os.name == 'nt', 'Windows private staging junction regression.')
    def test_cleanup_refuses_staging_junction_without_touching_target(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve(); library = root / 'library'; library.mkdir()
            stage = library / '.import-fixture'; stage.mkdir(); (stage / 'keep').write_text('staged')
            outside = root / 'outside'; outside.mkdir(); (outside / 'keep').write_text('original')
            link = stage / 'linked'
            result = subprocess.run(['cmd.exe', '/c', 'mklink', '/J', str(link), str(outside)], capture_output=True,
                                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            if result.returncode:
                self.skipTest('Junction creation unavailable.')
            try:
                with self.assertRaisesRegex(ValueError, 'Linked'):
                    _remove_private_staging(stage, library)
                self.assertEqual((outside / 'keep').read_text(), 'original')
                self.assertEqual((stage / 'keep').read_text(), 'staged')
            finally:
                link.rmdir()

    @unittest.skipUnless(os.name == 'nt', 'Windows copied read-only attribute regression.')
    def test_failed_import_removes_readonly_staging_preserving_original(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); original = fixture(root / 'original')
            message = original / 'Mania/CD/Messages/spanish.txt'
            message.chmod(stat.S_IREAD)
            before = message.read_bytes(), message.stat().st_file_attributes
            exe_hash = hashlib.sha256((original / 'Game.exe').read_bytes()).hexdigest()
            # Force validation failure after real copytree/copy2 copied readonly data.
            selected = replace(discover(original), sha256='0' * 64)
            with self.assertRaisesRegex(ValueError, 'changed'):
                prepare(selected, root / 'library/private')
            self.assertFalse((root / 'library/private').exists())
            self.assertFalse(list((root / 'library').glob('.import-*')))
            self.assertEqual((message.read_bytes(), message.stat().st_file_attributes), before)
            self.assertEqual(hashlib.sha256((original / 'Game.exe').read_bytes()).hexdigest(), exe_hash)

    @unittest.skipUnless(os.name == 'nt', 'Windows junction regression.')
    def test_windows_junction_inside_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = fixture(root / 'source'); outside = root / 'outside'
            outside.mkdir(); (outside / 'keep').write_text('original')
            link = source / 'Mania/CD/linked'
            result = subprocess.run(['cmd.exe', '/c', 'mklink', '/J', str(link), str(outside)],
                                    capture_output=True, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            if result.returncode:
                self.skipTest('Junction creation unavailable.')
            try:
                with self.assertRaisesRegex(ValueError, 'Linked'):
                    discover(source)
                with self.assertRaisesRegex(ValueError, 'Linked'):
                    discover_assets(source)
                self.assertEqual((outside / 'keep').read_text(), 'original')
            finally:
                # Remove the junction itself, never its target.
                link.rmdir()

    def test_symlink_source_is_rejected_before_discovery(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = fixture(root / 'source'); link = root / 'link'
            try:
                link.symlink_to(source, target_is_directory=True)
            except OSError:
                self.skipTest('Creating symlinks requires Windows developer mode or permission.')
            with self.assertRaisesRegex(ValueError, 'Linked'):
                discover(link)


if __name__ == '__main__':
    unittest.main()
