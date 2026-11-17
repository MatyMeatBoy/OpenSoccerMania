from pathlib import Path
import struct
import tempfile
import unittest

from PIL import Image
from opensoccermania.selftest import RESOURCE_PATHS, VENDOR_HASHES, validate_resources


def resources(root):
    for name in RESOURCE_PATHS:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if name in VENDOR_HASHES:
            path.write_bytes((Path(__file__).resolve().parents[1] / name).read_bytes())
        elif name.endswith('.dll'):
            raw = bytearray(128)
            raw[:2] = b'MZ'
            struct.pack_into('<I', raw, 0x3c, 64)
            raw[64:68] = b'PE\0\0'
            struct.pack_into('<H', raw, 68, 0x8664 if name.startswith('runtime/') else 0x14c)
            path.write_bytes(raw)
        elif name.endswith('.png'):
            Image.new('RGB', (4, 2), 'blue').save(path)
        else:
            path.write_bytes(b'\x00\x01\x00\x00synthetic font header')


class PortableResourceTests(unittest.TestCase):
    def test_source_and_frozen_vendor_pins_agree(self):
        from tools.package_release import VENDOR_FILES
        self.assertEqual(VENDOR_HASHES, VENDOR_FILES)

    def test_complete_package_and_mixed_architecture_contract(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            resources(root)
            hashes = validate_resources(root)
            self.assertEqual(set(hashes), set(RESOURCE_PATHS))
            self.assertTrue(all(len(digest) == 64 for digest in hashes.values()))
            native = root / 'native/d3d8.dll'
            native.write_bytes((root / 'runtime/SDL3.dll').read_bytes())
            with self.assertRaisesRegex(ValueError, 'architecture'):
                validate_resources(root)

    def test_missing_and_damaged_assets_fail_explicitly(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaisesRegex(ValueError, 'Missing'):
                validate_resources(root)
            resources(root)
            (root / 'assets/backgrounds/title-extension.png').write_bytes(b'bad image')
            with self.assertRaises(OSError):
                validate_resources(root)

    def test_vendor_missing_tampered_and_wrong_architecture(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); resources(root)
            for name in VENDOR_HASHES:
                path = root / name; original = path.read_bytes()
                path.unlink()
                with self.assertRaisesRegex(ValueError, 'Missing'):
                    validate_resources(root)
                path.write_bytes(original + b'tamper')
                with self.assertRaisesRegex(ValueError, 'vendor hash'):
                    validate_resources(root)
                path.write_bytes(original)
            vendor = root / 'runtime/graphics/dgVoodoo/d3d8.dll'
            vendor.write_bytes((root / 'runtime/SDL3.dll').read_bytes())
            with self.assertRaisesRegex(ValueError, 'architecture'):
                validate_resources(root)
