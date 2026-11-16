"""Run a private test on an isolated Win32 desktop, never switching the input desktop."""
from pathlib import Path
import argparse
import ctypes as C
from ctypes import wintypes as W
import subprocess
import sys
import uuid


def run(command,cwd,timeout,on_start=None,allow_forced_stop=False):
    user=C.WinDLL('user32',use_last_error=True)
    user.CreateDesktopW.argtypes=[W.LPCWSTR,W.LPCWSTR,C.c_void_p,W.DWORD,W.DWORD,C.c_void_p]
    user.CreateDesktopW.restype=W.HANDLE
    user.OpenInputDesktop.argtypes=[W.DWORD,W.BOOL,W.DWORD];user.OpenInputDesktop.restype=W.HANDLE
    user.GetUserObjectInformationW.argtypes=[W.HANDLE,C.c_int,C.c_void_p,W.DWORD,C.POINTER(W.DWORD)]
    user.CloseDesktop.argtypes=[W.HANDLE]

    def input_name():
        handle=user.OpenInputDesktop(0,False,1)
        if not handle:raise C.WinError(C.get_last_error())
        try:
            text=C.create_unicode_buffer(256);length=W.DWORD()
            if not user.GetUserObjectInformationW(handle,2,text,C.sizeof(text),C.byref(length)):raise C.WinError(C.get_last_error())
            return text.value
        finally:user.CloseDesktop(handle)

    original=input_name()
    name='OpenSoccerMania-Test-'+uuid.uuid4().hex[:12]
    desktop=user.CreateDesktopW(name,None,None,0,0x01ff,None)
    if not desktop:raise C.WinError(C.get_last_error())
    try:
        startup=subprocess.STARTUPINFO();startup.lpDesktop='WinSta0\\'+name
        startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        process=subprocess.Popen(command,cwd=cwd,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS,
                                 stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            if on_start:on_start(process,desktop)
            try:stdout,stderr=process.communicate(timeout=timeout)
            except subprocess.TimeoutExpired:
                if not allow_forced_stop:raise
                process.kill();stdout,stderr=process.communicate(timeout=5)
                print('OFFSCREEN: bounded probe required forced shutdown; clean game shutdown is not verified')
        except BaseException:
            process.kill();process.communicate(timeout=5);raise
        sys.stdout.write(stdout);sys.stderr.write(stderr)
        if input_name()!=original:raise RuntimeError('Input desktop changed unexpectedly.')
        print('OFFSCREEN: input desktop unchanged')
        return process.returncode
    finally:user.CloseDesktop(desktop)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cwd',type=Path,required=True)
    parser.add_argument('--timeout',type=float,default=30)
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    if not args.command:parser.error('Specify the private test executable.')
    sys.exit(run(args.command,args.cwd.resolve(),args.timeout))
