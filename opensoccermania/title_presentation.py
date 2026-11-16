"""Private runtime sidebar preparation; original title pixels stay in the game."""
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import tempfile

from PIL import Image

from .title_assets import (MAX_ARCHIVE_BYTES, TitleAssetError, _archive,
                           load_title_image, load_title_tiles)

VERSION = 1
SIZE = (2048, 512)
MAX_BACKGROUND_BYTES = 16 * 1024 * 1024
MAX_BACKGROUND_PIXELS = 16 * 1024 * 1024


def _atomic(path, data):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".title-", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _bmp(image):
    """BITMAPV4HEADER explicitly declares BGRA alpha masks."""
    width, height = image.size
    pixels = image.tobytes("raw", "BGRA", 0, -1)
    header = struct.pack("<IiiHHIIiiII", 108, width, height, 1, 32, 3,
                         len(pixels), 0, 0, 0, 0)
    header += struct.pack("<IIIII", 0xff0000, 0xff00, 0xff, 0xff000000, 0x73524742)
    header += bytes(48)
    return struct.pack("<2sIHHI", b"BM", 122 + len(pixels), 0, 0, 122) + header + pixels


def _fingerprint(tile):
    image = tile.image.convert("RGB")
    samples = []
    for fy in (0.125, 0.375, 0.625, 0.875):
        for fx in (0.125, 0.375, 0.625, 0.875):
            x = min(image.width - 1, int(image.width * fx))
            y = min(image.height - 1, int(image.height * fy))
            samples.append({"x": x, "y": y, "rgb": list(image.getpixel((x, y)))})
    return {"name": tile.name, "width": image.width, "height": image.height,
            "samples": samples}


def prepare_title_presentation(entry, brand, generated_background_path, cache_root):
    """Prepare alpha sidebar BMP and JSON, confined to the selected private entry.

    The 2048x512 texture describes a 4:1 panorama. Native rendering should use
    its normalized center rectangle to crop the outer panorama for narrower
    displays, and draw only outside the existing original 4:3 game title.
    """
    entry = Path(entry).resolve()
    cache = Path(cache_root).resolve()
    if not cache.is_relative_to(entry) or cache == entry:
        raise TitleAssetError("Title cache must be a subdirectory of the private library entry.")
    source = _archive(entry, brand)
    if source.stat().st_size > MAX_ARCHIVE_BYTES:
        raise TitleAssetError("Title source exceeds the size limit.")
    background = Path(generated_background_path)
    if not background.is_file() or background.stat().st_size > MAX_BACKGROUND_BYTES:
        raise TitleAssetError("Generated background is missing or exceeds the size limit.")
    with source.open("rb") as stream:
        source_data = stream.read(MAX_ARCHIVE_BYTES + 1)
    with background.open("rb") as stream:
        background_data = stream.read(MAX_BACKGROUND_BYTES + 1)
    if len(source_data) > MAX_ARCHIVE_BYTES or len(background_data) > MAX_BACKGROUND_BYTES:
        raise TitleAssetError("Title presentation input exceeds the size limit.")
    source_hash = hashlib.sha256(source_data).hexdigest()
    background_hash = hashlib.sha256(background_data).hexdigest()
    key = hashlib.sha256(f"{VERSION}:{brand}:{source_hash}:{background_hash}".encode()).hexdigest()
    cache.mkdir(parents=True, exist_ok=True)
    bmp_path, json_path = cache / f"title-{key}.bmp", cache / f"title-{key}.json"
    if json_path.is_file() and json_path.stat().st_size <= 128 * 1024 and bmp_path.is_file() and bmp_path.stat().st_size == 122 + SIZE[0] * SIZE[1] * 4:
        try:
            metadata = json.loads(json_path.read_text(encoding="utf-8"))
            if (metadata.get("cacheKey") == key and
                    metadata.get("backgroundSha256") == hashlib.sha256(bmp_path.read_bytes()).hexdigest()):
                metadata["backgroundBmp"] = str(bmp_path)
                return metadata
        except (OSError, ValueError):
            pass
    try:
        with Image.open(io.BytesIO(background_data)) as generated:
            if (generated.width < 1 or generated.height < 1 or
                    generated.width * generated.height > MAX_BACKGROUND_PIXELS):
                raise TitleAssetError("Generated background dimensions exceed the limit.")
            canvas = generated.convert("RGB").resize(SIZE, Image.Resampling.BILINEAR).convert("RGBA")
    except (OSError, Image.DecompressionBombError) as exc:
        raise TitleAssetError("Cannot decode the generated background.") from exc
    title = load_title_image(entry, brand).convert("RGB")
    if title.size != (640, 480):
        raise TitleAssetError("Only the verified 640x480 title presentation is supported.")
    tiles = load_title_tiles(entry, brand)
    width, height = SIZE
    center_width = round(height * 4 / 3)
    left = (width - center_width) // 2
    right = left + center_width
    # Transparent center contains no copied game image, even in its RGB bytes.
    canvas.paste((0, 0, 0, 0), (left, 0, right, height))
    pixels = canvas.load()
    for y in range(height):
        source_y = round(y * (title.height - 1) / (height - 1))
        for boundary, direction, source_x in ((left - 1, -1, 0), (right, 1, 639)):
            edge = title.getpixel((source_x, source_y))
            for distance in range(96):
                x = boundary + direction * distance
                t = distance / 95
                generated = pixels[x, y]
                pixels[x, y] = tuple(round(edge[c] * (1 - t) + generated[c] * t)
                                     for c in range(3)) + (255,)
    data = _bmp(canvas)
    metadata = {"version": VERSION, "brand": brand, "cacheKey": key,
                "sourceHash": source_hash, "generatedHash": background_hash,
                "originalSize": [640, 480], "backgroundSize": list(SIZE),
                "backgroundBmp": str(bmp_path),
                "backgroundSha256": hashlib.sha256(data).hexdigest(),
                "centerRect": [left, 0, right, height],
                "centerUV": [left / width, 0, right / width, 1],
                "blendWidth": 96, "tileFingerprints": [_fingerprint(t) for t in tiles],
                "metadataPath": str(json_path)}
    _atomic(bmp_path, data)
    _atomic(json_path, json.dumps(metadata, indent=2).encode("utf-8"))
    return metadata
