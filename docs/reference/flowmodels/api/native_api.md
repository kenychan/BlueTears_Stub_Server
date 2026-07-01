# API flow model — the native goEngine surface the Lua rides on

The Lua-first rule: game logic is in the Lua, but the Lua *calls down* into native engine functions
bound into the VM (world construct/register, codec, DB, RPC registration). This file is the map of that
**native surface** — the API the faithful host must supply (M-API/M-CODEC/M-DB). Each row: the Lua-facing
name, the native VA (pinned from the RE corpus, or `TBD` = recover via the `luaL_register`/`lua_pushcclosure`
sweep, method (d) below), and the host module that implements it.

Companion files: the wire RPC surface = `rpc_surface.md`; the login→roster→char call graph =
`../client/_MASTER.md`; the deep world-registration mechanics = `../client/gworld.md`.

---

## The COMPLETE native surface WITH addresses (method d — DONE 2026-07-01)

Two Ghidra sweeps recovered the seatable native surface (regen: `Res/ghidra_scripts/200_*`, `201_*`):
- **`luaL_Reg[]` arrays** → [`../lua_native_surface.md`](../lua_native_surface.md) (+ `.json`): **227 name→VA**
  across 23 arrays — the Lua 5.1 stdlib + luasocket + luafilesystem + the config lib. Exact (no heuristic):
  scanned `.rdata` for `{name_ptr→C-string, fn_ptr→.text}` pair-runs.
- **individual `lua_setglobal` registrations** → [`../lua_global_registrations.md`](../lua_global_registrations.md):
  the goEngine globals resolved by name-string→code-xref→pushed-fn-ptr (skipping CALL refs to the
  registration helpers). **9 clean + several 2-candidate.** `goLua_CreateObject=0x00c48010` matched the
  independently-known corpus value → pipeline validated. VAs from these are filled in below.

Note: object **methods** (`World:CreateObject`, `obj:FindObject`, `SetLuaNetFunc`) live on metatables, not
globals, so 201 correctly does NOT resolve them (13 `CreateObject` string uses = metatable keys) — they are
pinned via the construct-primitive RE (§7) + `gworld.md` instead.

---

## 1. Environment / singletons  (host: M-API `engine/api/host_context.py`)

| Lua-facing | native VA | role | host binding |
|---|---|---|---|
| `GetEnvironment()` | `0x00952610` | returns the global env object | `g["GetEnvironment"]` |
| `env:GetNObject(name)` | `0x0094ff00` | name→singleton (`"ServerWorld"`,`"GWorld"`,`"TPlayerCache"`, managers) | `env["GetNObject"]` → `self.singletons` |

## 2. World — the construct+register API  (host: M-API GWorld registry)

| Lua-facing | native VA | role | host binding |
|---|---|---|---|
| `World:CreateObject(className)` | TBD (native world method; §4b) | **one-shot construct + OID-register** — the register-for-TPlayer API (`ServerWorld:CreateObject("TPlayer")`, CStatus.lua:812) | `gworld_tab["CreateObject"]` |
| `World:FindObject(oid)` | `0x006481e0` → `0x00647a30` | resolve oid→object from map C | `gworld_tab["FindObject"]` → `self.gworld` |
| `ClientWorld:CreateLocalObject(cls)` | realizer `0x00c48760` | client-local build (3D preview etc.) | n/a (client-only) |
| `object:BackupReplicate(true)` | `0x00bcf870` | mark object for replication to the client (E3 trigger — envelope open, gworld.md §5) | M-REPL `engine/repl/replicate.py` |

## 3. Object model / Template  (host: M-LUA + M-API; mirrors `Lib/Template.lua`, `Lib/Com.lua`)

| Lua-facing | native VA | role | host binding |
|---|---|---|---|
| `Class(name, parent)` / `GetClass(name)` | TBD | define / fetch a Lua class | M-LUA `engine/lua/runtime.py` |
| `Com(name)` | TBD | component prototype lookup | M-LUA |
| `goLua_CreateObject(name, parent)` | `0x00c48010` (sweep-confirmed) | NiObject **shell** factory (Template DefaultImpl path) | `g["goLua_CreateObject"]` |
| `goLua_RegistObject(obj[,parent])` | `0x00c483d0` / `0x00c48520` (2 cands; fires on DefaultImpl only, `Lib/Template.lua:95`) | register the realized template | `g["goLua_RegistObject"]` |
| `reload_file` | `0x00c6bc30` / `0x00c6beb0` (2 cands) | force-load a template realize | M-LUA loader |
| `goLua_loadfile` | `0x00c6ab10` / `0x00c6b780` (2 cands) | load a Lua/data file | M-LUA loader |
| `obj:AddComponent(com)` | `0x00bcf510` → `0x00bd2cd0` (obj+0x18 insert by ordinal) | attach a component | `obj["AddComponent"]` |
| `obj:GetBaseComponent(com)` / `GetCom(name)` | TBD | fetch a component | `obj["GetBaseComponent"]` |

## 4. Codec (wire (de)serialize)  (host: M-CODEC `engine/codec/wire.py`)

| Lua-facing | native VA | role |
|---|---|---|
| `goLua_Pack(...)` | `0x00bb7630` | pack a value (tag-05 array / tag-07 object / tag-0b ref) |
| `goLua_PackFixed(...)` | `0x00bb7e70` | pack fixed-width scalar |
| `goLua_UnpackFixed(...)` | `0x00bb8220` | unpack fixed-width scalar |

## 5. DB relation  (host: M-DB `engine/db/store.py`)

| Lua-facing | native VA | role | host binding |
|---|---|---|---|
| `ObjectCache:getByRelation(id, "TAccount", "CharacterManager")` | TBD | resolve-only relation query → objectList (chars already owned) | `_getByRelationImpl` → `onLuaCompleteByRelation` |
| `DBCommandExecute("SP2_LoadPlayerData", args)` | TBD | shipped stored-proc player blob | `DBNatives.db_execute` |

## 6. RPC registration / routing  (host: M-NET + M-API)

| Lua-facing | native VA | role |
|---|---|---|
| `self:SetLuaNetFunc(name, RPCType.*)` | TBD | register a replicated method on the wire (the 1252-entry surface, `rpc_surface.md`) |
| `self:AddObjectFunction(name)` | TBD | per-template method (only 3 uses) |
| `obj:SetControlObject(ref)` | TBD | bind the controlled object (login: TClient/TAccount) |

## 7. Native construct primitives (below the Lua — the receiver side, PINNED)

These are NOT Lua-bound; they are the native machinery `World:CreateObject`/replication drive. Full graph
in `../client/gworld.md` §6.

| VA | role |
|---|---|
| `0x00bff990` | NewObject / full-construct entry; find-or-insert into map C |
| `0x00647860` | OID-map INSERT (map C) — the registration |
| `0x00647a30` | OID-map LOOKUP (map C) |
| `0x006481e0` | FindObject resolver |
| `0x00bd4b20` | component tail; writes obj+0x18 |
| `0x00bd2cd0` | obj+0x18 writer (self/RPC-routing map) |
| `0x00c024b0` | post-construct step |
| `0x00bb2580` | by-name component/type materializer |
| `0x00ba9420` | Lua-stack unbox of a resolved object |
| `0x00bcf510` | AddComponent RPC handler |
| `0x00bfdb40` | key-table dispatch → bff990 |
| `0x005e5650` | map-A (DAT_01825258) string/descriptor resolver (wire ObjectPtr) |

---

## Connection to the host (emulator shape)

The host RUNS the shipped Lua over these natives: the Lua calls `World:CreateObject`, `getByRelation`,
`SetLuaNetFunc`, `goLua_Pack*`; the host's M-API/M-DB/M-CODEC/M-NET provide them. Filling the `TBD` VAs
(method d) lets us **seat** any native live (NpSlayer) for a differential trace — the direct path to the
one open unknown (the E3 replication envelope, gworld.md §5).
