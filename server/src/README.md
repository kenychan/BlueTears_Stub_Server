# src/ — faithful host modules

The faithful HEAVY host **runs the shipped goEngine server Lua** (`Res/tw_lua_decompiled`) with
`IsServer()=true` over a host-owned object substrate, and serializes the result to the wire — so the
client's OWN mechanics construct the char, not force-patching. Design: `../docs/architecture.md`;
live state: `../../CODEX_LATEST_STATUS.md` (top).

## Run everything
`py server/run_selftests.py` — runs all module self-tests + the end-to-end demo (8/8 GREEN).
Each module is also runnable alone (its `__main__` is its self-test), e.g. `py server/src/host.py`.

## Built modules (all self-tests GREEN)

| Module | Folder | What it does |
|--------|--------|--------------|
| **M-OBJ**   | `obj/`   | host GWorld object/component model; Gate-2 logic (char renders iff `obj+0x18` self map populated) |
| **M-LUA**   | `lua/`   | runs the shipped OO+component+template Lua in standalone Lua 5.1 (`lupa.lua51`), server mode (`Firenze.LogServer`) |
| **M-API**   | `api/`   | the bounded goEngine domain natives (`GetEnvironment`/`GetNObject`/`getByRelation`/`GWorld`) + the Template/Com **object-construction layer**; runs the real `TPlayerCache:LoadPlayers` |
| **M-CODEC** | `codec/` | the tag-variant wire codec (`goLua_PackFixed`=`bb7e80`, RE'd), driven by the shipped `Class.Pack` |
| **M-REPL**  | `repl/`  | replication emit: a live objectList → faithful wire payload; `stub_bridge.py` = the transport entry point |
| **M-DB**    | `db/`    | stateful `CharStore` (JSON persistence) + the shipped `DBCommandExecute("SP2_LoadPlayerData")` native (`Session.lua:194`), backing the account→chars relation + per-char `xData` blob |
| **host**    | `host.py`| **FaithfulHost** — composes the stack: account → shipped LoadPlayers → faithful wire bytes; optional `db=` makes it stateful (persist/reload accounts) |

## Two serializers (don't conflate — see `../re/M-CODEC-wire-format.md` + status LATE-42)
- **M-CODEC tag-variant** (`goLua_PackFixed`/`bef480`) = **RPC args** (built, pinned).
- **`bb0060` descriptor-keyed component format** = the **construct/replication lane** (`component_tail`,
  partly RE'd + partly in the test stub) — used to replicate a component-bearing char object.

## Construct LANE — RESOLVED (LATE-53/54, live; supersedes the old D1 framing)
The construct lane is **NOT** tag-1/`c35140` (that was always the wrong, dead lane — D1). It is the
**wire-construct lane**: a parsed channel message → `FUN_00ca47e0` object-tree walker → `0xca492b`
per-node dispatch → **`bff990`** → `bb5f40` factory → full class ctor (builds `obj+0x14` components AND
`obj+0x18` routing). Proven live: the `TClient` control object `0x3e9` rides exactly this lane on a
normal connection (`BFF990_CALLER ret=0x00ca492d`). ⇒ the construct lane is **reachable**; the host's
job is to **emit each TAccount-tree node as a wire NewObject that parses+dispatches through it**.

## What's NOT built yet (the remaining build, LATE-54)
- **Host-side engine-emit of the per-node component tail** (`bb0060`/`bd4b20` descriptor-keyed format).
  Hand-forging it is blocked (LATE-16) ⇒ M-CODEC must engine-emit it from the M-OBJ object graph. It is
  "mostly in the test stub" but NOT yet faithfully in the host. **This is the keystone build now.**
- **NEXT MILESTONE E1:** host engine-emits ONE minimal real NewObject (`TClient`, 1 `Connector`) via
  M-CODEC, delivered over the live transport (`stub_bridge`), verified live by `BFF990_CALLER` firing
  for the HOST's distinctive pid → proves the host drives native construction (and resolves whether
  `0x3e9` was the host's object or the client's own bootstrap). Then extend TAccount → CharacterManager
  → TPlayer chars. Live procedure: `../re/LIVE-TEST-onloadplayers-recipe.md`.
- Full 3D body from the real `TPlayerDummy` template = a large package boot (20+ inherited components) —
  needed only for the Gate-1 body render, AFTER the row/construct.
