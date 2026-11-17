"""Network lobby and input timeline foundation; no game simulation integration."""
from .client import NetplayClient, NetplayError
from .protocol import Compatibility, InputTimeline

__all__ = ["NetplayClient", "NetplayError", "NetplayServer", "Compatibility", "InputTimeline"]


def __getattr__(name):
    if name == "NetplayServer":
        from .server import NetplayServer
        return NetplayServer
    raise AttributeError(name)
