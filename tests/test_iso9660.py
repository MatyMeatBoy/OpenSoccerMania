"""Synthetic, non-proprietary ISO9660/Joliet fixtures and reader checks."""
import hashlib
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from opensoccermania.disc_images import open_disc_image, probe_disc_image, find_7zip
from opensoccermania.sources import discover
from tests.test_disc_images import small_iso


def dual(number, width=4):
    return number.to_bytes(width, 'little') + number.to_bytes(width, 'big')


def record(name, extent, size, directory=False):
    value = bytearray(33 + len(name) + (len(name) % 2 == 0))
    value[0] = len(value); value[2:10] = dual(extent); value[10:18] = dual(size)
    value[25] = 2 if directory else 0; value[28:32] = dual(1, 2)
    value[32] = len(name); value[33:33 + len(name)] = name
    return value


def iso_tree(entries, joliet=False):
    directories = {()}
    for path in entries:
        parts = tuple(path.split('/'))
        for index in range(1, len(parts)):
            directories.add(parts[:index])
    first = 24 if joliet else 20
    directories = {path: first + index for index, path in enumerate(sorted(directories))}
    sector = first + len(directories); files = {}
    for path, data in sorted(entries.items()):
        files[tuple(path.split('/'))] = (sector, data)
        sector += max(1, (len(data) + 2047) // 2048)
    count = max(40, sector + 1); image = bytearray(count * 2048)
    def descriptor(kind, root):
        data = bytearray(2048); data[:7] = bytes([kind]) + b'CD001\x01'
        data[80:88] = dual(count); data[120:124] = dual(1, 2); data[124:128] = dual(1, 2)
        data[128:132] = dual(2048, 2); data[156:190] = record(b'\0', root, 2048, True)
        data[881] = 1
        return data
    image[16 * 2048:17 * 2048] = descriptor(1, 20 if joliet else first)
    end = 17
    if joliet:
        svd = descriptor(2, first); svd[88:91] = b'%/E'
        image[17 * 2048:18 * 2048] = svd; end = 18
        image[20 * 2048:20 * 2048 + 68] = record(b'\0', 20, 2048, True) + record(b'\1', 20, 2048, True)
    image[end * 2048:end * 2048 + 7] = b'\xffCD001\x01'
    encoding = 'utf-16-be' if joliet else 'ascii'
    for path, extent in directories.items():
        parent = directories[path[:-1]] if path else extent
        content = record(b'\0', extent, 2048, True) + record(b'\1', parent, 2048, True)
        for child, child_extent in directories.items():
            if child and child[:-1] == path:
                content += record(child[-1].encode(encoding), child_extent, 2048, True)
        for child, (child_extent, payload) in files.items():
            if child[:-1] == path:
                content += record((child[-1] + ';1').encode(encoding), child_extent, len(payload))
        if len(content) > 2048:
            raise AssertionError('Fixture directory exceeds one sector.')
        image[extent * 2048:extent * 2048 + len(content)] = content
    for extent, payload in files.values():
        image[extent * 2048:extent * 2048 + len(payload)] = payload
    return bytes(image)


def raw_image(cooked, mode=1):
    result = bytearray()
    for start in range(0, len(cooked), 2048):
        sector = bytearray(2352); sector[:12] = b'\0' + b'\xff' * 10 + b'\0'; sector[15] = mode
        offset = 16 if mode == 1 else 24
        sector[offset:offset + 2048] = cooked[start:start + 2048]
        result.extend(sector)
    return result


class IsoReaderTests(unittest.TestCase):
    def test_joliet_nested_installer_and_unicode_without_external_tools(self):
        entries = {'Setup/English/program files/LEGO Media/Soccer Mania/Game.exe': b'MZsynthetic',
                   'Mania/CD/Messages/Dutch.txt': b'HELLO:Hallo',
                   'Mania/CD/Sound/CutScene/DUT/test.ogg': b'OggSfixture',
                   'Mania/CD/Long filename caf\u00e9.txt': b'public fixture'}
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / 'joliet.iso'; image.write_bytes(iso_tree(entries, True))
            before = hashlib.sha256(image.read_bytes()).hexdigest()
            with patch.dict(os.environ, {'PATH': ''}), patch('opensoccermania.disc_images.find_7zip', side_effect=AssertionError('No installed software')), patch('opensoccermania.disc_images.subprocess.Popen', side_effect=AssertionError('No process allowed')):
                with open_disc_image(image) as tree:
                    for path, data in entries.items():
                        self.assertEqual((tree / path).read_bytes(), data)
                    source = discover(tree)
                    self.assertEqual(source.languages, ['Dutch']); self.assertEqual(source.voices, ['DUT'])
                    saved = tree
            self.assertFalse(saved.exists()); self.assertEqual(hashlib.sha256(image.read_bytes()).hexdigest(), before)

    @unittest.skipUnless(find_7zip(), 'Optional independent metadata reader for differential verification.')
    def test_builtin_joliet_bytes_match_independent_external_reader(self):
        entries = {'Long folder/Long filename caf\u00e9.txt': b'independent fixture', 'GAME.EXE': b'MZfixture'}
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / 'joliet.iso'; image.write_bytes(iso_tree(entries, True))
            with open_disc_image(image) as builtin, open_disc_image(image, extractor=find_7zip()) as independent:
                first = {p.relative_to(builtin).as_posix(): p.read_bytes() for p in builtin.rglob('*') if p.is_file()}
                second = {p.relative_to(independent).as_posix(): p.read_bytes() for p in independent.rglob('*') if p.is_file()}
                self.assertEqual(first, second)

    def test_boot_descriptor_before_primary_in_cooked_and_raw_images(self):
        image = bytearray(small_iso())
        image[17 * 2048:18 * 2048] = image[16 * 2048:17 * 2048]
        image[16 * 2048:17 * 2048] = bytes(2048)
        image[16 * 2048:16 * 2048 + 7] = b'\0CD001\x01'
        image[18 * 2048:18 * 2048 + 7] = b'\xffCD001\x01'
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name, data in (('boot.iso', image), ('boot.bin', raw_image(image))):
                path = root / name; path.write_bytes(data)
                self.assertTrue(probe_disc_image(path).supported)
                with open_disc_image(path) as tree:
                    self.assertEqual((tree / 'GAME.EXE').read_bytes(), b'MZsynthetic fixture')

    def test_raw_bin_cue_mode1_and_mode2_without_extractor(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); image = root / 'disc.bin'; cue = root / 'disc.cue'
            for mode in (1, 2):
                image.write_bytes(raw_image(small_iso(), mode))
                cue.write_text(f'FILE "disc.bin" BINARY\n TRACK 01 MODE{mode}/2352\n INDEX 01 00:00:00\n')
                before = image.read_bytes()
                with patch('opensoccermania.disc_images.find_7zip', side_effect=AssertionError('No installed software')):
                    with open_disc_image(cue) as tree:
                        self.assertEqual((tree / 'GAME.EXE').read_bytes(), b'MZsynthetic fixture')
                self.assertEqual(image.read_bytes(), before)

    def test_unsafe_and_corrupt_records_fail_before_yield(self):
        variants = []
        for name in (b'../EVIL;1', b'C:EVIL;1', b'CON;1', b'x\\evil;1', b'..;1', b'line\nfeed;1'):
            image = bytearray(small_iso()); payload = record(name, 21, 18)
            image[20 * 2048 + 68:21 * 2048] = (payload + bytes(2048 - 68 - len(payload)))
            variants.append(image)
        for offset, value in ((2, 250), (6, 250), (25, 128), (26, 1), (0, 33)):
            image = bytearray(small_iso()); image[20 * 2048 + 68 + offset] = value; variants.append(image)
        image = bytearray(small_iso()); cycle = record(b'CYCLE', 20, 2048, True)
        image[20 * 2048 + 68:21 * 2048] = cycle + bytes(2048 - 68 - len(cycle)); variants.append(image)
        image = bytearray(small_iso()); image[16 * 2048 + 80:16 * 2048 + 88] = dual(9999); variants.append(image)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'corrupt.iso'
            for index, image in enumerate(variants):
                with self.subTest(index=index):
                    path.write_bytes(image)
                    with self.assertRaises(ValueError):
                        with open_disc_image(path):
                            self.fail('Invalid ISO metadata must not be exposed.')

    def test_sector_record_boundaries_and_nonzero_padding_rejected(self):
        variants = []
        image = bytearray(small_iso())
        root = record(b'\0', 20, 2048, True) + record(b'\1', 20, 2048, True)
        index = 0
        while len(root) + 40 <= 2048:
            root += record(('F%03d;1' % index).encode(), 0, 0); index += 1
        root += record(b'OVERFLOW;1', 0, 0)
        image[20 * 2048:21 * 2048] = root[:2048]; variants.append(image)
        image = bytearray(small_iso())
        # Nonzero garbage after the first padding byte cannot hide another record.
        image[20 * 2048 + 180] = 1; variants.append(image)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'invalid.iso'
            for index, image in enumerate(variants):
                with self.subTest(index=index):
                    path.write_bytes(image)
                    with self.assertRaises(ValueError):
                        with open_disc_image(path):
                            self.fail('Corrupt directory sector must fail.')

    def test_unicode_joliet_windows_device_names_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'device.iso'
            for name in ('COM\u00b9.txt', 'LPT\u00b2', 'CONOUT$', 'CON .txt'):
                with self.subTest(name=name):
                    path.write_bytes(iso_tree({name: b'device data'}, True))
                    with self.assertRaises(ValueError):
                        with open_disc_image(path):
                            self.fail('A Joliet name must never select a Windows device.')

    def test_payload_count_metadata_limits_and_cleanup_on_caller_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'fixture.iso'; path.write_bytes(small_iso())
            with self.assertRaisesRegex(ValueError, 'limit'):
                with open_disc_image(path, max_bytes=1):
                    self.fail('Byte limit must fail.')
            for setting in ('MAX_FILES', 'MAX_METADATA_BYTES', 'MAX_DIRECTORY_DEPTH'):
                with patch('opensoccermania.disc_images.' + setting, -1), self.assertRaisesRegex(ValueError, 'limit|depth'):
                    with open_disc_image(path):
                        self.fail('Metadata/count/depth limit must fail.')
            with self.assertRaisesRegex(RuntimeError, 'caller failure'):
                with open_disc_image(path) as tree:
                    saved = tree; raise RuntimeError('caller failure')
            self.assertFalse(saved.exists())

    def test_case_collision_and_rock_ridge_link_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'fixture.iso'
            path.write_bytes(iso_tree({'A.TXT': b'a', 'a.txt': b'b'}))
            with self.assertRaisesRegex(ValueError, 'collid'):
                with open_disc_image(path):
                    self.fail('Case collisions must fail.')
            image = bytearray(small_iso()); value = record(b'LINK;1', 21, 0)
            value.extend(b'SL\x05\x01\x00'); value[0] = len(value)
            image[20 * 2048 + 68:21 * 2048] = value + bytes(2048 - 68 - len(value))
            path.write_bytes(image)
            with self.assertRaisesRegex(ValueError, 'Rock Ridge'):
                with open_disc_image(path):
                    self.fail('Symlink metadata must fail.')


if __name__ == '__main__':
    unittest.main()
