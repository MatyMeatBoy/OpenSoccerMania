import contextlib
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest

from tools.run_offscreen import run


@unittest.skipUnless(os.name == 'nt', 'Win32 private desktop integration.')
class OffscreenTests(unittest.TestCase):
    def test_nonzero_exit_and_both_output_streams(self):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            code = run([sys.executable, '-c',
                        'import sys; print("out"); print("err",file=sys.stderr); sys.exit(7)'],
                       Path.cwd(), 10)
        self.assertEqual(code, 7)
        self.assertIn('out', output.getvalue())
        self.assertIn('err', errors.getvalue())

    def test_bounded_forced_stop_reports_failure(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = run([sys.executable, '-c', 'import time; time.sleep(20)'],
                       Path.cwd(), .02, allow_forced_stop=True)
        self.assertNotEqual(code, 0)
        self.assertIn('forced shutdown', output.getvalue())

    def test_child_really_uses_private_desktop(self):
        with tempfile.TemporaryDirectory() as temporary:
            child = Path(temporary) / 'desktop child.py'
            child.write_text('''import ctypes as C
from ctypes import wintypes as W
user=C.windll.user32
user.GetThreadDesktop.argtypes=[W.DWORD]
user.GetThreadDesktop.restype=W.HANDLE
user.GetUserObjectInformationW.argtypes=[W.HANDLE,C.c_int,C.c_void_p,W.DWORD,C.POINTER(W.DWORD)]
desktop=user.GetThreadDesktop(C.windll.kernel32.GetCurrentThreadId())
name=C.create_unicode_buffer(256)
length=W.DWORD()
assert user.GetUserObjectInformationW(desktop,2,name,C.sizeof(name),C.byref(length))
print('CHILD_DESKTOP='+name.value)
assert name.value.startswith('OpenSoccerMania-Test-'),name.value
''')
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(run([sys.executable, str(child)], temporary, 10), 0)
            self.assertIn('CHILD_DESKTOP=OpenSoccerMania-Test-', output.getvalue())
            self.assertIn('input desktop unchanged', output.getvalue())


if __name__ == '__main__':
    unittest.main()
