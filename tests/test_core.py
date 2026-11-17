import tempfile
import unittest
from pathlib import Path
from opensoccermania.sources import discover, prepare
from opensoccermania.video import VideoSettings, configure_wrapper


def fixture(base):
    (base / 'Mania/CD/Messages').mkdir(parents=True)
    (base / 'Mania/CD/Messages/spanish.txt').write_text('HELLO:Hola')
    (base / 'Mania/CD/Sound/CutScene/SPA').mkdir(parents=True)
    (base / 'Mania/CD/Sound/CutScene/SPA/line.ogg').write_bytes(b'OggSfixture')
    (base / 'Game.exe').write_bytes(b'MZ' + b'fixture' * 100)
    return base


class SourceTests(unittest.TestCase):
    def test_installed_disc_and_private_copy(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            src = fixture(root / 'disc with spaces')
            found = discover(src)
            self.assertEqual(found.languages, ['Spanish'])
            self.assertEqual(found.voices, ['SPA'])
            target = prepare(found, root / 'private')
            self.assertTrue((target / 'Game.exe').is_file())
            self.assertEqual((src / 'Game.exe').read_bytes(), (target / 'Game.exe').read_bytes())
            self.assertTrue((target / 'Mania/CD/Messages/spanish.txt').is_file())

    def test_installer_tree(self):
        with tempfile.TemporaryDirectory() as t:
            root = fixture(Path(t))
            dest = root / 'Setup/English/program files/LEGO Media/Soccer Mania'
            dest.mkdir(parents=True)
            (root / 'Game.exe').replace(dest / 'Game.exe')
            # Windows runners may expose TEMP through an 8.3 alias. Discovery
            # canonicalizes paths; compare the resolved identity on both sides.
            self.assertEqual(discover(root).exe, (dest / 'Game.exe').resolve())

    def test_zero_executable_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = fixture(Path(t))
            (root / 'Game.exe').write_bytes(bytes(1024))
            with self.assertRaisesRegex(ValueError, 'executable'):
                discover(root)

    def test_missing_assets_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            (root / 'Game.exe').write_bytes(b'MZ' + bytes(100))
            with self.assertRaisesRegex(ValueError, 'data'):
                discover(root)

    def test_reimport_never_overwrites(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            src = fixture(root / 'source')
            (root / 'private').mkdir()
            (root / 'private/save').write_text('keep')
            with self.assertRaises(FileExistsError):
                prepare(discover(src), root / 'private')
            self.assertEqual((root / 'private/save').read_text(), 'keep')


class VideoTests(unittest.TestCase):
    def test_window_modes_override_game_control_and_release_mouse(self):
        import configparser
        for mode in ['Windowed','Borderless','Fullscreen']:
            settings=VideoSettings(display_mode=mode)
            text=configure_wrapper('[DirectX]\nAppControlledScreenMode = true\n',settings)
            parsed=configparser.ConfigParser();parsed.read_string(text)
            self.assertEqual(parsed['DirectX']['AppControlledScreenMode'],'false')
            self.assertEqual(parsed['General']['FullScreenMode'],str(mode=='Fullscreen').lower())
            self.assertEqual(parsed['General']['CaptureMouse'],str(mode=='Fullscreen').lower())
            self.assertEqual(parsed['General']['OutputAPI'],'d3d11_fl11_0')
            self.assertEqual(parsed['GeneralExt']['WindowedAttributes'],'borderless, fullscreensize' if mode=='Borderless' else '')
            self.assertNotIn('alwaysontop',text.lower())

    def test_safe_default_is_windowed(self):
        self.assertEqual(VideoSettings().window_mode,'Windowed')
        self.assertTrue(VideoSettings().horplus)
        self.assertFalse(VideoSettings(smooth_interpolation=True).smooth_interpolation)
        with self.assertRaises(ValueError):VideoSettings(display_mode='invalid')

    def test_adaptive_aspect_does_not_constrain_window_shape(self):
        adaptive=configure_wrapper('',VideoSettings(horplus=True,auto_aspect=True))
        fixed=configure_wrapper('',VideoSettings(horplus=True,auto_aspect=False))
        self.assertIn('KeepWindowAspectRatio = false',adaptive)
        self.assertIn('KeepWindowAspectRatio = true',fixed)
        with self.assertRaises(ValueError):VideoSettings(render_fps=59)

    def test_real_wrapper_version_preamble(self):
        text='; Wrapper version\nVersion = 0x279\n\n[General]\nScalingMode=stretched_4_3\n'
        result=configure_wrapper(text,VideoSettings(horplus=False))
        self.assertTrue(result.startswith('; Wrapper version\nVersion = 0x279'))
        self.assertIn('stretched_ar',result)

    def test_profiles_preserve_aspect_and_unknown_options(self):
        old = '[General]\nScalingMode = stretched_4_3\nFoo = keep\n[DirectX]\nResolution = h:1920, v:1440, refrate:59\n[Glide]\nResolution = h:1920, v:1440, refrate:59\n'
        new = configure_wrapper(old, VideoSettings(3840, 2160, 240,horplus=False))
        self.assertIn('stretched_ar', new)
        self.assertIn('Foo = keep', new)
        self.assertIn('h:3840, v:2160, refrate:240', new)
        self.assertNotIn('refrate:59', new)
        self.assertEqual(new.count('refrate:240'), 1)

    def test_invalid_profile_rejected(self):
        for args in [(7680,4320,240),(1920,1080,1000),(0,1080,60)]:
            with self.assertRaises(ValueError):
                VideoSettings(*args)


if __name__ == '__main__':
    unittest.main()
