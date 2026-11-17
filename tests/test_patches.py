"""Private-copy patch transaction and interrupted Windows import regressions."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import tempfile
import unittest
from unittest.mock import patch as mock_patch
from opensoccermania.patches import apply_patch, restore_patch, Patch


def fixture(root, readonly=False):
    root = root.resolve()
    source = root / 'original'; source.mkdir()
    private = root / 'private'; private.mkdir()
    data = b'MZ123456'; source_exe = source / 'Game.exe'; source_exe.write_bytes(data)
    if readonly:
        source_exe.chmod(stat.S_IREAD)
    exe = private / 'Game.exe'; shutil.copy2(source_exe, exe)
    digest = hashlib.sha256(data).hexdigest()
    (private / 'opensoccermania-source.json').write_text(json.dumps({'source': str(source), 'sha256': digest}))
    return source_exe, exe, Patch('fixture', digest, [(2, b'12', b'AB')]), data


class PatchTests(unittest.TestCase):
    def test_exact_hash_and_bytes_required_and_reversible(self):
        with tempfile.TemporaryDirectory() as temp:
            _, exe, patch, data = fixture(Path(temp))
            apply_patch(exe, patch); self.assertEqual(exe.read_bytes(), b'MZAB3456')
            apply_patch(exe, patch); restore_patch(exe)
            self.assertEqual(exe.read_bytes(), data)

    def test_mismatch_never_writes(self):
        with tempfile.TemporaryDirectory() as temp:
            _, exe, _, _ = fixture(Path(temp)); exe.write_bytes(b'MZunknown')
            with self.assertRaisesRegex(ValueError, 'version|changed'):
                apply_patch(exe, Patch('fixture', 'bad', [(2, b'12', b'AB')]))
            self.assertEqual(exe.read_bytes(), b'MZunknown')

    def test_changed_original_bytes_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            _, exe, patch, data = fixture(Path(temp))
            patch.edits = [(2, b'XX', b'AB')]
            with self.assertRaisesRegex(ValueError, 'bytes'):
                apply_patch(exe, patch)
            self.assertEqual(exe.read_bytes(), data)

    @unittest.skipUnless(os.name == 'nt', 'Windows copied read-only executable regression.')
    def test_readonly_private_copy_patches_and_restores_without_source_changes(self):
        with tempfile.TemporaryDirectory() as temp:
            original, exe, patch, data = fixture(Path(temp), readonly=True)
            before = original.read_bytes(), original.stat().st_file_attributes
            apply_patch(exe, patch)
            self.assertEqual(exe.read_bytes(), b'MZAB3456')
            exe.chmod(stat.S_IREAD)
            restore_patch(exe)
            self.assertEqual(exe.read_bytes(), data)
            self.assertEqual((original.read_bytes(), original.stat().st_file_attributes), before)
            self.assertEqual(exe.with_suffix('.exe.original').read_bytes(), data)

    def test_interrupted_original_plus_legacy_manifest_resumes(self):
        with tempfile.TemporaryDirectory() as temp:
            original, exe, patch, data = fixture(Path(temp), readonly=True)
            before_mode = original.stat().st_mode
            exe.with_suffix('.exe.original').write_bytes(data)
            exe.with_suffix('.patch.tmp').write_bytes(b'MZAB3456')
            exe.with_suffix('.patch.json').write_text(json.dumps({'patch': patch.name, 'original_sha256': patch.sha256,
                'patched_sha256': hashlib.sha256(b'MZAB3456').hexdigest()}))
            apply_patch(exe, patch); apply_patch(exe, patch)
            self.assertEqual(exe.read_bytes(), b'MZAB3456')
            restore_patch(exe); self.assertEqual(exe.read_bytes(), data)
            self.assertEqual(original.read_bytes(), data)
            self.assertEqual(original.stat().st_mode, before_mode)

    def test_failed_replacement_leaves_valid_recovery_and_retry(self):
        with tempfile.TemporaryDirectory() as temp:
            original, exe, patch, data = fixture(Path(temp), readonly=True)
            real_replace = os.replace
            def denied(source, destination):
                if Path(destination) == exe:
                    raise PermissionError('injected Windows access denied')
                return real_replace(source, destination)
            with mock_patch('opensoccermania.patches.os.replace', side_effect=denied):
                with self.assertRaisesRegex(PermissionError, 'private|retry'):
                    apply_patch(exe, patch)
            self.assertEqual(exe.read_bytes(), data)
            self.assertEqual(exe.with_suffix('.exe.original').read_bytes(), data)
            self.assertTrue(exe.with_suffix('.patch.json').exists())
            apply_patch(exe, patch); restore_patch(exe)
            self.assertEqual(original.read_bytes(), data)
            self.assertFalse(list(exe.parent.glob('.*.tmp')))

    def test_corrupt_backup_or_foreign_current_executable_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            _, exe, patch, data = fixture(Path(temp)); apply_patch(exe, patch)
            backup = exe.with_suffix('.exe.original'); backup.write_bytes(b'corrupt')
            for operation in (lambda: apply_patch(exe, patch), lambda: restore_patch(exe)):
                with self.assertRaisesRegex(ValueError, 'backup'):
                    operation()
                self.assertEqual(exe.read_bytes(), b'MZAB3456')
            backup.write_bytes(data); exe.write_bytes(b'MZforeign')
            for operation in (lambda: apply_patch(exe, patch), lambda: restore_patch(exe)):
                with self.assertRaisesRegex(ValueError, 'changed|version'):
                    operation()
                self.assertEqual(exe.read_bytes(), b'MZforeign')

    def test_manifest_hash_or_edits_cannot_authorize_foreign_bytes(self):
        with tempfile.TemporaryDirectory() as temp:
            _, exe, patch, _ = fixture(Path(temp)); apply_patch(exe, patch)
            record = exe.with_suffix('.patch.json'); info = json.loads(record.read_text())
            exe.write_bytes(b'MZforeign'); info['patched_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
            record.write_text(json.dumps(info))
            for operation in (lambda: apply_patch(exe, patch), lambda: restore_patch(exe)):
                with self.assertRaises(ValueError):
                    operation()
                self.assertEqual(exe.read_bytes(), b'MZforeign')

    def test_restore_failure_and_idempotent_retry_preserve_recovery(self):
        with tempfile.TemporaryDirectory() as temp:
            original, exe, patch, data = fixture(Path(temp)); apply_patch(exe, patch)
            real_replace = os.replace
            def denied(source, destination):
                if Path(destination) == exe:
                    raise PermissionError('injected restore denial')
                return real_replace(source, destination)
            with mock_patch('opensoccermania.patches.os.replace', side_effect=denied):
                with self.assertRaisesRegex(PermissionError, 'retry'):
                    restore_patch(exe)
            self.assertEqual(exe.read_bytes(), b'MZAB3456')
            self.assertTrue(exe.with_suffix('.patch.json').exists())
            restore_patch(exe); restore_patch(exe)
            self.assertEqual(exe.read_bytes(), data); self.assertEqual(original.read_bytes(), data)

    def test_supported_definition_overrides_tampered_record_edits(self):
        with tempfile.TemporaryDirectory() as temp:
            _, exe, patch, _ = fixture(Path(temp)); apply_patch(exe, patch)
            exe.write_bytes(b'MZXX3456')
            record = exe.with_suffix('.patch.json'); info = json.loads(record.read_text())
            info['patch'] = 'foreign'; info['edits'][0]['after'] = b'XX'.hex()
            info['patched_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
            record.write_text(json.dumps(info))
            with mock_patch('opensoccermania.patches.SUPPORTED_HASHES', [patch.sha256]), mock_patch('opensoccermania.patches.portable_startup', return_value=patch):
                with self.assertRaises(ValueError):
                    restore_patch(exe)
            self.assertEqual(exe.read_bytes(), b'MZXX3456')

    def test_hardlinked_target_never_changes_original(self):
        with tempfile.TemporaryDirectory() as temp:
            original, exe, patch, data = fixture(Path(temp)); exe.unlink()
            try:
                os.link(original, exe)
            except OSError:
                self.skipTest('Filesystem does not support hard links.')
            before = original.read_bytes(), original.stat().st_mode
            with self.assertRaisesRegex(ValueError, 'private|unlinked'):
                apply_patch(exe, patch)
            self.assertEqual((original.read_bytes(), original.stat().st_mode), before)
            self.assertEqual(exe.read_bytes(), data)

    def test_original_or_unproven_target_cannot_be_patched(self):
        with tempfile.TemporaryDirectory() as temp:
            original, exe, patch, data = fixture(Path(temp))
            with self.assertRaisesRegex(ValueError, 'private'):
                apply_patch(original, patch)
            self.assertEqual(original.read_bytes(), data)
            info = exe.parent / 'opensoccermania-source.json'
            info.write_text(json.dumps({'source': str(exe.parent), 'sha256': patch.sha256}))
            with self.assertRaisesRegex(ValueError, 'original|private'):
                apply_patch(exe, patch)
            self.assertEqual(exe.read_bytes(), data)


if __name__ == '__main__':
    unittest.main()
