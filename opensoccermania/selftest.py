"""Portable smoke test using an asset-free ISO; never starts a game."""
import hashlib
import json
import logging
from pathlib import Path
import struct
import tempfile

from PIL import Image

# Keep these exact redistribution pins aligned with package_release.py/backends.py.
VENDOR_HASHES = {
    'runtime/graphics/dgVoodoo/d3d8.dll': '25a0ceeb85fcc4675c6f964442eecc14fedd504effd39c931a627249ad4685e2',
    'runtime/graphics/dgVoodoo/dgVoodoo.conf': '45cc0eb8e6ed4f067aca1b2e4ac1268dfbd164bbbd06d264c53e8a60642c4dbf',
    'runtime/graphics/dgVoodoo/manifest.json': '44e4899cf4baf6d975f1f1e80ccf5c2914c97c1870082fd1dcf9c362f3396a56',
}
RESOURCE_PATHS = ('assets/fonts/Nunito.ttf', 'assets/backgrounds/title-extension.png',
                  'runtime/SDL3.dll', 'native/d3d8.dll', 'native/dinput8.dll', *VENDOR_HASHES, 'licenses/dgVoodoo.txt')


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
            expected = 0x8664 if name == 'runtime/SDL3.dll' else 0x14c
            offset = struct.unpack_from('<I', raw, 0x3c)[0] if len(raw) >= 64 else len(raw)
            if (raw[:2] != b'MZ' or offset + 6 > len(raw) or
                    raw[offset:offset + 4] != b'PE\0\0' or
                    struct.unpack_from('<H', raw, offset + 4)[0] != expected):
                raise ValueError(f'Wrong packaged DLL architecture: {name}')
        hashes[name] = hashlib.sha256(raw).hexdigest()
        if name in VENDOR_HASHES and hashes[name] != VENDOR_HASHES[name]:
            raise ValueError(f'Bundled graphics vendor hash mismatch: {name}')
    manifest = json.loads((root / 'runtime/graphics/dgVoodoo/manifest.json').read_text(encoding='utf-8'))
    if manifest.get('version') != '2.87.6' or manifest.get('files') != {Path(name).name: digest for name, digest in VENDOR_HASHES.items() if not name.endswith('.json')}:
        raise ValueError('Invalid bundled graphics vendor manifest.')
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
                from .smoke_fixture import write_game_iso
                iso=Path(temporary)/'synthetic.iso';write_game_iso(iso)
                old_mount=launcher.mounted_iso;old_popen=launcher.subprocess.Popen
                old_worker=app.run_work
                def no_mount(*args,**kwargs):raise ValueError('Exercise the included ISO reader.')
                def no_process(*args,**kwargs):raise AssertionError('Portable import must not start an external process.')
                try:
                    launcher.mounted_iso=no_mount;launcher.subprocess.Popen=no_process
                    app.run_work=lambda work:app.set_entry(work())
                    app.import_path(iso,is_iso=True)
                    if not app.entry or not app.save_video():
                        raise RuntimeError('Fresh ISO graphics provisioning failed.')
                    if not (app.entry/'d3d8_backend.dll').is_file() or not (app.entry/'dgVoodoo.conf').is_file():
                        raise RuntimeError('Included graphics files were not installed.')
                finally:
                    launcher.mounted_iso=old_mount;launcher.subprocess.Popen=old_popen;app.run_work=old_worker
                report.update(passed=True, pages=4, settings_isolated=True, synthetic_iso_import=True,
                              external_processes=False, graphics_provisioned=True)
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
