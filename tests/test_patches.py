import tempfile,unittest,hashlib
from pathlib import Path
from opensoccermania.patches import apply_patch, restore_patch, Patch

class PatchTests(unittest.TestCase):
    def test_exact_hash_and_bytes_required_and_reversible(self):
        with tempfile.TemporaryDirectory() as t:
            exe=Path(t)/'Game.exe'; data=b'MZ123456';exe.write_bytes(data)
            patch=Patch('fixture',hashlib.sha256(data).hexdigest(),[(2,b'12',b'AB')])
            apply_patch(exe,patch)
            self.assertEqual(exe.read_bytes(),b'MZAB3456')
            apply_patch(exe,patch)
            restore_patch(exe)
            self.assertEqual(exe.read_bytes(),data)
    def test_mismatch_never_writes(self):
        with tempfile.TemporaryDirectory() as t:
            exe=Path(t)/'Game.exe';exe.write_bytes(b'MZunknown')
            with self.assertRaisesRegex(ValueError,'version'):
                apply_patch(exe,Patch('fixture','bad',[(2,b'12',b'AB')]))
            self.assertEqual(exe.read_bytes(),b'MZunknown')
    def test_changed_original_bytes_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            exe=Path(t)/'Game.exe';data=b'MZ123456';exe.write_bytes(data)
            with self.assertRaisesRegex(ValueError,'bytes'):
                apply_patch(exe,Patch('fixture',hashlib.sha256(data).hexdigest(),[(2,b'XX',b'AB')]))
            self.assertEqual(exe.read_bytes(),data)
