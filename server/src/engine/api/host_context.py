"""M-API host context — supplies the bounded goEngine domain natives over live Lua objects.

Proven end-to-end (2026-06-28 LATE-37): the REAL shipped `TPlayerCache:LoadPlayers` server-RPC
runs on the host with these natives wired:
  GetEnvironment():GetNObject("TPlayerCache")   -> host singleton registry
  tpc:getByRelation(...) [shipped ObjectCache]  -> Callback() [shipped] -> getByRelationImpl (here)
  getByRelationImpl -> cb:onLuaCompleteByRelation(true, objectList) [shipped Callback]
  -> the Lua callback sets v.Player.m_AccountId, calls session:onLoadPlayersCacheResult.

DESIGN: the Lua table IS the object (M-LUA runs the shipped class system over plain tables), so
this context is the GWorld registry/index over those live tables — NOT a parallel object store.
(server/src/obj/object_model.py's dataclass store predates this and should be refactored into, or
replaced by, this registry; tracked as the next M-OBJ step.)

SCOPE NOTE: getByRelationImpl currently serves the TAccount->CharacterManager relation (the only
one LoadPlayers needs). make_char builds a test-fixture char (a Lua object carrying a Player
component); faithful char construction via the Template/Com system + CharacterManager will replace
it in the char-CREATE path.
"""
from __future__ import annotations

import re
from typing import Optional


class HostContext:
    """Owns the host's NObject singletons + GWorld registry + account/relation DB, and installs
    the goEngine domain natives into an M-LUA `LuaHost`."""

    def __init__(self, lua_host):
        self.h = lua_host
        self.L = lua_host.L
        self.singletons: dict[str, object] = {}      # GetNObject(name) -> live Lua object
        self.gworld: dict[int, object] = {}          # oid -> live Lua object (GWorld:FindObject)
        self.accounts: dict[int, list] = {}          # accountId -> CharacterManager children
        self._install_natives()

    # ---- small helpers over the Lua VM ----
    def _newtab(self):
        return self.L.eval("function() return {} end")()

    # ---- native surface installed into the shipped-Lua global env ----
    def _install_natives(self) -> None:
        g = self.h.g
        env = self._newtab()
        # env:GetNObject(name) -> the registered singleton (called with env as self)
        env["GetNObject"] = lambda _self, name: self.singletons.get(str(name))
        self._env = env
        g["GetEnvironment"] = lambda: env
        # GWorld is a global singleton table with :FindObject(oid) / :CreateObject(...) methods.
        gworld_tab = self._newtab()
        gworld_tab["FindObject"] = lambda _self, oid: self.gworld.get(int(oid))
        g["GWorld"] = gworld_tab
        self._gworld_tab = gworld_tab
        self._install_construct_natives()

    # ---- object-construction natives (the Template/Com DefaultImpl path, Lib/Template.lua) ----
    def _install_construct_natives(self) -> None:
        """goLua_CreateObject + obj:AddComponent/GetBaseComponent/RemoveComponent + goLua_RegistObject,
        so the shipped Template():Def builds a component-bearing object on the host. Components are
        stored as fields by class name (obj.Player = the Player component); parent template components
        are inherited (the engine clones the parent prototype)."""
        g = self.h.g
        self.templates: dict[str, object] = {}   # GoObjectTemplates mirror (name -> built object)

        def clsname(com):
            return self.L.eval("function(o) local m=getmetatable(o); return m and m['.class_name'] end")(com)

        def create_object(name, parent=None):
            obj = self._newtab(); obj["m_Name"] = str(name); obj["__coms"] = self._newtab()
            if parent is not None and str(parent) in self.templates:
                for k, v in list(self.templates[str(parent)]["__coms"].items()):
                    obj["__coms"][k] = v; obj[k] = v          # inherit parent template components
            def add_component(_self, com):
                nm = clsname(com) or "?"; obj["__coms"][str(nm)] = com; obj[str(nm)] = com
            obj["AddComponent"] = add_component
            obj["GetBaseComponent"] = lambda _self, com: (obj["__coms"][str(clsname(com))] if clsname(com) else None)
            obj["RemoveComponent"] = lambda _self, com: None
            obj["GetCom"] = lambda _self, nm: obj["__coms"][str(nm)]
            return obj

        g["goLua_CreateObject"] = lambda n, p=None: create_object(n, p)
        g["goLua_RegistObject"] = lambda obj, parent=None: self.templates.__setitem__(str(obj["m_Name"]), obj)
        # Gamebryo math constructors the shipped data/components reference at define time.
        g["NiPoint3"] = lambda x=0, y=0, z=0: self.L.table_from([x, y, z])
        g["NiPoint2"] = lambda x=0, y=0: self.L.table_from([x, y])
        g["NiColorA"] = lambda r=0, g_=0, b=0, a=1: self.L.table_from([r, g_, b, a])
        g["NiColor"] = lambda r=0, g_=0, b=0: self.L.table_from([r, g_, b])
        self._install_engine_natives()

    # ---- bounded goEngine util natives the shipped component/data Lua calls at load time ----
    def _install_engine_natives(self) -> None:
        """Faithful util surface (server-side) so the REAL component classes + their data files load:
        the shipped util libs that DEFINE globals (Table.lua=MakeTable, I18NManager.lua=_I2), plus the
        engine-provided natives the data files call at define time (NUtil.tableEx table utils, GetLocale,
        RoomID). Authentic semantics, not no-op stubs: tableEx mirrors the engine's invert/size/indices;
        GetLocale returns a stable host locale id; RoomID maps a map/room name to its id handle."""
        g = self.h.g
        # GetLocale(): the client locale key. This is the Taiwan (zh_tw) client — the shipped data
        # tables (ItemData.CashItemShopTabInfo[...], PlayerInitData CharacterDeletionPolicyRaw[...]) are
        # keyed by this exact string, and the Player data lives under Data/zh_tw/.
        g["GetLocale"] = lambda *a: "zh_tw"
        # RoomID(name): map/room name -> room-id handle (identity handle is faithful for load-time data).
        g["RoomID"] = lambda name=None, *a: name
        # GameLogBlock(tag, fn): the engine wraps a method with profiling/log; faithfully returns fn.
        g["GameLogBlock"] = lambda tag=None, fn=None, *a: fn
        # NUtil.tableEx: the engine's table utilities, implemented faithfully in Lua.
        self.h.execute('''
            NUtil = NUtil or {}
            NUtil.tableEx = NUtil.tableEx or {
              invert  = function(t) local r = {}; for k, v in pairs(t) do r[v] = k end; return r end,
              size    = function(t) local n = 0; for _ in pairs(t) do n = n + 1 end; return n end,
              indices = function(t) local r = {}; for k in pairs(t) do r[#r + 1] = k end; return r end,
            }
        ''')
        # shipped util libs that DEFINE the global helpers MakeTable / _I2 (load once; tolerate re-load).
        for lib in ("Table", "I18NManager"):
            try:
                self.h.load_class(lib)
            except Exception:
                pass

    def build_template_object(self, template_name: str):
        """Run the shipped Template():Def for `template_name` and return the built component-bearing
        object (the char prototype). Requires its component classes to be pre-loaded (CreateCom is
        GetClass-based). For the full real TPlayerDummy that means the package boot (sizable; see
        CODEX_LATEST_STATUS LATE-42); synthetic/identity component sets work directly."""
        self.h.load_class("Template")          # ensure Lib/Template.lua (also in core)
        self.h.load_class(template_name)        # runs Template():Def -> goLua_RegistObject
        return self.templates.get(template_name)

    def load_template_package(self, template_name: str):
        """Faithfully boot a SHIPPED template (e.g. the real `TPlayerDummy`): walk its file hierarchy
        (`TPlayerDummy` -> `TActor` -> `TNetObject`), collect every `Com("X")` component class across
        the chain, pre-load each (the shipped `CreateCom` is `GetClass`-based), THEN run the shipped
        `Template():Def` to build the full component-bearing object. This is the package boot the real
        char model needs — no synthetic stand-ins."""
        coms: list[str] = []
        name: Optional[str] = template_name
        visited: set[str] = set()
        while name and name not in visited:
            visited.add(name)
            path = self.h._resolve(name)
            if not path:
                break
            src = open(path, "r", encoding="utf-8", errors="replace").read()
            for cm in re.findall(r'Com\(\s*"([A-Za-z0-9_]+)"\s*\)', src):
                if cm not in coms:
                    coms.append(cm)
            m = re.search(r'Template\(\s*"' + re.escape(name) + r'"\s*,\s*(?:"([^"]+)"|nil)', src)
            name = m.group(1) if (m and m.group(1)) else None
        for cm in coms:
            try:
                self.h.load_class(cm)
            except Exception as e:               # surface which component class blocks the boot
                raise RuntimeError(f"component class {cm!r} failed to load: {e}") from e
        return self.build_template_object(template_name)

    # ---- the DB relation query behind ObjectCache:getByRelation ----
    def _getByRelationImpl(self, _cache, _from, uid, _template, _component, cb):
        """Native: resolve uid's TAccount->CharacterManager children, fire the shipped Callback."""
        chars = self.accounts.get(int(uid), [])
        objlist = self.L.table_from(chars)               # 1-based Lua sequence
        cb["onLuaCompleteByRelation"](cb, True, objlist)  # shipped Callback -> the Lua func
        return None

    # ---- registration API used by the host wiring / tests ----
    def register_singleton(self, name: str, obj) -> None:
        self.singletons[name] = obj

    def make_cache(self, classname: str = "TPlayerCache"):
        """Instantiate a shipped ObjectCache subclass, attach the native getByRelationImpl, and
        register it as the named NObject singleton."""
        self.h.load_class(classname)     # pulls ObjectCache -> NObject
        self.h.load_class("Callback")    # ObjectCache:getByRelation does `Callback()` at runtime
        cache = self.h.new(classname)
        cache["getByRelationImpl"] = self._getByRelationImpl
        self.register_singleton(classname, cache)
        return cache

    def make_char(self, name: str, oid: int, **player_fields):
        """Build a host char object (a Lua table with a Player component) and register it in GWorld.
        Test fixture for the char-LOAD path; the char-CREATE path will build these via Template/Com."""
        t = self._newtab()
        player = self._newtab()
        player["m_AccountId"] = 0
        player["m_Name"] = name
        for k, v in player_fields.items():
            player[k] = v
        t["Player"] = player
        t["__oid"] = oid
        self.gworld[int(oid)] = t
        return t

    def add_account(self, account_id: int, chars: list) -> None:
        self.accounts[int(account_id)] = list(chars)


# --------------------------------------------------------------------------------------
# Self-test: run the REAL shipped TPlayerCache:LoadPlayers end-to-end on the host.
# --------------------------------------------------------------------------------------
def _selftest() -> int:
    import os, sys
    sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
    from engine.lua.runtime import LuaHost

    print("M-API host-context self-test: shipped TPlayerCache:LoadPlayers")
    h = LuaHost()
    ctx = HostContext(h)
    tpc = ctx.make_cache("TPlayerCache")

    ACC = 1001
    ctx.add_account(ACC, [ctx.make_char("Alice", 0x3EB), ctx.make_char("Bob", 0x3EC)])

    # test session capturing the result RPC
    captured = {}
    session = ctx._newtab()
    def on_result(_self, _from, objectList):
        n = int(h.eval("function(t) return #t end")(objectList))
        captured["count"] = n
        captured["names"] = [objectList[i]["Player"]["m_Name"] for i in range(1, n + 1)]
    session["onLoadPlayersCacheResult"] = on_result

    tpc["LoadPlayers"](tpc, 0, ACC, session)   # <-- shipped server Lua

    # GWorld:FindObject from Lua (the native serverCreateCharacterResult uses) resolves the char.
    found = h.eval("function(oid) local o = GWorld:FindObject(oid); return o and o.Player.m_Name end")(0x3EB)
    checks = {
        "result RPC reached session": "count" in captured,
        "objectList count == 2":      captured.get("count") == 2,
        "names round-trip":           captured.get("names") == ["Alice", "Bob"],
        "m_AccountId set on chars":   all(c["Player"]["m_AccountId"] == ACC for c in ctx.accounts[ACC]),
        "GWorld:FindObject(oid) from Lua": found == "Alice",
    }
    for k, v in checks.items():
        print(f"  [{'OK' if v else 'XX'}] {k}: {v}")

    # ---- object-construction layer: build a component-bearing object via the shipped Template/Com ----
    # Synthetic component classes + template (real Class/Template machinery; no package-boot needed).
    h.execute('''
        Class("SynCompBase", "Component")
        Class("SynStatus", "SynCompBase"):Def({ NetVars = { m_Hp = 100, m_Level = 1 } })
        Class("SynPlayer", "SynCompBase"):Def({ NetVars = { m_AccountId = 0 } })
        Template("SynChar", nil):Def({
          Com("SynStatus"):Set({ m_Hp = 250 }),
          Com("SynPlayer"):Set({ m_AccountId = 4242 }),
        })
    ''')
    char = ctx.build_template_object("SynChar")
    cchecks = {
        "template object built":      char is not None,
        "has SynStatus component":    char is not None and char["SynStatus"] is not None,
        "has SynPlayer component":    char is not None and char["SynPlayer"] is not None,
        "Com:Set override applied (m_Hp=250)": char is not None and char["SynStatus"]["m_Hp"] == 250,
        "NetVar default kept (m_Level=1)":     char is not None and char["SynStatus"]["m_Level"] == 1,
        "SynPlayer.m_AccountId=4242":          char is not None and char["SynPlayer"]["m_AccountId"] == 4242,
    }
    for k, v in cchecks.items():
        print(f"  [{'OK' if v else 'XX'}] (construct) {k}: {v}")
    checks.update(cchecks)

    ok = all(checks.values())
    print("RESULT:", "GREEN" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(_selftest())
