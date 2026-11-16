"""Public synthetic fixtures only; no game artwork."""
import struct
import tempfile
import unittest
from pathlib import Path

from opensoccermania.title_assets import (TitleAssetError, available_brands,
    decode_bmb, load_title_image)


def chunk(tag, payload):
    return tag + struct.pack("<I", len(payload)) + payload


def archive(records):
    index = b"".join(struct.pack("<H", i) + name.encode() + b"\0"
                     for i, (name, *_) in enumerate(records)) + b"\xff\xff"
    out = struct.pack("<I", len(records)) + chunk(b"IDX2", index)
    for name, w, h, depth, pixels, palette in records:
        info = struct.pack("<HH", w, h) + bytes(16) + struct.pack("<I", depth) + bytes(8)
        out += chunk(b"INFO", info) + chunk(b"IMAG", pixels)
        if palette is not None:
            out += chunk(b"CLUT", palette)
        out += chunk(b"END ", b"")
    return out


class TitleAssetsTests(unittest.TestCase):
    def test_bgr_and_palette_alpha(self):
        records = [("raw", 1, 1, 24, bytes([3, 2, 1]), None),
                   ("palette", 2, 1, 8, bytes([0, 1]), bytes([1, 2, 3, 0, 4, 5, 6, 127])),
                   ("alpha", 1, 1, 32, bytes([3, 2, 1, 42]), None)]
        tiles = decode_bmb(archive(records))
        self.assertEqual(tiles[0].image.getpixel((0, 0)), (1, 2, 3))
        self.assertEqual([tiles[1].image.getpixel((x, 0)) for x in range(2)],
                         [(1, 2, 3, 0), (4, 5, 6, 127)])
        self.assertEqual(tiles[2].image.getpixel((0, 0)), (1, 2, 3, 42))

    def test_invalid_inputs(self):
        good = archive([("raw", 1, 1, 24, b"abc", None)])
        for data in [good[:n] for n in range(len(good))]:
            with self.assertRaises(TitleAssetError):
                decode_bmb(data)
        for record in [("bad", 0, 1, 24, b"", None),
                       ("bad", 2049, 1, 24, b"", None),
                       ("bad", 1, 1, 16, b"aa", None),
                       ("bad", 1, 1, 8, b"\1", b"1234")]:
            with self.assertRaises(TitleAssetError):
                decode_bmb(archive([record]))
        with self.assertRaises(TitleAssetError):
            decode_bmb(struct.pack("<I", 65) + good[4:])

    def test_private_brand_and_column_major_layout(self):
        records = []
        for i in range(8):
            size = 256 if i < 4 else 128
            records.append((f"MakeBG_{i + 2}", size, size, 24,
                            bytes([0, 0, i + 1]) * size * size, None))
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory) / "Mania/CD/FrontEnd/BGs"
            folder.mkdir(parents=True)
            (folder / "Title_FM.BMB").write_bytes(archive(records))
            self.assertEqual(available_brands(directory), ("football",))
            image = load_title_image(directory, "football")
            self.assertEqual(image.size, (640, 480))
            for xy, value in [((0, 0), 1), ((0, 256), 2), ((256, 0), 3),
                              ((256, 256), 4), ((512, 0), 5), ((512, 128), 6),
                              ((512, 256), 7), ((639, 479), 8)]:
                self.assertEqual(image.getpixel(xy), (value, 0, 0, 255))
            records[0] = ("unknown", *records[0][1:])
            (folder / "Title_FM.BMB").write_bytes(archive(records))
            with self.assertRaises(TitleAssetError):
                load_title_image(directory, "football")


if __name__ == "__main__":
    unittest.main()
