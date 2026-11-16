import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image

from opensoccermania.title_assets import TitleAssetError, TitleTile
from opensoccermania.title_presentation import prepare_title_presentation


class TitlePresentationTests(unittest.TestCase):
    def test_edges_center_fingerprints_and_invalidation(self):
        with tempfile.TemporaryDirectory() as directory:
            entry = Path(directory)
            folder = entry / "Mania/CD/FrontEnd/BGs"
            folder.mkdir(parents=True)
            source = folder / "Title_FM.BMB"
            source.write_bytes(b"synthetic source")
            bg = entry / "generated.png"
            Image.new("RGB", (32, 8), (90, 100, 110)).save(bg)
            title = Image.new("RGB", (640, 480), (9, 8, 7))
            for y in range(480):
                title.putpixel((0, y), (y % 256, 12, 34))
                title.putpixel((639, y), (56, y % 256, 78))
            tiles = tuple(TitleTile(str(i), Image.new("RGB", (8, 8), (i, 2, 3))) for i in range(8))
            with patch("opensoccermania.title_presentation.load_title_image", return_value=title), \
                 patch("opensoccermania.title_presentation.load_title_tiles", return_value=tiles):
                metadata = prepare_title_presentation(entry, "football", bg, entry / "cache")
                with Image.open(metadata["backgroundBmp"]) as image:
                    self.assertEqual(image.mode, "RGBA")
                    left, _, right, _ = metadata["centerRect"]
                    for y in (0, 256, 511):
                        sy = round(y * 479 / 511)
                        self.assertEqual(image.getpixel((left - 1, y)), title.getpixel((0, sy)) + (255,))
                        self.assertEqual(image.getpixel((right, y)), title.getpixel((639, sy)) + (255,))
                        self.assertEqual(image.getpixel((left - 96, y)), (90, 100, 110, 255))
                        self.assertEqual(image.getpixel((left, y)), (0, 0, 0, 0))
                        self.assertEqual(image.getpixel((right - 1, y)), (0, 0, 0, 0))
                self.assertEqual(len(metadata["tileFingerprints"]), 8)
                self.assertEqual(len(metadata["tileFingerprints"][0]["samples"]), 16)
                self.assertEqual(metadata["originalSize"], [640, 480])
                self.assertTrue(Path(metadata["metadataPath"]).is_file())
                cached = prepare_title_presentation(entry, "football", bg, entry / "cache")
                self.assertEqual(cached["cacheKey"], metadata["cacheKey"])
                source.write_bytes(b"changed source")
                changed = prepare_title_presentation(entry, "football", bg, entry / "cache")
                self.assertNotEqual(changed["cacheKey"], metadata["cacheKey"])
                Image.new("RGB", (32, 8), (1, 2, 3)).save(bg)
                newer = prepare_title_presentation(entry, "football", bg, entry / "cache")
                self.assertNotEqual(newer["cacheKey"], changed["cacheKey"])
                with self.assertRaises(TitleAssetError):
                    prepare_title_presentation(entry, "football", bg, entry.parent / "outside")
                with patch("opensoccermania.title_presentation.MAX_BACKGROUND_BYTES", 1):
                    with self.assertRaises(TitleAssetError):
                        prepare_title_presentation(entry, "football", bg, entry / "cache")
                bg.write_bytes(b"invalid image")
                with self.assertRaises(TitleAssetError):
                    prepare_title_presentation(entry, "football", bg, entry / "cache")


if __name__ == "__main__":
    unittest.main()
