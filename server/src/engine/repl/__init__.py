"""M-REPL — host-side replication emit.

Turns a live host objectList (the char objects M-API's TPlayerCache:LoadPlayers produced) into the
faithful `onLoadPlayersCacheResult` wire payload: a tag-0x05 array of tag-0x07 char objects, each
serialized by M-CODEC via the shipped Class.Pack. The proven transport + DownCall envelope already
exist in the test stub (Res/gss_stub_server_v4.py: build_onloadplayers_downcall +
native_nid("onLoadPlayersCacheResult")); M-REPL supplies the faithful objectList bytes to inject in
place of the stub's forged OID list. The faithful-bytes-vs-client verdict is the UAC live test.
"""
from .replicate import Replicator

__all__ = ["Replicator"]
