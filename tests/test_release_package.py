import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile
from unittest.mock import patch
from tools.package_release import package_release


def git(repo, *arguments):
    result = subprocess.run(['git', '-C', str(repo), *arguments], check=True, capture_output=True,
                            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    return result.stdout.decode().strip()


def fixture(root):
    git(root, 'init', '-q')
    for name, data in {'README.md': b'public source', 'LICENSE': b'license',
                       'opensoccermania/main.py': b'print("source")\n',
                       'runtime/SDL3.dll': b'public redistributable fixture',
                       'licenses/SDL3.txt': b'SDL license',
                       'third_party/minhook/build/MinGW/Makefile': b'all:\n'}.items():
        path = root / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
    git(root, 'add', '.')
    git(root, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'public fixture')
    return git(root, 'rev-parse', 'HEAD')


class ReleasePackageTests(unittest.TestCase):
    def test_committed_inventory_reproducible_and_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo = root / 'repo'; repo.mkdir(); revision = fixture(repo)
            (repo / 'README.md').write_text('UNCOMMITTED MUST NOT SHIP')
            (repo / 'local').mkdir(); (repo / 'local/Game.exe').write_bytes(b'private')
            first = package_release(repo, revision, root / 'one/release.zip', '0.1.0-pre.2')
            second = package_release(repo, revision, root / 'two/release.zip', '0.1.0-pre.2')
            self.assertEqual(first.path.read_bytes(), second.path.read_bytes())
            digest = hashlib.sha256(first.path.read_bytes()).hexdigest()
            self.assertEqual(first.sha256, digest)
            self.assertEqual(first.checksum_path.read_text().strip(), digest + '  release.zip')
            with zipfile.ZipFile(first.path) as archive:
                prefix = 'OpenSoccerMania-0.1.0-pre.2-source/'
                self.assertEqual(archive.read(prefix + 'README.md'), b'public source')
                self.assertFalse(any('/local/' in name for name in archive.namelist()))
                manifest = json.loads(archive.read(prefix + 'SOURCE-MANIFEST.json'))
                self.assertEqual(manifest['source_commit'], revision)
                for entry in manifest['files']:
                    raw = archive.read(prefix + entry['path'])
                    self.assertEqual(entry['sha256'], hashlib.sha256(raw).hexdigest())
                    self.assertEqual(entry['size'], len(raw))
                self.assertIn(prefix + 'third_party/minhook/build/MinGW/Makefile', archive.namelist())
            with self.assertRaises(FileExistsError):
                package_release(repo, revision, first.path, '0.1.0-pre.2')
            self.assertEqual(first.sha256, hashlib.sha256(first.path.read_bytes()).hexdigest())

    def test_tracked_private_or_wrapper_files_refuse_package(self):
        for bad in ('local/Game.exe', 'assets/Game.exe', 'runtime/d3d8.dll', 'local/capture.png', 'build/bridge.dll', 'ElectronicArts-LegoSoccerMania/README.md', 'assets/disc.iso'):
            with self.subTest(path=bad), tempfile.TemporaryDirectory() as temp:
                root = Path(temp); repo = root / 'repo'; repo.mkdir(); fixture(repo)
                path = repo / bad; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b'private fixture')
                git(repo, 'add', '-f', bad)
                git(repo, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'bad fixture')
                output = root / 'release.zip'
                with self.assertRaises(ValueError):
                    package_release(repo, git(repo, 'rev-parse', 'HEAD'), output, '0.1.0-pre.2')
                self.assertFalse(output.exists())
                self.assertFalse(output.with_suffix('.zip.sha256').exists())

    def test_tracked_link_is_rejected_and_failed_publication_rolls_back(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo = root / 'repo'; repo.mkdir(); revision = fixture(repo)
            output = root / 'release.zip'
            real_link = os.link; calls = []
            def fail_second(source, target):
                calls.append(target)
                if len(calls) == 2:
                    raise OSError('injected sidecar publication failure')
                return real_link(source, target)
            with patch('tools.package_release.os.link', side_effect=fail_second):
                with self.assertRaisesRegex(OSError, 'injected'):
                    package_release(repo, revision, output, '0.1.0-pre.2')
            self.assertFalse(output.exists())
            self.assertFalse(output.with_suffix('.zip.sha256').exists())
            self.assertFalse(list(root.glob('.source-release-*')))
            blob = git(repo, 'rev-parse', revision + ':README.md')
            git(repo, 'update-index', '--add', '--cacheinfo', '120000,' + blob + ',tools/link')
            git(repo, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'tracked link fixture')
            with self.assertRaisesRegex(ValueError, 'Linked'):
                package_release(repo, git(repo, 'rev-parse', 'HEAD'), output, '0.1.0-pre.2')
            self.assertFalse(output.exists())

    def test_revision_version_and_sidecar_are_immutable(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); repo = root / 'repo'; repo.mkdir(); revision = fixture(repo)
            output = root / 'release.zip'; sidecar = output.with_suffix('.zip.sha256'); sidecar.write_text('keep')
            with self.assertRaises(FileExistsError):
                package_release(repo, revision, output, '0.1.0-pre.2')
            self.assertFalse(output.exists()); self.assertEqual(sidecar.read_text(), 'keep')
            for reference, version in (('HEAD', '0.1.0-pre.2'), (revision, '../private')):
                with self.assertRaises(ValueError):
                    package_release(repo, reference, root / 'other.zip', version)


if __name__ == '__main__':
    unittest.main()
