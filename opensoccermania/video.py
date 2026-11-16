"""Graphics presentation settings; preserve the original simulation rate."""
from dataclasses import dataclass
import configparser
import io
import re

RESOLUTIONS = ['1280 × 720', '1920 × 1080', '2560 × 1440', '3840 × 2160', '2560 × 1080', '3440 × 1440', '3840 × 1600', '3840 × 1080', '1920 × 1440']
REFRESH_RATES = [60, 75, 100, 120, 144, 165, 180, 200, 240]
AA_MODES = {'Game default': 'appdriven', '2× MSAA': '2x', '4× MSAA': '4x', '8× MSAA': '8x', '16× MSAA (if supported)': '16x'}
FILTER_MODES = {'Game default': 'appdriven', 'Bilinear': 'bilinear', 'Trilinear': 'trilinear', '16× anisotropic': '16'}
DISPLAY_MODES = ['Windowed', 'Borderless', 'Fullscreen']
RENDER_LIMITS = {'60 FPS':60, '120 FPS':120, '144 FPS':144, '165 FPS':165,
                 '240 FPS':240, '360 FPS':360, '480 FPS':480, 'Unlimited':0}


@dataclass
class VideoSettings:
    width: int = 1920
    height: int = 1080
    refresh: int = 60
    aa: str = '4× MSAA'
    filtering: str = '16× anisotropic'
    vsync: bool = True
    fullscreen: bool = False
    horplus: bool = False
    display_mode: str | None = None
    performance_hud: bool = True
    overlay_scale: int = 125
    run_background: bool = True
    integer_movies: bool = True
    render_fps: int = 240
    auto_aspect: bool = True
    sprite_seam_fix: bool = True
    modern_presentation: bool = True
    smooth_interpolation: bool = True

    @property
    def window_mode(self):
        return self.display_mode or ('Fullscreen' if self.fullscreen else 'Windowed')

    def __post_init__(self):
        if not (640 <= self.width <= 3840 and 480 <= self.height <= 2160 and self.refresh in REFRESH_RATES):
            raise ValueError('Choose a resolution up to 3840 × 2160 and a refresh rate from 60 to 240 Hz.')
        if self.aa not in AA_MODES or self.filtering not in FILTER_MODES:
            raise ValueError('Unsupported antialiasing or texture filtering mode.')
        if self.window_mode not in DISPLAY_MODES:
            raise ValueError('Unsupported display mode.')
        if self.overlay_scale not in [75,100,125,150,175,200]:
            raise ValueError('Choose an overlay scale from 75% to 200%.')
        if self.render_fps not in RENDER_LIMITS.values():
            raise ValueError('Choose a supported render FPS limit or Unlimited.')


def configure_wrapper(text, settings):
    config = configparser.ConfigParser(interpolation=None, strict=False)
    config.optionxform = str
    match=re.search(r'^\s*\[[^\]\r\n]+\]',text,re.MULTILINE)
    preamble=text[:match.start()] if match else text
    config.read_string(text[match.start():] if match else '')
    def put(section, key, value):
        if not config.has_section(section):
            config.add_section(section)
        # Match existing keys without leaving case-variant duplicates.
        for existing in list(config[section]):
            if existing.lower() == key.lower():
                config.remove_option(section, existing)
        config.set(section, key, str(value))
    put('General', 'ScalingMode', 'stretched' if settings.horplus else 'stretched_ar')
    put('General', 'KeepWindowAspectRatio', str(not(settings.horplus and settings.auto_aspect)).lower())
    fullscreen=settings.window_mode=='Fullscreen'
    put('General', 'FullScreenMode', str(fullscreen).lower())
    put('General', 'CaptureMouse', str(fullscreen).lower())
    put('General', 'CenterAppWindow', 'true')
    put('General', 'OutputAPI', 'd3d11_fl11_0')
    put('GeneralExt', 'FreeMouse', str(not fullscreen).lower())
    put('GeneralExt', 'PresentationModel', 'flip_discard' if settings.modern_presentation else 'auto')
    put('GeneralExt', 'FPSLimit', settings.render_fps)
    put('GeneralExt', 'WindowedAttributes', 'borderless, fullscreensize' if settings.window_mode=='Borderless' else '')
    put('GeneralExt', 'FullscreenAttributes', '')
    # Otherwise the game's fullscreen request overrides FullScreenMode=false.
    put('DirectX', 'AppControlledScreenMode', 'false')
    put('General', 'EnumerateRefreshRates', 'true')
    put('DirectX', 'Resolution', f'h:{settings.width}, v:{settings.height}, refrate:{settings.refresh}')
    put('DirectX', 'Antialiasing', AA_MODES[settings.aa])
    put('DirectX', 'Filtering', FILTER_MODES[settings.filtering])
    put('DirectX', 'KeepFilterIfPointSampled', 'true')
    put('DirectX', 'ForceVerticalSync', str(settings.vsync).lower())
    put('DirectX', 'dgVoodooWatermark', 'false')
    # Remove the unrelated Glide override inherited from the supplied package.
    if config.has_section('Glide'):
        for key in list(config['Glide']):
            if key.lower() == 'resolution':
                config.remove_option('Glide', key)
    result = io.StringIO()
    config.write(result)
    return preamble + result.getvalue()
