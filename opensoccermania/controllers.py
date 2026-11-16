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
RAW_XBOX={0:0,1:1,2:2,3:3,4:9,5:10,6:4,7:6,8:7,9:8}
DIRECTIONS={11:'Up',12:'Down',13:'Left',14:'Right'}
KEYS={'Enter':13,'Escape':27,'Space':32,'Shift':16,'F1':112,'PageUp':33,'PageDown':34,'Up':38,'Down':40,'Left':37,'Right':39,**{c:ord(c) for c in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'}}


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


class Foreground:
    def __init__(self):
        self.user=C.windll.user32
        self.user.GetForegroundWindow.restype=W.HWND
        self.user.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]

    def is_active(self,pid):
        target=W.DWORD()
        self.user.GetWindowThreadProcessId(self.user.GetForegroundWindow(),C.byref(target))
        return bool(pid and target.value==pid)


class SettingsChord:
    """Reserve Start until release; a View+Start gesture never leaks pause.

    View has no standalone action. After a chord both buttons must be released
    before another gesture can begin. Focus/device changes reset the gesture.
    """
    def __init__(self):
        self.pending=False
        self.latched=False

    def update(self,buttons):
        buttons=set(buttons)
        start=6 in buttons
        view=4 in buttons
        pulse=set()
        if self.latched:
            if not start and not view:self.latched=False
        elif start and view:
            self.latched=True
            self.pending=False
            pulse.add('F1')
        elif start:
            self.pending=True
        elif self.pending:
            self.pending=False
            pulse.add('Space')
        return buttons-{4,6},pulse


class UIStateMailbox:
    """Read the optional native UI protocol; absent/invalid pages return None."""
    def __init__(self,pid):
        self.name=f'Local\\OpenSoccerMania-UI-{pid}'
        self.page=None

    def read(self):
        if self.page is None:
            kernel=C.windll.kernel32
            kernel.OpenFileMappingW.argtypes=[W.DWORD,W.BOOL,W.LPCWSTR]
            kernel.OpenFileMappingW.restype=W.HANDLE
            kernel.CloseHandle.argtypes=[W.HANDLE]
            handle=kernel.OpenFileMappingW(4,False,self.name)
            if not handle:return None
            try:self.page=mmap.mmap(-1,32,tagname=self.name,access=mmap.ACCESS_READ)
            finally:kernel.CloseHandle(handle)
        first=self.page[:32]
        second=self.page[:32]
        if first!=second:return None
        magic,version,flags,revision,*_=struct.unpack('<8I',first)
        if magic!=0x554d534f or version!=1:return None
        return {'panel_open':bool(flags&1),'gameplay_active':bool(flags&2),'revision':revision}

    def close(self):
        if self.page:self.page.close();self.page=None


def effective_mode(manual,automatic,state):
    if state and state['panel_open']:return 'Menu'
    if not automatic or state is None:return manual
    return 'Match' if state['gameplay_active'] and not state['panel_open'] else 'Menu'


class InputMailbox:
    """A private DirectInput state page; never synthesizes desktop keystrokes."""
    def __init__(self,pid):
        self.page=mmap.mmap(-1,272,tagname=f'Local\\OpenSoccerMania-Input-{pid}')
        self.sequence=0;self.user=C.windll.user32

    def write(self,keys,enabled=True,background_test=False,controller_present=False):
        data=bytearray(256)
        for key in keys:
            scan=self.user.MapVirtualKeyW(KEYS[key],0)
            if key in (*DIRECTIONS.values(),'PageUp','PageDown'):scan|=0x80
            if 0<=scan<256:data[scan]=0x80
        self.sequence+=2
        self.page.seek(4);self.page.write(struct.pack('<I',self.sequence-1))
        self.page.seek(8);self.page.write(struct.pack('<II',int(enabled)|(2 if background_test else 0)|(4 if controller_present else 0),0)+data)
        self.page.seek(0);self.page.write(struct.pack('<II',0x494d534f,self.sequence))

    def close(self):
        self.write(set(),False);self.page.close()


class ControllerBridge:
    def __init__(self,root,pid=None,log=None):
        self.root=Path(root);self.pid=pid;self.log=log
        self.mode='Menu';self.automatic_mode=True;self.deadzone=.2;self.enabled=True
        self.mapping={'Menu':dict(MENU),'Match':dict(MATCH)}
        self.selected_id=None
        self.raw_mapping=dict(RAW_XBOX)
        self.status='Starting SDL controller discovery…';self.devices=[]
        self.stop_event=threading.Event();self.thread=None

    def start(self):
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()

    def stop(self):
        self.stop_event.set()
        if self.thread:self.thread.join(timeout=2)

    def run(self):
        held=set();handles={};sdl=None;keyboard=None;initialized=False;mailbox=None;mailbox_pid=None;ui_mailbox=None;chord=SettingsChord();last_device=None;pulses={}
        try:
            path=self.root/'runtime/SDL3.dll'
            if not path.exists():raise FileNotFoundError('SDL3.dll is missing from runtime/.')
            sdl=C.CDLL(str(path))
            def bind(name,result,args):
                fn=getattr(sdl,name);fn.restype=result;fn.argtypes=args;return fn
            bind('SDL_SetHint',C.c_bool,[C.c_char_p,C.c_char_p])
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
            sdl.SDL_SetHint(b'SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS',b'1')
            sdl.SDL_SetHint(b'SDL_JOYSTICK_HIDAPI',b'1')
            if not sdl.SDL_Init(0x2200):raise RuntimeError((sdl.SDL_GetError() or b'SDL initialization failed').decode())
            initialized=True;keyboard=Foreground();last_scan=0
            while not self.stop_event.is_set():
                sdl.SDL_PumpEvents()
                if time.monotonic()-last_scan>.5:
                    count=C.c_int();array=sdl.SDL_GetJoysticks(C.byref(count));ids=set(array[i] for i in range(count.value)) if array else set()
                    if array:sdl.SDL_free(array)
                    for identity in list(handles):
                        if identity not in ids:
                            pointer,is_gamepad,name=handles.pop(identity)
                            (sdl.SDL_CloseGamepad if is_gamepad else sdl.SDL_CloseJoystick)(pointer)
                    for identity in sorted(ids-handles.keys()):
                        is_gamepad=sdl.SDL_IsGamepad(identity)
                        pointer=(sdl.SDL_OpenGamepad if is_gamepad else sdl.SDL_OpenJoystick)(identity)
                        if pointer:
                            name=(sdl.SDL_GetGamepadName if is_gamepad else sdl.SDL_GetJoystickName)(pointer)
                            handles[identity]=(pointer,is_gamepad,(name or b'Controller').decode(errors='replace'))
                    self.devices=[{'id':i,'name':v[2],'mapped':v[1]} for i,v in handles.items()]
                    self.status=', '.join(d['name'] for d in self.devices) or 'No controllers connected. Hot-plug discovery is active.'
                    last_scan=time.monotonic()
                active=keyboard.is_active(self.pid)
                wanted=set()
                identity=self.selected_id if self.selected_id in handles else next(iter(handles),None)
                if (identity,self.pid)!=last_device or not active or not self.enabled:
                    chord=SettingsChord();pulses.clear()
                last_device=(identity,self.pid)
                if identity is not None and active and self.enabled:
                    pointer,is_gamepad,name=handles[identity]
                    if is_gamepad:
                        buttons={b for b in range(15) if sdl.SDL_GetGamepadButton(pointer,b)}
                        axes=(sdl.SDL_GetGamepadAxis(pointer,0)/32767,sdl.SDL_GetGamepadAxis(pointer,1)/32767)
                    else:
                        buttons={b for b in range(min(128,sdl.SDL_GetNumJoystickButtons(pointer))) if sdl.SDL_GetJoystickButton(pointer,b)}
                        buttons={self.raw_mapping[b] for b in buttons if b in self.raw_mapping}
                        axes=tuple(sdl.SDL_GetJoystickAxis(pointer,i)/32767 if i<sdl.SDL_GetNumJoystickAxes(pointer) else 0 for i in range(2))
                        if sdl.SDL_GetNumJoystickHats(pointer):
                            hat=sdl.SDL_GetJoystickHat(pointer,0)
                            buttons.update(b for bit,b in [(1,11),(2,14),(4,12),(8,13)] if hat&bit)
                    buttons,pulse=chord.update(buttons)
                    state=ui_mailbox.read() if ui_mailbox else None
                    mode=effective_mode(self.mode,self.automatic_mode,state)
                    now=time.monotonic()
                    pulses.update({key:now+.08 for key in pulse})
                    pulses={key:until for key,until in pulses.items() if until>now}
                    bindings=self.mapping
                    if state and state['panel_open']:
                        pulses.pop('Space',None)
                        bindings={**self.mapping,'Menu':{0:'Enter',1:'Escape',9:'PageUp',10:'PageDown'}}
                    wanted=desired_keys(buttons,axes,mode,max(.05,min(.95,self.deadzone)),bindings)|pulses.keys()
                if self.pid!=mailbox_pid:
                    if mailbox:mailbox.close();mailbox=None
                    if ui_mailbox:ui_mailbox.close();ui_mailbox=None
                    mailbox_pid=self.pid
                    if self.pid:
                        mailbox=InputMailbox(self.pid)
                        ui_mailbox=UIStateMailbox(self.pid)
                if mailbox:mailbox.write(wanted,active and self.enabled,controller_present=identity is not None)
                held=wanted
                self.stop_event.wait(.008)
        except Exception as error:
            self.status=f'Controller bridge unavailable: {error}'
            if self.log:self.log.exception('Controller bridge failure')
        finally:
            if mailbox:mailbox.close()
            if ui_mailbox:ui_mailbox.close()
            if sdl and initialized:
                for pointer,is_gamepad,name in handles.values():(sdl.SDL_CloseGamepad if is_gamepad else sdl.SDL_CloseJoystick)(pointer)
                sdl.SDL_Quit()
