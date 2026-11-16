"""Inspect and import a game without launching it."""
from pathlib import Path
from opensoccermania.sources import discover, prepare
import json
root=Path(__file__).resolve().parent.parent
src=discover(root/'Lego Football Mania Dutch')
print(src,flush=True)
dest=root/'local/library/football-mania'
if not dest.exists():prepare(src,dest,lambda s:print(s,flush=True))
print('PREPARED',dest,flush=True)
