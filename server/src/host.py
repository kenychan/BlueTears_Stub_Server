"""FaithfulHost — the composed host entry point (M-LUA + M-API + M-CODEC + M-REPL).

Wires the whole stack into one object and runs the real char-load → replication chain:
  setup_account(id, chars) -> build host char objects + a CharacterManager tree
  load_players(id)         -> run the shipped TPlayerCache:LoadPlayers -> live objectList
  replicate(objectList)    -> faithful replication bytes (M-REPL)

This is the integration layer the per-module self-tests don't cover; the __main__ demo runs the
full chain end-to-end and prints the bytes a transport would carry. (Live delivery to the client is
the UAC step; see server/re/LIVE-TEST-onloadplayers-recipe.md.)
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

from engine.lua.runtime import LuaHost            # noqa: E402
from engine.api.host_context import HostContext   # noqa: E402
from engine.codec.wire import WireCodec, WireBuffer  # noqa: E402
from engine.repl.replicate import Replicator      # noqa: E402
from engine.db.store import CharStore, DBNatives  # noqa: E402
# per-Lua-object host operations (mirror ghidraRE/output/flowmodels/client/<object>.md)
from char import build as _charbuild              # noqa: E402
from account import account as _account           # noqa: E402
from charactermanager import roster as _roster    # noqa: E402


class FaithfulHost:
    def __init__(self, char_classname: str = "TPlayerDummy", db: CharStore | None = None):
        self.lua = LuaHost()
        self.ctx = HostContext(self.lua)        # GetEnvironment/GWorld/getByRelation + construct natives
        self.codec = WireCodec(self.lua)         # goLua_Pack/PackFixed/UnpackFixed
        self.repl = Replicator(self.lua, self.codec)
        # M-DB: optional persistence. When attached, accounts/chars survive across host instances and
        # the shipped DBCommandExecute("SP2_LoadPlayerData") native resolves player blobs from it.
        self.db = db
        self.dbn = DBNatives(self.lua, db) if db is not None else None
        self.tpc = self.ctx.make_cache("TPlayerCache")
        # CharacterManager (the shipped char create/load logic) instantiated on the host
        self.lua.load_class("CharacterManager")
        self.cm = self.lua.new("CharacterManager")
        self.replicated: list = []      # (oid, bytes) captured from BackupReplicate
        self._classname = char_classname
        # define the char + Player-component classes once (directly, avoiding new()'s file resolver).
        # The char carries identity NetVars (tag-0x07 body) AND a Player component, because the shipped
        # TPlayerCache:LoadPlayers does `v.Player.m_AccountId = accountId`.
        self.lua.execute('''
            if GetClass("HostCharBase") == nil then Class("HostCharBase") end
            if GetClass("HostPlayerComp") == nil then
              Class("HostPlayerComp", "HostCharBase"):Def({ NetVars = { m_AccountId = 0 } })
            end
        ''')
        self.lua.execute(
            f'if GetClass("{char_classname}") == nil then '
            f'Class("{char_classname}", "HostCharBase"):Def({{ NetVars = {{ m_Level = 1, m_Name = "" }} }}) end'
        )

    # The per-object HOST OPERATIONS live in server/src/{char,account,charactermanager}/ (mirroring
    # the client flowmodel tree); FaithfulHost is the composition root and delegates to them. The
    # substrate (engine/*) does the VM/codec/repl/db work; these modules run the shipped Lua RPCs.

    # --- char tier (char/build.py) ---
    def _new_char(self, oid: int, account_id: int, level: int = 1, name: str = ""):
        return _charbuild.new_char(self, oid, account_id, level, name)

    def _backup_replicate(self, oid: int, obj):
        return _charbuild.backup_replicate(self, oid, obj)

    # --- account tier (account/account.py) ---
    def setup_account(self, account_id: int, char_specs: list[tuple]):
        return _account.setup_account(self, account_id, char_specs)

    def load_account(self, account_id: int):
        return _account.load_account(self, account_id)

    # --- CharacterManager / TPlayerCache roster tier (charactermanager/roster.py) ---
    def create_character_result(self, oid: int, info=None):
        return _roster.create_character_result(self, oid, info)

    def load_players(self, account_id: int):
        return _roster.load_players(self, account_id)

    def replicate(self, objectlist) -> bytes:
        return _roster.replicate(self, objectlist)

    def char_load_to_wire(self, account_id: int) -> bytes:
        return _roster.char_load_to_wire(self, account_id)


# --------------------------------------------------------------------------------------
def _demo() -> int:
    print("FaithfulHost end-to-end demo: account -> LoadPlayers (shipped Lua) -> faithful wire bytes")
    host = FaithfulHost()
    ACC = 1001
    host.setup_account(ACC, [(0x3EB, 7, "DemoWizard"), (0x3EC, 3, "DemoKnight")])

    # 1) the shipped server RPC produces the live objectList
    objlist = host.load_players(ACC)
    n = int(host.lua.eval("function(t) return #t end")(objlist))
    names = [objlist[i]["m_Name"] for i in range(1, n + 1)]
    accts = [objlist[i]["Player"]["m_AccountId"] for i in range(1, n + 1)]   # set by shipped Lua

    # 2) M-REPL serializes it to faithful wire bytes
    payload = host.char_load_to_wire(ACC)
    decoded = host.codec.unpack_value(WireBuffer(payload))

    checks = {
        "LoadPlayers objectList count == 2": n == 2,
        "names round-trip":                  names == ["DemoWizard", "DemoKnight"],
        "m_AccountId set by shipped Lua":     accts == [ACC, ACC],
        "wire: array of 2 tag-07 objects":    isinstance(decoded, list) and len(decoded) == 2,
        "wire className == 'TPlayerDummy'":   all(e.get("__class") == "TPlayerDummy" for e in decoded),
        "GWorld:FindObject resolves char":    bool(host.lua.eval("function(o) return GWorld:FindObject(o) ~= nil end")(0x3EB)),
    }
    # 3) char-CREATE RPC: run the shipped serverCreateCharacterResult -> BackupReplicate -> M-REPL
    host.create_character_result(0x3EB, info=None)   # shipped CharacterManager logic
    rep = [b for (o, b) in host.replicated if o == 0x3EB]

    checks.update({
        "serverCreateCharacterResult ran (BackupReplicate fired)": len(rep) == 1,
        "BackupReplicate produced tag-07 bytes": bool(rep) and rep[0][0] == 0x07,
    })

    # 4) M-DB stateful round-trip: persist via one host, reload into a FRESH host from the same db,
    #    then run the shipped LoadPlayers -- chars survive without setup_account.
    import tempfile
    dbpath = os.path.join(tempfile.mkdtemp(prefix="host_demo_"), "db.json")
    h1 = FaithfulHost(db=CharStore(dbpath))
    h1.setup_account(2002, [(0x401, 9, "PersistMage"), (0x402, 5, "PersistRogue")])
    h2 = FaithfulHost(db=CharStore(dbpath))           # fresh host, same db file
    reloaded = h2.load_account(2002)
    ol2 = h2.load_players(2002)
    n2 = int(h2.lua.eval("function(t) return #t end")(ol2))
    blob = h2.dbn.db_execute("SP2_LoadPlayerData", [0x401])     # shipped-native player blob lookup
    blob_present = bool(h2.lua.eval("function(r) return r[1] and r[1][1] and r[1][1].xData ~= nil end")(blob))
    checks.update({
        "M-DB persist+reload: 2 chars from disk": len(reloaded) == 2,
        "M-DB reloaded host LoadPlayers count==2": n2 == 2,
        "M-DB SP2_LoadPlayerData returns a blob":  blob_present,
    })
    try:
        os.remove(dbpath); os.rmdir(os.path.dirname(dbpath))
    except OSError:
        pass

    for k, v in checks.items():
        print(f"  [{'OK' if v else 'XX'}] {k}: {v}")
    print(f"  faithful payload ({len(payload)} bytes): {payload.hex()}")
    if rep:
        print(f"  BackupReplicate bytes: {rep[0].hex()}")
    ok = all(checks.values())
    print("RESULT:", "GREEN" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(_demo())
