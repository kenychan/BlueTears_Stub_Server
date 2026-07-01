"""M-CODEC wire codec — see server/re/M-CODEC-wire-format.md.

Tag-variant encoding (goLua_PackFixed = FUN_00bb7e80), tag = the goLua value type:
  nil=0x00  bool=0x01(+1B)  number=0x03(+8B double)  string=0x04(+u32 len+bytes)
  table=0x05(+u16 count + elems)  object=0x07(+className string + body)
The shipped Class.Pack drives it: goLua_Pack(source,"size",N) header, then N x goLua_PackFixed.

Byte widths are taken from the reader-side RE (bef480); writer-side widths are confirmed live
(the UAC round-trip against the real client), so they are isolated in WireBuffer for easy revision.
"""
from __future__ import annotations

import struct

# tags = goLua value type codes (FUN_00bb7e80 switch)
TAG_NIL, TAG_BOOL, TAG_NUM, TAG_STR, TAG_TABLE, TAG_OBJ, TAG_REF = 0x00, 0x01, 0x03, 0x04, 0x05, 0x07, 0x0B


class WireBuffer:
    """Byte buffer with the stream primitives (source vtbl +0x40/0x44/0x50/0x54/0x58)."""
    def __init__(self, data: bytes = b""):
        self.buf = bytearray(data)
        self.pos = 0

    # writers
    def w_tag(self, t: int):    self.buf.append(t & 0xFF)            # +0x44
    def w_count(self, n: int):  self.buf += struct.pack("<H", n & 0xFFFF)   # +0x40 (u16)
    def w_double(self, d: float): self.buf += struct.pack("<d", float(d))   # +0x50
    def w_bool(self, b):        self.buf.append(1 if b else 0)       # +0x54
    def w_string(self, s: str):                                       # +0x58 (u32 len + bytes)
        raw = s.encode("utf-8")
        self.buf += struct.pack("<I", len(raw))
        self.buf += raw
    def w_u32(self, n: int):    self.buf += struct.pack("<I", n & 0xFFFFFFFF)

    # readers (mirror, for the round-trip)
    def r_tag(self) -> int:
        v = self.buf[self.pos]; self.pos += 1; return v
    def r_count(self) -> int:
        v = struct.unpack_from("<H", self.buf, self.pos)[0]; self.pos += 2; return v
    def r_double(self) -> float:
        v = struct.unpack_from("<d", self.buf, self.pos)[0]; self.pos += 8; return v
    def r_bool(self) -> bool:
        v = self.buf[self.pos]; self.pos += 1; return v != 0
    def r_string(self) -> str:
        n = struct.unpack_from("<I", self.buf, self.pos)[0]; self.pos += 4
        s = self.buf[self.pos:self.pos + n].decode("utf-8"); self.pos += n; return s
    def r_u32(self) -> int:
        v = struct.unpack_from("<I", self.buf, self.pos)[0]; self.pos += 4; return v

    def bytes(self) -> bytes:
        return bytes(self.buf)


class WireCodec:
    """Installs goLua_Pack/goLua_PackFixed/goLua_UnpackFixed into an M-LUA LuaHost, so the shipped
    Class.Pack serializes host objects to a WireBuffer in the faithful tag-variant format."""

    def __init__(self, lua_host):
        self.h = lua_host
        self.L = lua_host.L
        self._install()

    # map a lupa value to the goLua wire tag
    def _tag_of(self, value) -> int:
        t = self.L.eval("type")(value)
        if t == "nil":     return TAG_NIL
        if t == "boolean": return TAG_BOOL
        if t == "number":  return TAG_NUM
        if t == "string":  return TAG_STR
        if t == "table":
            # class instance (registered class) -> object(7); plain array -> table(5)
            cn = self.L.eval("function(o) local m=getmetatable(o); return m and m['.class_name'] end")(value)
            return TAG_OBJ if cn is not None else TAG_TABLE
        raise TypeError(f"goLua_PackFixed: unsupported value type {t!r}")

    def _classname(self, obj):
        return self.L.eval("function(o) local m=getmetatable(o); return m and m['.class_name'] end")(obj)

    # ---- the host natives (the shipped Lua calls these) ----
    def _goLua_Pack(self, source, key, value):
        # Class.Pack only uses goLua_Pack(source, "size", N) -> the field-count frame header.
        source.w_count(int(value))
        return None

    def _goLua_PackFixed(self, source, value):
        tag = self._tag_of(value)
        source.w_tag(tag)
        if tag == TAG_NIL:
            pass
        elif tag == TAG_BOOL:
            source.w_bool(value)
        elif tag == TAG_NUM:
            source.w_double(value)
        elif tag == TAG_STR:
            source.w_string(str(value))
        elif tag == TAG_TABLE:
            n = int(self.L.eval("function(t) return #t end")(value))
            source.w_count(n)
            for i in range(1, n + 1):
                self._goLua_PackFixed(source, value[i])
        elif tag == TAG_OBJ:
            source.w_string(str(self._classname(value)))
            self.pack_object_body(source, value)  # className + body (tag-0x07 char element)
        return None

    def _goLua_UnpackFixed(self, source, target, key):
        # mirror reader for the round-trip self-test (the real one is the C++ bef480; this is host)
        val = self.unpack_value(source)
        if target is not None and key is not None:
            target[key] = val
        return val

    def pack_value(self, source: WireBuffer, value) -> WireBuffer:
        """Public: tag-variant-encode a single value into `source` (used by M-REPL for RPC args)."""
        self._goLua_PackFixed(source, value)
        return source

    # tag-0x0b object REFERENCE (variant_codec.tag_0x0b: u32 descriptor_key + u32 object_id).
    # Refs aren't a Lua value type, so this is an explicit emitter — the faithful flow sends refs
    # in onLoadPlayers AFTER the objects are replicated (server/re/LIVE-TEST recipe "Corrected flow").
    NOBJECT_WRAPPER_KEY = 0x160FF5A6   # nid"Object" (the stage-1 wrapper key the ObjectPtr writer emits)

    def pack_ref(self, source: WireBuffer, object_id: int, descriptor_key: int | None = None) -> WireBuffer:
        source.w_tag(TAG_REF)
        source.w_u32(self.NOBJECT_WRAPPER_KEY if descriptor_key is None else descriptor_key)
        source.w_u32(int(object_id))
        return source

    def pack_object_body(self, source: WireBuffer, obj):
        """Tag-0x07 object body = the object's own packed NetVars (recurse via shipped Class.Pack)."""
        obj["Pack"](obj, source, True)

    # ---- host-side reader (round-trip only) ----
    def unpack_value(self, source: WireBuffer):
        tag = source.r_tag()
        if tag == TAG_NIL:   return None
        if tag == TAG_BOOL:  return source.r_bool()
        if tag == TAG_NUM:   return source.r_double()
        if tag == TAG_STR:   return source.r_string()
        if tag == TAG_TABLE:
            n = source.r_count()
            return [self.unpack_value(source) for _ in range(n)]
        if tag == TAG_OBJ:
            cn = source.r_string()
            return {"__class": cn, "__body": self.read_frame(source)}
        if tag == TAG_REF:
            key = source.r_u32(); oid = source.r_u32()
            return {"__ref": oid, "__key": key}
        raise ValueError(f"unpack: unknown tag 0x{tag:02x}")

    def read_frame(self, source: WireBuffer):
        """Read a Class.Pack frame: size header + that many variant values."""
        n = source.r_count()
        return [self.unpack_value(source) for _ in range(n)]

    def _install(self):
        g = self.h.g
        g["goLua_Pack"] = self._goLua_Pack
        g["goLua_PackFixed"] = self._goLua_PackFixed
        g["goLua_UnpackFixed"] = self._goLua_UnpackFixed

    # convenience: serialize a host object via the shipped Class.Pack
    def pack(self, obj, is_backup: bool = True) -> WireBuffer:
        buf = WireBuffer()
        obj["Pack"](obj, buf, is_backup)
        return buf


# --------------------------------------------------------------------------------------
def _selftest() -> int:
    import os, sys
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
    from engine.lua.runtime import LuaHost

    print("M-CODEC self-test: shipped Class.Pack -> tag-variant bytes -> round-trip")
    h = LuaHost()
    codec = WireCodec(h)
    # a class with sorted PackVars (NetVars): m_AccountId, m_Level (lexicographic)
    h.execute('Class("ProbeBase"); Class("ProbeChild","ProbeBase"):Def({ NetVars = { m_AccountId = 0, m_Level = 1 } })')
    inst = h.new("ProbeChild")
    inst["m_AccountId"] = 4242
    inst["m_Level"] = 7

    buf = codec.pack(inst, is_backup=True)        # shipped Class.Pack drives goLua_Pack/PackFixed
    raw = buf.bytes()
    frame = codec.read_frame(WireBuffer(raw))     # size header + values

    checks = {
        "emitted bytes":        len(raw) > 0,
        "frame field count==2": len(frame) == 2,
        "PackVars order (m_AccountId, m_Level)": frame == [4242.0, 7.0],
        "header byte 0 = count 2": raw[0] == 2 and raw[1] == 0,
        "first value tag = 0x03 (number)": raw[2] == TAG_NUM,
    }
    # all-tags round-trip (internal-consistency coverage of every wire tag)
    def rt(value):
        b = WireBuffer(); codec.pack_value(b, value)
        return codec.unpack_value(WireBuffer(b.bytes()))
    arr = h.eval("function() return {10, 20, 30} end")()
    nested = h.eval("function() return { {1,2}, {3} } end")()
    refbuf = WireBuffer(); codec.pack_ref(refbuf, 0x3EB)
    tagchecks = {
        "tag nil":    rt(None) is None,
        "tag bool":   rt(True) is True and rt(False) is False,
        "tag number": rt(3.5) == 3.5,
        "tag string": rt("hi") == "hi",
        "tag table":  rt(arr) == [10.0, 20.0, 30.0],
        "tag nested table": rt(nested) == [[1.0, 2.0], [3.0]],
        "tag ref (0x0b u32 key+oid)": codec.unpack_value(WireBuffer(refbuf.bytes())) == {"__ref": 0x3EB, "__key": codec.NOBJECT_WRAPPER_KEY},
    }
    checks.update(tagchecks)
    for k, v in checks.items():
        print(f"  [{'OK' if v else 'XX'}] {k}: {v}")
    print("  bytes:", raw.hex())
    ok = all(checks.values())
    print("RESULT:", "GREEN" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(_selftest())
