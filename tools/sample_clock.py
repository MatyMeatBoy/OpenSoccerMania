"""Read-only timing samples from the explicitly recorded private test process."""
from pathlib import Path
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import struct
import time


def sample(duration, output):
    root = Path(__file__).resolve().parents[1]
    pid = int((root / 'local/test-game.pid').read_text())
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
    kernel.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD)]
    kernel.CloseHandle.argtypes = [W.HANDLE]
    handle = kernel.OpenProcess(0x1010, False, pid)
    if not handle:
        raise C.WinError(C.get_last_error())
    try:
        path = C.create_unicode_buffer(32768)
        length = W.DWORD(len(path))
        if not kernel.QueryFullProcessImageNameW(handle, 0, path, C.byref(length)):
            raise C.WinError(C.get_last_error())
        executable = Path(path.value).resolve()
        if not executable.is_relative_to(root / 'local/library') or executable.name.lower() != 'game.exe':
            raise ValueError('The recorded process is not a private imported game.')

        def read(address, size):
            data = C.create_string_buffer(size)
            copied = C.c_size_t()
            if not kernel.ReadProcessMemory(handle, address, data, size, C.byref(copied)) or copied.value != size:
                raise C.WinError(C.get_last_error())
            return data.raw

        floats = {'configured_logic_hz': 0x5a36ec, 'scheduler_rate': 0x61fc24,
                  'simulation_phase': 0x5e8214, 'consumed_steps': 0x5e8210,
                  'interpolation': 0x5e8218, 'camera_aspect': 0x5e6cb8,
                  'game_speed_percent': 0x631e6c}
        records = []
        start = time.perf_counter()
        while time.perf_counter() - start < duration:
            row = {'seconds': time.perf_counter() - start}
            row.update({name: struct.unpack('<f', read(address, 4))[0] for name, address in floats.items()})
            records.append(row)
            time.sleep(.1)
        result = {'pid': pid, 'executable': str(executable),
                  'selected_registry': read(0x623580, 256).split(b'\0')[0].decode('ascii', errors='replace'),
                  'config': (executable.parent / 'opensoccermania.ini').read_text(), 'samples': records}
        output = Path(output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2))
        print(json.dumps({key: sorted(set(round(row[key], 5) for row in records))[:12]
                          for key in ['configured_logic_hz', 'scheduler_rate', 'camera_aspect']}, indent=2))
        print(result['selected_registry'])
    finally:
        kernel.CloseHandle(handle)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--duration', type=float, default=10)
    parser.add_argument('--output', type=Path, default=Path('local/timing-evidence/samples.json'))
    args = parser.parse_args()
    sample(args.duration, args.output)
