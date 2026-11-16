"""The launcher's rounded («bubble») theme: ttk elements drawn from antialiased rounded images made at start.

Tk has no rounded widgets; ttk can draw any element from images, stretched with fixed borders (nine-slice). This
makes those images with Pillow (drawn 4x and reduced, so the curves are smooth) and replaces the elements of
buttons, check boxes, combo boxes, entries, cards (label frames), notebook tabs and scroll bars. Every page of the
launcher uses ttk widgets, so all of them get the rounded look without being rewritten.

Design scale (kept in one place so the pages stay consistent):
  spacing   4 · 8 · 12 · 16 · 24 px
  radii     RADIUS (controls) · RADIUS + 4 (cards) · half the height (pills: navigation, tabs, bars, chips)
  type      title 21 heavy · heading 13 bold · body 11 · caption 10            (fonts.py chooses the family)
  depth     two levels: controls cast a short soft shadow, sheets and cards a wider one
"""
from PIL import Image, ImageDraw, ImageFilter, ImageTk

from . import artwork, fonts

GREEN, GREEN_HI, CREAM, AMBER, INK = '#163d32', '#245846', '#f5f1e5', '#dfa648', '#203c34'
SOFT, LINE, WHITE, MUTED = '#e7e6d8', '#d3d4c4', '#fffdf6', '#597067'
DESK, DESK_EDGE, LEAF = '#e9e3cf', '#c9bf9f', '#3e8a63'      # the textured sheet behind the pages; leaf green
BAR, BAR_TEXT = '#10302a', '#dfe9df'                         # the window's own title bar
RADIUS = 14
SCALE = 4
SP = (4, 8, 12, 16, 24)


def rounded(size, radius, fill, outline=None, width=1, shadow=False, fill2=None, gloss=0, press=0):
    """A rounded rectangle on a transparent background, size in pixels.

    `fill2` makes a vertical gradient from `fill`; `gloss` adds a soft light line under the top edge; `shadow` leaves
    room for a short soft drop shadow (2 px at the sides, 4 px below); `press` moves the shape down (pressed look).
    """
    w, h = size
    left, top, right, bottom = (2, 1 + press, 2, 4 - press) if shadow else (0, 0, 0, 0)
    box = (left * SCALE, top * SCALE, (w - right) * SCALE - 1, (h - bottom) * SCALE - 1)
    r = radius * SCALE
    out = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    if shadow and not press:
        shade = Image.new('L', (w, h), 0)
        ImageDraw.Draw(shade).rounded_rectangle((left, top + 2, w - right - 1, h - bottom + 1), radius, fill=70)
        out = Image.new('RGBA', (w, h), (44, 38, 20, 0))
        out.putalpha(shade.filter(ImageFilter.GaussianBlur(1.6)))
    mask = Image.new('L', (w * SCALE, h * SCALE), 0)
    ImageDraw.Draw(mask).rounded_rectangle(box, r, fill=255)
    body = (artwork.vertical_gradient((w * SCALE, h * SCALE), fill, fill2) if fill2
            else Image.new('RGB', (w * SCALE, h * SCALE), artwork.rgb(fill)))
    big = Image.new('RGBA', (w * SCALE, h * SCALE), (0, 0, 0, 0))
    big.paste(body, (0, 0), mask)
    d = ImageDraw.Draw(big)
    if gloss:
        light = Image.new('L', (w * SCALE, h * SCALE), 0)
        inset = (width if outline else 0) * SCALE
        ImageDraw.Draw(light).rounded_rectangle((box[0] + inset, box[1] + inset, box[2] - inset, box[3] - inset),
                                                max(1, r - inset), outline=gloss, width=SCALE)
        fade = Image.linear_gradient('L').resize((w * SCALE, h * SCALE)).point(lambda v: max(0, 255 - v * 3))
        big.paste(Image.new('RGB', big.size, (255, 255, 255)), (0, 0), Image.composite(light, Image.new('L', big.size, 0), fade))
    if outline:
        d.rounded_rectangle(box, r, outline=outline, width=width * SCALE)
    out.alpha_composite(big.resize((w, h), Image.LANCZOS))
    return out


def pill(size, fill, **options):
    return rounded(size, size[1] // 2, fill, **options)


class Bubble:
    """Keeps the PhotoImages alive for as long as the style uses them."""

    def __init__(self, root):
        self.root, self.images = root, []

    def img(self, picture):
        photo = ImageTk.PhotoImage(picture, master=self.root)
        self.images.append(photo)
        return photo

    def _element(self, style, name, *args, **options):
        try:
            style.element_create(name, 'image', *args, **options)
        except Exception:   # the element exists already (a second launcher in one process): keep it
            pass

    def apply(self, style):
        body, caption = fonts.ui(11), fonts.ui(10)
        style.configure('.', font=body, background=CREAM, foreground=INK, focuscolor=CREAM)
        style.configure('TFrame', background=CREAM)
        style.configure('TLabel', background=CREAM, foreground=INK)
        style.configure('Title.TLabel', font=fonts.display(21), foreground=GREEN)
        style.configure('Heading.TLabel', font=fonts.ui(13, 'bold'), foreground=GREEN)
        style.configure('Strong.TLabel', font=fonts.ui(11, 'bold'))
        style.configure('Muted.TLabel', foreground=MUTED)
        style.configure('Caption.TLabel', foreground=MUTED, font=caption)
        style.configure('Status.TLabel', background=DESK, font=caption)

        b = RADIUS + 3
        size = (2 * b + 8, 2 * b + 8)
        button = {state: self.img(rounded(size, RADIUS, top, LINE, shadow=True, fill2=bottom, gloss=gloss, press=press))
                  for state, top, bottom, gloss, press in (('normal', '#f4f2e8', '#e3e1d1', 150, 0),
                                                           ('active', '#fbfaf2', '#ebe9db', 170, 0),
                                                           ('pressed', '#d9d7c6', '#dedccc', 0, 1),
                                                           ('disabled', '#eeede5', '#eeede5', 0, 1))}
        self._element(style, 'Bubble.button', button['normal'], ('disabled', button['disabled']),
                      ('pressed', button['pressed']), ('active', button['active']), border=b, padding=(2, 1, 2, 4), sticky='nsew')
        style.layout('TButton', [('Bubble.button', {'sticky': 'nsew', 'children': [
            ('Button.padding', {'sticky': 'nsew', 'children': [('Button.label', {'sticky': 'nsew'})]})]})])
        style.configure('TButton', padding=(16, 7, 16, 8), background=CREAM, foreground=INK, anchor='center',
                        font=fonts.ui(11, 'bold'))
        style.map('TButton', foreground=[('disabled', '#9aa59f')], background=[('active', CREAM)])

        accent = {state: self.img(rounded(size, RADIUS, top, shadow=True, fill2=bottom, gloss=gloss, press=press))
                  for state, top, bottom, gloss, press in (('normal', '#2b6b55', GREEN, 90, 0),
                                                           ('active', '#358068', '#1d4d3f', 110, 0),
                                                           ('pressed', '#0f2e25', '#123329', 0, 1),
                                                           ('disabled', '#8fa197', '#8fa197', 0, 1))}
        self._element(style, 'Accent.button', accent['normal'], ('disabled', accent['disabled']),
                      ('pressed', accent['pressed']), ('active', accent['active']), border=b, padding=(2, 1, 2, 4), sticky='nsew')
        style.layout('Accent.TButton', [('Accent.button', {'sticky': 'nsew', 'children': [
            ('Button.padding', {'sticky': 'nsew', 'children': [('Button.label', {'sticky': 'nsew'})]})]})])
        style.configure('Accent.TButton', foreground=CREAM, background=CREAM, font=fonts.ui(13, 'bold'),
                        padding=(26, 10, 26, 11))
        style.map('Accent.TButton', foreground=[('disabled', '#e4eae4')], background=[('active', CREAM)])

        # the main button while the game data waits for its verification: lighter, so it does not read as «play»
        verify = {state: self.img(rounded(size, RADIUS, top, outline, shadow=True, fill2=bottom, gloss=gloss, press=press))
                  for state, top, bottom, outline, gloss, press in (('normal', '#e6f3e6', '#bcdcc5', '#8fbf9f', 150, 0),
                                                                    ('active', '#f1f9ef', '#cbe5d2', '#8fbf9f', 170, 0),
                                                                    ('pressed', '#aed0b8', '#a6c9b0', '#8fbf9f', 0, 1),
                                                                    ('disabled', '#e4eae0', '#e4eae0', '#cdd6c9', 0, 1))}
        self._element(style, 'Verify.button', verify['normal'], ('disabled', verify['disabled']),
                      ('pressed', verify['pressed']), ('active', verify['active']), border=b, padding=(2, 1, 2, 4), sticky='nsew')
        style.layout('Verify.TButton', [('Verify.button', {'sticky': 'nsew', 'children': [
            ('Button.padding', {'sticky': 'nsew', 'children': [('Button.label', {'sticky': 'nsew'})]})]})])
        style.configure('Verify.TButton', foreground=GREEN, background=CREAM, font=fonts.ui(13, 'bold'),
                        padding=(26, 10, 26, 11))
        style.map('Verify.TButton', foreground=[('disabled', '#7f9388')], background=[('active', CREAM)])

        fb = RADIUS - 1
        fsize = (2 * fb + 8, 2 * fb + 8)
        field = {state: self.img(rounded(fsize, RADIUS - 3, fill, outline, width=width))
                 for state, fill, outline, width in (('normal', WHITE, LINE, 1), ('focus', WHITE, LEAF, 2),
                                                     ('active', WHITE, '#b9bfae', 1), ('disabled', '#efeee6', LINE, 1))}
        for name in ('Combobox', 'Entry', 'Spinbox'):
            self._element(style, f'Bubble.{name}.field', field['normal'], ('disabled', field['disabled']),
                          ('focus', field['focus']), ('active', field['active']), border=fb, padding=2, sticky='nsew')
        arrow = Image.new('RGBA', (26, 16), (0, 0, 0, 0))
        arrow.alpha_composite(artwork.icon('chevron', 16, GREEN), (2, 0))
        faded = Image.new('RGBA', (26, 16), (0, 0, 0, 0))
        faded.alpha_composite(artwork.icon('chevron', 16, '#a9b2a9'), (2, 0))
        self._element(style, 'Bubble.Combobox.downarrow', self.img(arrow), ('disabled', self.img(faded)), sticky='')
        style.layout('TCombobox', [('Bubble.Combobox.field', {'sticky': 'nsew', 'children': [
            ('Bubble.Combobox.downarrow', {'side': 'right', 'sticky': ''}),
            ('Combobox.padding', {'expand': '1', 'sticky': 'nsew', 'children': [
                ('Combobox.textarea', {'sticky': 'nsew'})]})]})])
        style.configure('TCombobox', padding=(10, 6, 4, 6), background=CREAM, fieldbackground=WHITE,
                        foreground=INK, bordercolor=WHITE, lightcolor=WHITE, darkcolor=WHITE)
        style.map('TCombobox', fieldbackground=[('disabled', '#efeee6'), ('readonly', WHITE)], background=[('active', CREAM)],
                  foreground=[('disabled', '#9aa59f')],
                  selectbackground=[('readonly', WHITE)], selectforeground=[('readonly', INK)])
        style.layout('TEntry', [('Bubble.Entry.field', {'sticky': 'nsew', 'children': [
            ('Entry.padding', {'sticky': 'nsew', 'children': [('Entry.textarea', {'sticky': 'nsew'})]})]})])
        style.configure('TEntry', padding=(10, 6, 10, 6), fieldbackground=WHITE, foreground=INK,
                        selectbackground=GREEN, selectforeground=CREAM)
        self.root.option_add('*TCombobox*Listbox.background', WHITE)
        self.root.option_add('*TCombobox*Listbox.foreground', INK)
        self.root.option_add('*TCombobox*Listbox.selectBackground', GREEN)
        self.root.option_add('*TCombobox*Listbox.selectForeground', CREAM)
        self.root.option_add('*TCombobox*Listbox.font', body)
        self.root.option_add('*TCombobox*Listbox.borderWidth', 0)
        self.root.option_add('*TCombobox.font', body)
        self.root.option_add('*TEntry.font', body)

        def check(fill, outline, mark=None, fill2=None):
            picture = Image.new('RGBA', (30, 22), (0, 0, 0, 0))
            picture.alpha_composite(rounded((22, 22), 7, fill, outline, width=1 if mark is None else 0, fill2=fill2), (0, 0))
            if mark:
                picture.alpha_composite(artwork.icon('check', 16, mark), (3, 3))
            return self.img(picture)
        boxes = {'off': check(WHITE, '#b9bfae'), 'hover': check(WHITE, LEAF), 'on': check('#2f755c', None, CREAM, GREEN),
                 'on_hover': check('#3a8a6d', None, WHITE, '#1d4d3f'), 'off_disabled': check('#efeee6', LINE),
                 'on_disabled': check('#9fb0a6', None, '#eef1ea')}
        self._element(style, 'Bubble.check', boxes['off'], (('selected', 'disabled'), boxes['on_disabled']),
                      ('disabled', boxes['off_disabled']), (('selected', 'active'), boxes['on_hover']),
                      ('selected', boxes['on']), ('active', boxes['hover']), sticky='w')
        style.layout('TCheckbutton', [('Checkbutton.padding', {'sticky': 'nsew', 'children': [
            ('Bubble.check', {'side': 'left', 'sticky': ''}),
            ('Checkbutton.label', {'side': 'left', 'sticky': 'nsew'})]})])
        style.configure('TCheckbutton', background=CREAM, foreground=INK, padding=(0, 3))
        style.map('TCheckbutton', background=[('active', CREAM)], foreground=[('disabled', '#9aa59f')])

        # the card is the page's colour with a soft border and shadow: labels inside keep a matching background
        card = self.img(artwork.panel((72, 72), RADIUS + 4, CREAM, LINE, shadow=44, blur=4, offset=2, margin=6))
        self._element(style, 'Bubble.card', card, border=RADIUS + 12, padding=6, sticky='nsew')
        style.layout('TLabelframe', [('Bubble.card', {'sticky': 'nsew'})])
        style.configure('TLabelframe', background=CREAM, padding=(16, 12, 16, 14))
        style.configure('TLabelframe.Label', background=CREAM, foreground=MUTED, font=fonts.ui(10, 'bold'))

        tab = {state: self.img(pill((60, 36), fill, **extra)) for state, fill, extra in
               (('normal', '#ebe9dc', {'outline': LINE}), ('active', '#f6f4ea', {'outline': '#b9bfae'}),
                ('selected', '#2b6b55', {'fill2': GREEN, 'gloss': 80}))}
        self._element(style, 'Bubble.tab', tab['normal'], ('selected', tab['selected']),
                      ('active', tab['active']), border=17, padding=0, sticky='nsew')
        style.layout('TNotebook.Tab', [('Bubble.tab', {'sticky': 'nsew', 'children': [
            ('Notebook.padding', {'side': 'top', 'sticky': 'nsew', 'children': [
                ('Notebook.label', {'side': 'top', 'sticky': ''})]})]})])
        style.layout('TNotebook', [('Notebook.client', {'sticky': 'nswe'})])
        style.configure('TNotebook', background=CREAM, borderwidth=0, tabmargins=(0, 0, 0, 10), bordercolor=CREAM,
                        lightcolor=CREAM, darkcolor=CREAM)
        style.configure('TNotebook.Tab', padding=(22, 7, 22, 9), background=CREAM, foreground=INK, borderwidth=0,
                        font=fonts.ui(11, 'bold'))
        style.map('TNotebook.Tab', foreground=[('selected', CREAM)], background=[('selected', CREAM)],
                  expand=[('selected', (0, 0, 0, 0))], padding=[('selected', (22, 7, 22, 9))])
        style.layout('Pages.TNotebook.Tab', [])   # the sidebar chooses the launcher's pages
        style.configure('Pages.TNotebook', background=CREAM, borderwidth=0, tabmargins=(0, 0, 0, 0))

        thumb = {state: self.img(pill((12, 28), fill)) for state, fill in
                 (('normal', '#d2d2c1'), ('active', '#a9b5a6'), ('pressed', '#6f8f80'))}
        trough = self.img(Image.new('RGBA', (12, 28), (0, 0, 0, 0)))
        self._element(style, 'Bubble.Vertical.Scrollbar.trough', trough, border=(0, 6), sticky='ns')
        self._element(style, 'Bubble.Vertical.Scrollbar.thumb', thumb['normal'], ('pressed', thumb['pressed']),
                      ('active', thumb['active']), border=(5, 6), sticky='ns')
        style.layout('Vertical.TScrollbar', [('Bubble.Vertical.Scrollbar.trough', {'sticky': 'ns', 'children': [
            ('Bubble.Vertical.Scrollbar.thumb', {'expand': '1', 'sticky': 'nswe'})]})])
        style.configure('TScrollbar', background=CREAM, troughcolor=CREAM, bordercolor=CREAM, arrowcolor=GREEN,
                        lightcolor=CREAM, darkcolor=CREAM, relief='flat', borderwidth=0)

        for option, value in (('*Text.font', body), ('*Text.background', WHITE), ('*Text.foreground', INK),
                              ('*Text.relief', 'flat'), ('*Text.borderWidth', 0), ('*Text.highlightThickness', 0),
                              ('*Text.selectBackground', GREEN), ('*Text.selectForeground', CREAM),
                              ('*Toplevel.background', CREAM), ('*Menu.font', body)):
            self.root.option_add(option, value)
