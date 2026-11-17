"""Run a private test on an isolated Win32 desktop, never switching the input desktop."""
from pathlib import Path
import argparse
import ctypes as C
from ctypes import wintypes as W
import subprocess
import sys
import os
import locale
import tempfile
import uuid


class _DesktopProcess:
    """Minimal process interface with an actual Win32 STARTUPINFO.lpDesktop.

    subprocess.STARTUPINFO accepts arbitrary Python attributes, but CPython's
    CreateProcess adapter does not pass lpDesktop to Windows.
    """
    def __init__(self, command, cwd, desktop):
        import msvcrt

        class Startup(C.Structure):
            _fields_ = [('cb', W.DWORD), ('lpReserved', W.LPWSTR),
                        ('lpDesktop', W.LPWSTR), ('lpTitle', W.LPWSTR),
                        ('dwX', W.DWORD), ('dwY', W.DWORD),
                        ('dwXSize', W.DWORD), ('dwYSize', W.DWORD),
                        ('dwXCountChars', W.DWORD), ('dwYCountChars', W.DWORD),
                        ('dwFillAttribute', W.DWORD), ('dwFlags', W.DWORD),
                        ('wShowWindow', W.WORD), ('cbReserved2', W.WORD),
                        ('lpReserved2', C.POINTER(W.BYTE)),
                        ('hStdInput', W.HANDLE), ('hStdOutput', W.HANDLE),
                        ('hStdError', W.HANDLE)]

        class Information(C.Structure):
            _fields_ = [('hProcess', W.HANDLE), ('hThread', W.HANDLE),
                        ('dwProcessId', W.DWORD), ('dwThreadId', W.DWORD)]

        self.kernel = C.WinDLL('kernel32', use_last_error=True)
        self.kernel.CreateProcessW.argtypes = [W.LPCWSTR, W.LPWSTR, C.c_void_p,
            C.c_void_p, W.BOOL, W.DWORD, C.c_void_p, W.LPCWSTR,
            C.POINTER(Startup), C.POINTER(Information)]
        self.kernel.CreateProcessW.restype = W.BOOL
        self.kernel.CloseHandle.argtypes = [W.HANDLE]
        self.kernel.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
        self.kernel.WaitForSingleObject.restype = W.DWORD
        self.kernel.GetExitCodeProcess.argtypes = [W.HANDLE, C.POINTER(W.DWORD)]
        self.kernel.TerminateProcess.argtypes = [W.HANDLE, W.UINT]
        self.command = command
        self.returncode = None
        self.handle = None
        self.streams = []
        try:
            self.streams = [open(os.devnull, 'rb'), tempfile.TemporaryFile(),
                            tempfile.TemporaryFile()]
            handles = [msvcrt.get_osfhandle(stream.fileno()) for stream in self.streams]
            for handle in handles:
                os.set_handle_inheritable(handle, True)
            try:
                startup = Startup()
                startup.cb = C.sizeof(startup)
                startup.lpDesktop = desktop
                startup.dwFlags = 0x100 | 1  # STARTF_USESTDHANDLES | USESHOWWINDOW
                startup.wShowWindow = 0
                startup.hStdInput, startup.hStdOutput, startup.hStdError = handles
                information = Information()
                line = C.create_unicode_buffer(subprocess.list2cmdline([str(x) for x in command]))
                if not self.kernel.CreateProcessW(None, line, None, None, True,
                        subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS,
                        None, str(Path(cwd).resolve()), C.byref(startup), C.byref(information)):
                    raise C.WinError(C.get_last_error())
                self.handle = information.hProcess
                self.pid = information.dwProcessId
                self.kernel.CloseHandle(information.hThread)
            finally:
                for handle in handles:
                    os.set_handle_inheritable(handle, False)
        except BaseException:
            self.close()
            raise

    def poll(self):
        if self.returncode is None:
            wait = self.kernel.WaitForSingleObject(self.handle, 0)
            if wait == 0:
                code = W.DWORD()
                if not self.kernel.GetExitCodeProcess(self.handle, C.byref(code)):
                    raise C.WinError(C.get_last_error())
                self.returncode = code.value
            elif wait != 258:  # WAIT_TIMEOUT
                raise C.WinError(C.get_last_error())
        return self.returncode

    def kill(self):
        if self.poll() is None and not self.kernel.TerminateProcess(self.handle, 1):
            if self.poll() is None:
                raise C.WinError(C.get_last_error())

    def communicate(self, timeout):
        wait = self.kernel.WaitForSingleObject(self.handle, min(int(timeout * 1000), 0xfffffffe))
        if wait == 258:
            raise subprocess.TimeoutExpired(self.command, timeout)
        if wait != 0:
            raise C.WinError(C.get_last_error())
        self.poll()
        result = []
        for stream in self.streams[1:]:
            stream.seek(0)
            result.append(stream.read().decode(locale.getpreferredencoding(False), errors='replace')
                          .replace('\r\n', '\n').replace('\r', '\n'))
        return tuple(result)

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None
        for stream in self.streams:
            stream.close()


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
        process=_DesktopProcess(command,cwd,'WinSta0\\'+name)
        try:
            if on_start:on_start(process,desktop)
            try:stdout,stderr=process.communicate(timeout=timeout)
            except subprocess.TimeoutExpired:
                if not allow_forced_stop:raise
                process.kill();stdout,stderr=process.communicate(timeout=5)
                print('OFFSCREEN: bounded probe required forced shutdown; clean game shutdown is not verified')
            sys.stdout.write(stdout);sys.stderr.write(stderr)
            if input_name()!=original:raise RuntimeError('Input desktop changed unexpectedly.')
            print('OFFSCREEN: input desktop unchanged')
            return process.returncode
        except BaseException:
            process.kill();process.communicate(timeout=5);raise
        finally:process.close()
    finally:user.CloseDesktop(desktop)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cwd',type=Path,required=True)
    parser.add_argument('--timeout',type=float,default=30)
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    if not args.command:parser.error('Specify the private test executable.')
    sys.exit(run(args.command,args.cwd.resolve(),args.timeout))
