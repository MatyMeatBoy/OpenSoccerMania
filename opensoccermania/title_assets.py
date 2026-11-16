"""Bounded, read-only access to titles in an owner-provided private library.

No title graphics are bundled, written to disk, or reconstructed from guessed
placement. BMB textures are returned at their encoded dimensions.
"""
from dataclasses import dataclass
from pathlib import Path
import struct

from PIL import Image

MAX_ARCHIVE_BYTES = 16 * 1024 * 1024
MAX_TILES = 64
MAX_DIMENSION = 2048
MAX_PIXELS = 4 * 1024 * 1024
BRAND_FILES = {"soccer": "Title_SM.BMB", "football": "Title_FM.BMB"}


class TitleAssetError(ValueError):
    """Missing, unsupported or damaged private title data."""


@dataclass(frozen=True)
class TitleTile:
    name: str
    image: Image.Image


def _archive(entry, brand):
    if brand not in BRAND_FILES:
        raise TitleAssetError("Unknown title brand; choose soccer or football.")
    folder = Path(entry) / "Mania" / "CD" / "FrontEnd" / "BGs"
    # Only inspect the known private entry layout, never recursively search.
    if not folder.is_dir():
        raise TitleAssetError("Private library title folder is missing.")
    path = next((p for p in folder.iterdir() if p.name.casefold() == BRAND_FILES[brand].casefold()), None)
    if path is None or not path.is_file() or path.is_symlink():
        raise TitleAssetError("This title brand is not present in the private library.")
    if not path.resolve().is_relative_to(Path(entry).resolve()):
        raise TitleAssetError("Title archive must stay inside the private library entry.")
    return path


def available_brands(entry):
    """Return present brand identifiers; presence does not guarantee valid decoding."""
    result = []
    for brand in BRAND_FILES:
        try:
            _archive(entry, brand)
        except TitleAssetError:
            continue
        result.append(brand)
    return tuple(result)


def decode_bmb(data):
    """Decode uncompressed 8/24/32-bit texture records, preserving CLUT alpha.

    Returns named tiles, not a composite. Unknown encodings fail closed.
    """
    def fail(message):
        raise TitleAssetError(message)

    if len(data) < 12 or len(data) > MAX_ARCHIVE_BYTES:
        fail("Title archive size is invalid.")
    count = struct.unpack_from("<I", data)[0]
    if not 1 <= count <= MAX_TILES:
        fail("Title tile count is out of range.")
    pos = 4

    def chunk():
        nonlocal pos
        if pos + 8 > len(data):
            fail("Truncated title chunk header.")
        tag, size = struct.unpack_from("<4sI", data, pos)
        pos += 8
        if size > len(data) - pos:
            fail("Truncated title chunk payload.")
        payload = data[pos:pos + size]
        pos += size
        return tag, payload

    tag, index = chunk()
    if tag != b"IDX2":
        fail("Title index is missing.")
    names = []
    cursor = 0
    for expected in range(count):
        if cursor + 2 > len(index):
            fail("Truncated title index.")
        number = struct.unpack_from("<H", index, cursor)[0]
        cursor += 2
        end = index.find(b"\0", cursor)
        if number != expected or end < cursor or end - cursor > 255:
            fail("Invalid title index entry.")
        try:
            names.append(index[cursor:end].decode("ascii"))
        except UnicodeDecodeError:
            fail("Unsupported title index name.")
        cursor = end + 1
    if index[cursor:] != b"\xff\xff":
        fail("Invalid title index terminator.")
    tiles = []
    total_pixels = 0
    for name in names:
        tag, info = chunk()
        if tag != b"INFO" or len(info) != 32:
            fail("Invalid title texture information.")
        width, height = struct.unpack_from("<HH", info)
        depth = struct.unpack_from("<I", info, 20)[0]
        if not (0 < width <= MAX_DIMENSION and 0 < height <= MAX_DIMENSION):
            fail("Title texture dimensions are out of range.")
        total_pixels += width * height
        if total_pixels > MAX_PIXELS or depth not in (8, 24, 32):
            fail("Unsupported title texture size or pixel format.")
        tag, pixels = chunk()
        if tag != b"IMAG" or len(pixels) != width * height * (depth // 8):
            fail("Title pixel payload does not match its dimensions.")
        tag, payload = chunk()
        if depth == 8:
            if tag != b"CLUT" or not payload or len(payload) % 4 or len(payload) > 1024:
                fail("Invalid title palette.")
            if max(pixels) >= len(payload) // 4:
                fail("Title palette index is out of range.")
            # CLUT entries are byte RGBA; do not discard their alpha channel.
            rgba = b"".join(payload[value * 4:value * 4 + 4] for value in pixels)
            image = Image.frombytes("RGBA", (width, height), rgba)
            tag, payload = chunk()
        else:
            image = Image.frombytes("RGB" if depth == 24 else "RGBA", (width, height), pixels,
                                    "raw", "BGR" if depth == 24 else "BGRA")
        if tag != b"END " or payload:
            fail("Title texture terminator is missing.")
        tiles.append(TitleTile(name, image))
    if pos != len(data):
        fail("Unexpected trailing title data.")
    return tuple(tiles)


def load_title_tiles(entry, brand):
    """Decode a selected private archive in memory, without creating a cache."""
    path = _archive(entry, brand)
    try:
        with path.open("rb") as stream:
            data = stream.read(MAX_ARCHIVE_BYTES + 1)
        return decode_bmb(data)
    except OSError as exc:
        raise TitleAssetError("Cannot read the private title archive.") from exc


def load_title_image(entry, brand):
    """Assemble the verified MakeBG title layout at its native 640x480 size.

    Other tile layouts fail closed. Tiles are column-major; the final 32 rows
    are texture padding. Indexed palettes use RGBA while truecolor uses BGR.
    """
    tiles = load_title_tiles(entry, brand)
    if len(tiles) == 1:
        return tiles[0].image
    first = 10 if brand == "soccer" else 2
    if (tuple(t.name for t in tiles) != tuple(f"MakeBG_{i}" for i in range(first, first + 8))
            or tuple(t.image.size for t in tiles) != ((256, 256),) * 4 + ((128, 128),) * 4):
        raise TitleAssetError("Unverified title tile layout; use text branding.")
    canvas = Image.new("RGBA", (640, 512))
    positions = ((0, 0), (0, 256), (256, 0), (256, 256),
                 (512, 0), (512, 128), (512, 256), (512, 384))
    for tile, position in zip(tiles, positions):
        canvas.paste(tile.image, position)
    return canvas.crop((0, 0, 640, 480))
