"""Controller display families; actions retain SDL south/east/west/north positions."""
from enum import IntEnum


class ControllerFamily(IntEnum):
    XBOX360 = 1
    PS4 = 2
    SWITCH_PRO = 3
    STEAM_DECK = 4


PROMPT_STYLES = ('Auto', 'Xbox 360', 'PlayStation 4', 'Switch Pro', 'Steam Deck')
_FAMILIES = {'Xbox 360': ControllerFamily.XBOX360,
             'PlayStation 4': ControllerFamily.PS4,
             'Switch Pro': ControllerFamily.SWITCH_PRO, 'Steam Deck': ControllerFamily.STEAM_DECK}
_LABELS = {ControllerFamily.XBOX360: ('A', 'B', 'X', 'Y'),
           ControllerFamily.PS4: ('Cross', 'Circle', 'Square', 'Triangle'),
           ControllerFamily.SWITCH_PRO: ('B', 'A', 'Y', 'X'),
           ControllerFamily.STEAM_DECK: ('A', 'B', 'X', 'Y')}
_ASSET_DIRS = {ControllerFamily.XBOX360: 'xbox360',
               ControllerFamily.PS4: 'ps4', ControllerFamily.SWITCH_PRO: 'switch_pro', ControllerFamily.STEAM_DECK: 'steam_deck'}
BUTTON_ASSETS = ('south', 'east', 'west', 'north', 'left_shoulder', 'right_shoulder',
                 'left_trigger', 'right_trigger', 'back', 'start', 'left_stick',
                 'right_stick', 'dpad')


def _family(value):
    try:
        return ControllerFamily(value)
    except (ValueError, TypeError):
        return ControllerFamily.XBOX360


def detect_family(gamepad_type=0, name=''):
    # SDL_GamepadType values since SDL 3.2; mapped type takes precedence over names.
    if gamepad_type in (2, 3):
        return ControllerFamily.XBOX360
    if gamepad_type in (4, 5, 6):
        return ControllerFamily.PS4
    if gamepad_type in (7, 8, 9, 10):
        return ControllerFamily.SWITCH_PRO
    if gamepad_type == 12:  # SDL Steam family; Deck-compatible positional glyphs.
        return ControllerFamily.STEAM_DECK
    name = name.casefold()
    if 'steam deck' in name or 'steamdeck' in name:
        return ControllerFamily.STEAM_DECK
    if any(token in name for token in ('dualshock', 'dualsense', 'ps4', 'playstation', 'sony wireless')):
        return ControllerFamily.PS4
    if any(token in name for token in ('nintendo', 'switch pro', 'joy-con')):
        return ControllerFamily.SWITCH_PRO
    return ControllerFamily.XBOX360


def face_labels(family):
    return _LABELS[_family(family)]


def resolve_family(style, detected=ControllerFamily.XBOX360):
    return _FAMILIES.get(style, _family(detected))


def family_flags(family):
    try:
        return int(ControllerFamily(family)) << 8
    except (ValueError, TypeError):
        return 0


def asset_path(family, button):
    if button not in BUTTON_ASSETS:
        raise ValueError('Unknown controller button asset')
    return f'controllers/{_ASSET_DIRS[_family(family)]}/{button}.png'


def active_family(devices, selected=None, recent=None):
    """Choose a display owner from connected devices, never merge input states."""
    for identity in (selected, recent):
        if identity in devices:
            return _family(devices[identity])
    return _family(next(iter(devices.values()), ControllerFamily.XBOX360))


def scheme_rows(family):
    family = _family(family)
    labels = list(face_labels(family))
    labels += {ControllerFamily.XBOX360: ('LB', 'RB', 'Start', 'Back'),
               ControllerFamily.PS4: ('L1', 'R1', 'Options', 'Share'),
               ControllerFamily.SWITCH_PRO: ('L', 'R', 'Plus', 'Minus'),
               ControllerFamily.STEAM_DECK: ('L1', 'R1', 'Menu', 'View')}[family]
    roles = ('south', 'east', 'west', 'north', 'left_shoulder', 'right_shoulder', 'start', 'back')
    actions = ('Pass / slide tackle', 'Shoot', 'Long ball / foot-in tackle', 'Power-up',
               'Sprint', 'Switch player / skill move', 'Pause', 'Skip video')
    return tuple(zip(roles, labels, actions))


def family_name(family):
    return next(label for label, value in _FAMILIES.items() if value == _family(family))
