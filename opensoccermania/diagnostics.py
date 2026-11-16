"""Private per-session logs, native diagnostics and launcher error reporting."""
from datetime import datetime, timezone
from pathlib import Path
import json
import logging
from logging.handlers import RotatingFileHandler
import uuid


def logger(home):
    folder=Path(home)/'logs';folder.mkdir(parents=True,exist_ok=True)
    result=logging.getLogger('opensoccermania')
    if not result.handlers:
        handler=RotatingFileHandler(folder/'launcher.log',maxBytes=2_000_000,backupCount=3,encoding='utf-8')
        handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
        result.addHandler(handler);result.setLevel(logging.INFO)
    return result


class Session:
    def __init__(self,home,entry,settings):
        self.entry=Path(entry)
        stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
        self.folder=Path(home)/'logs'/f'{stamp}-{uuid.uuid4().hex[:6]}'
        self.folder.mkdir(parents=True)
        self.offsets={p.name:p.stat().st_size for p in self.entry.glob('opensoccermania-*.log')}
        self.info={'started_utc':stamp,'game':str(self.entry),'graphics':settings,'status':'starting'}
        self.stream=(self.folder/'process.log').open('w',encoding='utf-8')
        self.write()

    def write(self):
        (self.folder/'session.json').write_text(json.dumps(self.info,indent=2),encoding='utf-8')

    def finish(self,code):
        self.stream.close()
        self.info.update({'exit_code':code,'status':'closed','ended_utc':datetime.now(timezone.utc).isoformat()})
        for path in self.entry.glob('opensoccermania-*.log'):
            with path.open('rb') as source:
                source.seek(self.offsets.get(path.name,0))
                (self.folder/path.name).write_bytes(source.read())
        self.write()
