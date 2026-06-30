# LUA_QUICKREF — the client Lua map (read before grepping Lua; the flow_model.json for Lua)

**Why this exists (user, 2026-06-30):** we keep re-grepping the client Lua and forgetting its
shape between sessions. This is the persistent tree — the Lua analogue of
`ghidraRE/output/flow_model.json` (C control-flow). Keep it CURRENT: after any Lua walk that
establishes a new node/edge/bridge, add it here (standing rule, mirrors the flow_model rule).
Readable mirror of the Lua = `Res/tw_lua_decompiled/`; authoritative `.luo` = `Res/tw_extracted_named_v4/Lua`.

---

## §0 THE C↔LUA BOUNDARY MODEL (ground truth — the thing we kept missing)

The client is a **C engine (goEngine/Gamebryo) that drives timing**, plus a **Lua VM that holds
the object/construct LOGIC**. They are one thread, synchronous across the FFI, but the dependency
is: **C reaches a point → calls into Lua → Lua DECIDES + calls back into C primitives → returns to C.**

Consequence (why our 3 native channels stayed silent on the construct skip): a Lua *branch decision*
(`EmptyImpl` vs `DefaultImpl`) calls no `RunLogLn_Service`/`print`, raises no exception, emits no
`OutputDebugString`. It silently returns a different table. **To witness it you must wrap the
boundary functions** (this file's §3 list) so each crossing/branch becomes a log line.

### Bridge functions = NATIVE (C), callable FROM Lua  (the literal gap; seat/​wrap these)
| Lua name | Role | C side |
|----------|------|--------|
| `goLua_CreateObject(name,parent)` | make the NiObject shell | native factory |
| `goLua_RegistObject(obj,parent)` | **register obj into the C registry** (`0x183e0e0`/goLua) — **`EmptyImpl` SKIPS this** | native |
| `goLua_loadfile(name)` | load a Lua object file (cached) | native |
| `reload_file(name)` | **re-realize** a template file (un-cached; re-runs `Template()`) | native |
| `obj:AddComponent / RemoveComponent / GetBaseComponent` | attach/detach components | native methods |
| `ClientWorld:CreateLocalObject(name)` | build a registered instance (preview/local) | native method |
| `GWorld:FindObject(oid) / CreateObject` | resolve/create by OID | native methods |
| `GetClass(name)` / `class:new()` | component class factory (used by `CreateCom`) | native |
| `GetNID(name)` | def-table NID hash | native |
| `AddObjectFunction(objName,key,fn)` | register an RPC/method on a template | native |

### Logic functions = LUA (defined in the mirror; the DECISIONS live here)
| Lua fn | File:line | Role |
|--------|-----------|------|
| `Template(obj,parent,bForceLoading,bForceChildLoading)` | `Lib/Template.lua:120` | **the construct gate** (see §1) |
| `Com(name)` / `:Set{}` / `:Def{}` | `Lib/Template.lua:38` | component-table builder |
| `CreateDeferedTemplate(name)` | `Lib/Template.lua:173` | realize a deferred template → runs `DefaultImpl` |
| `CreateCom(name)` | `Lib/Component.lua:239` | `GetClass(name):new()` (logs `Error` if class missing) |
| `IsKindOfTemplate(a,b)` | `Lib/Template.lua:150` | walk `GoTemplateHierarchy` |
| `IsClient` `IsTool` `IsForceLoading` `DefaultImpl` `EmptyImpl` `DeferedImpl` | `Lib/Template.lua` (file-LOCALS) | branch helpers — NOT wrappable by name; infer via the globals they call |

---

## §1 THE CONSTRUCT GATE — `Lib/Template.lua:120-147`  (THE WALL)

```
Template(objName, parentName, bForceLoading, bForceChildLoading):
  if IsClient() and not IsTool():                 -- IsClient() = (Firenze.LogServer == nil); native-bound, NOT Lua-flippable (LATE-110 TEST2)
    if bForceLoading:                              -- (TItem/TMap/TSkill_Monster take this) → falls through ↓ to DefaultImpl
        if bForceChildLoading: GoTemplateForceLoading[objName]=true   -- (file-LOCAL table)
    elif IsKindOfTemplate(parent,"TNetObject") and not IsForceLoading(parent):
        return {Def = EmptyImpl}                   -- ← TPlayer LANDS HERE: zero components, NO goLua_RegistObject
    else:
        return {Def = DeferedImpl}                 -- stored in GoDeferedTemplates; realized later by CreateDeferedTemplate
  return {Object=goLua_CreateObject(...), Def = DefaultImpl}   -- FULL: CreateCom×N + AddComponent + goLua_RegistObject
```

- **`EmptyImpl` (`:99`)** — only `AddObjectFunction` for fn-valued entries. **No components, no register.**
  → this single skip explains BOTH "TPlayer not constructible" negatives: `bb2580('TPlayer')`=NULL and
  `CreateLocalObject('TPlayer')`=nil are just "never registered" (`goLua_RegistObject` never ran).
- **`DefaultImpl` (`:63`)** — `for each Com: CreateCom → (Clone base) → set Vars → obj:AddComponent`;
  then **`goLua_RegistObject(obj, parent)` (`:95`)** + `GoObjectTemplates[name]=obj`. The full char.
- **`DeferedImpl` (`:114`)** — stash `{self,comTable}` in `GoDeferedTemplates`; `CreateDeferedTemplate(name)`
  (`:173`) later runs `goLua_CreateObject` + `DefaultImpl`. This is how force-loaded hierarchies realize.

**The char templates carry their FULL component list in the client's own Lua:**
- `Object/Main/Player/TPlayer.lua:1` → `if Firenze.TestMode then return end` then
  `Template("TPlayer","TActor"):Def({ DBCom, Player, CStatusPlayer, CPrivateStatus, User, Physics,
  CBody, PlayerController, CSkillBook, CInventory, CEquipment })` — 11 comps, **ignored by EmptyImpl on client.**
- `Object/Main/Player/TPlayerDummy.lua:1` → `Template("TPlayerDummy","TActor"):Def({ 8 comps })`.
  **Builds full via `CreateLocalObject`→`c48760`→`CreateDeferedTemplate`→`DefaultImpl`** ⇒ proves
  DefaultImpl-on-client WORKS. Difference from TPlayer = which realize path ran, nothing else.
- `Object/Main/TActor.lua:1` `Template("TActor","TNetObject")` · `TNetObject.lua:1` `Template("TNetObject")` — no force.
- Force-loaded (→ DefaultImpl on client): `TItem` `TMap` `TSkill_Monster` (`,true,true)`.

**C entry into this gate:** `c48760` (TPlayer realizer; native bb5f40 if `DAT_0148087c` realized, else Lua
`CreateDeferedTemplate`) is called by `c35140` (tag-1) and the live construct lanes. So C signals → Lua realizes.

---

## §2 CHAR-SELECT / ROSTER FLOW (Lua chain; established LATE-59/86/95/108)

```
TPlayerCache:LoadPlayers → session:onLoadPlayersCacheResult(objectList)   -- objectList = OID refs
  each elem → ba9420 (native resolve); unresolved → nil → table deletes the slot (empty roster)
CharacterManager (CharacterManager.lua): getAllPlayers, AddCharacter, GWorld:FindObject(oid) (:85/93/101/129)
  native LoadList = a620c0 (parses arg3 std::list of bare char Object*); per-elem a60f00 reads Player+0x4c = OID identity
char CREATION converges on the SAME load: User:createCharacter → server persists+replicates →
  onCreateCharacterResult → "CharacterCreated" → UpdateCharacter() re-runs roster load;
  serverCreateCharacterResult only post-decorates a FindObject(oid) with items
UIIntro.lua: AddCharacter handler (:56) paints a row ONLY if CharacterSelectionDialog is loaded (:94);
  the 3D preview = CreateTUIControlObject → ClientWorld:CreateLocalObject('TPlayerDummy') (:220-253)
```
Roster char = **server-construct-into-GWorld THEN resolve by OID** — the resolve lanes never construct.

---

## §3 SELF-DIAGNOSIS — wrap these to log the C↔Lua gap (validate/trace any construct)

Inject via the Lua-eval seat (profile 78 one-shot @`0x010b0490`; recipe
`ghidraRE/output/claude_lua_eval_string_recipe.md`). Log with `RunLogLn_Service("LUALOG", ...)` →
tees to `TW/Bin/client_internal.log` (CLIENTLOG_TAP). Wrap, calling the original after logging:
`Template` (log obj/parent/force + branch via returned table shape: has `.Object`→Default,
has `.ParentName` only→Defered, neither→Empty), `CreateDeferedTemplate`, `CreateCom`,
`goLua_RegistObject`. That sequence makes the silent branch + every crossing visible.

---

## §4 Pointers
- C control-flow: `ghidraRE/output/flow_model.json` · wire/object dict: `protocol_schema_index.json` / `PROTOCOL_QUICKREF.md`.
- Construct mechanism + dead lanes: `server/re/D-create-leg-onloadplayers.md`; route map `NORTH_STAR.md`; live state `CODEX_LATEST_STATUS.md` (top).
- Lua-eval seat recipe: `ghidraRE/output/claude_lua_eval_string_recipe.md`.
