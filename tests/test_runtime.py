import hashlib,tempfile,unittest,json
from pathlib import Path
from unittest.mock import patch
from opensoccermania.runtime import install_bridge,configure_bridge,read_live_settings
from opensoccermania.video import VideoSettings
from opensoccermania.sources import mounted_iso

class RuntimeTests(unittest.TestCase):
    def test_title_background_uses_private_relative_path_and_independent_region(self):
        with tempfile.TemporaryDirectory() as t:
            entry=Path(t).resolve()
            metadata={'backgroundBmp':str(entry/'opensoccermania-title/title.bmp'),'centerRect':[682,0,1365,512]}
            with patch('opensoccermania.title_presentation.prepare_title_presentation',return_value=metadata):
                configure_bridge(entry,VideoSettings(edition='Soccer Mania',language='Spanish'))
            data=(entry/'opensoccermania.ini').read_text()
            self.assertIn('TitleBackgroundPath = opensoccermania-title/title.bmp',data)
            self.assertIn('TitleCenterLeft = 682',data)
            self.assertIn('TitleCenterRight = 1365',data)
            self.assertIn('Edition = SOCCER',data)
            self.assertIn('Language = SPANISH',data)
            self.assertNotIn(str(entry),data)

    def test_live_panel_only_accepts_valid_options(self):
        with tempfile.TemporaryDirectory() as t:
            entry=Path(t)
            settings=VideoSettings(edition='Soccer Mania',language='Spanish')
            (entry/'opensoccermania.ini').write_text('[Graphics]\nCameraZoom=2\nOverlayScale=175\nRenderFPS=480\nSmoothInterpolation=0\nXboxPrompts=1\nOutputWidth=999999\n[Game]\nLanguage=UNKNOWN\n')
            changed=read_live_settings(entry,settings)
            self.assertEqual(changed.camera_zoom,'Distant view (experimental)')
            self.assertEqual(changed.overlay_scale,175)
            self.assertEqual(changed.render_fps,480)
            self.assertFalse(changed.smooth_interpolation)
            self.assertEqual((changed.width,changed.edition,changed.language),(1920,'Soccer Mania','Spanish'))
            (entry/'opensoccermania.ini').write_text('[Graphics]\nCameraZoom=-1\nOverlayScale=9999\nRenderFPS=-10\nSmoothInterpolation=7\n')
            self.assertEqual(read_live_settings(entry,settings),settings)
    def test_bridge_keeps_backend_and_detects_unexpected_changes(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);entry=root/'game';entry.mkdir();bridge=root/'bridge.dll';bridge.write_bytes(b'new')
            (entry/'D3D8.dll').write_bytes(b'old-wrapper')
            install_bridge(entry,bridge)
            self.assertEqual((entry/'d3d8_backend.dll').read_bytes(),b'old-wrapper')
            self.assertEqual((entry/'D3D8.dll').read_bytes(),b'new')
            install_bridge(entry,bridge)
            (entry/'D3D8.dll').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'changed'):install_bridge(entry,bridge)
    def test_unknown_executable_cannot_enable_engine_hooks(self):
        with tempfile.TemporaryDirectory() as t:
            (Path(t)/'Game.exe').write_bytes(b'MZ-unknown-game-version')
            configure_bridge(Path(t),VideoSettings(3440,1440,240,horplus=True))
            data=(Path(t)/'opensoccermania.ini').read_text()
            self.assertIn('ExperimentalHorPlus = 1',data)
            self.assertIn('VerifiedExecutable = 0',data)
            self.assertIn('RenderFPS = 240',data)
            self.assertIn('SpriteSeamFix = 7',data)
            self.assertNotIn('SimulationHz',data)
    def test_verified_executable_retains_independent_render_limit(self):
        with tempfile.TemporaryDirectory() as t:
            executable=b'MZ-test-fixture'
            (Path(t)/'Game.exe').write_bytes(executable)
            with patch('opensoccermania.runtime.SUPPORTED_RUNTIME_HASHES',{hashlib.sha256(executable).hexdigest()}):
                configure_bridge(Path(t),VideoSettings(render_fps=360,auto_aspect=False))
            data=(Path(t)/'opensoccermania.ini').read_text()
            self.assertIn('VerifiedExecutable = 1',data)
            self.assertIn('RenderFPS = 360',data)
            self.assertIn('AutoWindowAspect = 0',data)
            self.assertIn('SmoothInterpolation = 0',data)
            configure_bridge(Path(t),VideoSettings(smooth_interpolation=False))
            self.assertIn('SmoothInterpolation = 0',(Path(t)/'opensoccermania.ini').read_text())
    def test_iso_owned_mount_detached_on_import_error(self):
        with tempfile.TemporaryDirectory() as t:
            iso=Path(t)/"player's game.iso";iso.write_bytes(b'fixture')
            result=type('Result',(),{'returncode':0,'stdout':json.dumps({'root':'E:\\','owned':True}),'stderr':''})()
            with patch('opensoccermania.sources.subprocess.run',return_value=result) as run:
                with self.assertRaisesRegex(ValueError,'fixture failure'):
                    with mounted_iso(iso):raise ValueError('fixture failure')
                self.assertEqual(run.call_count,2)
                self.assertIn('Dismount-DiskImage',run.call_args.args[0][-1])
                self.assertIn("player''s game.iso",run.call_args.args[0][-1])
    def test_iso_existing_mount_not_detached(self):
        with tempfile.TemporaryDirectory() as t:
            iso=Path(t)/'game.iso';iso.write_bytes(b'fixture')
            result=type('Result',(),{'returncode':0,'stdout':json.dumps({'root':'E:\\','owned':False}),'stderr':''})()
            with patch('opensoccermania.sources.subprocess.run',return_value=result) as run:
                with mounted_iso(iso):pass
                self.assertEqual(run.call_count,1)
