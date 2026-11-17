"""SDL3 settings navigation; gameplay stays on each physical DirectInput pad."""
import ctypes as C
from ctypes import wintypes as W
import threading
import time
import mmap
import struct
from pathlib import Path

MENU={0:'Enter',1:'Escape',6:'Space'}
MATCH={0:'S',1:'D',2:'A',3:'W',9:'Shift',10:'Q',6:'Space'}
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


def overlay_keys(buttons,axes,panel_open,deadzone,pulses=()):
    # Never translate a physical pad into the game's keyboard player slot.
    keys={'F1'} if 'F1' in pulses else set()
    if panel_open:
        keys |= desired_keys(buttons,axes,'Menu',deadzone,{'Menu':{0:'Enter',1:'Escape',9:'PageUp',10:'PageDown'}})
    return keys


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
    """Defer Start and View until release; their chord suppresses both actions."""
    def __init__(self):self.pending=False;self.view_pending=False;self.latched=False
    def update(self,buttons):
        buttons=set(buttons);start=6 in buttons;view=4 in buttons;pulse=set()
        if self.latched:
            if not start and not view:self.latched=False
        elif start and view:
            self.latched=True;self.pending=self.view_pending=False;pulse.add('F1')
        else:
            if start:self.pending=True
            elif self.pending:self.pending=False;pulse.add('Space')
            if view:self.view_pending=True
            elif self.view_pending:self.view_pending=False;pulse.add('Escape')
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
        return {'panel_open':bool(flags&1),'gameplay_active':bool(flags&2),'movie_active':bool(flags&4),'revision':revision}

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

    def write(self,keys,enabled=True,background_test=False,controller_present=False,controller_active=False,activity_serial=0,movie_skip=False):
        data=bytearray(256)
        for key in keys:
            scan=self.user.MapVirtualKeyW(KEYS[key],0)
            if key in (*DIRECTIONS.values(),'PageUp','PageDown'):scan|=0x80
            if 0<=scan<256:data[scan]=0x80
        self.sequence=(self.sequence+2)&0xfffffffe
        self.page.seek(4);self.page.write(struct.pack('<I',(self.sequence-1)&0xffffffff))
        self.page.seek(8);self.page.write(struct.pack('<II',int(enabled)|(2 if background_test else 0)|(4 if controller_present else 0)|(8 if controller_active else 0)|(16 if movie_skip else 0),activity_serial&0xffffffff)+data)
        self.page.seek(0);self.page.write(struct.pack('<II',0x494d534f,self.sequence))

    def close(self):
        self.write(set(),False);self.page.close()


PAD_PAGE_BYTES=160


def pad_axes(buttons,axes,deadzone):
    values=[0 if abs(value)<=deadzone else max(-32767,min(32767,round(value*32767))) for value in axes]
    if 13 in buttons:values[0]=-32767
    elif 14 in buttons:values[0]=32767
    if 11 in buttons:values[1]=-32767
    elif 12 in buttons:values[1]=32767
    return tuple(values)


class PadSlots:
    """Stable native-player slots; never combine two physical controllers."""
    def __init__(self):self.identities=[None]*4
    def update(self,identities):
        identities=set(identities)
        self.identities=[identity if identity in identities else None for identity in self.identities]
        for identity in sorted(identities-set(self.identities)):
            if None not in self.identities:break
            self.identities[self.identities.index(None)]=identity
        return tuple(self.identities)


class PadMailbox:
    def __init__(self,pid):
        self.page=mmap.mmap(-1,PAD_PAGE_BYTES,tagname=f'Local\\OpenSoccerMania-Pads-{pid}')
        self.sequence=0
    def write(self,states,enabled=True,gameplay=False):
        slots=bytearray(128)
        for slot,state in enumerate(states[:4]):
            if state is None:continue
            identity,x,y,buttons=state
            struct.pack_into('<IIiiIIII',slots,slot*32,identity,1,x,y,buttons,0,0,0)
        self.sequence=(self.sequence+2)&0xfffffffe
        self.page[8:12]=struct.pack('<I',(self.sequence-1)&0xffffffff)
        self.page[12:]=struct.pack('<5I',int(enabled)|(2 if gameplay else 0),0,0,0,0)+slots
        self.page[:12]=struct.pack('<III',0x504d534f,1,self.sequence)
    def close(self):self.write([],False);self.page.close()


class ControllerBridge:
    def __init__(self,root,pid=None,log=None):
        self.root=Path(root);self.pid=pid;self.log=log
        self.mode='Menu';self.automatic_mode=True;self.deadzone=.2;self.enabled=True
        self.mapping={'Menu':dict(MENU),'Match':dict(MATCH)}
        self.selected_id=None;self.raw_mapping=dict(RAW_XBOX)
        self.status='Starting SDL controller discovery...';self.devices=[]
        self.stop_event=threading.Event();self.thread=None
    def start(self):
        if self.thread and self.thread.is_alive():return
        self.stop_event.clear()
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
    def stop(self):
        self.stop_event.set()
        if self.thread:self.thread.join(timeout=2)
    def run(self):
        handles={};sdl=None;initialized=False;mailbox=None;pad_mailbox=None;ui_mailbox=None;mailbox_pid=None
        slots=PadSlots();gestures={};pulses={};last_activity=-10.;overlay_identity=None;activity_serial=0;previous_samples={}
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
            initialized=True;foreground=Foreground();last_scan=0
            while not self.stop_event.is_set():
                sdl.SDL_PumpEvents();now=time.monotonic()
                if now-last_scan>.5:
                    count=C.c_int();array=sdl.SDL_GetJoysticks(C.byref(count));ids=set(array[i] for i in range(count.value)) if array else set()
                    if array:sdl.SDL_free(array)
                    for identity in list(handles):
                        if identity not in ids:
                            pointer,mapped,_=handles.pop(identity)
                            (sdl.SDL_CloseGamepad if mapped else sdl.SDL_CloseJoystick)(pointer)
                            gestures.pop(identity,None);pulses.pop(identity,None);previous_samples.pop(identity,None)
                    for identity in sorted(ids-handles.keys()):
                        mapped=sdl.SDL_IsGamepad(identity)
                        pointer=(sdl.SDL_OpenGamepad if mapped else sdl.SDL_OpenJoystick)(identity)
                        if pointer:
                            name=(sdl.SDL_GetGamepadName if mapped else sdl.SDL_GetJoystickName)(pointer)
                            handles[identity]=(pointer,mapped,(name or b'Controller').decode(errors='replace'))
                    slots.update(handles)
                    self.devices=[{'id':i,'name':v[2],'mapped':v[1],'slot':slots.identities.index(i)+1 if i in slots.identities else None} for i,v in handles.items()]
                    self.status=', '.join(f"{d['name']} (pad {d['slot']})" if d['slot'] else f"{d['name']} (four-pad limit)" for d in self.devices) or 'No controllers connected. Hot-plug discovery is active.'
                    last_scan=now
                if self.pid!=mailbox_pid:
                    if mailbox:mailbox.close()
                    if pad_mailbox:pad_mailbox.close()
                    if ui_mailbox:ui_mailbox.close()
                    mailbox=pad_mailbox=ui_mailbox=None;mailbox_pid=self.pid;gestures.clear();pulses.clear()
                    if self.pid:
                        mailbox=InputMailbox(self.pid);pad_mailbox=PadMailbox(self.pid);ui_mailbox=UIStateMailbox(self.pid)
                active=foreground.is_active(self.pid) and self.enabled
                if not active:gestures.clear();pulses.clear();previous_samples.clear()
                state=ui_mailbox.read() if ui_mailbox else None
                panel=bool(state and state['panel_open']);movie=bool(state and state.get('movie_active'));samples={};native_states=[];f1=False;skip=False
                deadzone=max(.05,min(.95,self.deadzone))
                for identity in slots.identities:
                    if identity is None:native_states.append(None);continue
                    pointer,mapped,_=handles[identity]
                    if mapped:
                        buttons={b for b in range(15) if sdl.SDL_GetGamepadButton(pointer,b)}
                        axes=tuple(sdl.SDL_GetGamepadAxis(pointer,i)/32767 for i in range(2))
                        buttons.update(15+i for i in range(2) if sdl.SDL_GetGamepadAxis(pointer,4+i)>16384)
                    else:
                        raw={b for b in range(min(128,sdl.SDL_GetNumJoystickButtons(pointer))) if sdl.SDL_GetJoystickButton(pointer,b)}
                        buttons={self.raw_mapping[b] for b in raw if b in self.raw_mapping}
                        axes=tuple(sdl.SDL_GetJoystickAxis(pointer,i)/32767 if i<sdl.SDL_GetNumJoystickAxes(pointer) else 0 for i in range(2))
                        if sdl.SDL_GetNumJoystickHats(pointer):
                            hat=sdl.SDL_GetJoystickHat(pointer,0);buttons.update(b for bit,b in [(1,11),(2,14),(4,12),(8,13)] if hat&bit)
                    nonneutral=buttons or any(abs(value)>deadzone for value in axes)
                    sample=(frozenset(buttons),tuple(round(value*32767) for value in axes))
                    previous=previous_samples.get(identity)
                    if active:
                        if nonneutral:last_activity=now;overlay_identity=identity
                        if (previous is None and nonneutral) or (previous is not None and previous[0]!=sample and (nonneutral or previous[1])):
                            activity_serial=(activity_serial+1)&0xffffffff;last_activity=now
                        previous_samples[identity]=(sample,bool(nonneutral))
                    clean,pulse=gestures.setdefault(identity,SettingsChord()).update(buttons) if active else (set(),set())
                    pending=pulses.setdefault(identity,{})
                    pending.update({key:now+.08 for key in pulse});pending={key:until for key,until in pending.items() if until>now};pulses[identity]=pending
                    f1 |= 'F1' in pending
                    skip |= movie and 'Escape' in pending
                    x,y=pad_axes(clean,axes,deadzone) if active else (0,0)
                    native_buttons=set(clean)-set(DIRECTIONS)
                    if 'Space' in pending:native_buttons.add(6)
                    mask=sum(1<<button for button in native_buttons if 0<=button<32)
                    native_states.append((identity,x,y,mask))
                    samples[identity]=(clean,axes)
                selected=self.selected_id if self.selected_id in samples else overlay_identity
                wanted={'F1'} if f1 and active else set()
                if skip and active and not panel:wanted.add('Escape')
                if active and panel and selected in samples:
                    buttons,axes=samples[selected];wanted|=overlay_keys(buttons,axes,True,deadzone)
                if mailbox:mailbox.write(wanted,active,controller_present=bool(handles),controller_active=active and now-last_activity<2.,activity_serial=activity_serial,movie_skip=skip and not panel)
                if pad_mailbox:pad_mailbox.write(native_states,active,effective_mode(self.mode,self.automatic_mode,state)=='Match')
                self.stop_event.wait(.008)
        except Exception as error:
            self.status=f'Controller bridge unavailable: {error}'
            if self.log:self.log.exception('Controller bridge failure')
        finally:
            if mailbox:mailbox.close()
            if pad_mailbox:pad_mailbox.close()
            if ui_mailbox:ui_mailbox.close()
            if sdl and initialized:
                for pointer,mapped,_ in handles.values():(sdl.SDL_CloseGamepad if mapped else sdl.SDL_CloseJoystick)(pointer)
                sdl.SDL_Quit()
