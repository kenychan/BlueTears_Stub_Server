"""M-LUA — run the shipped goEngine shared Lua server-side, faithfully.

WHY THIS EXISTS (the HEAVY-host keystone, de-risked 2026-06-28):
  goEngine class files are SHARED — the same `Class("X","Base"):Def{...}` files run on the
  client and the server, branching on IsServer()/IsClient(). The faithful host therefore does
  NOT reimplement the server's game logic; it *runs the shipped Lua* with IsServer()=true over
  a real object substrate (M-OBJ), exactly as the dead GIP/master server did.

  Feasibility was PROVEN with a live Lua 5.1 VM (lupa.lua51), not argued statically:
    - `Lib/Class.lua` (the 809-line OO core) loads and runs UNMODIFIED in standalone Lua 5.1.
    - The class/instance/field/method machinery is pure shipped Lua over PLAIN TABLES:
      `__goLua_index`/`__goLua_newindex`/`__goLua_classindex` (defined in Class.lua) drive field
      get/set + method dispatch; `new()`/`append()`/`clone()` install NetVar defaults + run Init.
    - Define(Object->Component->TestPlayer) -> instantiate -> Init ran -> NetVar default present
      -> field set/get -> method dispatch -> isa() inheritance walk: ALL GREEN.
    - The ONLY natives the whole slice touched: GetNID, goLua_loadfile, the backing `.new`.

  ⇒ The host owes a BOUNDED native surface, not a reimplementation:
    PLUMBING (this module): GetNID, goLua_loadfile(include), goLua_setmetatable,
       goLua_index/newindex (= the shipped __goLua_* reference impls), per-class backing `.new`,
       RunLogLn_Service, CopyTable, GetClassPiece.
    DOMAIN (M-API/M-REPL, wired on top): GetEnvironment/GetNObject, getByRelation (DB),
       GWorld:FindObject/CreateObject, BackupReplicate, GetOwner/GetComponent, DBCommandExecute,
       goLua_Pack/PackFixed (the WIRE serialization — itself shipped-Lua-driven, see Class.Pack).

The native backing `.new` is pluggable (native_backing=) so M-OBJ can supply a HostObject-backed
instance later; the default is a plain Lua table (sufficient — the shipped Lua does the rest).
"""
from __future__ import annotations

import os
from typing import Callable, Optional

from lupa import lua51

# Repo-relative default to the REAL TW client Lua mirror (per CLAUDE.md: the readable mirror,
# NOT extracted_named_v4). server/src/engine/lua/runtime.py -> repo root is four dirs up.
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
DEFAULT_LUA_ROOT = os.path.join(_REPO, "Res", "tw_lua_decompiled")

# Server-side load precedence (IsServer()=true): prefer Server/ + Main/, never Client/.
# A class basename present in multiple variant dirs resolves to the first matching bucket.
_DIR_PRECEDENCE = ("Class/Server", "Object/Server", "Class/Main", "Object/Main",
                   "Lib", "ClassEx", "SharedLib", "Class/Master", "Config")

PLUMBING_NATIVES = (
    "GetNID", "goLua_loadfile", "goLua_setmetatable", "goLua_index", "goLua_newindex",
    "RunLogLn_Service", "CopyTable", "GetClassPiece",
)


class LuaHost:
    """A Lua 5.1 VM with the shipped OO core loaded and the plumbing native shim wired.

    capture_missing=True installs a universal recording stub for any undefined global, so a
    real shipped class can be loaded to ENUMERATE the domain-native surface it touches
    (the symbols then live in .native_reads / .native_calls). capture_missing=False (default)
    is strict: undefined natives raise, which is what you want for a faithful run.
    """

    def __init__(self, lua_root: str = DEFAULT_LUA_ROOT, *,
                 native_backing: Optional[Callable[[str], object]] = None,
                 capture_missing: bool = False):
        self.lua_root = lua_root
        self.L = lua51.LuaRuntime(unpack_returned_tuples=True)
        self.g = self.L.globals()
        self._loaded: set[str] = set()
        self._native_backing = native_backing
        self.native_reads: set[str] = set()
        self.native_calls: dict[str, int] = {}
        self._nid: dict[str, int] = {}
        self._file_index = self._build_file_index()
        self._wire_plumbing()
        self._load_core()
        if capture_missing:
            self._install_missing_capture()

    # ---- class-name -> file resolver (the goLua_loadfile/include substrate) ----
    def _build_file_index(self) -> dict[str, str]:
        index: dict[str, str] = {}
        ranked: dict[str, int] = {}
        for dirpath, _dirs, files in os.walk(self.lua_root):
            rel = os.path.relpath(dirpath, self.lua_root).replace("\\", "/")
            if rel.startswith("Class/Client") or rel.startswith("Object/Client") \
               or rel.startswith("ClassEx/ClientOnly"):
                continue  # server host never loads client-only classes
            rank = next((i for i, p in enumerate(_DIR_PRECEDENCE) if rel.startswith(p)), 999)
            for fn in files:
                if not fn.endswith(".lua"):
                    continue
                base = fn[:-4]
                if base not in ranked or rank < ranked[base]:
                    ranked[base] = rank
                    index[base] = os.path.join(dirpath, fn)
        return index

    def _resolve(self, name: str) -> Optional[str]:
        return self._file_index.get(name)

    def _loadfile(self, name) -> None:
        name = str(name)
        if name in self._loaded:
            return
        self._loaded.add(name)  # mark first to break include cycles
        path = self._resolve(name)
        if path is None:
            self.native_reads.add(f"<unresolved-class:{name}>")
            return
        src = open(path, "r", encoding="utf-8", errors="replace").read()
        chunk = self.L.eval("loadstring")(src, "@" + name)
        if chunk is not None:
            chunk()

    # ---- plumbing natives (must exist BEFORE Class.lua runs) ----
    def _GetNID(self, name) -> int:
        name = str(name)
        if name not in self._nid:
            self._nid[name] = 0x1000 + len(self._nid)
        return self._nid[name]

    def _backing_new(self, native_name: str):
        if self._native_backing is not None:
            return self._native_backing(native_name)
        return self.L.eval("function() return {} end")()

    def _wire_plumbing(self) -> None:
        g = self.g
        g["editor"] = False
        # Server mode: the shared Lua's IsClient()=`Firenze.LogServer==nil` (Lib/Template.lua),
        # so a non-nil LogServer flips the shipped classes to IsServer()=true — the whole point
        # of the host. (Same switch as the LATE-8b in-client realization trick.)
        g["Firenze"] = self.L.eval("{ LogServer = {}, IsTool = false }")
        g["GetNID"] = self._GetNID
        g["goLua_loadfile"] = self._loadfile
        g["RunLogLn_Service"] = lambda *a: self._tick("RunLogLn_Service")
        g["CopyTable"] = self.L.eval("function(a,b) return true end")
        g["GetClassPiece"] = self.L.eval("function(n) return nil end")
        # backing factory for native_name="" (pure-Lua classes) and a registrar for named ones.
        g["__py_backing_new"] = self._backing_new
        self.L.execute(r"""
            local function backing(native_name)
              return { ['.new'] = function() return __py_backing_new(native_name or "") end }
            end
            rawset(_G, "", backing(""))
            __register_native_backing = function(nm) rawset(_G, nm, backing(nm)) end
        """)

    def _tick(self, name: str):
        self.native_calls[name] = self.native_calls.get(name, 0) + 1

    def _load_core(self) -> None:
        self._loadfile("Class")  # Lib/Class.lua via the resolver
        # Wire the metamethods the core just defined to its OWN Lua reference impls,
        # so instances are plain tables fully driven by shipped Lua.
        self.L.execute(r"""
            goLua_index    = __goLua_index
            goLua_newindex = __goLua_newindex
            goLua_setmetatable = setmetatable
            if GetClass == nil then
              function GetClass(n)
                local c = rawget(_G, n)
                if type(c) == 'table' and c['.type'] == ObjectTypes.CLASS then return c end
                return nil
              end
            end
        """)
        # The rest of the OO/object bootstrap the real engine boots before any game class:
        #   Lib/Component.lua  -> Class("Component","NObject"), the replicate enums
        #     (NormalReplicate/OwnerOnly/NotReplicate), the real CreateCom, and the
        #     component-as-field projection (Component.__index -> obj:GetCom(name)).
        #   Lib/Template.lua   -> Template()/Com()/IsKindOfTemplate/CreateDeferedTemplate
        #     (the object-template system used to construct component-bearing objects).
        for boot in ("Component", "Template"):
            self._loadfile(boot)

    # ---- capture mode: record undefined-global READS but return nil ----
    # (A deep-stub _G.__index corrupts the class system's own `_G[name]` existence/nil
    #  checks -> false "inheritance error". Record-and-return-nil enumerates the domain
    #  surface a real class touches without breaking class lookups; a class-def call into a
    #  genuinely-missing native may still raise, by which point the read is already recorded.)
    def _install_missing_capture(self) -> None:
        recorded = self.native_reads
        def on_read(k):
            recorded.add(str(k))
            return None
        self.g["__py_on_missing"] = on_read
        self.L.execute(r"""
            setmetatable(_G, { __index = function(t, k) return __py_on_missing(k) end })
        """)

    # ---- public helpers ----
    def execute(self, src: str):
        return self.L.execute(src)

    def eval(self, src: str):
        return self.L.eval(src)

    def load_class(self, name: str) -> None:
        self._loadfile(name)

    def new(self, classname: str):
        self._loadfile(classname)
        cls = self.g[classname]
        if cls is None:
            raise KeyError(f"class {classname} did not register a global after load")
        return cls["new"](cls)


# --------------------------------------------------------------------------------------
# Self-test: reproduce the GREEN proof against the REAL shipped Lib/Class.lua, then do a
# bounded capture pass on a real server class to surface its domain-native footprint.
# --------------------------------------------------------------------------------------
def _selftest() -> int:
    print("M-LUA runtime self-test")
    print("  lua_root:", DEFAULT_LUA_ROOT)
    h = LuaHost()
    print("  VM:", h.eval("_VERSION"), "| OO core loaded:",
          bool(h.eval("rawget(_G,'Class') ~= nil")))

    # Define a representative server logic class over the shipped core and exercise it.
    # Synthetic names (absent from the tree) so the strict proof tests the CORE machinery
    # without dragging in the real class chain; the capture pass below loads real classes.
    h.execute(r"""
        Class("ProbeBase")
        Class("ProbeChild", "ProbeBase"):Def({
          NetVars = { m_AccountId = 0, m_Level = 1 },
          Init = function(self) self.m_initialised = true end,
          SetLevel = function(self, n) self.m_Level = n; return self.m_Level end,
        })
    """)
    p = h.new("ProbeChild")
    checks = {
        "instantiated":  p is not None,
        "Init ran":      p["m_initialised"] is True,
        "NetVar default": p["m_Level"] == 1,
    }
    p["m_AccountId"] = 4242
    checks["field set/get"] = (p["m_AccountId"] == 4242)
    checks["method dispatch"] = (p["SetLevel"](p, 7) == 7 and p["m_Level"] == 7)
    checks["isa() walk"] = (p["isa"](p, "ProbeBase") is True)

    ok = all(checks.values())
    for k, v in checks.items():
        print(f"    [{'OK' if v else 'XX'}] {k}: {v}")
    print("  plumbing natives called:", dict(sorted(h.native_calls.items())))

    # Real shipped server class: load + instantiate CharacterManager in SERVER mode through
    # the universal plain-table backing (proves the host runs real classes, not just synthetic).
    real_ok = False
    try:
        h2 = LuaHost()  # Firenze.LogServer set -> IsServer()=true
        h2.load_class("CharacterManager")        # pulls NObject->Computer->Component->CharacterManager
        cm = h2.new("CharacterManager")
        real_checks = {
            "server mode":      bool(h2.eval("Firenze.LogServer ~= nil")),
            "instantiated":     cm is not None,
            "GetName":          (cm["GetName"](cm) == "CharacterManager"),
            "LuaCreateCharacter present":       cm["LuaCreateCharacter"] is not None,
            "serverCreateCharacterResult present": cm["serverCreateCharacterResult"] is not None,
        }
        real_ok = all(real_checks.values())
        print("  [real class] CharacterManager base chain:", sorted(h2._loaded))
        for k, v in real_checks.items():
            print(f"    [{'OK' if v else 'XX'}] {k}: {v}")
    except Exception as e:
        print("  [real class] stopped at:", type(e).__name__, str(e).splitlines()[0][:90])

    allok = ok and real_ok
    print("RESULT:", "GREEN" if allok else "FAIL")
    return 0 if allok else 1


if __name__ == "__main__":
    raise SystemExit(_selftest())
