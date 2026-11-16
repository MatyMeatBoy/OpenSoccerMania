import hashlib
import json
import os
import subprocess
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch
from tests.test_release_package import fixture
from tools.package_portable import package_portable, REQUIRED_FILES


def portable_fixture(root):
    root.mkdir()
    for name in REQUIRED_FILES:
        path = root / name; path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'MZfixture' if name.endswith(('.exe', '.dll')) else b'public fixture')
    (root / 'python311.dll').write_bytes(b'MZpython fixture')
    (root / '_tcl_data').mkdir(); (root / '_tcl_data/init.tcl').write_text('public dependency')
    return root


class PortablePackageTests(unittest.TestCase):
    def build(self, repo, revision, tree, output, expected):
        return package_portable(repo, revision, tree, output,
                                bridge=expected / 'd3d8.dll', input_bridge=expected / 'dinput8.dll',
                                sdl=expected / 'SDL3.dll')

    def fixture(self, root):
        repo = root / 'repo'; repo.mkdir(); revision = fixture(repo)
        tree = portable_fixture(root / 'portable'); expected = root / 'expected'; expected.mkdir()
        for name, member in (('d3d8.dll', 'native/d3d8.dll'), ('dinput8.dll', 'native/dinput8.dll'), ('SDL3.dll', 'runtime/SDL3.dll')):
            (expected / name).write_bytes((tree / member).read_bytes())
        return repo, revision, tree, expected

    def test_exact_members_inventory_reproducible_and_immutable(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo, revision, tree, expected = self.fixture(root)
            first = self.build(repo, revision, tree, root / 'one/portable.zip', expected)
            second = self.build(repo, revision, tree, root / 'two/portable.zip', expected)
            self.assertEqual(first.path.read_bytes(), second.path.read_bytes())
            self.assertEqual(first.sha256, hashlib.sha256(first.path.read_bytes()).hexdigest())
            with zipfile.ZipFile(first.path) as archive:
                manifest = json.loads(archive.read('OpenSoccerMania/PORTABLE-MANIFEST.json'))
                self.assertEqual(manifest['source_commit'], revision)
                for entry in manifest['files']:
                    data = archive.read('OpenSoccerMania/' + entry['path'])
                    self.assertEqual(data, (tree / entry['path']).read_bytes())
                    self.assertEqual(hashlib.sha256(data).hexdigest(), entry['sha256'])
                self.assertTrue(all(name.startswith('OpenSoccerMania/') for name in archive.namelist()))
            with self.assertRaises(FileExistsError):
                self.build(repo, revision, tree, first.path, expected)

    def test_private_game_and_wrapper_paths_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo, revision, tree, expected = self.fixture(root)
            for name in ('local/settings.json', 'Mania/CD/Sound/private.ogg', 'Game.exe', 'Game.exe.original',
                         'native/d3d8_backend.dll', 'dgVoodoo.conf', 'build/candidate.txt',
                         'opensoccermania-source.json', 'disc.iso', 'GAME.DAT', 'assets-copy/owner.dat',
                         'original/game.dll', 'game.dll', 'PIL/private.dll', 'Title_FM.BMB'):
                with self.subTest(name=name):
                    path = tree / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'private fixture')
                    with self.assertRaises(ValueError):
                        self.build(repo, revision, tree, root / 'rejected.zip', expected)
                    self.assertFalse((root / 'rejected.zip').exists())
                    path.unlink()
                    parent = path.parent
                    while parent != tree and not any(parent.iterdir()):
                        parent.rmdir(); parent = parent.parent

    def test_required_files_and_native_hash_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo, revision, tree, expected = self.fixture(root)
            (tree / 'native/d3d8.dll').write_bytes(b'foreign wrapper')
            with self.assertRaisesRegex(ValueError, 'hash'):
                self.build(repo, revision, tree, root / 'rejected.zip', expected)
            (tree / 'native/d3d8.dll').write_bytes((expected / 'd3d8.dll').read_bytes())
            (tree / 'licenses/SDL3.txt').unlink()
            with self.assertRaisesRegex(ValueError, 'required'):
                self.build(repo, revision, tree, root / 'rejected.zip', expected)

    @unittest.skipUnless(os.name == 'nt', 'Windows junction regression.')
    def test_linked_portable_dependency_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo, revision, tree, expected = self.fixture(root)
            outside = root / 'outside'; outside.mkdir(); (outside / 'private.txt').write_text('keep')
            link = tree / 'linked-dependency'
            result = subprocess.run(['cmd.exe', '/c', 'mklink', '/J', str(link), str(outside)], capture_output=True,
                                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            if result.returncode:
                self.skipTest('Junction creation unavailable.')
            try:
                with self.assertRaisesRegex(ValueError, 'links'):
                    self.build(repo, revision, tree, root / 'rejected.zip', expected)
                self.assertEqual((outside / 'private.txt').read_text(), 'keep')
                self.assertFalse((root / 'rejected.zip').exists())
            finally:
                link.rmdir()

    def test_publication_rollback_preserves_source(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo, revision, tree, expected = self.fixture(root)
            before = {p.relative_to(tree).as_posix(): p.read_bytes() for p in tree.rglob('*') if p.is_file()}
            real_link = os.link; calls = []
            def fail_second(source, target):
                calls.append(target)
                if len(calls) == 2:
                    raise OSError('injected failure')
                return real_link(source, target)
            with patch('tools.package_portable.os.link', side_effect=fail_second):
                with self.assertRaisesRegex(OSError, 'injected'):
                    self.build(repo, revision, tree, root / 'portable.zip', expected)
            self.assertFalse((root / 'portable.zip').exists())
            self.assertFalse((root / 'portable.zip.sha256').exists())
            self.assertFalse(list(root.glob('.portable-release-*')))
            self.assertEqual(before, {p.relative_to(tree).as_posix(): p.read_bytes() for p in tree.rglob('*') if p.is_file()})


if __name__ == '__main__':
    unittest.main()
