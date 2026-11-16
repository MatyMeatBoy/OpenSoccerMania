"""Exercise F11 against a private test window without sending desktop input."""
from pathlib import Path
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from opensoccermania.runtime import configure_bridge,install_bridge
from opensoccermania.video import VideoSettings,configure_wrapper


def verify(initial):
    entry=ROOT/'local/library/football-mania'
    u=C.windll.user32
    u.GetWindowLongW.argtypes=[W.HWND,C.c_int]
    u.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
    u.SendMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
    u.GetWindowRect.argtypes=[W.HWND,C.POINTER(W.RECT)]
    before=[u.GetSystemMetrics(0),u.GetSystemMetrics(1)]
    paths=[entry/'opensoccermania.ini',entry/'dgVoodoo.conf']
    backups={path:path.read_bytes() for path in paths}
    log=entry/'opensoccermania-render.log'
    offset=log.stat().st_size if log.exists() else 0
    process=None
    window=None
    try:
        settings=VideoSettings(horplus=True,display_mode=initial,vsync=False)
        install_bridge(entry,ROOT/'build/native/d3d8.dll')
        configure_bridge(entry,settings)
        paths[1].write_text(configure_wrapper(paths[1].read_text(),settings))
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=4
        process=subprocess.Popen([str(entry/'Game.exe'),'.\\Mania\\CD\\'],cwd=entry,startupinfo=startup)
        windows=[]
        @C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
        def visit(hwnd,param):
            pid=W.DWORD();u.GetWindowThreadProcessId(hwnd,C.byref(pid))
            title=C.create_unicode_buffer(256);u.GetWindowTextW(hwnd,title,256)
            if pid.value==process.pid and title.value=='Football Mania':windows.append(hwnd)
            return True
        for _ in range(100):
            u.EnumWindows(visit,0)
            if windows:break
            if process.poll() is not None:raise AssertionError('Game exited before creating its window.')
            time.sleep(.1)
        assert windows,'Game window not found'
        window=windows[0];time.sleep(3)
        def state():
            rect=W.RECT();assert u.GetWindowRect(window,C.byref(rect))
            style=u.GetWindowLongW(window,-16)&0xffffffff
            return {'rect':[rect.left,rect.top,rect.right,rect.bottom],
                    'caption':bool(style&0xc00000),'topmost':bool(u.GetWindowLongW(window,-20)&8)}
        states=[state()]
        for _ in range(2):
            u.SendMessageW(window,0x100,0x7a,1) # WM_KEYDOWN / F11; bypass the legacy queue filter
            time.sleep(1)
            u.SendMessageW(window,0x101,0x7a,0xc0000001)
            states.append(state())
        u.SendMessageW(window,0x100,0x7a,0x40000001) # held-key autorepeat must not toggle
        time.sleep(.5)
        assert state()==states[-1],'Held F11 toggled repeatedly'
        assert [s['caption'] for s in states]==([True,False,True] if initial=='Windowed' else [False,True,False]),states
        assert not any(s['topmost'] for s in states),states
        borderless=states[1] if initial=='Windowed' else states[0]
        # This test uses the primary-monitor private test window.
        assert borderless['rect']==[0,0,*before],borderless
        assert before==[u.GetSystemMetrics(0),u.GetSystemMetrics(1)]
        newlog=log.read_bytes()[offset:].decode(errors='replace')
        assert newlog.count('WINDOW_MODE source=hotkey')==2,newlog
        result={'initial':initial,'desktop':before,'states':states,'log':newlog}
        out=ROOT/'local/timing-evidence'/f'window-toggle-{initial.lower()}.json'
        out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(result,indent=2))
        print(json.dumps({'initial':initial,'states':states},indent=2))
    finally:
        if process and process.poll() is None:
            if window:u.PostMessageW(window,0x10,0,0)
            try:process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.terminate();process.wait(timeout=5)
                raise AssertionError('Game did not close cleanly')
        for path,data in backups.items():path.write_bytes(data)
    assert process.returncode==0,process.returncode


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--initial',choices=['Windowed','Borderless'],default='Windowed')
    verify(parser.parse_args().initial)
