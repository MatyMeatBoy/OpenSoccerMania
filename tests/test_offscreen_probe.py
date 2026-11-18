"""The diagnostic CLI must advertise render limits that it can configure."""
from pathlib import Path
import re
import subprocess
import sys
import unittest

from opensoccermania.video import VideoSettings


class OffscreenProbeCliTests(unittest.TestCase):
    def test_every_advertised_render_limit_is_accepted(self):
        script = Path(__file__).resolve().parents[1] / 'tools/offscreen_game_probe.py'
        result = subprocess.run([sys.executable, str(script), '--help'],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        choices = re.search(r'--fps\s+\{([^}]+)\}', result.stdout)
        self.assertIsNotNone(choices, result.stdout)
        for value in choices.group(1).split(','):
            with self.subTest(advertised=value):
                VideoSettings(render_fps=int(value))
