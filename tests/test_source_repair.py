import tempfile
import unittest
import os
import subprocess
from pathlib import Path
from opensoccermania.sources import discover, discover_assets, prepare, prepare_with_executable
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
