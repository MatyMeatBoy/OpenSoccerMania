"""Private-copy compatibility setup, native bridge deployment and launch settings."""
from pathlib import Path
import configparser
import hashlib
import io
import shutil
from .sources import child
from .patches import apply_patch, portable_startup, SUPPORTED_HASHES, SUPPORTED_RUNTIME_HASHES


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
    config['Game']={'Language':getattr(settings,'language','English').upper()}
    out=io.StringIO();config.write(out)
    path=Path(entry)/'opensoccermania.ini'
    temporary=path.with_suffix('.tmp');temporary.write_text(out.getvalue(),encoding='ascii');temporary.replace(path)


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
