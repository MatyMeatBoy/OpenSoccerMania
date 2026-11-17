"""Private-copy compatibility setup, native bridge deployment and launch settings."""
from pathlib import Path
import configparser
import hashlib
import io
import shutil
import logging
from .sources import child
from .patches import apply_patch, portable_startup, SUPPORTED_HASHES, SUPPORTED_RUNTIME_HASHES
from .video import CAMERA_ZOOMS,RENDER_LIMITS
from .controller_prompts import PROMPT_STYLES


def setup_portable(entry, original_hash):
    entry=Path(entry)
    if original_hash in SUPPORTED_HASHES:
        apply_patch(child(entry,'Game.exe'),portable_startup(original_hash))
        return True
    return False


def install_bridge(entry, bridge):
    entry,bridge=Path(entry),Path(bridge)
    if not bridge.is_file():
        raise ValueError('The native render bridge is missing. Build it with tools/build_native.ps1.')
    target=child(entry,'d3d8.dll') or entry/'d3d8.dll'
    backup=entry/'d3d8_backend.dll'
    marker=entry/'opensoccermania-bridge.sha256'
    digest=hashlib.sha256(bridge.read_bytes()).hexdigest()
    if marker.exists():
        expected=marker.read_text().strip()
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=expected:
            raise ValueError('The installed render bridge changed. Restore it before upgrading.')
        if expected==digest:return
    elif target.exists():
        if backup.exists():
            raise ValueError('A graphics backend already exists. No files were replaced.')
        target.rename(backup)
    temporary=entry/'d3d8.install.tmp'
    try:
        shutil.copy2(bridge,temporary)
        temporary.replace(target)
        marker.write_text(digest,encoding='ascii')
    except Exception:
        if not marker.exists() and backup.exists() and not target.exists():backup.rename(target)
        raise


def configure_bridge(entry,settings):
    config=configparser.ConfigParser(interpolation=None)
    config.optionxform=str
    config['Graphics']={'OutputWidth':str(settings.width),'OutputHeight':str(settings.height),'ExperimentalHorPlus':str(int(settings.horplus)), 'DisplayMode':settings.window_mode,'PerformanceHud':str(int(settings.performance_hud)),'OverlayScale':str(settings.overlay_scale),'RunInBackground':str(int(settings.run_background)),'IntegerMovies':str(int(settings.integer_movies))}
    executable=child(Path(entry),'Game.exe')
    verified=bool(executable and hashlib.sha256(executable.read_bytes()).hexdigest() in SUPPORTED_RUNTIME_HASHES)
    config['Graphics']['VerifiedExecutable']=str(int(verified))
    config['Graphics']['RenderFPS']=str(getattr(settings,'render_fps',240))
    config['Graphics']['AutoWindowAspect']=str(int(getattr(settings,'auto_aspect',True)))
    config['Graphics']['SpriteSeamFix']='7' if settings.sprite_seam_fix else '0'
    config['Graphics']['VSync']=str(int(settings.vsync))
    config['Graphics']['SmoothInterpolation']='0'
    config['Graphics']['CameraZoom']=str(CAMERA_ZOOMS.index(settings.camera_zoom))
    config['Graphics']['XboxPrompts']=str(int(settings.xbox_prompts))
    config['Graphics']['ControllerPromptFamily']=str(PROMPT_STYLES.index(settings.controller_prompt_style))
    config['Graphics']['TitleExtension']=str(int(settings.title_extension))
    config['Game']={'Language':getattr(settings,'language','English').upper()}
    config['Game']['Edition']='SOCCER' if settings.edition=='Soccer Mania' else 'FOOTBALL'
    root=Path(__file__).resolve().parents[1]
    config['UI']={'FontPath':str(root/'assets/fonts/Nunito.ttf') if str(root).isascii() else ''}
    if settings.title_extension:
        from .title_assets import TitleAssetError
        from .title_presentation import prepare_title_presentation
        try:
            title=prepare_title_presentation(entry,'soccer' if settings.edition=='Soccer Mania' else 'football',root/'assets/backgrounds/title-extension.png',Path(entry)/'opensoccermania-title')
            config['UI']['TitleBackgroundPath']=Path(title['backgroundBmp']).relative_to(Path(entry).resolve()).as_posix()
            config['UI']['TitleCenterLeft']=str(title['centerRect'][0])
            config['UI']['TitleCenterRight']=str(title['centerRect'][2])
        except (OSError,ValueError,TitleAssetError) as error:
            logging.getLogger('opensoccermania').warning('Title extension unavailable; preserving original presentation: %s',error)
    out=io.StringIO();config.write(out)
    path=Path(entry)/'opensoccermania.ini'
    temporary=path.with_suffix('.tmp');temporary.write_text(out.getvalue(),encoding='ascii');temporary.replace(path)


def read_live_settings(entry,settings):
    """Read only validated options changed by the native panel; preserve others."""
    from dataclasses import replace
    config=configparser.ConfigParser(interpolation=None)
    try:config.read(Path(entry)/'opensoccermania.ini',encoding='ascii')
    except (OSError,ValueError,configparser.Error):return settings
    if not config.has_section('Graphics'):return settings
    values={}
    for key,field in [('PerformanceHud','performance_hud'),('XboxPrompts','xbox_prompts')]:
        try:
            value=config.getint('Graphics',key)
            if value in (0,1):values[field]=bool(value)
        except (ValueError,configparser.Error):pass
    for key,field,allowed in [('OverlayScale','overlay_scale',[75,100,125,150,175,200]),('RenderFPS','render_fps',list(RENDER_LIMITS.values()))]:
        try:
            value=config.getint('Graphics',key)
            if value in allowed:values[field]=value
        except (ValueError,configparser.Error):pass
    try:
        zoom=config.getint('Graphics','CameraZoom')
        if 0<=zoom<len(CAMERA_ZOOMS):values['camera_zoom']=CAMERA_ZOOMS[zoom]
    except (ValueError,configparser.Error):pass
    try:
        style=config.getint('Graphics','ControllerPromptFamily')
        if 0<=style<len(PROMPT_STYLES):values['controller_prompt_style']=PROMPT_STYLES[style]
    except (ValueError,configparser.Error):pass
    return replace(settings,**values)


def write_live_prompt_style(entry, style):
    """Change one INI key without replacing unrelated live panel preferences."""
    index = PROMPT_STYLES.index(style)
    path = Path(entry)/'opensoccermania.ini'
    import os
    if os.name == 'nt':
        import ctypes as C
        writer = C.WinDLL('kernel32', use_last_error=True).WritePrivateProfileStringW
        writer.argtypes = [C.c_wchar_p, C.c_wchar_p, C.c_wchar_p, C.c_wchar_p]
        writer.restype = C.c_int
        if not writer('Graphics', 'ControllerPromptFamily', str(index), str(path.resolve())):
            raise C.WinError(C.get_last_error())
    else:
        config = configparser.ConfigParser(interpolation=None)
        if path.exists():
            with path.open(encoding='ascii') as stream:
                config.read_file(stream)
        if not config.has_section('Graphics'):
            config.add_section('Graphics')
        config['Graphics']['ControllerPromptFamily'] = str(index)
        out = io.StringIO(); config.write(out)
        temporary = path.with_suffix('.prompt.tmp')
        temporary.write_text(out.getvalue(), encoding='ascii'); temporary.replace(path)


def install_input_bridge(entry, bridge):
    entry,bridge=Path(entry),Path(bridge)
    target=child(entry,'dinput8.dll') or entry/'dinput8.dll'
    marker=entry/'opensoccermania-input.sha256'
    if not bridge.is_file():raise ValueError('The input compatibility bridge is missing. Rebuild the native components.')
    digest=hashlib.sha256(bridge.read_bytes()).hexdigest()
    if target.exists():
        if not marker.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=marker.read_text().strip():
            raise ValueError('An unknown dinput8.dll is already installed. It was not replaced.')
    temp=entry/'dinput8.install.tmp';shutil.copy2(bridge,temp);temp.replace(target)
    marker.write_text(digest,encoding='ascii')
