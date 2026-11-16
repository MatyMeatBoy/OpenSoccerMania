from pathlib import Path
import ctypes,subprocess,time,json
from ctypes import wintypes
root=Path.cwd();entry=root/'local/library/football-mania';u=ctypes.windll.user32
before=[u.GetSystemMetrics(0),u.GetSystemMetrics(1)]
log=entry/'opensoccermania-render.log';initial=log.stat().st_size if log.exists() else 0
p=subprocess.Popen([str(entry/'Game.exe'),'.\\Mania\\CD\\'],cwd=entry)
try:
 time.sleep(5);windows=[]
 @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
 def visit(hwnd,param):
  pid=wintypes.DWORD();u.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
  if pid.value==p.pid and u.IsWindowVisible(hwnd):
   text=ctypes.create_unicode_buffer(1024);u.GetWindowTextW(hwnd,text,1024)
   rect=wintypes.RECT();u.GetWindowRect(hwnd,ctypes.byref(rect));style=u.GetWindowLongW(hwnd,-16)&0xffffffff
   windows.append({'title':text.value,'rect':[rect.left,rect.top,rect.right,rect.bottom],'caption':bool(style&0xc00000),'topmost':bool(u.GetWindowLongW(hwnd,-20)&8)})
  return True
 u.EnumWindows(visit,0)
 newlog=log.read_bytes()[initial:].decode(errors='replace') if log.exists() else ''
 result={'screen_before':before,'screen_after':[u.GetSystemMetrics(0),u.GetSystemMetrics(1)],'running':p.poll() is None,'windows':windows,'render_log':newlog}
 print(json.dumps(result,indent=2))
 assert before==result['screen_after'] and result['running']
 assert any(w['title']=='Football Mania' and w['caption'] and not w['topmost'] for w in windows)
 assert 'windowed=1' in newlog
 (root/'local/windowed-verification.json').write_text(json.dumps(result,indent=2))
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
