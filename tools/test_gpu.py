"""Build and run asset-free native GPU regressions on isolated Win32 desktops."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from PIL import Image
from opensoccermania.backends import bundled_backend
from opensoccermania.title_presentation import _bmp
from opensoccermania.video import VideoSettings, configure_wrapper
from tools.run_offscreen import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, required=True)
    args = parser.parse_args()
    compiler = args.toolchain / 'bin/i686-w64-mingw32-clang++.exe'
    bridge = ROOT / 'build/native/d3d8.dll'
    input_bridge = ROOT / 'build/native/dinput8.dll'
    for required in (compiler, bridge, input_bridge):
        if not required.is_file():
            parser.error(f'Missing build dependency: {required}; build native components first.')
    backend, config = bundled_backend(ROOT)
    output = ROOT / 'build/tests'
    output.mkdir(parents=True, exist_ok=True)
    # A fresh, retained directory prevents old private fixtures/configs from
    # silently determining whether a regression passes.
    workspace = Path(tempfile.mkdtemp(prefix='gpu-', dir=output))
    report = {'bridge_sha256': hashlib.sha256(bridge.read_bytes()).hexdigest(),
              'backend_sha256': hashlib.sha256(backend.read_bytes()).hexdigest(),
              'cases': [], 'passed': False}
    try:
        cases = ('screen_render', 'world_effects_render', 'settings_render',
                 'menu_background_render', 'title_render', 'layered_com',
                 'layered_window', 'input_mailbox', 'device_reset', 'shader_reset', 'clear_rect', 'save_metadata_runtime', 'portable_imports')
        for name in cases:
            directory = workspace / name
            directory.mkdir()
            executable = directory / f'{name}.exe'
            subprocess.run([str(compiler), '-std=c++17', '-O2', '-Wall', '-Wextra',
                            '-static', '-Wl,--image-base,0x400000',
                            '-Wl,--disable-dynamicbase',
                            str(ROOT / f'tests/native_{name}.cpp'), '-lgdi32',
                            '-ldxguid', '-ldinput8', '-lshell32', '-ladvapi32',
                            '-o', str(executable)], check=True)
            shutil.copy2(bridge, directory / 'd3d8.dll')
            shutil.copy2(backend, directory / 'd3d8_backend.dll')
            shutil.copy2(input_bridge, directory / 'dinput8.dll')
            settings = VideoSettings(horplus=True, display_mode='Windowed',
                                     vsync=False, render_fps=0)
            (directory / 'dgVoodoo.conf').write_text(
                configure_wrapper(config.read_text(), settings))
            (directory / 'opensoccermania.ini').write_text(
                '[Graphics]\nOutputWidth=1920\nOutputHeight=1080\n'
                'DisplayMode=Windowed\nExperimentalHorPlus=1\n'
                f'VerifiedExecutable={int(name in ("world_effects_render","save_metadata_runtime"))}\n'
                'AutoWindowAspect=0\nPerformanceHud=0\nRenderFPS=0\n'
                'VSync=0\nSpriteSeamFix=7\nOriginalAspect=0\n'
                '[Diagnostics]\nIsolateInstance=1\n')
            if name == 'title_render':
                image = Image.new('RGBA', (2048, 512), (0x12, 0x34, 0x56, 255))
                image.paste((0, 0, 0, 0), (682, 0, 1365, 512))
                (directory / 'synthetic.bmp').write_bytes(_bmp(image))
                (directory / 'title-test.ini').write_text(
                    '[UI]\nTitleBackgroundPath=synthetic.bmp\n')
            variants = [()]
            if name == 'screen_render':
                variants += [('--horizon',), ('--far-boundary',), ('--color-only',)]
            elif name == 'clear_rect':
                variants += [('--original-aspect',)]
            elif name == 'device_reset':
                variants += [('--adaptive',)]
            elif name == 'layered_window':
                variants += [('--adaptive',), ('--original-aspect',)]
            elif name == 'save_metadata_runtime':
                variants += [('--unsupported',), ('--trace',)]
            elif name == 'input_mailbox':
                variants = [(str(directory / 'dinput8.dll'),)]
            for variant in variants:
                code = run([str(executable), *variant], directory, 45)
                report['cases'].append({'name': name, 'arguments': list(variant),
                                        'exit_code': code})
                if code:
                    raise RuntimeError(f'GPU/native integration failed: {name} {variant}: {code}')
                print(f'PASS {name} {" ".join(variant)}', flush=True)
        report['passed'] = True
    finally:
        (workspace / 'report.json').write_text(json.dumps(report, indent=2))
        print(f'GPU evidence: {workspace / "report.json"}', flush=True)


if __name__ == '__main__':
    main()
