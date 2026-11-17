import errno
import os
import stat
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from opensoccermania.storage import select_home


class StorageTests(unittest.TestCase):
    def test_protected_logs_or_library_uses_writable_user_storage(self):
        for blocked_name in ('logs', 'library'):
            with self.subTest(blocked_name=blocked_name), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                home = root / 'local'
                (home / blocked_name).mkdir(parents=True)
                original = tempfile.TemporaryFile

                def permission_gate(*args, **kwargs):
                    if Path(kwargs['dir']) == home / blocked_name:
                        raise PermissionError(errno.EACCES, 'Protected data subdirectory')
                    return original(*args, **kwargs)

                with patch('opensoccermania.storage.tempfile.TemporaryFile', side_effect=permission_gate):
                    self.assertEqual(select_home(root, {'LOCALAPPDATA': str(root / 'user')}),
                                     root / 'user/OpenSoccerMania')

    @unittest.skipUnless(os.name == 'nt', 'Windows read-only preferences.')
    def test_readonly_preferences_select_user_storage(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            home = root / 'local'
            home.mkdir()
            preferences = home / 'launcher.json'
            preferences.write_text('preserve')
            preferences.chmod(stat.S_IREAD)
            try:
                self.assertEqual(select_home(root, {'LOCALAPPDATA': str(root / 'user')}),
                                 root / 'user/OpenSoccerMania')
                self.assertEqual(preferences.read_text(), 'preserve')
            finally:
                preferences.chmod(stat.S_IREAD | stat.S_IWRITE)

    def test_writable_portable_keeps_existing_preferences(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'portable'
            home = root / 'local'
            home.mkdir(parents=True)
            (home / 'launcher.json').write_text('existing preferences')
            fallback = Path(temporary) / 'user'
            self.assertEqual(select_home(root, {'LOCALAPPDATA': str(fallback)}), home)
            self.assertEqual((home / 'launcher.json').read_text(), 'existing preferences')
            self.assertFalse(fallback.exists())

    def test_permission_denied_uses_user_storage_without_changing_originals(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'protected'
            home = root / 'local'
            home.mkdir(parents=True)
            (home / 'launcher.json').write_text('original')
            fallback = Path(temporary) / 'user'
            original = tempfile.TemporaryFile

            def permission_gate(*args, **kwargs):
                if Path(kwargs['dir']) == home:
                    raise PermissionError(errno.EACCES, 'Standard user cannot write here')
                return original(*args, **kwargs)

            with patch('opensoccermania.storage.tempfile.TemporaryFile', side_effect=permission_gate):
                selected = select_home(root, {'LOCALAPPDATA': str(fallback)})
            self.assertEqual(selected, fallback / 'OpenSoccerMania')
            (selected / 'launcher.json').write_text('user preferences')
            self.assertEqual((home / 'launcher.json').read_text(), 'original')

    def test_non_directory_local_path_can_fall_back(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'local').write_text('unrelated file')
            selected = select_home(root, {'LOCALAPPDATA': str(root / 'user')})
            self.assertEqual(selected, root / 'user/OpenSoccerMania')
            self.assertEqual((root / 'local').read_text(), 'unrelated file')

    def test_disk_full_does_not_silently_switch_existing_library(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch('opensoccermania.storage.tempfile.TemporaryFile',
                       side_effect=OSError(errno.ENOSPC, 'Disk full')):
                with self.assertRaises(OSError) as caught:
                    select_home(root, {'LOCALAPPDATA': str(root / 'user')})
            self.assertEqual(caught.exception.errno, errno.ENOSPC)
            self.assertFalse((root / 'user').exists())

    def test_unwritable_user_storage_has_actionable_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with patch('opensoccermania.storage.tempfile.TemporaryFile',
                       side_effect=PermissionError(errno.EACCES, 'denied')):
                with self.assertRaisesRegex(OSError, 'writable folder'):
                    select_home(root, {'LOCALAPPDATA': str(root / 'user')})


if __name__ == '__main__':
    unittest.main()
