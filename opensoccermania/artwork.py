"""Original artwork for the launcher, drawn by code when it starts: paper grain, soft shadows, gradients, leaves and
the icons of the navigation and the window buttons. Nothing here comes from the game: every shape is made below with
Pillow (drawn several times larger and reduced, so curves are smooth) and kept in memory for the session.
"""
import functools
import math

from PIL import Image, ImageChops, ImageDraw, ImageFilter

SS = 4            # supersampling of shapes
ICON_SS = 8       # supersampling of icons (small, so cheap)


def rgb(colour):
    colour = colour.lstrip('#')
    return tuple(int(colour[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    """Colour between two hex colours, as hex."""
    a, b = rgb(a), rgb(b)
    return '#%02x%02x%02x' % tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def vertical_gradient(size, top, bottom):
    w, h = size
    column = Image.new('RGB', (1, max(2, h)))
    t, b = rgb(top), rgb(bottom)
    column.putdata([tuple(round(t[i] + (b[i] - t[i]) * y / max(1, h - 1)) for i in range(3)) for y in range(max(2, h))])
    return column.resize((w, h), Image.BILINEAR)


@functools.lru_cache(maxsize=4)
def _grain(strength):
    """A tile of film/paper grain around mid grey; made once."""
    tile = Image.effect_noise((160, 160), strength).filter(ImageFilter.GaussianBlur(0.45))
    return Image.merge('RGB', (tile, tile, tile))


def add_grain(image, strength=7):
    """Paper grain over an RGB image (the tile repeats; at this strength the seam does not show)."""
    tile = _grain(strength)
    noise = Image.new('RGB', image.size)
    for x in range(0, image.width, tile.width):
        for y in range(0, image.height, tile.height):
            noise.paste(tile, (x, y))
    return ImageChops.add(image, noise, 1, -128)


def vignette(image, colour, strength=0.22):
    """Darkens the corners a little towards `colour`: the light falls on the middle of the sheet."""
    w, h = image.size
    mask = Image.radial_gradient('L').resize((max(2, w // 8), max(2, h // 8)), Image.BILINEAR).resize((w, h), Image.BILINEAR)
    mask = mask.point(lambda v: int(max(0, v - 70) * strength * 255 / 185))
    return Image.composite(Image.new('RGB', image.size, rgb(colour)), image, mask)


def leaf(draw, x, y, length, angle, fill, width=0.36, rib=None):
    """One leaf: two arcs meeting at the stem and the tip, with an optional midrib."""
    ca, sa = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    points = []
    for side in (1, -1):
        steps = range(0, 21) if side == 1 else range(20, -1, -1)
        for i in steps:
            t = i / 20
            bulge = math.sin(math.pi * t ** 0.8) * width * length * side
            px, py = t * length, bulge
            points.append((x + px * ca - py * sa, y + px * sa + py * ca))
    draw.polygon(points, fill=fill)
    if rib:
        draw.line((x, y, x + length * 0.92 * ca, y + length * 0.92 * sa), fill=rib, width=max(1, int(length * 0.03)))


def sprig(image, x, y, scale, colour, alpha, flip=False):
    """A small branch with leaves, composited softly onto an RGB image (sidebar and backdrop decoration)."""
    layer = Image.new('RGBA', image.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    c = rgb(colour) + (alpha,)
    rib = rgb(colour) + (min(255, alpha + 30),)
    s = -1 if flip else 1
    stem = [(x, y), (x + s * 30 * scale, y - 60 * scale), (x + s * 42 * scale, y - 128 * scale)]
    d.line(stem, fill=rib, width=max(1, int(2.2 * scale)), joint='curve')
    for (px, py), ang, length in (((x + s * 6 * scale, y - 14 * scale), 205, 62), ((x + s * 12 * scale, y - 30 * scale), 318, 70),
                                  ((x + s * 24 * scale, y - 52 * scale), 214, 58), ((x + s * 30 * scale, y - 70 * scale), 322, 60),
                                  ((x + s * 36 * scale, y - 94 * scale), 222, 46), ((x + s * 40 * scale, y - 108 * scale), 318, 44),
                                  ((x + s * 42 * scale, y - 126 * scale), 272, 46)):
        leaf(d, px, py, length * scale, ang if s == 1 else 180 - ang, c, rib=rib)
    image.paste(layer, (0, 0), layer)


def paper(size, base, edge, grain=7, leaves=None):
    """The textured sheet behind the pages: grain, a soft vignette and faint leaves in the corners."""
    image = Image.new('RGB', size, rgb(base))
    if leaves:
        w, h = size
        sprig(image, w - 150, h + 26, 1.5, leaves, 34)
        sprig(image, w - 40, h + 60, 1.9, leaves, 24, flip=True)
    return add_grain(vignette(image, edge), grain)


def sidebar_background(size, top, bottom, leaves):
    """The navigation column: a deep green that lightens towards the top, leaves at the foot, fine grain."""
    w, h = size
    image = vertical_gradient(size, top, bottom)
    glow = Image.radial_gradient('L').resize((w * 2, 360), Image.BILINEAR).point(lambda v: max(0, 150 - v) // 5)
    light = Image.new('RGB', (w * 2, 360), rgb(leaves))
    image.paste(light, (-w // 2, -190), glow)
    sprig(image, -18, h - 60, 1.55, leaves, 46)
    sprig(image, w - 30, h - 96, 1.2, leaves, 30, flip=True)
    return add_grain(image, 5)


def panel(size, radius, fill, outline=None, shadow=46, blur=7, offset=3, margin=12, fill2=None):
    """A rounded sheet with a soft drop shadow, on transparency. The sheet is inset by `margin` on every side."""
    w, h = size
    out = Image.new('RGBA', size, (0, 0, 0, 0))
    if shadow:
        shade = Image.new('L', size, 0)
        ImageDraw.Draw(shade).rounded_rectangle((margin, margin + offset, w - margin - 1, h - margin - 1 + offset), radius, fill=shadow)
        out = Image.new('RGBA', size, (52, 44, 24, 0))   # a warm shadow, not grey
        out.putalpha(shade.filter(ImageFilter.GaussianBlur(blur)))
    big = Image.new('L', (w * SS, h * SS), 0)
    ImageDraw.Draw(big).rounded_rectangle((margin * SS, margin * SS, (w - margin) * SS - 1, (h - margin) * SS - 1), radius * SS, fill=255)
    mask = big.resize(size, Image.LANCZOS)
    body = vertical_gradient(size, fill, fill2) if fill2 else Image.new('RGB', size, rgb(fill))
    out.paste(body, (0, 0), mask)
    if outline:
        ring = Image.new('L', (w * SS, h * SS), 0)
        ImageDraw.Draw(ring).rounded_rectangle((margin * SS, margin * SS, (w - margin) * SS - 1, (h - margin) * SS - 1),
                                               radius * SS, outline=255, width=SS)
        out.paste(Image.new('RGB', size, rgb(outline)), (0, 0), ring.resize(size, Image.LANCZOS))
    return out


# --- icons ---------------------------------------------------------------------------------------------------------

class _Pen:
    """Draws on a 24-unit grid into a mask; strokes have round ends and joints."""

    def __init__(self, size):
        self.n = size * ICON_SS
        self.k = self.n / 24
        self.mask = Image.new('L', (self.n, self.n), 0)
        self.d = ImageDraw.Draw(self.mask)

    def p(self, points):
        return [(x * self.k, y * self.k) for x, y in points]

    def line(self, points, width=2.0, ink=255):
        pts, w = self.p(points), max(1, round(width * self.k))
        self.d.line(pts, fill=ink, width=w, joint='curve')
        for x, y in pts:
            self.d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=ink)

    def dot(self, x, y, r, ink=255):
        k = self.k
        self.d.ellipse(((x - r) * k, (y - r) * k, (x + r) * k, (y + r) * k), fill=ink)

    def box(self, x0, y0, x1, y1, r, ink=255, outline=None):
        k = self.k
        if outline:
            self.d.rounded_rectangle((x0 * k, y0 * k, x1 * k, y1 * k), r * k, outline=ink, width=max(1, round(outline * k)))
        else:
            self.d.rounded_rectangle((x0 * k, y0 * k, x1 * k, y1 * k), r * k, fill=ink)

    def poly(self, points, ink=255):
        self.d.polygon(self.p(points), fill=ink)

    def arc(self, x, y, r, start, end, width):
        k = self.k
        self.d.arc(((x - r) * k, (y - r) * k, (x + r) * k, (y + r) * k), start, end, fill=255, width=max(1, round(width * k)))


def _home(p):
    p.line([(3.6, 11.4), (12, 4.2), (20.4, 11.4)], 2.1)
    p.line([(6.2, 10.6), (6.2, 19.4), (17.8, 19.4), (17.8, 10.6)], 2.1)
    p.box(10.2, 13.4, 13.8, 20, 1.2)


def _album(p):
    p.box(7.2, 3.2, 21, 14.6, 2.4, outline=1.9)
    p.box(1.8, 6.6, 18, 21.4, 3.2, ink=0)
    p.box(3, 7.8, 16.8, 20.2, 2.4)
    p.dot(7, 11.6, 1.5, ink=0)
    p.poly([(4.8, 18.4), (9.2, 13.4), (11.6, 16), (13.2, 14.4), (15.4, 18.4)], ink=0)


def _options(p):
    for y, x in ((6, 9), (12, 15.6), (18, 7.6)):
        p.line([(3.6, y), (20.4, y)], 1.9)
        p.dot(x, y, 3.7, ink=0)
        p.dot(x, y, 2.6)


def _controls(p):
    p.box(2.6, 6.8, 21.4, 16.6, 4.9)
    p.dot(5.9, 15.4, 3.9)
    p.dot(18.1, 15.4, 3.9)
    p.box(5.6, 10.7, 10.4, 12.5, 0.5, ink=0)
    p.box(7.1, 9.2, 8.9, 14, 0.5, ink=0)
    p.dot(15, 12.9, 1.35, ink=0)
    p.dot(17.8, 10.5, 1.35, ink=0)


def _star(p, x, y, r, waist=0.24):
    points = []
    for i in range(8):
        radius = r if i % 2 == 0 else r * waist
        a = math.radians(i * 45 - 90)
        points.append((x + radius * math.cos(a), y + radius * math.sin(a)))
    p.poly(points)
    p.line(points + points[:1], 0.7)


def _mods(p):
    _star(p, 9.6, 13.4, 7.6)
    _star(p, 18.2, 6.2, 3.9)
    p.dot(18.6, 18.4, 1.3)


def _updates(p):
    p.arc(12, 12, 8.2, 25, 300, 2.1)
    a = math.radians(300)
    px, py = 12 + 7.15 * math.cos(a), 12 + 7.15 * math.sin(a)
    tx, ty = math.cos(a + math.pi / 2), math.sin(a + math.pi / 2)
    nx, ny = math.cos(a), math.sin(a)
    p.poly([(px + tx * 4.4, py + ty * 4.4), (px + nx * 3.6, py + ny * 3.6), (px - nx * 3.6, py - ny * 3.6)])
    e = math.radians(25)
    p.dot(12 + 7.15 * math.cos(e), 12 + 7.15 * math.sin(e), 1.05)
    p.line([(12, 8.6), (12, 14.6)], 1.9)
    p.line([(9.4, 12.4), (12, 15), (14.6, 12.4)], 1.9)


def _play(p):
    pts = [(8.4, 5.4), (19, 12), (8.4, 18.6)]
    p.poly(pts)
    p.line(pts + pts[:1], 2.2)


def _camera(p):
    p.box(2.4, 7, 21.6, 19.6, 3.2)
    p.box(8, 4.2, 14.4, 9, 1.6)
    p.dot(12, 13.3, 4.5, ink=0)
    p.dot(12, 13.3, 2.9)
    p.dot(18.2, 10.2, 1.05, ink=0)


def _minimize(p):
    p.line([(6.5, 12.5), (17.5, 12.5)], 1.5)


def _maximize(p):
    p.box(6.5, 6.5, 17.5, 17.5, 2.2, outline=1.5)


def _restore(p):
    p.box(9, 5.5, 18.5, 15, 2.2, outline=1.5)
    p.box(4.3, 7.8, 16.2, 19.7, 3, ink=0)
    p.box(5.5, 9, 15, 18.5, 2.2, outline=1.5)


def _close(p):
    p.line([(7, 7), (17, 17)], 1.6)
    p.line([(17, 7), (7, 17)], 1.6)


def _check(p):
    p.line([(5.6, 12.6), (10, 17), (18.4, 7.6)], 2.8)


def _chevron(p):
    p.line([(6.6, 9.6), (12, 15), (17.4, 9.6)], 2.3)


ICONS = {'home': _home, 'album': _album, 'options': _options, 'controls': _controls, 'mods': _mods,
         'updates': _updates, 'play': _play, 'camera': _camera, 'minimize': _minimize, 'maximize': _maximize,
         'restore': _restore, 'close': _close, 'check': _check, 'chevron': _chevron}


@functools.lru_cache(maxsize=128)
def icon(name, size, colour):
    """The named icon as an RGBA image of `size` pixels in one colour."""
    pen = _Pen(size)
    ICONS[name](pen)
    image = Image.new('RGBA', (size, size), rgb(colour) + (0,))
    image.putalpha(pen.mask.resize((size, size), Image.LANCZOS))
    return image


def stretch(tile, size, border):
    """Nine-slice: `tile` enlarged to `size` keeping its `border` pixels (corners, shadow) unscaled."""
    w, h = size
    tw, th = tile.size
    b = min(border, w // 2, h // 2)
    out = Image.new('RGBA', size, (0, 0, 0, 0))
    xs = ((0, b, 0, b), (b, tw - b, b, w - b), (tw - b, tw, w - b, w))
    ys = ((0, b, 0, b), (b, th - b, b, h - b), (th - b, th, h - b, h))
    for sx0, sx1, dx0, dx1 in xs:
        for sy0, sy1, dy0, dy1 in ys:
            if dx1 > dx0 and dy1 > dy0:
                out.paste(tile.crop((sx0, sy0, sx1, sy1)).resize((dx1 - dx0, dy1 - dy0), Image.BILINEAR), (dx0, dy0))
    return out


def with_alpha(image, alpha):
    """The same RGBA image with its opacity multiplied by alpha (0..1)."""
    out = image.copy()
    out.putalpha(image.getchannel('A').point(lambda v: int(v * alpha)))
    return out
