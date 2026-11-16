"""SDL3 controller-to-keyboard bridge, scoped to the focused game process."""
import ctypes as C
from ctypes import wintypes as W
import threading
import time
import mmap
import struct
from pathlib import Path

MENU={0:'Enter',1:'Escape',6:'Space'}
MATCH={0:'S',1:'D',2:'A',3:'W',9:'Q',10:'Shift',6:'Space'}
DIRECTIONS={11:'Up',12:'Down',13:'Left',14:'Right'}
KEYS={'Enter':13,'Escape':27,'Space':32,'Shift':16,'Up':38,'Down':40,'Left':37,'Right':39,**{c:ord(c) for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'}}


def desired_keys(buttons,axes,mode,deadzone,mapping=None):
    bindings=(mapping or {}).get(mode, MENU if mode=='Menu' else MATCH)
    result={key for button,key in {**bindings,**DIRECTIONS}.items() if button in buttons}
    x,y=axes
    if x>deadzone:result.add('Right')
    if x<-deadzone:result.add('Left')
    if y>deadzone:result.add('Down')
    if y<-deadzone:result.add('Up')
    return result


def transitions(held,wanted):
    return wanted-held,held-wanted


class Keyboard:
    def __init__(self):
        pointer=C.c_size_t
        class Mouse(C.Structure):_fields_=[('dx',W.LONG),('dy',W.LONG),('data',W.DWORD),('flags',W.DWORD),('time',W.DWORD),('extra',pointer)]
        class Key(C.Structure):_fields_=[('vk',W.WORD),('scan',W.WORD),('flags',W.DWORD),('time',W.DWORD),('extra',pointer)]
        class Union(C.Union):_fields_=[('mouse',Mouse),('key',Key)]
        class Input(C.Structure):_fields_=[('kind',W.DWORD),('value',Union)]
        self.Input,self.Key=Input,Key
        self.user=C.windll.user32
        self.user.SendInput.argtypes=[W.UINT,C.POINTER(Input),C.c_int]

    def send(self,key,down):
        virtual=KEYS[key];scan=self.user.MapVirtualKeyW(virtual,0)
        flags=8|(0 if down else 2)|(1 if key in DIRECTIONS.values() else 0)
        event=self.Input();event.kind=1;event.value.key=self.Key(0,scan,flags,0,0)
        if self.user.SendInput(1,C.byref(event),C.sizeof(event))!=1:raise OSError('Windows rejected controller keyboard input.')


class InputMailbox:
    """A private DirectInput state page; never synthesizes desktop keystrokes."""
    def __init__(self,pid):
        self.page=mmap.mmap(-1,272,tagname=f'Local\\OpenSoccerMania-Input-{pid}')
        self.sequence=0;self.user=C.windll.user32

    def write(self,keys,enabled=True,background_test=False):
        data=bytearray(256)
        for key in keys:
            scan=self.user.MapVirtualKeyW(KEYS[key],0)
            if key in DIRECTIONS.values():scan|=0x80
            if 0<=scan<256:data[scan]=0x80
        self.sequence+=2
        self.page.seek(4);self.page.write(struct.pack('<I',self.sequence-1))
        self.page.seek(8);self.page.write(struct.pack('<II',int(enabled)|(2 if background_test else 0),0)+data)
        self.page.seek(0);self.page.write(struct.pack('<II',0x494d534f,self.sequence))

    def close(self):
        self.write(set(),False);self.page.close()


class ControllerBridge:
    def __init__(self,root,pid=None,log=None):
        self.root=Path(root);self.pid=pid;self.log=log
        self.mode='Menu';self.deadzone=.2;self.enabled=True
        self.mapping={'Menu':dict(MENU),'Match':dict(MATCH)}
        self.selected_id=None
        self.status='Starting SDL controller discovery…';self.devices=[]
        self.stop_event=threading.Event();self.thread=None

    def start(self):
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()

    def stop(self):
        self.stop_event.set()
        if self.thread:self.thread.join(timeout=2)

    def run(self):
        held=set();handles={};sdl=None;keyboard=None;initialized=False;mailbox=None;mailbox_pid=None
        try:
            path=self.root/'runtime/SDL3.dll'
            if not path.exists():raise FileNotFoundError('SDL3.dll is missing from runtime/.')
            sdl=C.CDLL(str(path))
            def bind(name,result,args):
                fn=getattr(sdl,name);fn.restype=result;fn.argtypes=args;return fn
            bind('SDL_Init',C.c_bool,[C.c_uint32]);bind('SDL_Quit',None,[])
            bind('SDL_PumpEvents',None,[]);bind('SDL_GetError',C.c_char_p,[])
            bind('SDL_GetJoysticks',C.POINTER(C.c_uint32),[C.POINTER(C.c_int)])
            bind('SDL_free',None,[C.c_void_p]);bind('SDL_IsGamepad',C.c_bool,[C.c_uint32])
            for prefix in ['Gamepad','Joystick']:
                bind('SDL_Open'+prefix,C.c_void_p,[C.c_uint32]);bind('SDL_Close'+prefix,None,[C.c_void_p])
                bind('SDL_Get'+prefix+'Name',C.c_char_p,[C.c_void_p])
                bind('SDL_Get'+prefix+'Axis',C.c_int16,[C.c_void_p,C.c_int])
                bind('SDL_Get'+prefix+'Button',C.c_bool,[C.c_void_p,C.c_int])
            bind('SDL_GetNumJoystickButtons',C.c_int,[C.c_void_p]);bind('SDL_GetNumJoystickAxes',C.c_int,[C.c_void_p])
            bind('SDL_GetJoystickHat',C.c_uint8,[C.c_void_p,C.c_int]);bind('SDL_GetNumJoystickHats',C.c_int,[C.c_void_p])
            if not sdl.SDL_Init(0x2000):raise RuntimeError((sdl.SDL_GetError() or b'SDL initialization failed').decode())
            initialized=True;keyboard=Keyboard();last_scan=0;previous_view=False
            while not self.stop_event.is_set():
                sdl.SDL_PumpEvents()
                if time.monotonic()-last_scan>.5:
                    count=C.c_int();array=sdl.SDL_GetJoysticks(C.byref(count));ids=set(array[i] for i in range(count.value)) if array else set()
                    if array:sdl.SDL_free(array)
                    for identity in list(handles):
                        if identity not in ids:
                            pointer,is_gamepad,name=handles.pop(identity)
                            (sdl.SDL_CloseGamepad if is_gamepad else sdl.SDL_CloseJoystick)(pointer)
                    for identity in ids-handles.keys():
                        is_gamepad=sdl.SDL_IsGamepad(identity)
                        pointer=(sdl.SDL_OpenGamepad if is_gamepad else sdl.SDL_OpenJoystick)(identity)
                        if pointer:
                            name=(sdl.SDL_GetGamepadName if is_gamepad else sdl.SDL_GetJoystickName)(pointer)
                            handles[identity]=(pointer,is_gamepad,(name or b'Controller').decode(errors='replace'))
                    self.devices=[{'id':i,'name':v[2],'mapped':v[1]} for i,v in handles.items()]
                    self.status=', '.join(d['name'] for d in self.devices) or 'No controllers connected. Hot-plug discovery is active.'
                    last_scan=time.monotonic()
                active=False
                if self.pid:
                    target=W.DWORD();keyboard.user.GetWindowThreadProcessId(keyboard.user.GetForegroundWindow(),C.byref(target));active=target.value==self.pid
                wanted=set();view=False
                if handles and active and self.enabled:
                    pointer,is_gamepad,name=handles.get(self.selected_id,next(iter(handles.values())))
                    if is_gamepad:
                        buttons={b for b in range(15) if sdl.SDL_GetGamepadButton(pointer,b)}
                        axes=(sdl.SDL_GetGamepadAxis(pointer,0)/32767,sdl.SDL_GetGamepadAxis(pointer,1)/32767)
                    else:
                        buttons={b for b in range(min(15,sdl.SDL_GetNumJoystickButtons(pointer))) if sdl.SDL_GetJoystickButton(pointer,b)}
                        axes=tuple(sdl.SDL_GetJoystickAxis(pointer,i)/32767 if i<sdl.SDL_GetNumJoystickAxes(pointer) else 0 for i in range(2))
                        if sdl.SDL_GetNumJoystickHats(pointer):
                            hat=sdl.SDL_GetJoystickHat(pointer,0)
                            buttons.update(b for bit,b in [(1,11),(2,14),(4,12),(8,13)] if hat&bit)
                    view=4 in buttons
                    if view and not previous_view:self.mode='Match' if self.mode=='Menu' else 'Menu'
                    wanted=desired_keys(buttons,axes,self.mode,self.deadzone,self.mapping)
                if self.pid!=mailbox_pid:
                    if mailbox:mailbox.close();mailbox=None
                    mailbox_pid=self.pid
                    if self.pid:mailbox=InputMailbox(self.pid)
                if mailbox:mailbox.write(wanted,active and self.enabled)
                held=wanted;previous_view=view
                self.stop_event.wait(.008)
        except Exception as error:
            self.status=f'Controller bridge unavailable: {error}'
            if self.log:self.log.exception('Controller bridge failure')
        finally:
            if mailbox:mailbox.close()
            if sdl and initialized:
                for pointer,is_gamepad,name in handles.values():(sdl.SDL_CloseGamepad if is_gamepad else sdl.SDL_CloseJoystick)(pointer)
                sdl.SDL_Quit()
