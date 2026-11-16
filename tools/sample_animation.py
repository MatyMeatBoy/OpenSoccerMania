"""Read animation-state samples from an explicitly selected private game PID."""
from pathlib import Path
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import struct
import time
import hashlib
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from opensoccermania.patches import SUPPORTED_RUNTIME_HASHES


def sample(pid,duration,output):
    root=Path(__file__).resolve().parents[1]
    kernel=C.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];kernel.OpenProcess.restype=W.HANDLE
    kernel.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
    kernel.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,C.POINTER(W.DWORD)]
    kernel.CloseHandle.argtypes=[W.HANDLE]
    handle=kernel.OpenProcess(0x1010,False,pid)
    if not handle:raise C.WinError(C.get_last_error())
    try:
        path=C.create_unicode_buffer(32768);size=W.DWORD(len(path))
        if not kernel.QueryFullProcessImageNameW(handle,0,path,C.byref(size)):raise C.WinError(C.get_last_error())
        executable=Path(path.value).resolve()
        if not executable.is_relative_to(root/'local') or executable.name.lower()!='game.exe':raise ValueError('Expected an owned private game process.')
        if hashlib.sha256(executable.read_bytes()).hexdigest() not in SUPPORTED_RUNTIME_HASHES:raise ValueError('Unknown executable layout.')
        def read(address,length):
            data=C.create_string_buffer(length);copied=C.c_size_t()
            if not kernel.ReadProcessMemory(handle,address,data,length,C.byref(copied)) or copied.value!=length:raise C.WinError(C.get_last_error())
            return data.raw
        records=[];started=time.perf_counter()
        while time.perf_counter()-started<duration:
            pool=struct.unpack('<I',read(0x5b75d0,4))[0]
            count=struct.unpack('<I',read(0x5b75cc,4))[0]
            if count>512:raise ValueError('Unexpected animation-pool size; binary layout was not assumed.')
            actors=[]
            if pool and count:
                raw=read(pool,min(count,16)*0x368)
                for i in range(min(count,16)):
                    data=raw[i*0x368:(i+1)*0x368]
                    # The pool contains SA states themselves. Actor objects point to
                    # these states through actor+0x1e4; state+0x1e4 is not a pointer.
                    actor={'index':i,'position':struct.unpack_from('<3f',data,0x13c),
                           'time':struct.unpack_from('<f',data,0x1c)[0],
                           'step':struct.unpack_from('<f',data,0x20)[0],
                           'clip':struct.unpack_from('<I',data,0x2c)[0],
                           'render_position':struct.unpack_from('<3f',data,0x13c),
                           'render_rotation':struct.unpack_from('<4f',data,0x12c)}
                    bones=struct.unpack_from('<I',data,0x158)[0]
                    if bones:
                        try:actor['bone_rotation']=struct.unpack('<4f',read(bones,16))
                        except OSError:actor['unreadable_bones']=True
                    actors.append(actor)
            records.append({'seconds':time.perf_counter()-started,'animation_hz':struct.unpack('<f',read(0x68a6a8,4))[0],
                            'logic_hz':struct.unpack('<f',read(0x5a36ec,4))[0],
                            'simulation_phase':struct.unpack('<f',read(0x5e8214,4))[0],
                            'alpha':struct.unpack('<f',read(0x5e8218,4))[0], 'count':count,'actors':actors})
            time.sleep(.002)
        result={'pid':pid,'executable':str(executable),'samples':records}
        output=Path(output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2))
        print(json.dumps({'samples':len(records),'actors':records[-1]['count'] if records else 0,'output':str(output)}))
    finally:kernel.CloseHandle(handle)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid',type=int,required=True)
    parser.add_argument('--duration',type=float,default=5)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();sample(args.pid,args.duration,args.output)
