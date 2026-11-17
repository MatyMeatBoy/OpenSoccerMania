"""English desktop launcher with local-only game imports."""
from dataclasses import asdict
from pathlib import Path
import json
import os
import queue
import subprocess
import sys
import threading
import hashlib
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageDraw, ImageTk, ImageFilter
from . import __version__, fonts, artwork
from .theme import Bubble, CREAM, GREEN, AMBER, MUTED
from .sources import discover, discover_assets, prepare, prepare_with_executable, mounted_iso, open_disc_image, probe_disc_image, child
from .video import VideoSettings, RESOLUTIONS, REFRESH_RATES, AA_MODES, FILTER_MODES, DISPLAY_MODES, RENDER_LIMITS, CAMERA_ZOOMS, EDITIONS, LANGUAGES, configure_wrapper
from .runtime import setup_portable, install_bridge, install_input_bridge, configure_bridge, read_live_settings
from .backends import ensure_backend, install_backend
from .title_assets import load_title_image,TitleAssetError
from .diagnostics import logger, Session
from .controllers import ControllerBridge, MATCH, KEYS, RAW_XBOX

ROOT = Path(sys.executable).parent if getattr(sys, 'frozen', False) else Path(__file__).resolve().parent.parent
HOME = ROOT / 'local'

class ExecutableSourceRequired(ValueError):
    def __init__(self,path,is_image):self.path,self.is_image=path,is_image


def stadium(size):
    """Original miniature pitch, softly rounded plastic and printed paper grain."""
    w, h = size
    image = artwork.vertical_gradient(size, '#235b46', '#102f2b')
    image = artwork.add_grain(image, 4)
    draw = ImageDraw.Draw(image)
    for x in range(-h, w, 58):
        draw.polygon([(x, h), (x+90, 0), (x+118, 0), (x+28, h)], fill='#285a45')
    # Raised pitch tile with softened corners and miniature studs.
    x0, y0, x1, y1 = int(w*.51), 27, w-28, h-26
    draw.rounded_rectangle((x0+4,y0+8,x1+4,y1+8), 27, fill='#0b2724')
    draw.rounded_rectangle((x0,y0,x1,y1), 27, fill='#6f9d5c', outline='#accd8a', width=2)
    for x in range(x0+19,x1-8,24):
        for y in range(y0+19,y1-8,24):
            draw.ellipse((x-5,y-4,x+5,y+6), fill='#5b874a')
            draw.ellipse((x-5,y-6,x+5,y+4), fill='#85af6c', outline='#92ba79')
    inset=20
    draw.rounded_rectangle((x0+inset,y0+inset,x1-inset,y1-inset), 9, outline='#e6edcb',width=2)
    cx,cy=(x0+x1)//2,(y0+y1)//2
    draw.line((cx,y0+inset,cx,y1-inset),fill='#e6edcb',width=2)
    draw.ellipse((cx-30,cy-30,cx+30,cy+30),outline='#e6edcb',width=2)
    draw.rectangle((x0+inset,cy-39,x0+55,cy+39),outline='#e6edcb',width=2)
    draw.rectangle((x1-55,cy-39,x1-inset,cy+39),outline='#e6edcb',width=2)
    for px,py,c in [(cx-44,cy-36,'#efba4d'),(cx+55,cy+33,'#d9614f'),(cx-64,cy+38,'#efba4d'),(cx+40,cy-40,'#d9614f')]:
        draw.rounded_rectangle((px-8,py-4,px+9,py+18),5,fill='#1b3a32')
        draw.rounded_rectangle((px-9,py-7,px+8,py+14),5,fill=c)
        draw.ellipse((px-6,py-15,px+6,py-3),fill='#f9dd90',outline='#ddad56')
    draw.ellipse((cx-8,cy-8,cx+8,cy+8),fill='#faf6df',outline='#244537',width=2)
    draw.regular_polygon((cx,cy,4),5,fill='#244537')
    return image


class Launcher:
    def __init__(self, root):
        self.root = root
        self.queue = queue.Queue()
        self.busy = False
        self.process = None
        self.session = None
        self.log=logger(HOME)
        def callback_error(kind,value,traceback):
            self.log.error('Unhandled launcher callback',exc_info=(kind,value,traceback))
            messagebox.showerror('OpenSoccerMania error',f'{value}\n\nDetails were saved to local/logs/launcher.log.')
        root.report_callback_exception=callback_error
        self.entry = None
        self.images=[]
        self.prefs_path=HOME/'launcher.json'
        try:
            self.prefs=json.loads(self.prefs_path.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            self.prefs={}
        root.title('OpenSoccerMania')
        root.geometry('1180x820')
        root.minsize(1020,760)
        root.configure(bg=CREAM)
        fonts.load(ROOT)
        import tkinter.font as tkfont
        fonts.confirm(tkfont.families(root))
        style=ttk.Style(root);style.theme_use('clam')
        self.theme=Bubble(root);self.theme.apply(style)
        self.status=tk.StringVar(value='Choose your game files to get started.')
        self._shell()
        self._home()
        self._video()
        self._compatibility()
        self._controls()
        self.controller=ControllerBridge(ROOT,log=self.log)
        controls=self.prefs.get('controls',{})
        self.controller.enabled=controls.get('enabled',True)
        self.controller.deadzone=controls.get('deadzone',.2)
        self.controller.automatic_mode=controls.get('automatic_mode',True)
        self.controller.mapping['Match']={int(k):v for k,v in controls.get('mapping',MATCH).items()}
        self.controller.raw_mapping={int(k):int(v) for k,v in controls.get('raw_mapping',RAW_XBOX).items()}
        self.controller.start()
        self.show('Home')
        saved=self.prefs.get('entry')
        if saved and (Path(saved)/'opensoccermania-source.json').is_file():
            self.set_entry(Path(saved))
        root.after(100,self.poll)
        root.protocol('WM_DELETE_WINDOW',self.close)

    def _shell(self):
        side=tk.Frame(self.root,bg=GREEN,width=210)
        side.pack(side='left',fill='y');side.pack_propagate(False)
        tk.Label(side,text='OPEN\nSOCCER\nMANIA',bg=GREEN,fg=CREAM,font=fonts.display(23),justify='left').pack(anchor='w',padx=25,pady=(32,6))
        tk.Label(side,text='BUILD YOUR NEXT MATCH',bg=GREEN,fg='#b4c9a4',font=fonts.ui(8,'bold')).pack(anchor='w',padx=25,pady=(0,40))
        self.nav={}
        for name in ['Home','Graphics','Controls','Compatibility']:
            button=tk.Button(side,text=name,command=lambda n=name:self.show(n),bg=GREEN,fg=CREAM,font=fonts.ui(12,'bold'),relief='flat',borderwidth=0,anchor='w',padx=20,pady=13,activebackground='#2b5945',activeforeground=CREAM,cursor='hand2')
            button.pack(fill='x',padx=12,pady=5);self.nav[name]=button
        tk.Label(side,text='Your game.\nA fresh home pitch.',bg=GREEN,fg='#bdd2b2',font=fonts.ui(12),justify='left').pack(side='bottom',anchor='w',padx=25,pady=30)
        tk.Label(side,text=f'OpenSoccerMania {__version__}',bg=GREEN,fg='#9cb9a2',font=fonts.ui(9)).pack(side='bottom',anchor='w',padx=25)
        right=tk.Frame(self.root,bg='#e6deca');right.pack(side='left',fill='both',expand=True)
        self.statusbar=tk.Label(right,textvariable=self.status,bg='#e6deca',fg=GREEN,font=fonts.ui(10),anchor='w',wraplength=850,padx=22,pady=12)
        self.statusbar.pack(side='bottom',fill='x')
        self.host=tk.Frame(right,bg=CREAM);self.host.pack(fill='both',expand=True,padx=14,pady=(14,0))
        self.pages={n:ttk.Frame(self.host,padding=24) for n in self.nav}

    def heading(self,frame,kicker,title,body):
        ttk.Label(frame,text=kicker,style='Caption.TLabel').pack(anchor='w',pady=(0,7))
        ttk.Label(frame,text=title,style='Title.TLabel').pack(anchor='w')
        ttk.Label(frame,text=body,style='Muted.TLabel',wraplength=790).pack(anchor='w',pady=(9,20))

    def _home(self):
        frame=self.pages['Home']
        self.heading(frame,'WELCOME TO YOUR HOME PITCH','Small bricks. Big match energy.','Bring your own LEGO Soccer Mania files. We will prepare a private copy for OpenSoccerMania.')
        hero=tk.Canvas(frame,height=225,bg=GREEN,highlightthickness=0)
        hero.pack(fill='x',pady=(0,22))
        def paint(event):
            picture=ImageTk.PhotoImage(stadium((max(650,event.width),225)),master=self.root)
            self.hero_image=picture;hero.delete('all');hero.create_image(0,0,image=picture,anchor='nw')
            hero.create_text(24,40,anchor='nw',text='THE BEAUTIFUL\nBRICK GAME.',fill=CREAM,font=fonts.display(24))
            hero.create_text(26,135,anchor='nw',text='Original charm.\nRoom for a better view.',fill='#dae4c9',font=fonts.ui(12))
            if getattr(self,'title_preview_image',None):hero.create_image(max(650,event.width)-28,20,image=self.title_preview_image,anchor='ne')
        hero.bind('<Configure>',paint)
        self.hero=hero;self.paint_hero=paint
        card=ttk.LabelFrame(frame,text=' YOUR GAME ',padding=18);card.pack(fill='x')
        self.game_title=tk.StringVar(value='Let’s get the ball rolling.')
        self.game_detail=tk.StringVar(value='Choose an ISO, an extracted disc, or an installed game folder.')
        ttk.Label(card,textvariable=self.game_title,style='Heading.TLabel').pack(anchor='w')
        ttk.Label(card,textvariable=self.game_detail,style='Muted.TLabel',wraplength=740).pack(anchor='w',pady=(7,15))
        self.library_choice=tk.StringVar();self.library_entries={}
        self.library_selector=ttk.Combobox(card,textvariable=self.library_choice,state='readonly')
        self.library_selector.pack(fill='x',pady=(0,10));self.library_selector.bind('<<ComboboxSelected>>',lambda event:self.choose_library())
        actions=ttk.Frame(card);actions.pack(fill='x')
        self.play_button=ttk.Button(actions,text='Choose game files  →',style='Accent.TButton',command=self.play)
        self.play_button.pack(side='left',padx=(0,12))
        self.folder_button=ttk.Button(actions,text='Choose folder…',command=self.import_folder);self.folder_button.pack(side='left',padx=(0,8))
        self.iso_button=ttk.Button(actions,text='Choose disc image…',command=self.import_iso);self.iso_button.pack(side='left')
        bottom=ttk.Frame(frame);bottom.pack(fill='x',pady=(22,0))
        saved=self.prefs.get('video',{})
        self.edition=tk.StringVar(value=saved.get('edition','Football Mania'));self.language=tk.StringVar(value=saved.get('language','English'))
        for title,var,values in [('REGIONAL EDITION',self.edition,EDITIONS),('TEXT & VOICES',self.language,LANGUAGES)]:
            cell=ttk.Frame(bottom);cell.pack(side='left',fill='both',expand=True,padx=(0,15));ttk.Label(cell,text=title,style='Caption.TLabel').pack(anchor='w')
            combo=ttk.Combobox(cell,textvariable=var,values=values,state='readonly');combo.pack(fill='x',pady=(7,0));combo.bind('<<ComboboxSelected>>',lambda event,v=var:self.edition_changed(v is self.edition))
        self.refresh_library()

    def _video(self):
        outer=self.pages['Graphics']
        canvas=tk.Canvas(outer,bg=CREAM,highlightthickness=0)
        bar=ttk.Scrollbar(outer,orient='vertical',command=canvas.yview)
        canvas.configure(yscrollcommand=bar.set);bar.pack(side='right',fill='y');canvas.pack(side='left',fill='both',expand=True)
        frame=ttk.Frame(canvas);content=canvas.create_window(0,0,window=frame,anchor='nw')
        frame.bind('<Configure>',lambda event:canvas.configure(scrollregion=canvas.bbox('all')))
        canvas.bind('<Configure>',lambda event:canvas.itemconfigure(content,width=event.width))
        self.graphics_canvas=canvas
        self.root.bind('<MouseWheel>',lambda event:canvas.yview_scroll(-int(event.delta/120),'units') if self.pages['Graphics'].winfo_ismapped() else None,add='+')
        self.heading(frame,'MAKE IT YOURS','A little more stadium.','Direct3D 11 presentation through dgVoodoo. Display Hz and game FPS are different.')
        saved=self.prefs.get('video',{})
        self.res=tk.StringVar(value=f"{saved.get('width',1920)} × {saved.get('height',1080)}")
        self.hz=tk.StringVar(value=str(saved.get('refresh',60)))
        self.aa=tk.StringVar(value=saved.get('aa','4× MSAA'))
        self.filter=tk.StringVar(value=saved.get('filtering','16× anisotropic'))
        self.vsync=tk.BooleanVar(value=saved.get('vsync',True))
        self.display_mode=tk.StringVar(value=saved.get('display_mode') or ('Fullscreen' if saved.get('fullscreen',False) else 'Windowed'))
        self.horplus=tk.BooleanVar(value=saved.get('horplus',True))
        self.performance_hud=tk.BooleanVar(value=saved.get('performance_hud',True))
        self.overlay_scale=tk.StringVar(value=f"{saved.get('overlay_scale',125)}%")
        self.run_background=tk.BooleanVar(value=saved.get('run_background',True))
        self.integer_movies=tk.BooleanVar(value=saved.get('integer_movies',True))
        self.render_fps=tk.StringVar(value=next((label for label,value in RENDER_LIMITS.items() if value==saved.get('render_fps',240)), '240 FPS'))
        self.auto_aspect=tk.BooleanVar(value=saved.get('auto_aspect',True))
        self.sprite_seam_fix=tk.BooleanVar(value=saved.get('sprite_seam_fix',True))
        self.modern_presentation=tk.BooleanVar(value=saved.get('modern_presentation',True))
        self.camera_zoom=tk.StringVar(value=saved.get('camera_zoom','Original zoom'))
        self.xbox_prompts=tk.BooleanVar(value=saved.get('xbox_prompts',True))
        self.title_extension=tk.BooleanVar(value=saved.get('title_extension',True))
        card=ttk.LabelFrame(frame,text=' DISPLAY & IMAGE QUALITY ',padding=18);card.pack(fill='x')
        for row,(label,var,values) in enumerate([('Internal render resolution',self.res,RESOLUTIONS),('Fullscreen refresh rate (Hz)',self.hz,list(map(str,REFRESH_RATES))),('Antialiasing',self.aa,list(AA_MODES)),('Texture filtering',self.filter,list(FILTER_MODES))]):
            ttk.Label(card,text=label).grid(row=row,column=0,sticky='w',padx=(0,30),pady=8)
            ttk.Combobox(card,textvariable=var,values=values,state='readonly',width=27).grid(row=row,column=1,sticky='ew',pady=8)
        card.columnconfigure(1,weight=1)
        ttk.Label(card,text='Display mode').grid(row=4,column=0,sticky='w',pady=(12,5))
        ttk.Combobox(card,textvariable=self.display_mode,values=DISPLAY_MODES,state='readonly',width=27).grid(row=4,column=1,sticky='ew',pady=(12,5))
        ttk.Checkbutton(card,text='Vertical sync',variable=self.vsync).grid(row=5,columnspan=2,sticky='w',pady=5)
        ttk.Checkbutton(card,text='Experimental Hor+ widescreen / ultrawide bridge',variable=self.horplus).grid(row=6,columnspan=2,sticky='w',pady=5)
        ttk.Label(card,text='Default: preserve the original 4:3 view with side bars. Hor+ corrects projection and immediate-mode HUD drawing; some scenes may still need fixes.',style='Muted.TLabel',wraplength=730).grid(row=7,columnspan=2,sticky='w',pady=(10,0))
        ttk.Checkbutton(card,text='Run in the background (windowed / borderless)',variable=self.run_background).grid(row=8,columnspan=2,sticky='w',pady=6)
        ttk.Checkbutton(card,text='Integer-scale cinematics when possible; preserve aspect ratio',variable=self.integer_movies).grid(row=9,columnspan=2,sticky='w',pady=6)
        ttk.Checkbutton(card,text='Detailed performance HUD (F10 to toggle)',variable=self.performance_hud).grid(row=10,columnspan=2,sticky='w',pady=6)
        ttk.Label(card,text='Performance HUD size').grid(row=11,column=0,sticky='w',pady=7)
        ttk.Combobox(card,textvariable=self.overlay_scale,values=['75%','100%','125%','150%','175%','200%'],state='readonly').grid(row=11,column=1,sticky='ew')
        ttk.Label(card,text='Render FPS limit').grid(row=12,column=0,sticky='w',pady=8)
        ttk.Combobox(card,textvariable=self.render_fps,values=list(RENDER_LIMITS),state='readonly').grid(row=12,column=1,sticky='ew')
        ttk.Checkbutton(card,text='Adapt Hor+ camera to the window shape',variable=self.auto_aspect).grid(row=13,columnspan=2,sticky='w',pady=6)
        ttk.Checkbutton(card,text='Fix seams in menus and tiled images',variable=self.sprite_seam_fix).grid(row=15,columnspan=2,sticky='w',pady=6)
        ttk.Checkbutton(card,text='Low-latency presentation',variable=self.modern_presentation).grid(row=16,columnspan=2,sticky='w',pady=6)
        ttk.Label(card,text='Camera framing').grid(row=18,column=0,sticky='w',pady=8)
        ttk.Combobox(card,textvariable=self.camera_zoom,values=CAMERA_ZOOMS,state='readonly').grid(row=18,column=1,sticky='ew')
        ttk.Checkbutton(card,text='Xbox button hints for connected controllers',variable=self.xbox_prompts).grid(row=19,columnspan=2,sticky='w',pady=6)
        ttk.Checkbutton(card,text='Generated title background in widescreen sidebars',variable=self.title_extension).grid(row=20,columnspan=2,sticky='w',pady=6)
        ttk.Label(card,text='F1 or View / Select + Start opens in-game settings. Original zoom keeps the original camera distance and vertical framing; distant lenses are optional.',style='Muted.TLabel',wraplength=730).grid(row=21,columnspan=2,sticky='w',pady=(8,0))
        ttk.Label(card,text='F11 / Alt+Enter: toggle Windowed and Borderless. The render limit preserves the original simulation rate on verified executable versions. VSync can limit FPS to your display refresh. Windowed and borderless modes use the desktop refresh rate.',style='Muted.TLabel',wraplength=730).grid(row=14,columnspan=2,sticky='w',pady=(8,0))
        ttk.Button(frame,text='Save graphics profile',style='Accent.TButton',command=self.save_video).pack(anchor='w',pady=18)
        ttk.Label(frame,text='VRR · FreeSync · G-SYNC',style='Heading.TLabel').pack(anchor='w',pady=(5,5))
        ttk.Label(frame,text='Enable adaptive sync in your monitor and GPU control panel. Select a supported display mode here. VRR activation and range are not yet detected; changing this profile does not enable the driver setting.',style='Muted.TLabel',wraplength=780).pack(anchor='w')
        ttk.Label(frame,text='IN DEVELOPMENT',style='Caption.TLabel').pack(anchor='w',pady=(18,6))
        ttk.Label(frame,text='High-FPS rendering and camera/frustum hooks are experimental and version-checked. VRS and temporal AA are not implemented. 16× MSAA requires GPU support.',style='Muted.TLabel',wraplength=780).pack(anchor='w')

    def _compatibility(self):
        frame=self.pages['Compatibility']
        self.heading(frame,'PRESERVE & UNDERSTAND','A good match starts with good files.','Inspect your imported game, its languages, executable and available graphics wrapper.')
        self.report=tk.Text(frame,wrap='word',font=fonts.ui(11),bg='#fffdf6',fg=GREEN,relief='flat',padx=18,pady=18,height=19)
        self.report.pack(fill='both',expand=True)
        self.report.insert('1.0','Import a game to see its compatibility report.\n\nSupported executable versions receive reversible startup patches in the private imported copy. Original sources remain unchanged. Unknown versions are excluded from engine hooks.\n\nExperimental rendering and animation timing still require acceptance testing; this is not a complete engine reimplementation.')
        self.report.configure(state='disabled')
        ttk.Button(frame,text='Open local library',command=self.open_library).pack(anchor='w',pady=(16,0))
        ttk.Button(frame,text='Open diagnostic logs',command=self.open_logs).pack(anchor='w',pady=(8,0))

    def _controls(self):
        outer=self.pages['Controls'];canvas=tk.Canvas(outer,bg=CREAM,highlightthickness=0)
        bar=ttk.Scrollbar(outer,orient='vertical',command=canvas.yview);canvas.configure(yscrollcommand=bar.set)
        bar.pack(side='right',fill='y');canvas.pack(side='left',fill='both',expand=True)
        frame=ttk.Frame(canvas);content=canvas.create_window(0,0,window=frame,anchor='nw')
        frame.bind('<Configure>',lambda event:canvas.configure(scrollregion=canvas.bbox('all')))
        canvas.bind('<Configure>',lambda event:canvas.itemconfigure(content,width=event.width))
        self.root.bind('<MouseWheel>',lambda event:canvas.yview_scroll(-int(event.delta/120),'units') if outer.winfo_ismapped() else None,add='+')
        self.heading(frame,'YOUR TEAM. YOUR CONTROLLER.','Pick up and play.','SDL3 handles XInput, mapped gamepads and raw DirectInput/HID devices. The bridge controls the game only while its window has focus.')
        self.controller_status=tk.StringVar(value='Starting controller discovery…')
        ttk.Label(frame,textvariable=self.controller_status,style='Heading.TLabel',wraplength=760).pack(anchor='w',pady=(0,14))
        settings=self.prefs.get('controls',{})
        self.controller_enabled=tk.BooleanVar(value=settings.get('enabled',True))
        self.controller_auto=tk.BooleanVar(value=settings.get('automatic_mode',True))
        self.controller_mode=tk.StringVar(value='Menu')
        self.controller_deadzone=tk.StringVar(value=f"{round(settings.get('deadzone',.2)*100)}%")
        ttk.Checkbutton(frame,text='Enable SDL controllers (up to four separate gamepads)',variable=self.controller_enabled).pack(anchor='w',pady=8)
        ttk.Checkbutton(frame,text='Automatically switch between game and menu controls',variable=self.controller_auto).pack(anchor='w',pady=4)
        self.device_choice=tk.StringVar(value='Automatic');self.device_options={'Automatic':None};self.device_signature=None
        ttk.Label(frame,text='Controller for internal settings navigation').pack(anchor='w',pady=(8,4))
        self.device_selector=ttk.Combobox(frame,textvariable=self.device_choice,values=['Automatic'],state='readonly');self.device_selector.pack(fill='x')
        self.device_selector.bind('<<ComboboxSelected>>',lambda event:setattr(self.controller,'selected_id',self.device_options.get(self.device_choice.get())))
        row=ttk.Frame(frame);row.pack(fill='x',pady=8)
        ttk.Label(row,text='Input layout').pack(side='left',padx=(0,15))
        mode=ttk.Combobox(row,textvariable=self.controller_mode,values=['Menu','Match'],state='readonly',width=12);mode.pack(side='left')
        mode.bind('<<ComboboxSelected>>',lambda event:setattr(self.controller,'mode',self.controller_mode.get()))
        ttk.Label(row,text='Stick deadzone').pack(side='left',padx=(24,12))
        ttk.Combobox(row,textvariable=self.controller_deadzone,values=['10%','15%','20%','25%','30%'],state='readonly',width=9).pack(side='left')
        ttk.Label(frame,text='View / Select + Start opens in-game settings. Menu: A confirms, B goes back. Match: A passes, B shoots, X plays a long ball, Y activates power-ups. LB sprints and RB switches players. Select / View skips videos when released. Start pauses; Start + Select opens settings without also pausing or skipping.',style='Muted.TLabel',wraplength=760).pack(anchor='w',pady=(8,15))
        card=ttk.LabelFrame(frame,text=' XBOX-STYLE MATCH LAYOUT ',padding=15);card.pack(fill='x')
        self.controller_keys={}
        for row,(button,action) in enumerate([('A','Pass / slide tackle'),('B','Shoot'),('X','Long ball / foot-in tackle'),('Y','Power-up'),('LB','Sprint'),('RB','Switch player / skill move'),('Start / Menu','Pause'),('Select / View','Skip video')]):
            ttk.Label(card,text=button).grid(row=row,column=0,sticky='w',padx=(0,30),pady=4)
            ttk.Label(card,text=action).grid(row=row,column=1,sticky='w',pady=4)
        card.columnconfigure(1,weight=1)
        raw=ttk.LabelFrame(frame,text=' UNMAPPED DEVICE BUTTONS ',padding=15);raw.pack(fill='x',pady=(18,0))
        ttk.Label(raw,text='Only used for raw joysticks without an SDL gamepad mapping. Button numbers start at zero.',style='Muted.TLabel',wraplength=700).grid(row=0,columnspan=2,sticky='w',pady=(0,8))
        mapping={int(k):int(v) for k,v in settings.get('raw_mapping',RAW_XBOX).items()};self.raw_buttons={}
        for row,(button,label) in enumerate([(0,'A'),(1,'B'),(2,'X'),(3,'Y'),(9,'LB'),(10,'RB'),(4,'View / Select'),(6,'Start / Menu')],1):
            variable=tk.StringVar(value=str(next((k for k,v in mapping.items() if v==button),-1)));self.raw_buttons[button]=variable
            ttk.Label(raw,text=label).grid(row=row,column=0,sticky='w',padx=(0,25),pady=3)
            ttk.Combobox(raw,textvariable=variable,values=['-1',*map(str,range(32))],state='readonly',width=14).grid(row=row,column=1,sticky='ew',pady=3)
        raw.columnconfigure(1,weight=1)
        ttk.Button(frame,text='Save controller profile',style='Accent.TButton',command=self.save_controls).pack(anchor='w',pady=15)
        ttk.Label(frame,text='Left stick / D-pad: movement. SDL handles mapped XInput, DirectInput and HID devices. Raw devices can use the button profile above. Each connected pad has a separate native controller slot; the keyboard remains independent. Choose each player in the game. Physical four-controller play still needs acceptance testing.',style='Muted.TLabel',wraplength=760).pack(anchor='w')

    def save_controls(self):
        raw={}
        for button,var in self.raw_buttons.items():
            index=int(var.get())
            if index<0:continue
            if index in raw:self.status.set('Assign a different physical button to each raw controller action.');return
            raw[index]=button
        settings={'enabled':self.controller_enabled.get(),'automatic_mode':self.controller_auto.get(),'deadzone':int(self.controller_deadzone.get().rstrip('%'))/100,'mapping':{k:v.get() for k,v in self.controller_keys.items()}}
        settings['raw_mapping']=raw;self.controller.raw_mapping=raw
        self.controller.automatic_mode=settings['automatic_mode']
        self.controller.enabled=settings['enabled'];self.controller.deadzone=settings['deadzone'];self.controller.mapping['Match']=settings['mapping']
        self.prefs['controls']=settings;self.persist();self.status.set('Controller profile saved. Game input is released whenever another app has focus.')

    def refresh_library(self):
        folder=HOME/'library';self.library_entries={}
        if folder.is_dir():
            for entry in sorted(folder.iterdir())[:200]:
                if entry.is_symlink() or not entry.is_dir():continue
                try:
                    info=json.loads((entry/'opensoccermania-source.json').read_text(encoding='utf-8'))
                    label=f"{Path(info['source']).name} · {entry.name}"
                    if info.get('asset_repair'):label+=' · private runtime repair'
                    self.library_entries[label]=entry
                except (OSError,ValueError,KeyError):continue
        self.library_selector.configure(values=list(self.library_entries))
        for label,entry in self.library_entries.items():
            if getattr(self,'entry',None)==entry:self.library_choice.set(label)

    def choose_library(self):
        if self.busy or self.process is not None:return
        path=self.library_entries.get(self.library_choice.get())
        if path:self.set_entry(path)

    def refresh_title_preview(self):
        self.title_preview_image=None
        if self.entry:
            try:
                picture=load_title_image(self.entry,'soccer' if self.edition.get()=='Soccer Mania' else 'football')
                picture.thumbnail((240,180),Image.Resampling.LANCZOS)
                self.title_preview_image=ImageTk.PhotoImage(picture,master=self.root)
            except (OSError,ValueError,TitleAssetError) as error:self.log.info('Title preview unavailable: %s',error)
        self.paint_hero(type('Size',(),{'width':max(650,self.hero.winfo_width())})())

    def edition_changed(self,edition_changed=False):
        if edition_changed and self.language.get() in ('American','English'):
            self.language.set('American' if self.edition.get()=='Soccer Mania' else 'English')
        self.prefs.setdefault('video',{}).update(edition=self.edition.get(),language=self.language.get())
        self.persist();self.refresh_title_preview();self.status.set('Edition and language apply on the next launch. The original simulation clock is preserved.')

    def sync_live_settings(self):
        if not self.entry:return
        try:
            settings=read_live_settings(self.entry,VideoSettings(**self.prefs.get('video',{})))
            self.prefs['video']=asdict(settings);self.persist()
            self.camera_zoom.set(settings.camera_zoom);self.performance_hud.set(settings.performance_hud)
            self.overlay_scale.set(f'{settings.overlay_scale}%');self.xbox_prompts.set(settings.xbox_prompts)
            self.render_fps.set(next(label for label,value in RENDER_LIMITS.items() if value==settings.render_fps))
        except (OSError,ValueError,TypeError) as error:self.log.warning('Could not synchronize in-game preferences: %s',error)

    def show(self,name):
        for page in self.pages.values():page.pack_forget()
        self.pages[name].pack(fill='both',expand=True)
        for n,b in self.nav.items():b.configure(bg='#2b5945' if n==name else GREEN)

    def persist(self):
        HOME.mkdir(parents=True,exist_ok=True)
        temporary=self.prefs_path.with_suffix('.tmp')
        temporary.write_text(json.dumps(self.prefs,indent=2),encoding='utf-8');temporary.replace(self.prefs_path)

    def set_entry(self,path):
        self.entry=path;self.prefs['entry']=str(path);self.persist()
        try:
            info=json.loads((path/'opensoccermania-source.json').read_text(encoding='utf-8'))
            self.game_title.set('Your team is ready.')
            self.game_detail.set(f"LEGO Soccer Mania · {len(info['languages'])} text variants · {len(info['voices'])} voice languages\n{path}")
            self.play_button.configure(text='Kick off  →')
            wrapper=child(path,'D3D8.dll')
            text=f"Executable SHA-256\n{info['sha256']}\n\nText variants\n{', '.join(info['languages'])}\n\nVoices\n{', '.join(info['voices'])}\n\nD3D8 wrapper: {'present' if wrapper else 'not supplied'}\n\nSource preserved at\n{info['source']}\n\nSupported hashes receive reversible startup patches in the private copy. Version-checked engine hooks separate rendering from the original simulation clock and adjust the camera lens.\n\nFull-match timing, character animations and all-map rendering remain unverified."
            self.report.configure(state='normal');self.report.delete('1.0','end');self.report.insert('1.0',text);self.report.configure(state='disabled')
            self.status.set('Game imported. Graphics profiles affect your private copy only.')
            self.sync_live_settings();self.refresh_library();self.refresh_title_preview()
        except (OSError,ValueError,KeyError) as error:
            self.entry=None;self.status.set(f'Library entry needs repair: {error}')

    def run_work(self,work):
        if self.busy or self.process is not None:return
        self.busy=True
        self.library_selector.configure(state='disabled')
        for b in [self.play_button,self.folder_button,self.iso_button]:b.configure(state='disabled')
        def run():
            try:self.queue.put(('ready',work()))
            except ExecutableSourceRequired as error:self.queue.put(('donor',(error.path,error.is_image)))
            except Exception as error:
                self.log.exception('Game import failed');self.queue.put(('error',str(error)))
        threading.Thread(target=run,daemon=True).start()

    def import_path(self,path,is_iso=False,donor=None,backend=None):
        def work():
            def copy(root):
                try:source=discover(root);assets=None
                except ValueError:
                    assets=discover_assets(root)
                    if donor is None:raise ExecutableSourceRequired(path,is_iso)
                    source=discover(donor)
                key=hashlib.sha256(str(Path(path).resolve()).encode('utf-8')).hexdigest()[:10]
                target=HOME/'library'/f'{source.sha256[:16]}-{key}'
                if not target.exists():
                    progress=lambda s:self.queue.put(('status',s))
                    if assets:prepare_with_executable(assets,source,target,progress)
                    else:prepare(source,target,progress)
                    manifest=target/'opensoccermania-source.json';info=json.loads(manifest.read_text(encoding='utf-8'))
                    info.update(source=str(Path(path).resolve()),source_kind='disc image' if is_iso else 'folder')
                    if is_iso:info['source_image']=str(Path(path).resolve())
                    manifest.write_text(json.dumps(info,indent=2),encoding='utf-8')
                self.queue.put(('status','Preparing the included graphics runtime...'))
                if backend:install_backend(target,*backend)
                ensure_backend(target,ROOT)
                setup_portable(target,source.sha256)
                return target
            if is_iso:
                probe=probe_disc_image(path)
                if not probe.supported:raise ValueError(probe.reason)
                with open_disc_image(path) as root:return copy(root)
            return copy(path)
        self.status.set('Inspecting game files…');self.run_work(work)

    def import_folder(self):
        if self.busy or self.process is not None:return
        path=filedialog.askdirectory(title='Choose the full disc or installed game folder')
        if path:self.import_path(path)

    def import_iso(self):
        if self.busy or self.process is not None:return
        path=filedialog.askopenfilename(title='Choose your game disc image',filetypes=[('Disc image','*.iso *.bin *.cue *.img *.mdf *.mds *.nrg'),('All files','*.*')])
        if path:self.import_path(path,True)

    def save_video(self):
        if self.busy or self.process is not None:
            self.status.set('Wait for the current operation to finish before changing graphics.');return False
        try:
            w,h=map(int,self.res.get().split(' × '))
            settings=VideoSettings(w,h,int(self.hz.get()),self.aa.get(),self.filter.get(),self.vsync.get(),self.display_mode.get()=='Fullscreen',self.horplus.get(),self.display_mode.get(),self.performance_hud.get(),int(self.overlay_scale.get().rstrip('%')),self.run_background.get(),self.integer_movies.get())
            settings.render_fps=RENDER_LIMITS[self.render_fps.get()]
            settings.auto_aspect=self.auto_aspect.get()
            settings.sprite_seam_fix=self.sprite_seam_fix.get()
            settings.modern_presentation=self.modern_presentation.get()
            settings.smooth_interpolation=False
            settings.camera_zoom=self.camera_zoom.get();settings.xbox_prompts=self.xbox_prompts.get()
            settings.edition=self.edition.get();settings.language=self.language.get();settings.title_extension=self.title_extension.get()
            settings.__post_init__()
            if self.entry:
                ensure_backend(self.entry,ROOT)
                source_info=json.loads((self.entry/'opensoccermania-source.json').read_text(encoding='utf-8'))
                setup_portable(self.entry,source_info.get('sha256',''))
                config=child(self.entry,'dgVoodoo.conf')
                wrapper=child(self.entry,'d3d8.dll')
                if not config or not wrapper:
                    raise ValueError('The included graphics runtime could not be prepared. Extract the complete OpenSoccerMania package again.')
                bridge=ROOT/'build/native/d3d8.dll'
                if not bridge.exists():bridge=ROOT/'native/d3d8.dll'
                install_bridge(self.entry,bridge)
                install_input_bridge(self.entry,bridge.with_name('dinput8.dll'))
                configure_bridge(self.entry,settings)
                backup=config.with_suffix('.conf.original')
                if not backup.exists():backup.write_bytes(config.read_bytes())
                temporary=config.with_suffix('.conf.tmp')
                temporary.write_text(configure_wrapper(config.read_text(encoding='utf-8-sig'),settings),encoding='utf-8')
                temporary.replace(config)
                self.status.set('Graphics profile applied to your private copy. Display mode support is checked by the GPU when the game starts.')
            else:self.status.set('Graphics profile saved. Import a game to apply it.')
            self.prefs['video']=asdict(settings);self.persist()
            return True
        except (ValueError,OSError) as error:
            self.log.exception('Graphics setup failed')
            messagebox.showerror('Could not save graphics',str(error));return False

    def play(self):
        if self.busy or self.process is not None:return
        if not self.entry:self.import_folder();return
        try:
            exe=child(self.entry,'Game.exe')
            if not exe:raise ValueError('Game.exe is missing. Import your game again.')
            # Apply the safe windowed default even on the first launch after importing.
            if not self.save_video():return
            self.session=Session(HOME,self.entry,self.prefs.get('video',{}))
            self.process=subprocess.Popen([str(exe),'.\\Mania\\CD\\'],cwd=self.entry,stdout=self.session.stream,stderr=subprocess.STDOUT)
            self.library_selector.configure(state='disabled')
            self.controller.pid=self.process.pid
            self.log.info('Game launched pid=%s logs=%s',self.process.pid,self.session.folder)
            self.status.set('Game launched. Waiting for the game process…')
            self.play_button.configure(state='disabled')
        except (OSError,ValueError) as error:
            self.log.exception('Game launch failed')
            if self.session:self.session.finish(-1);self.session=None
            messagebox.showerror('Could not start the game',str(error))

    def poll(self):
        try:
            while True:
                kind,value=self.queue.get_nowait()
                if kind=='status':self.status.set(value);continue
                self.busy=False
                self.library_selector.configure(state='readonly')
                for b in [self.play_button,self.folder_button,self.iso_button]:b.configure(state='normal')
                if kind=='ready':self.set_entry(value)
                elif kind=='donor':
                    self.status.set('This source has usable assets but needs an intact executable from another copy you own.')
                    donor=filedialog.askdirectory(title='Choose an intact Soccer / Football Mania copy for private runtime repair')
                    if donor:self.import_path(value[0],value[1],donor)
                else:self.status.set(value);messagebox.showerror('Could not import game',value)
        except queue.Empty:pass
        if self.process is not None and self.process.poll() is not None:
            code=self.process.returncode;self.process=None;self.play_button.configure(state='normal')
            self.controller.pid=None
            self.library_selector.configure(state='readonly');self.sync_live_settings()
            if self.session:self.session.finish(code);self.session=None
            self.log.info('Game exited code=%s',code)
            self.status.set(f'Game closed (exit code {code}).' if code else 'Match finished. Ready when you are.')
        if hasattr(self,'controller'):
            self.controller_status.set(self.controller.status)
            self.controller_mode.set(self.controller.mode)
            signature=tuple((d['id'],d['name']) for d in self.controller.devices)
            if signature!=self.device_signature:
                self.device_signature=signature;self.device_options={'Automatic':None,**{f"{d['name']} [{d['id']}]":d['id'] for d in self.controller.devices}}
                self.device_selector.configure(values=list(self.device_options))
                if self.device_choice.get() not in self.device_options:self.device_choice.set('Automatic');self.controller.selected_id=None
        self.root.after(100,self.poll)

    def open_library(self):
        HOME.mkdir(parents=True,exist_ok=True);os.startfile(str(HOME))

    def open_logs(self):
        path=HOME/'logs';path.mkdir(parents=True,exist_ok=True);os.startfile(str(path))

    def close(self):
        if self.busy:
            self.status.set('Please wait for the import to finish before closing.');return
        self.controller.stop()
        if self.session:
            self.session.info['status']='launcher_closed_while_game_running';self.session.write();self.session.stream.close()
        self.root.destroy()


def main():
    if len(sys.argv)==3 and sys.argv[1]=='--smoke-test-report':
        from .selftest import run
        raise SystemExit(run(sys.argv[2]))
    root=tk.Tk();Launcher(root);root.mainloop()


if __name__=='__main__':main()
