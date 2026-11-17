"""Build the native BGRA face atlas from pinned, unmodified CC0 source PNGs."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import zlib

from PIL import Image

HEADER = struct.Struct('<4s7I')
WIDTH = HEIGHT = 64
FAMILIES = ('xbox360', 'ps4', 'switch_pro', 'steam_deck')
FACES = ('south', 'east', 'west', 'north')


def build_atlas(root):
    root = Path(root)
    manifest = json.loads((root/'assets/controllers/manifest.json').read_text())
    hashes = {row['path']: row['sha256'] for row in manifest['files']}
    payload = bytearray()
    for family in FAMILIES:
        for face in FACES:
            relative = f'assets/controllers/{family}/{face}.png'
            path = root/relative
            if hashlib.sha256(path.read_bytes()).hexdigest() != hashes[relative]:
                raise ValueError(f'Controller source asset hash mismatch: {relative}')
            with Image.open(path) as original:
                icon = original.convert('RGBA').resize((54, 54), Image.Resampling.LANCZOS)
            tile = Image.new('RGBA', (WIDTH, HEIGHT))
            tile.paste(icon, (5, 5))
            payload.extend(tile.tobytes('raw', 'BGRA'))
    return HEADER.pack(b'OSMP', 1, WIDTH, HEIGHT, len(FAMILIES), len(FACES), len(payload), zlib.crc32(payload)) + payload


def validate_atlas(data):
    if len(data) < HEADER.size:
        raise ValueError('Truncated controller atlas header')
    magic, version, width, height, families, faces, length, checksum = HEADER.unpack_from(data)
    if (magic, version, width, height, families, faces) != (b'OSMP', 1, 64, 64, 4, 4):
        raise ValueError('Unsupported controller atlas')
    if length != 4*4*64*64*4 or len(data) != HEADER.size + length:
        raise ValueError('Invalid controller atlas size')
    if zlib.crc32(data[HEADER.size:]) != checksum:
        raise ValueError('Controller atlas checksum mismatch')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    expected = build_atlas(root)
    validate_atlas(expected)
    path = root/'assets/controllers/controller_prompts.bin'
    if args.check:
        if not path.is_file() or path.read_bytes() != expected:
            raise SystemExit('Controller atlas differs; run tools/build_controller_prompts.py')
    else:
        path.write_bytes(expected)
    print(f'Controller atlas verified: {len(expected)} bytes, 16 faces')


if __name__ == '__main__':
    main()
