# 2026-06-30 — The client builds a full TPlayer LOCALLY (construct wall broken)

**Result:** the live client constructs a **full 25-component, registered, `obj+0x18`-bearing
`TPlayer`** in-process, using its OWN engine + OWN Lua data, driven by the engine's OWN
force-load mechanism. This retracts the earlier "the roster char is architecturally
un-constructable in this standalone" conclusion.

## Root cause (the real wall)

`Lib/Template.lua:120-147` is the construct gate. On the client (`IsClient()` = `Firenze.LogServer==nil`,
native-bound), a `TNetObject` descendant such as `TPlayer` is realized as **`EmptyImpl`**:
zero components, and it **skips `goLua_RegistObject`** (Template.lua:95). That skip is why the
object is absent from both construct registries — i.e. why `bb2580('TPlayer')` returned NULL and
`CreateLocalObject('TPlayer')` returned nil. Those were *symptoms of EmptyImpl*, not a missing lane.

The full **`DefaultImpl`** path (CreateCom per component + `AddComponent` + `goLua_RegistObject`)
is taken only when the hierarchy is **force-loaded** (`bForceLoading`, the same mechanism `TItem`/
`TMap`/`TSkill_Monster` use). The full 25-component `TPlayer` definition already lives in the
client's own `Object/Main/Player/TPlayer.lua`.

## Recipe (in-VM, via the NpSlayer Lua-eval seat, profile 78 @0x010b0490)

```lua
-- TestMode guard: Firenze.TestMode=true live -> TPlayer.lua:1 early-returns; null for the reload
local oldTM = Firenze.TestMode; Firenze.TestMode = nil
-- 1) engine's OWN force-load of the actor hierarchy (sets GoTemplateForceLoading[TActor])
_Template('TActor','TNetObject', true, true)
-- 2) re-realize TPlayer: with force active it now takes the Defered branch (not Empty)
reload_file('TPlayer')                       -- Template(TPlayer,TActor) => Defered
-- 3) realize via DefaultImpl: CreateCom x25 + AddComponent + goLua_RegistObject
CreateDeferedTemplate('TPlayer')
-- 4) the client's OWN construct now succeeds (was nil before)
local o = ClientWorld:CreateLocalObject('TPlayer')   -- real object, full components, obj+0x18
Firenze.TestMode = oldTM
```

Native confirmation in the same run: `c48760` (TPlayer NID `0x9f4a5bcd` realizer) reached, then
`bd2cd0` writes `obj+0x18` for `cls38=0x9f4a5bcd` — the full native construct, not just a Lua table.

## C↔Lua boundary (validated)

The construct is a C→Lua / Lua→C ping-pong: C signals the realize (`c48760` → Lua
`CreateDeferedTemplate`), Lua decides the branch and calls back into C primitives (`CreateCom`×25,
`goLua_RegistObject`). The EmptyImpl/DefaultImpl branch is a *silent* Lua return — no log, no throw,
no `OutputDebugString` — which is why native-only channels never caught it. Wrapping the boundary
functions (`Template`/`CreateDeferedTemplate`/`CreateCom`/`goLua_RegistObject`) makes every crossing
visible; see the `[GAP]` evidence log.

## Caveat / next

The force-built `TPlayer` is currently an ORPHAN (full components + `obj+0x18` but no world owner) and
faults at teardown. Next: give it an owner/OID and register it into GWorld / `CharacterManager+0x28`
so `getAllPlayers` → `bd3660` gates → `AddCharacter` → a visible ROW; then drive the force-realize +
char data from the host's roster trigger (`onLoadPlayers`/`serverCreateCharacterResult`) so the host
supplies data and the client builds + registers each char.

Primary evidence: `evidence/2026-06-30_tplayer_local_construct/gap_log_test5.txt`
Lua map: `docs/LUA_QUICKREF.md`
