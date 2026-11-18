"""Keep an owned game probe on a separate desktop; mute only its audio session."""
from pathlib import Path
import ctypes as C
from ctypes import wintypes as W
import json
import shutil
import subprocess
import sys
import threading
import time
import argparse

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from opensoccermania.video import RENDER_LIMITS,VideoSettings,configure_wrapper
from opensoccermania.runtime import configure_bridge
from tools.run_offscreen import run


def main(fps=60,duration=240):
    source=ROOT/'local/library/football-mania'
    entry=ROOT/'local/offscreen-game'
    evidence=ROOT/'local/offscreen-probe'
    entry.mkdir(exist_ok=True);evidence.mkdir(exist_ok=True)
    for name in ['Game.exe','game.dat','d3d8_backend.dll','dgVoodoo.conf']:
        candidate=source/name
        if candidate.exists():shutil.copy2(candidate,entry/name)
    for name in ['d3d8.dll','dinput8.dll']:shutil.copy2(ROOT/'build/native'/name,entry/name)
    if (source/'Messages').is_dir() and not (entry/'Messages').exists():shutil.copytree(source/'Messages',entry/'Messages')
    mania=entry/'Mania'
    if not mania.exists():
        def quote(value):return "'"+str(value).replace("'","''")+"'"
        result=subprocess.run(['powershell','-NoProfile','-Command',f'New-Item -ItemType Junction -Path {quote(mania)} -Target {quote(source/"Mania")} | Out-Null'],capture_output=True,text=True)
        if result.returncode:raise RuntimeError(result.stderr)
    if mania.resolve()!=(source/'Mania').resolve():raise ValueError('Unexpected test-data junction.')
    settings=VideoSettings(horplus=True,display_mode='Windowed',vsync=False,render_fps=fps)
    configure_bridge(entry,settings)
    config=entry/'opensoccermania.ini';config.write_text(config.read_text()+'\n[Diagnostics]\nIsolateInstance=1\nTraceAnimation=1\n')
    conf=entry/'dgVoodoo.conf';conf.write_text(configure_wrapper(conf.read_text(),settings))
    stop=evidence/'stop.request'
    if stop.exists():stop.unlink()
    (evidence/'configuration.json').write_text(json.dumps({
        'requested_render_fps':fps,'render_fps':settings.render_fps,
        'simulation_hz_changed':False}),encoding='utf-8')

    def probe(process,desktop):
        started=time.monotonic();finished=threading.Event()
        (evidence/'pid.txt').write_text(str(process.pid))
        def mute():
            while not finished.is_set() and process.poll() is None:
                subprocess.run([str(ROOT/'local/mute-test-audio.exe'),str(process.pid)],creationflags=subprocess.CREATE_NO_WINDOW,capture_output=True)
                finished.wait(.2 if time.monotonic()-started<20 else 5)
        worker=threading.Thread(target=mute,daemon=True);worker.start()
        try:
            while process.poll() is None and time.monotonic()-started<duration and not stop.exists():
                (evidence/'status.json').write_text(json.dumps({'pid':process.pid,'elapsed_seconds':round(time.monotonic()-started,1),'running':True,'desktop':'isolated','entry':str(entry)}))
                time.sleep(1)
        finally:
            user=C.WinDLL('user32',use_last_error=True)
            user.EnumDesktopWindows.argtypes=[W.HANDLE,C.c_void_p,W.LPARAM]
            user.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
            user.PostMessageW.restype=W.BOOL
            user.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
            user.GetWindowThreadProcessId.restype=W.DWORD
            user.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
            user.GetClassNameW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
            user.GetClientRect.argtypes=[W.HWND,C.POINTER(W.RECT)]
            user.IsIconic.argtypes=[W.HWND];user.IsIconic.restype=W.BOOL
            receipt={'pid':process.pid,'windows':[],'messages':[]}
            @C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
            def inspect(hwnd,param):
                pid=W.DWORD();thread=user.GetWindowThreadProcessId(hwnd,C.byref(pid))
                if pid.value==process.pid:
                    title=C.create_unicode_buffer(256);user.GetWindowTextW(hwnd,title,len(title))
                    cls=C.create_unicode_buffer(256);user.GetClassNameW(hwnd,cls,len(cls))
                    if title.value in ['Football Mania','Soccer Mania'] or cls.value=='SD Window':
                        rect=W.RECT();valid=user.GetClientRect(hwnd,C.byref(rect))
                        receipt['windows'].append({'hwnd':int(hwnd),'thread':int(thread),
                            'title':title.value,'class':cls.value,
                            'client':[rect.right,rect.bottom] if valid else None,
                            'minimized':bool(user.IsIconic(hwnd))})
                return True
            if process.poll() is None:
                enumerated=user.EnumDesktopWindows(desktop,inspect,0)
                receipt['enumerated']=bool(enumerated)
                if not enumerated:receipt['enumeration_error']=C.get_last_error()
            close_started=time.monotonic()
            for window in receipt['windows']:
                posted=user.PostMessageW(window['hwnd'],0x10,0,0)
                message={'message':'WM_CLOSE','hwnd':window['hwnd'],'posted':bool(posted)}
                if not posted:message['error']=C.get_last_error()
                receipt['messages'].append(message)
            # Let the actual window close finish before stopping observation.
            # WM_QUIT is not a substitute for the application's close path.
            while process.poll() is None and time.monotonic()-close_started<8:
                time.sleep(.1)
            receipt['exit_after_close']=process.poll()
            receipt['seconds']=time.monotonic()-close_started
            (evidence/'close-receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
            finished.set();worker.join(timeout=3)
    code=run([str(entry/'Game.exe'),'.\\Mania\\CD\\'],entry,10,on_start=probe,allow_forced_stop=True)
    (evidence/'result.json').write_text(json.dumps({'exit_code':code,'desktop_unchanged':True,'clean_shutdown':code==0}))
    (evidence/'status.json').write_text(json.dumps({'running':False,'exit_code':code}))
    return code


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fps',type=int,choices=sorted(set(RENDER_LIMITS.values())),default=60,
                        help='Requested render cap; 0 means Unlimited. Logic Hz is unchanged.')
    parser.add_argument('--duration',type=int,default=240)
    args=parser.parse_args();sys.exit(main(args.fps,args.duration))
