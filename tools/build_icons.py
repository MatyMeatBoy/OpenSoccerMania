"""Build original football/brick branding as SVG, PNG and multi-size ICO."""
from pathlib import Path
import html
import math

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'assets/branding'


def build():
    FOLDER.mkdir(parents=True, exist_ok=True)
    image = Image.new('RGBA', (1024, 1024))
    draw = ImageDraw.Draw(image)
    scale = 16
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">',
           '<title>OpenSoccerMania</title>']

    def rounded(box, radius, color):
        x, y, right, bottom = box
        draw.rounded_rectangle(tuple(round(v * scale) for v in box),
                               radius=radius * scale, fill=color)
        svg.append(f'<rect x="{x}" y="{y}" width="{right-x}" height="{bottom-y}" rx="{radius}" fill="{color}"/>')

    def circle(x, y, radius, color):
        draw.ellipse(tuple(round(v * scale) for v in (x-radius, y-radius, x+radius, y+radius)), fill=color)
        svg.append(f'<circle cx="{x}" cy="{y}" r="{radius}" fill="{color}"/>')

    def polygon(points, color):
        draw.polygon([(round(x*scale), round(y*scale)) for x, y in points], fill=color)
        encoded = ' '.join(f'{x:.3f},{y:.3f}' for x, y in points)
        svg.append(f'<polygon points="{html.escape(encoded)}" fill="{color}"/>')

    def line(points, color, width):
        draw.line([(round(x*scale), round(y*scale)) for x, y in points], fill=color, width=round(width*scale))
        encoded = ' '.join(f'{x:.3f},{y:.3f}' for x, y in points)
        svg.append(f'<polyline points="{encoded}" fill="none" stroke="{color}" stroke-width="{width}"/>')

    green, dark, cream = '#235b46', '#102f2b', '#fff7e6'
    rounded((0, 0, 64, 64), 14, dark)
    rounded((3, 3, 61, 61), 12, green)
    circle(39, 25, 20, dark)
    circle(39, 25, 17.5, cream)
    pentagon = [(39+7.5*math.cos(math.radians(a)), 25+7.5*math.sin(math.radians(a)))
                for a in (-90, -18, 54, 126, 198)]
    polygon(pentagon, dark)
    for x, y in pentagon:
        line([(x, y), (39+(x-39)*2.35, 25+(y-25)*2.35)], dark, 1.5)
    rounded((7, 39, 43, 57), 4, dark)
    rounded((9, 39, 41, 54), 3, '#c88d38')
    rounded((9, 38, 41, 49), 3, '#efba4d')
    for x in (17, 32):
        circle(x, 37.5, 5, '#c88d38')
        circle(x, 36, 4.5, '#ffda78')
    svg.append('</svg>')
    (FOLDER / 'favicon.svg').write_text('\n'.join(svg)+'\n', encoding='utf-8')
    for name, size in (('icon.png', 256), ('favicon-16.png', 16),
                       ('favicon-32.png', 32), ('apple-touch-icon.png', 180)):
        image.resize((size, size), Image.Resampling.LANCZOS).save(FOLDER / name)
    image.resize((256, 256), Image.Resampling.LANCZOS).save(
        FOLDER / 'favicon.ico', sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])


if __name__ == '__main__':
    build()
