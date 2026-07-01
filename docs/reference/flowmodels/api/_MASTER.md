# API flow model — MASTER index

The complete API surface of the client, in two layers, plus how they connect. Built to be a fast lookup
("what RPCs does X expose / what native does the Lua ride on / where does the host implement it") and to
surface **breakthrough seats** (any native with a VA can be seated live).

## The two layers

| layer | file | what it is | how it was enumerated |
|---|---|---|---|
| **wire RPC surface** | [`rpc_surface.md`](rpc_surface.md) | the 1252 `SetLuaNetFunc` registrations (1209 distinct methods) every Lua object exposes on the wire, grouped by owning class, split Client/Server/Master | grep `SetLuaNetFunc(` over `Res/tw_lua_decompiled` (method **a**) |
| **native goEngine surface** | [`native_api.md`](native_api.md) | the native functions bound into the Lua VM (world construct/register, codec, DB, RPC-reg) + the receiver-side construct primitives, with VAs pinned or `TBD` | RE corpus + host bindings (method **c**); complete VA table pending the `luaL_register` sweep (method **d**) |

Machine-readable: `ghidraRE/output/api_surface_index.json` (class → RPCs → types/line).

## The four ways to enumerate API functions (reference)

- **(a) `SetLuaNetFunc(`** — the wire RPC surface, already split client/server. ✅ done → `rpc_surface.md`.
- **(b) `AddObjectFunction(`** — per-template method surface. ✅ swept (only 3 uses; not a major surface).
- **(c) Lua-side residue** — `Global:method` / bare-global calls minus Lua-defined names = the natives *in
  use*. ✅ partial (the login/roster/char natives → `native_api.md`).
- **(d) Ghidra `luaL_register` / `lua_pushcclosure` sites** — the definitive native surface **with VAs to
  seat**. ❌ not yet run; this is what fills every `TBD` in `native_api.md`.

## How the layers connect (the call direction)

```
client Lua object  ──SetLuaNetFunc──▶  wire RPC (Client/Server/Master)      [rpc_surface.md]
        │  (its logic calls down into…)
        ▼
native goEngine API  (World:CreateObject, getByRelation, goLua_Pack*, …)    [native_api.md]
        │  (which drives the receiver primitives…)
        ▼
construct/register primitives (bff990 → 647860 → map C; bd4b20 obj+0x18)    [../client/gworld.md §6]
```

The concrete login→roster→char walk over this stack (which RPC calls which, per object) is the cross-object
graph in [`../client/_MASTER.md`](../client/_MASTER.md); each engine object's internal flow is its file in
`../client/`. The **server counterparts** that RUN these (over the host substrate) live in
`server/src/{char,account,charactermanager,net}/`.

## Highest-value entries for the current front (E3)

The open unknown is the char **replication envelope** (`../client/gworld.md` §5). The API entries that bear
on it — good seats once (d) pins their VAs:
- `object:BackupReplicate(true)` — the char-delivery trigger (native_api §2).
- `World:CreateObject("TPlayer")` — the server-side construct+register (native_api §2 / gworld §4b).
- `ObjectCache:getByRelation` → `onLoadPlayersCacheResult` — how the client asks for owned chars
  (rpc_surface: `Session`, `CharacterManager`, `TPlayerCache`).
- The receiver primitives `bff990`/`647860` (native_api §7) — already pinned; the envelope is what feeds them.
