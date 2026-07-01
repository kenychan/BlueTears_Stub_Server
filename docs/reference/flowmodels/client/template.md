# Template system — the construct gate (engine globals, C↔Lua boundary)

The native factory/register globals + the Lua construct gate. Runs on WHICHEVER process builds an
object. Server counterpart: `server/src/template/` (the host's own `Template`/`Com` over Lua tables =
M-LUA `lua/runtime.py`). Full detail: `LUA_QUICKREF.md §1`.

## Native globals (callable from Lua)
| global | role |
|---|---|
| `goLua_CreateObject(name,parent)` | make the NiObject shell (native factory) |
| `goLua_RegistObject(obj,parent)` | **register obj into the C registry** — `EmptyImpl` SKIPS this |
| `reload_file(name)` / `goLua_loadfile(name)` | re-realize / load a template file (native) |
| `GetClass(name)` / `class:new()` | component class factory (used by `CreateCom`) |
| `GetNID(name)` | def-table NID hash |
| `AddObjectFunction(objName,key,fn)` | register an RPC/method on a template |

## The gate (Lua) — `Lib/Template.lua:120`
```
Template(objName,parent,bForceLoading,bForceChildLoading):
  if IsClient() and not IsTool():
    if bForceLoading: … → DefaultImpl              (TItem/TMap/TSkill_Monster)
    elif IsKindOfTemplate(parent,"TNetObject") and not IsForceLoading(parent):
        return {Def=EmptyImpl}                     ← TPlayer LANDS HERE: no comps, NO goLua_RegistObject
    else: return {Def=DeferedImpl}
  return {Object=goLua_CreateObject(...), Def=DefaultImpl}   FULL: CreateCom×N + AddComponent + goLua_RegistObject
```
- `DefaultImpl` (`:63`) — full build + `goLua_RegistObject` (`:95`) → the registered object.
- `EmptyImpl` (`:99`) — `AddObjectFunction` only; **no components, no register**.
- `DeferedImpl` (`:114`) / `CreateDeferedTemplate` (`:173`) — deferred realize → later runs DefaultImpl.
- `CreateCom(name)` = `GetClass(name):new()` (`Lib/Component.lua:239`).

## Edges
`template ─▶ object (AddComponent)` · `─▶ gworld (goLua_RegistObject → OID/registry)` · consumed by
`clientworld (CreateLocalObject)` and the native construct lane (`c48760`←`c35140`).

## Self-diagnosis
Wrap `Template`/`CreateDeferedTemplate`/`CreateCom`/`goLua_RegistObject` via the Lua-eval seat to log
the silent Empty-vs-Default branch (`LUA_QUICKREF.md §3`, C↔Lua GAP channel).
