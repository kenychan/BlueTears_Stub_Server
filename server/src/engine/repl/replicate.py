"""M-REPL replicator — serialize a host objectList into the FULL-OBJECT replication payload.

Produces a tag-0x05 array of tag-0x07 objects, each = className + the object's Class.Pack body
(server/re/M-CODEC-wire-format.md). This is the **general object-replication format** the engine uses
for BackupReplicate / the master-construct lane (the full char object crossing the wire so the client
constructs it in its GWorld + registry).

⚠ This is NOT the onLoadPlayers char-LIST element. Per LATE-19/20 (Res/gss_stub_server_v4.py:1736) the
onLoadPlayers char element is className-ONLY (a body tail derails the +0x1b parse), and that arg
already walls at the 005e5650 registry (LATE-22). The faithful flow replicates the char OBJECTS first
(this full-body Pack) and then sends onLoadPlayers REFERENCES that resolve. See
server/re/LIVE-TEST-onloadplayers-recipe.md "Corrected flow". Chain: M-LUA -> M-API (LoadPlayers) ->
M-REPL (this, full-object replication) -> [transport] -> client construct.
"""
from __future__ import annotations

import sys, os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
from engine.codec.wire import WireBuffer, WireCodec, TAG_TABLE, TAG_OBJ  # noqa: E402


class Replicator:
    def __init__(self, lua_host, codec: WireCodec):
        self.h = lua_host
        self.L = lua_host.L
        self.codec = codec

    def pack_objectlist(self, char_objs) -> WireBuffer:
        """tag-0x05 array of tag-0x07 char objects. `char_objs` = a Python list of live char class
        instances, or a Lua array table (e.g. the objectList M-API getByRelation produced)."""
        if isinstance(char_objs, (list, tuple)):
            arr = self.L.table_from(list(char_objs))
        else:
            arr = char_objs  # already a Lua array
        buf = WireBuffer()
        self.codec.pack_value(buf, arr)   # -> tag-05[count][tag-07 per element]
        return buf

    def onloadplayers_payload(self, char_objs) -> bytes:
        """The objectList arg bytes to inject into the onLoadPlayersCacheResult DownCall."""
        return self.pack_objectlist(char_objs).bytes()

    def onloadplayers_refs(self, oids) -> bytes:
        """The CORRECTED-flow onLoadPlayers payload: a tag-0x05 array of tag-0x0b object REFERENCES
        (sent AFTER the char objects have been replicated, so the refs resolve client-side). See
        server/re/LIVE-TEST-onloadplayers-recipe.md "Corrected flow"."""
        buf = WireBuffer()
        buf.w_tag(TAG_TABLE); buf.w_count(len(oids))
        for oid in oids:
            self.codec.pack_ref(buf, int(oid))
        return buf.bytes()

    def replicate_loadplayers(self, host_ctx, account_id: int) -> bytes:
        """Full chain: run the shipped TPlayerCache:LoadPlayers via M-API to get the live objectList,
        then serialize it. Requires host_ctx.singletons['TPlayerCache'] + an account with char
        class-instances. Returns the faithful objectList payload bytes for the transport."""
        captured = {}
        session = host_ctx._newtab()
        session["onLoadPlayersCacheResult"] = lambda _self, _from, objlist: captured.__setitem__("ol", objlist)
        tpc = host_ctx.singletons["TPlayerCache"]
        tpc["LoadPlayers"](tpc, 0, account_id, session)
        return self.onloadplayers_payload(captured["ol"])


# --------------------------------------------------------------------------------------
def _selftest() -> int:
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
    from engine.lua.runtime import LuaHost
    from engine.codec.wire import WireBuffer as WB

    print("M-REPL self-test: host objectList -> onLoadPlayersCacheResult payload (tag-05 of tag-07)")
    h = LuaHost()
    codec = WireCodec(h)
    repl = Replicator(h, codec)

    # minimal char CLASS instances (className drives the tag-0x07 identity the client reads).
    # Synthetic name (the real "TPlayerDummy" collides with the template file); stands in for the
    # real Template/Com char until the package-boot char build lands -- the wire SHAPE is identical.
    h.execute('''
        Class("ReplCharBase")
        Class("ReplChar", "ReplCharBase"):Def({ NetVars = { m_AccountId = 0, m_Level = 1 } })
    ''')
    chars = []
    for acct, lvl in ((1001, 7), (1001, 3)):
        c = h.new("ReplChar"); c["m_AccountId"] = acct; c["m_Level"] = lvl
        chars.append(c)

    payload = repl.onloadplayers_payload(chars)

    # decode the payload to verify shape
    buf = WB(payload)
    decoded = codec.unpack_value(buf)   # -> list of {"__class","__body"}

    checks = {
        "payload emitted":          len(payload) > 0,
        "outer is array (tag-05)":  payload[0] == TAG_TABLE,
        "array count == 2":         isinstance(decoded, list) and len(decoded) == 2,
        "elem0 is object (tag-07)": decoded and decoded[0].get("__class") == "ReplChar",
        "elem className faithful":  all(e.get("__class") == "ReplChar" for e in decoded),
        "elem body carries NetVars (m_AccountId, m_Level)": decoded and decoded[0]["__body"] == [1001.0, 7.0],
    }
    # corrected-flow refs payload (tag-05 array of tag-0b refs)
    refs = repl.onloadplayers_refs([0x3EB, 0x3EC])
    refs_decoded = codec.unpack_value(WireBuffer(refs))
    checks["refs payload (array of 2 tag-0b refs)"] = (
        isinstance(refs_decoded, list) and len(refs_decoded) == 2
        and refs_decoded[0].get("__ref") == 0x3EB
    )
    for k, v in checks.items():
        print(f"  [{'OK' if v else 'XX'}] {k}: {v}")
    print("  payload bytes:", payload.hex())
    ok = all(checks.values())
    print("RESULT:", "GREEN" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(_selftest())
