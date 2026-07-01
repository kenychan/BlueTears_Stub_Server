"""M-CODEC — the goEngine wire codec, host implementation.

Implements the tag-variant wire format (server/re/M-CODEC-wire-format.md, RE'd from the writer
goLua_PackFixed=FUN_00bb7e80 + reader bef480) as the host natives goLua_Pack/goLua_PackFixed/
goLua_UnpackFixed, driven by the shipped Lua Class.Pack so the host emits exactly what the real
server did. Client-correctness of the byte widths is the deferred live test; the self-test here
proves internal Pack<->Unpack consistency.
"""
from .wire import WireBuffer, WireCodec

__all__ = ["WireBuffer", "WireCodec"]
