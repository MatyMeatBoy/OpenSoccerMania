import tempfile
import unittest
import hashlib
import sys
import os
from unittest.mock import patch
from pathlib import Path
from opensoccermania import disc_images


def cooked():
    data = bytearray(20 * 2048)
    data[16 * 2048:16 * 2048 + 7] = b'\x01CD001\x01'
    data[16 * 2048 + 80:16 * 2048 + 88] = (20).to_bytes(4, 'little') + (20).to_bytes(4, 'big')
    return bytes(data)


def small_iso():
    # A synthetic ISO9660 volume; no proprietary game content or extra package.
    image = bytearray(24 * 2048)
    def dual(number, width):
        return number.to_bytes(width, 'little') + number.to_bytes(width, 'big')
    def record(name, extent, size, directory=False):
        length = 33 + len(name) + (len(name) % 2 == 0)
        result = bytearray(length)
        result[0] = length
        result[2:10] = dual(extent, 4)
        result[10:18] = dual(size, 4)
        result[18:25] = bytes((126, 1, 1, 0, 0, 0, 0))
        result[25] = 2 if directory else 0
        result[28:32] = dual(1, 2)
        result[32] = len(name)
        result[33:33 + len(name)] = name
        return result
    pvd = bytearray(2048)
    pvd[:7] = b'\x01CD001\x01'
    pvd[8:40] = b'TEST'.ljust(32, b' ')
    pvd[40:72] = b'OWNER_TEST'.ljust(32, b' ')
    pvd[80:88] = dual(24, 4)
    pvd[120:124] = dual(1, 2)
    pvd[124:128] = dual(1, 2)
    pvd[128:132] = dual(2048, 2)
    pvd[156:190] = record(b'\x00', 20, 2048, True)
    pvd[881] = 1
    image[16 * 2048:17 * 2048] = pvd
    image[17 * 2048:17 * 2048 + 7] = b'\xffCD001\x01'
    payload = b'MZsynthetic fixture'
    directory = record(b'\x00', 20, 2048, True) + record(b'\x01', 20, 2048, True) + record(b'GAME.EXE;1', 21, len(payload))
    image[20 * 2048:20 * 2048 + len(directory)] = directory
    image[21 * 2048:21 * 2048 + len(payload)] = payload
    return bytes(image)


class DiscImageTests(unittest.TestCase):
    def test_content_probe_and_unsupported_variants(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for ext in ('iso', 'img', 'bin', 'mdf', 'nrg'):
                path = root / ('disc.' + ext)
                path.write_bytes(cooked())
                self.assertTrue(disc_images.probe_disc_image(path).supported)
            path.write_bytes(b'not an optical filesystem')
            self.assertFalse(disc_images.probe_disc_image(path).supported)

    def test_raw_mode1_and_cleanup(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            raw = root / 'disc.bin'
            sectors = []
            for i in range(20):
                sector = bytearray(2352)
                sector[:12] = b'\x00' + b'\xff' * 10 + b'\x00'
                sector[15] = 1
                sector[16:2064] = cooked()[i * 2048:(i + 1) * 2048]
                sectors.append(sector)
            raw.write_bytes(b''.join(sectors))
            self.assertEqual(disc_images.probe_disc_image(raw).sector_size, 2352)
            with disc_images.normalized_disc_image(raw) as normalized:
                self.assertEqual(normalized.read_bytes(), cooked())
                saved = normalized
            self.assertFalse(saved.exists())
            self.assertEqual(raw.read_bytes(), b''.join(sectors))

    def test_mode2_form1_and_corrupt_sector_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'disc.mdf'
            sectors = []
            for i in range(20):
                sector = bytearray(2352)
                sector[:12] = b'\x00' + b'\xff' * 10 + b'\x00'
                sector[15] = 2
                sector[24:2072] = cooked()[i * 2048:(i + 1) * 2048]
                sectors.append(sector)
            path.write_bytes(b''.join(sectors))
            with disc_images.normalized_disc_image(path) as normalized:
                self.assertEqual(normalized.read_bytes(), cooked())
            sectors[0][18] = 0x20
            sectors[0][22] = 0x20
            original = b''.join(sectors)
            path.write_bytes(original)
            with self.assertRaisesRegex(ValueError, 'Form 2'):
                with disc_images.normalized_disc_image(path):
                    self.fail('MODE2 Form 2 must not be normalized as Form 1.')
            self.assertEqual(path.read_bytes(), original)

    def test_cue_rejects_traversal_and_multitrack(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'disc.bin').write_bytes(cooked())
            cue = root / 'disc.cue'
            cue.write_text('FILE "disc.bin" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n')
            self.assertTrue(disc_images.probe_disc_image(cue).supported)
            for text in ('FILE "../disc.bin" BINARY\n', 'FILE "disc.bin" BINARY\n TRACK 01 AUDIO\n', 'FILE "disc.bin" BINARY\n TRACK 01 MODE1/2048\n TRACK 02 AUDIO\n'):
                cue.write_text(text)
                self.assertFalse(disc_images.probe_disc_image(cue).supported)

    def test_default_import_needs_no_installed_tool_or_process(self):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / 'clean machine.iso'; image.write_bytes(small_iso())
            before = hashlib.sha256(image.read_bytes()).hexdigest()
            with patch.dict(os.environ, {'PATH': ''}), patch.object(disc_images, 'find_7zip', side_effect=AssertionError('No installed extractor allowed')), patch.object(disc_images.subprocess, 'Popen', side_effect=AssertionError('No external process allowed')):
                with disc_images.open_disc_image(image) as extracted:
                    self.assertEqual((extracted / 'GAME.EXE').read_bytes(), b'MZsynthetic fixture')
                    saved = extracted
            self.assertFalse(saved.exists())
            self.assertEqual(hashlib.sha256(image.read_bytes()).hexdigest(), before)

    @unittest.skipUnless(disc_images.find_7zip(), '7-Zip is an optional import dependency.')
    def test_installed_extractor_private_tree_and_cleanup(self):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / 'owner image.iso'
            image.write_bytes(small_iso())
            original_hash = hashlib.sha256(image.read_bytes()).hexdigest()
            with disc_images.open_disc_image(image, extractor=disc_images.find_7zip()) as extracted:
                self.assertEqual((extracted / 'GAME.EXE').read_bytes(), b'MZsynthetic fixture')
                saved = extracted
            self.assertFalse(saved.exists())
            self.assertEqual(hashlib.sha256(image.read_bytes()).hexdigest(), original_hash)
            with self.assertRaisesRegex(ValueError, 'limit'):
                with disc_images.open_disc_image(image, max_bytes=1):
                    self.fail('Oversized contents must not be exposed.')

    def test_extractor_process_has_exact_output_and_time_limits(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'bounded'
            with self.assertRaisesRegex(ValueError, 'limit'):
                disc_images._run_bounded([sys.executable, '-c', 'import sys; sys.stdout.buffer.write(bytes(100000))'], output, 10)
            self.assertLessEqual(output.stat().st_size, 10)
            with self.assertRaisesRegex(ValueError, 'time limit'):
                disc_images._run_bounded([sys.executable, '-c', 'import time; time.sleep(10)'], output, 10, timeout=0.1)

    def test_listing_rejects_escape_links_and_case_collisions(self):
        for name in ('../outside', '/outside', 'C:\\outside', 'ok:stream', 'x/../outside', 'CON', 'aux.txt', 'bad*name'):
            with self.assertRaises(ValueError):
                disc_images._parse_listing('Path = ' + name + '\nSize = 1\nFolder = -\n', 100)
        with self.assertRaises(ValueError):
            disc_images._parse_listing('Path = link\nSize = 1\nSymbolic Link = outside\n', 100)
        with self.assertRaises(ValueError):
            disc_images._parse_listing('Path = A\nSize = 1\n\nPath = a\nSize = 1\n', 100)


if __name__ == '__main__':
    unittest.main()
