"""Explicit portable-package smoke test; never imports or starts a game."""
import hashlib
import json
import logging
from pathlib import Path
import struct
import tempfile

from PIL import Image

RESOURCE_PATHS = ('assets/fonts/Nunito.ttf', 'assets/backgrounds/title-extension.png',
                  'runtime/SDL3.dll', 'native/d3d8.dll', 'native/dinput8.dll')


def validate_resources(root):
    root = Path(root)
    hashes = {}
    for name in RESOURCE_PATHS:
        path = root / name
        if not path.is_file():
            raise ValueError(f'Missing packaged resource: {name}')
        if path.stat().st_size > 64 * 1024 * 1024:
            raise ValueError(f'Oversized packaged resource: {name}')
        raw = path.read_bytes()
        if name.endswith('.dll'):
            expected = 0x8664 if name.startswith('runtime/') else 0x14c
            offset = struct.unpack_from('<I', raw, 0x3c)[0] if len(raw) >= 64 else len(raw)
            if (raw[:2] != b'MZ' or offset + 6 > len(raw) or
                    raw[offset:offset + 4] != b'PE\0\0' or
                    struct.unpack_from('<H', raw, offset + 4)[0] != expected):
                raise ValueError(f'Wrong packaged DLL architecture: {name}')
        hashes[name] = hashlib.sha256(raw).hexdigest()
    with Image.open(root / 'assets/backgrounds/title-extension.png') as image:
        image.verify()
    if not (root / 'assets/fonts/Nunito.ttf').read_bytes().startswith(b'\x00\x01\x00\x00'):
        raise ValueError('Invalid packaged Nunito font.')
    return hashes


def run(report_path):
    from . import __version__
    from . import launcher
    import tkinter as tk

    report = {'version': __version__, 'passed': False, 'game_started': False}
    original_home = launcher.HOME
    log = logging.getLogger('opensoccermania')
    initial_handlers = set(log.handlers)
    root = app = None
    try:
        report['resources'] = validate_resources(launcher.ROOT)
        with tempfile.TemporaryDirectory(prefix='OpenSoccerMania-Smoke-') as temporary:
            launcher.HOME = Path(temporary)
            try:
                root = tk.Tk()
                root.withdraw()
                app = launcher.Launcher(root)
                errors = []
                root.report_callback_exception = lambda kind, value, trace: errors.append(str(value))
                for page in ('Home', 'Graphics', 'Controls', 'Compatibility'):
                    app.show(page)
                    root.update_idletasks()
                if errors:
                    raise RuntimeError('; '.join(errors))
                report.update(passed=True, pages=4, settings_isolated=True)
            finally:
                if app:
                    app.close()
                    root = None
                if root:
                    root.destroy()
                for handler in set(log.handlers) - initial_handlers:
                    log.removeHandler(handler)
                    handler.close()
    except Exception as error:
        report['error'] = f'{type(error).__name__}: {error}'
    finally:
        launcher.HOME = original_home
        Path(report_path).write_text(json.dumps(report, indent=2), encoding='utf-8')
    return 0 if report['passed'] else 1
