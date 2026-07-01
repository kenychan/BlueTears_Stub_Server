"""M-DB — host persistence + the shipped goEngine DB native (DBCommandExecute).

The faithful host is STATEFUL: accounts and their characters live in a CharStore (JSON on disk, or
in-memory), and the shipped server Lua reads them through the DB native it ACTUALLY calls --
  Res/tw_lua_decompiled/Class/Main/Session.lua:194
    DBCommandExecute(pidClient, "SP2_LoadPlayerData", {uid}, function(succ, result)
      local record = result[1] and result[1][1]
      if record then blob = record.xData end ...)
so SP2_LoadPlayerData returns a result-SET shape: result[1][1].xData = the player's serialized blob.

Two faithful data sources this store backs:
  * getByRelation(accountId, "TAccount", "CharacterManager") -> the account's char objectList
    (M-API HostContext._getByRelationImpl; a DB relation query on a real server). CharStore.get_chars.
  * DBCommandExecute(pid, "SP2_LoadPlayerData", {uid}, cb)    -> the per-char xData blob.
    CharStore.get_player_blob, dispatched by DBNatives.

CharStore = the persistence (plain Python + JSON). DBNatives = the Lua-facing native surface,
installed into an M-LUA LuaHost; it marshals store records into the nested Lua result tables the
shipped Lua indexes. The two are decoupled so the store has no Lua dependency.
"""
from __future__ import annotations

import json
import os
from typing import Callable, Optional


def _norm_blob(xdata) -> Optional[str]:
    """JSON-safe blob: bytes -> hex string, str kept, None kept. (xData is opaque to the shipped
    Lua -- it just hands it to gDataModule:ReplicatePlayerData -- so a hex string round-trips fine.)"""
    if xdata is None:
        return None
    if isinstance(xdata, (bytes, bytearray)):
        return bytes(xdata).hex()
    return str(xdata)


class CharStore:
    """Stateful account+character store. JSON-backed when `path` is set; in-memory otherwise.

    Schema: {version, next_oid, accounts: { "<id>": {account_id, chars: [char-record, ...]} }}
    char-record: {oid, uid, account_id, name, level, class_name, xData}.
    `uid` is the persistent character key SP2_LoadPlayerData is keyed by; `oid` is the runtime GWorld
    object id (defaults to uid). xData = the serialized player blob (hex string or None).
    """

    VERSION = 1
    BASE_OID = 0x3EB   # 1003 -- the pid the char-select lane has used throughout (CODEX status)

    def __init__(self, path: Optional[str] = None):
        self.path = path
        self.data = {"version": self.VERSION, "next_oid": self.BASE_OID, "accounts": {}}
        if path and os.path.exists(path):
            self.load()

    # ---- persistence ----
    def load(self) -> "CharStore":
        with open(self.path, "r", encoding="utf-8") as f:
            self.data = json.load(f)
        self.data.setdefault("version", self.VERSION)
        self.data.setdefault("next_oid", self.BASE_OID)
        self.data.setdefault("accounts", {})
        return self

    def save(self) -> "CharStore":
        if not self.path:
            return self
        d = os.path.dirname(os.path.abspath(self.path))
        if d:
            os.makedirs(d, exist_ok=True)
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(self.data, f, indent=2)
        os.replace(tmp, self.path)        # atomic replace -- never a half-written db
        return self

    # ---- oid allocation ----
    def allocate_oid(self) -> int:
        oid = int(self.data["next_oid"])
        self.data["next_oid"] = oid + 1
        return oid

    # ---- account / char CRUD ----
    def _acct(self, account_id) -> dict:
        key = str(int(account_id))
        acct = self.data["accounts"].get(key)
        if acct is None:
            acct = {"account_id": int(account_id), "chars": []}
            self.data["accounts"][key] = acct
        return acct

    def add_account(self, account_id) -> dict:
        return self._acct(account_id)

    def add_char(self, account_id, *, name: str, level: int = 1,
                 class_name: str = "TPlayerDummy", oid: Optional[int] = None,
                 uid: Optional[int] = None, xdata=None) -> dict:
        acct = self._acct(account_id)
        if oid is None:
            oid = self.allocate_oid()
        if uid is None:
            uid = oid
        rec = {"oid": int(oid), "uid": int(uid), "account_id": int(account_id),
               "name": str(name), "level": int(level), "class_name": str(class_name),
               "xData": _norm_blob(xdata)}
        acct["chars"].append(rec)
        return rec

    def get_chars(self, account_id) -> list:
        acct = self.data["accounts"].get(str(int(account_id)))
        return [dict(r) for r in acct["chars"]] if acct else []

    def find_char(self, uid) -> Optional[dict]:
        for acct in self.data["accounts"].values():
            for rec in acct["chars"]:
                if int(rec["uid"]) == int(uid):
                    return rec
        return None

    def get_player_blob(self, uid):
        rec = self.find_char(uid)
        return rec.get("xData") if rec else None

    def set_player_blob(self, uid, xdata) -> Optional[dict]:
        rec = self.find_char(uid)
        if rec is not None:
            rec["xData"] = _norm_blob(xdata)
        return rec


class DBNatives:
    """Installs the shipped DB native DBCommandExecute into an M-LUA LuaHost, dispatching stored
    procedures against a CharStore. The faithful SP the shipped Lua calls is SP2_LoadPlayerData;
    others get a default success+empty-result handler (the host doesn't model every SP)."""

    def __init__(self, lua_host, store: CharStore):
        self.h = lua_host
        self.L = lua_host.L
        self.store = store
        self.sps: dict[str, Callable] = {}
        self._register_default_sps()
        self._install()

    def register_sp(self, name: str, handler: Callable) -> None:
        self.sps[str(name)] = handler

    def _register_default_sps(self) -> None:
        self.register_sp("SP2_LoadPlayerData", self._sp_load_player_data)

    # SP2_LoadPlayerData({uid}) -> result-set { { { xData = blob } } } so result[1][1].xData resolves.
    def _sp_load_player_data(self, args):
        uid = self._first_arg(args)
        if uid is None:
            return []
        blob = self.store.get_player_blob(uid)
        if blob is None:
            return []                       # no rows -> result[1] nil -> blob nil (shipped handles it)
        return [[{"xData": blob}]]

    @staticmethod
    def _first_arg(args):
        """args is the Lua array table {uid} (1-based) -- or a python list when called directly."""
        if args is None:
            return None
        try:
            v = args[1]                     # Lua 1-based
            if v is not None:
                return v
        except Exception:
            pass
        try:
            return args[0]
        except Exception:
            return None

    # ---- the Lua-facing native ----
    def _db_command_execute(self, pid, sp_name, args, callback):
        handler = self.sps.get(str(sp_name))
        if handler is None:
            succ, result = True, []          # unknown SP: faithful default (host doesn't model it)
        else:
            try:
                result, succ = handler(args), True
            except Exception:
                result, succ = [], False
        lua_result = self._to_lua(result)
        if callback is not None:
            callback(succ, lua_result)
        return None

    def db_execute(self, sp_name: str, args=None):
        """Direct (synchronous) host entry -- returns the marshalled result-set (the gMaster:DBExecute
        shape). Lua-side gMaster binding is future; DBCommandExecute is the path the shipped Lua uses."""
        handler = self.sps.get(str(sp_name))
        return self._to_lua(handler(args) if handler else [])

    def _to_lua(self, value):
        if isinstance(value, dict):
            return self.L.table_from({k: self._to_lua(v) for k, v in value.items()})
        if isinstance(value, (list, tuple)):
            return self.L.table_from([self._to_lua(v) for v in value])
        return value

    def _install(self) -> None:
        self.h.g["DBCommandExecute"] = self._db_command_execute


# --------------------------------------------------------------------------------------
# Self-test: persist accounts+chars, save to disk, reload into a fresh store, then drive the
# shipped-style DBCommandExecute("SP2_LoadPlayerData", {uid}) through the live Lua VM exactly as
# Session.lua:194 does, and assert the blob round-trips. Plus persistence + unknown-SP coverage.
# --------------------------------------------------------------------------------------
def _selftest() -> int:
    import sys
    import tempfile
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
    from engine.lua.runtime import LuaHost

    print("M-DB self-test: CharStore persistence + shipped DBCommandExecute(SP2_LoadPlayerData)")

    tmpdir = tempfile.mkdtemp(prefix="mdb_")
    dbpath = os.path.join(tmpdir, "host_db.json")

    # 1) build + persist a store
    s1 = CharStore(dbpath)
    s1.add_account(1001)
    c1 = s1.add_char(1001, name="DemoWizard", level=7, oid=0x3EB, xdata=b"\x02\x00\x03blobA")
    c2 = s1.add_char(1001, name="DemoKnight", level=3, oid=0x3EC, xdata=b"\x02\x00\x03blobB")
    s1.save()

    # 2) reload from disk into a FRESH store (proves on-disk persistence)
    s2 = CharStore(dbpath)
    chars = s2.get_chars(1001)

    # 3) drive DBCommandExecute through the live Lua VM, mirroring Session.lua:194's usage
    h = LuaHost()
    DBNatives(h, s2)
    request = h.eval(r"""
        function(uid)
          local out = {}
          DBCommandExecute(0, "SP2_LoadPlayerData", {uid}, function(succ, result)
            out.succ = succ
            local record = result[1] and result[1][1]
            out.blob = record and record.xData
          end)
          return out.succ, out.blob
        end
    """)
    succ1, blob1 = request(0x3EB)            # DemoWizard's uid
    succ_missing, blob_missing = request(0x999)   # not in store -> empty result-set -> nil blob

    # unknown SP -> default success + empty result (host doesn't crash on an unmodeled SP)
    unk = h.eval(r"""
        function()
          local got = {}
          DBCommandExecute(0, "SP_Unmodeled", {1}, function(succ, result)
            got.succ = succ; got.n = #result
          end)
          return got.succ, got.n
        end
    """)()
    unk_succ, unk_n = unk

    # 4) blob mutation persists
    s2.set_player_blob(0x3EB, b"\xffNEW")
    blob_after = s2.get_player_blob(0x3EB)

    checks = {
        "store saved to disk":              os.path.exists(dbpath),
        "reload: 2 chars":                  len(chars) == 2,
        "reload: oids preserved":           [c["oid"] for c in chars] == [0x3EB, 0x3EC],
        "reload: names preserved":          [c["name"] for c in chars] == ["DemoWizard", "DemoKnight"],
        "next_oid advanced past chars":     s2.data["next_oid"] >= 0x3EB,
        "DBCommandExecute succ":            succ1 is True,
        "SP2_LoadPlayerData blob round-trips (result[1][1].xData)": blob1 == b"\x02\x00\x03blobA".hex(),
        "missing uid -> nil blob (empty result-set)": blob_missing is None and succ_missing is True,
        "unknown SP -> default success+empty": unk_succ is True and unk_n == 0,
        "set_player_blob mutates record":   blob_after == b"\xffNEW".hex(),
    }
    for k, v in checks.items():
        print(f"  [{'OK' if v else 'XX'}] {k}: {v}")

    # cleanup the temp db
    try:
        os.remove(dbpath)
        os.rmdir(tmpdir)
    except OSError:
        pass

    ok = all(checks.values())
    print("RESULT:", "GREEN" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(_selftest())
