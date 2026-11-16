"""The launcher's typeface: Nunito (SIL Open Font License 1.1), loaded privately for this process.

`assets/fonts/Nunito[wght].ttf` is the unmodified variable font from Google Fonts' repository (see
assets/fonts/README.md and licenses/Nunito-OFL.txt). On Windows it is registered with AddFontResourceExW and
FR_PRIVATE: only this process sees it, nothing is installed and nothing is left behind. GDI exposes the regular and
bold weights as the family Â«NunitoÂ» and the heavier named instances as their own families (Â«Nunito ExtraBoldÂ»,
Â«Nunito BlackÂ»); those are the ones the launcher uses. When the file is missing or cannot be registered (another
system, a damaged package) every helper answers with Segoe UI, the launcher's previous typeface.
"""
import os
from pathlib import Path

FAMILY, DISPLAY_FAMILY, FALLBACK = 'Nunito', 'Nunito ExtraBold', 'Segoe UI'
FR_PRIVATE = 0x10
_state = {'loaded': None}


def font_files(root):
    """The font files shipped beside the launcher, in a stable order."""
    folder = Path(root) / 'assets' / 'fonts'
    try:
        return sorted(p for p in folder.iterdir() if p.suffix.lower() in ('.ttf', '.otf') and p.is_file())
    except OSError:
        return []


def _add_font_resource(path):
    """Fonts added from one file for this process only (0 when Windows refuses it)."""
    import ctypes
    return ctypes.windll.gdi32.AddFontResourceExW(str(path), FR_PRIVATE, 0)


def register(files, add=None, platform=None):
    """Registers every file; True when at least one font was added. Never raises: a failure means the fallback."""
    if (platform or os.name) != 'nt':
        return False
    add = add or _add_font_resource
    added = 0
    for path in files:
        try:
            added += max(0, int(add(path)))
        except Exception:
            continue
    return added > 0


def load(root=None, *, add=None, platform=None):
    """Loads the launcher's fonts once per process, before any Tk font is made. Returns whether Nunito is usable."""
    if _state['loaded'] is None or add is not None:
        if root is None:
            root = Path(__file__).resolve().parent.parent
        _state['loaded'] = register(font_files(root), add, platform)
    return _state['loaded']


def confirm(families):
    """After Tk exists: keep Nunito only if Tk really lists it (a registration Windows accepted but cannot use)."""
    if _state['loaded'] and FAMILY not in set(families):
        _state['loaded'] = False
    return bool(_state['loaded'])


def loaded():
    return bool(_state['loaded'])


def family():
    return FAMILY if _state['loaded'] else FALLBACK


def ui(size, weight='normal'):
    """A Tk font description for interface text: weight 'normal' or 'bold'."""
    return (family(), size, 'bold') if weight == 'bold' else (family(), size)


def display(size):
    """Titles and the brand: the heavy rounded cut, or bold Segoe UI as the fallback."""
    return (DISPLAY_FAMILY, size) if _state['loaded'] else (FALLBACK, size, 'bold')
