"""Bridge — produce the FULL-OBJECT replication payload for a char (className + Class.Pack body).

⚠ CORRECTION (LATE-41): this is NOT a drop-in for the onLoadPlayers char-list arg. That arg's element
is className-ONLY (Res/gss_stub_server_v4.py:1736; a body tail derails the +0x1b parse, LATE-19/20),
and it already walls at the 005e5650 registry (LATE-22). This bridge emits the full-object
replication format (tag-0x05 array of tag-0x07 className+body), which is the format for the
BackupReplicate / master-construct lane -- the faithful flow that replicates char OBJECTS to the
client (so it constructs them) BEFORE onLoadPlayers sends resolving REFERENCES. See
server/re/LIVE-TEST-onloadplayers-recipe.md "Corrected flow" + D1 (re/D1-c35140-dispatch.md) for the
lane question. Use this payload to drive the replication-construct experiment, not the onLoadPlayers
element swap.

NOTE: until the Template/Com package boot lands (LATE-38), the char here is a minimal class instance
with the correct className + NetVars -- enough to exercise the codec/replication format; the full
component set is the follow-on.
"""
from __future__ import annotations

import os
import sys

_SRC = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, _SRC)

from engine.lua.runtime import LuaHost          # noqa: E402
from engine.codec.wire import WireCodec          # noqa: E402
from engine.repl.replicate import Replicator     # noqa: E402


def _build_host():
    h = LuaHost()
    codec = WireCodec(h)
    return h, codec, Replicator(h, codec)


def faithful_onloadplayers_payload(chars, classname: str = "TPlayerDummy") -> bytes:
    """chars = list of (oid, account_id[, level]). Returns the faithful objectList payload bytes.

    Builds a char CLASS instance per entry (className = `classname`, default "TPlayerDummy" so the
    client's type-registry lookup matches), with m_AccountId/m_Level NetVars, and serializes the
    objectList via M-REPL. Defines the class directly (not via new()'s file resolver) to avoid the
    real TPlayerDummy template-file collision."""
    h, codec, repl = _build_host()
    # define a class genuinely named `classname` without triggering the template-file load
    h.execute('Class("HostCharBase")')
    h.execute(f'Class("{classname}", "HostCharBase"):Def({{ NetVars = {{ m_AccountId = 0, m_Level = 1 }} }})')
    cls = h.g[classname]
    objs = []
    for entry in chars:
        oid, acct = entry[0], entry[1]
        lvl = entry[2] if len(entry) > 2 else 1
        inst = cls["new"](cls)           # instantiate via the class table (no _loadfile)
        inst["m_AccountId"] = acct
        inst["m_Level"] = lvl
        objs.append(inst)
    return repl.onloadplayers_payload(objs)


def faithful_replicate_first_sequence(char_specs, account_id: int = 1001,
                                      classname: str = "TPlayerDummy"):
    """The TRUTHFUL replicate-first flow (LATE-129, recipe "Corrected flow"): for each char run the
    SHIPPED CharacterManager:serverCreateCharacterResult -> BackupReplicate (full-object M-CODEC bytes),
    then build the onLoadPlayers REFERENCE payload that resolves them. The faithful host (server/src)
    produces every byte from the shipped server Lua; the stub ships them via the ("raw", bytes) hook,
    pure transport. char_specs = [(oid, level, name), ...].
    Returns {"replication": [(oid, raw_bytes), ...], "onload_refs": raw_bytes}.
    """
    from host import FaithfulHost            # server/src/host.py  (sys.path has _SRC)
    h = FaithfulHost(char_classname=classname)
    h.setup_account(account_id, [(int(o), lvl, nm) for (o, lvl, nm) in char_specs])
    replication = []
    for (oid, _lvl, _nm) in char_specs:
        h.replicated.clear()
        h.create_character_result(int(oid), info=None)     # shipped Lua -> BackupReplicate
        got = [b for (o, b) in h.replicated if o == int(oid)]
        replication.append((int(oid), got[0] if got else b""))
    oids = [int(oid) for (oid, _l, _n) in char_specs]
    onload_refs = h.repl.onloadplayers_refs(oids)
    return {"replication": replication, "onload_refs": onload_refs}


# --------------------------------------------------------------------------------------
def _selftest() -> int:
    from engine.codec.wire import WireBuffer, WireCodec as _WC

    print("transport-bridge self-test: faithful onLoadPlayers payload for a 'TPlayerDummy' char")
    payload = faithful_onloadplayers_payload([(0x3EB, 1001, 7), (0x3EC, 1001, 3)])

    # decode to verify the wire shape the stub will inject
    h = LuaHost(); codec = _WC(h)
    decoded = codec.unpack_value(WireBuffer(payload))

    checks = {
        "payload emitted":            len(payload) > 0,
        "array of 2 objects":         isinstance(decoded, list) and len(decoded) == 2,
        "className == 'TPlayerDummy'": all(e.get("__class") == "TPlayerDummy" for e in decoded),
        "char0 body (m_AccountId,m_Level)": decoded and decoded[0]["__body"] == [1001.0, 7.0],
    }
    for k, v in checks.items():
        print(f"  [{'OK' if v else 'XX'}] {k}: {v}")
    print("  payload bytes:", payload.hex())
    ok = all(checks.values())
    print("RESULT:", "GREEN" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(_selftest())
