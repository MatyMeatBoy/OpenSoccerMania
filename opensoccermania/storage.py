"""Keep mutable launcher/game files writable without machine-wide privileges."""
import errno
import os
from pathlib import Path
import tempfile


def _probe_directory(folder):
    folder.mkdir(parents=True, exist_ok=True)
    # os.access is not a reliable Windows ACL check. Actually create/write and
    # delete a disposable file before selecting a library location.
    with tempfile.TemporaryFile(dir=folder, prefix='.opensoccermania-write-') as stream:
        stream.write(b'OpenSoccerMania')
        stream.flush()
    directory = Path(tempfile.mkdtemp(dir=folder, prefix='.opensoccermania-directory-'))
    directory.rmdir()


def _writable(folder):
    _probe_directory(folder)
    for name in ('logs', 'library'):
        _probe_directory(folder / name)
    for relative in ('launcher.json', 'logs/launcher.log'):
        existing = folder / relative
        if existing.exists():
            # Probe access to existing mutable files too: a writable directory
            # can still contain an administrator-owned/read-only preferences file.
            with existing.open('ab'):
                pass


def select_home(root, environ=None):
    """Preserve writable portable storage, otherwise use this user's AppData."""
    environ = os.environ if environ is None else environ
    portable = Path(root) / 'local'
    try:
        _writable(portable)
        return portable
    except OSError as error:
        if error.errno not in (errno.EACCES, errno.EPERM, errno.EROFS,
                               errno.EEXIST, errno.ENOTDIR):
            raise
    base = Path(environ.get('LOCALAPPDATA') or Path.home() / 'AppData/Local')
    if not base.is_absolute():
        raise OSError('LOCALAPPDATA must name an absolute writable folder.')
    home = base / 'OpenSoccerMania'
    try:
        _writable(home)
    except OSError as error:
        raise OSError(f'Could not create user data in {home}. Extract OpenSoccerMania '
                      'to a writable folder in your user profile and retry.') from error
    return home
