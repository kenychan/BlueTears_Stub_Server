# GWorld — client-side flowmodel (native world registry + the char-registration path)

**Purpose (NORTH_STAR §4):** consolidate, in ONE place, what the client's native `GWorld` is, the
subfunctions it fronts (construct → OID-register → component-attach → resolve), and where the ONE
open unknown sits — the trigger that makes the client natively build+register a **char** — so the
faithful host (`server/run_server.py` + `server/src`) can emit that trigger for E3.

Every claim cites the RE doc that established it (verify-before-claim). Addresses are in
`NClient_unpacked.exe` / `qqxj_tw.gpr`. This file is the client-RE companion to
`ghidraRE/output/flow_model.json`; the host-side counterpart is `server/SERVER_QUICKREF.md §2`.

---

## 0. What `GWorld` is

`GWorld` is a **native engine world singleton** (goEngine/Firenze over Gamebryo), exposed to Lua as a
global. It is present on BOTH the client and server processes — the real client HAS a full GWorld; we
never build a client GWorld (`char-delivery-server-side-replication-late144`). Lua reaches many engine
services through it (`GetGameTime`, `SetDisconnected`, and crucially `FindObject`). It is NOT a
"master function" defined in Lua — it is native (`claude_gamebryo_carveout_map.md:49` lists
`GWorld`/`ServerWorld` inside the goEngine carve-out, not the shipped Lua).

For the char question, the ONE GWorld service that matters is:

> `GWorld:FindObject(oid)` → returns the object registered under engine OID `oid`, or nil.

Server Lua uses `GWorld:FindObject` (`CharacterManager.lua` serverCreateCharacterResult:85,
LuaCheckAccountValidation:129 with `ServerCom`); client Lua uses `ClientWorld:FindObject`
(`Player.lua:518/1004/1029`). Both front the SAME native OID→object table on their own process.

---

## 1. The three maps (THE crux — prior sessions conflated them)

Source: `claude_ba9420_resolver_lever.md:102-124`.

| # | structure | addr / owner | keyed by | role |
|---|---|---|---|---|
| A | descriptor / object-resolver map | `DAT_01825258` | string/descriptor identity | what `bb5f40`/`005e5650` and the **wire ObjectPtr** consult |
| B | per-class descriptor singleton cache | `DAT_0184a90c` | (single slot) | lazily caches the *type descriptor*; `+0x24` is a vtable method. **NOT an object registry** |
| C | **GWorld OID→object table** | GWorld/world-manager member (the `manager+0x1c` map, `claude_loginserverresult_wire_spec.md:93`) | engine OID / `GetID()` | the directory `GWorld:FindObject(oid)` reads |

**Decisive consequence:** a `CreateLocalObject`'d dummy has `GetID()==1` but `FindObject(1)` fails —
it was never inserted into map **C**, and the wire ObjectPtr's scalar is matched against map **A**
(string-keyed), where it also isn't. So an isolated local build is invisible to both resolution paths
(`claude_ba9420_resolver_lever.md:118-128`). **Residency = being inserted into map C by the native
construct path, not just existing as an object.**

---

## 2. Native registration primitives (the subfunctions GWorld fronts)

### 2.1 `FUN_00bff990` — the full-construct / NewObject entry (`bff990`)
The reachable `NewObject`. Dispatched via key-table `@0xbfdb40` (NOT `bbf750`/`0x183e0e0`, LATE-51/52
falsified). Reads the wire/reader fields and writes them into the new object (`flow_model.json`
`0x00bff990` node):
- 1st u32 → `object+0x4c` (GetRepTime/SetRepTime)
- 2nd u32 → used for the **map-C lookup** and stored at `object+0x3c` (the server pid)
- 3rd u32 → `object+0x38` (`m_nTName`, the type NID)
- bool → `object+0x54`

Internally it does a **find-or-insert against map C**:
- **lookup** `0x00647a30` at `0x00bffa8f` (`claude_addcomponent_target.md:59`)
- **insert** `0x00647860` on the miss path (`claude_newobject_hook_anchors.md:103`
  `0x00bffadc: CALL 0x00647860`)

then calls the component tail **`0x00bd4b20`** before returning, plus **`0x00c024b0`**
(`flow_model.json` `0x00bff990`). `OnNewObjectCreated` recognizes `TAccount` and resolves `Session`;
`OnSigSetControlObject` splits `TClient→Master_Connecting` vs `TAccount→Master_SignIn`.

**Owner gate:** `bff990` succeeds only for `owner!=0`; an isolated (ownerless) build is why a char
"lacks an owner" (PROJECT_RULES keystone pivot). The fix is structural — build the char as a subnode
of an owner (CharacterManager), not force-patch `obj+0x18`.

### 2.2 `0x00647860` — OID-map INSERT (`oid_map_647860`)  ·  `0x00647a30` — OID-map LOOKUP
Insert is called from ~113 sites across construction (`claude_addcomponent_target.md:40-54`,
`claude_loginserverresult_argunpack.md:10`). Lookup `647a30` is the read side; the OID→object
**resolver** is `FUN_006481e0` (calls `647a30`) → emits **"No Object for OID"** when absent
(`claude_loginserverresult_wire_spec.md:40`). This is the native body of `FindObject(oid)`.

### 2.3 `FUN_00bd4b20` — component tail (writes `obj+0x18`)
Called inline by `bff990`. Writes `object+0x18` **only after** a non-empty component iterator and a
non-null `0x00bb2580` materializer result (`flow_model.json` `0x00bd4b20`). The key03 NewObject tail
alone does NOT populate `obj+0x18` (profile16, `claude_loginserverresult_wire_spec.md:52`).

### 2.4 `FUN_00bd2cd0` — the `obj+0x18` writer (self/RPC-routing map)
`obj+0x18` = the self/RPC-routing map (this-object's RPC-routable components), written ONLY by full
construction / `bd2cd0` (memory `obj18-session-is-template-component`; `claude_obj14_vs_obj18_correction.md:25`).
`AddComponent` (handler `FUN_00bcf510`) reads one component ObjectPtr via `ba9420` then
`FUN_00bd2cd0` inserts it into the target's `obj+0x18` keyed by descriptor ordinal
(**Player=1 / CStatusPlayer=2 / User=4**) (`claude_loginserverresult_wire_spec.md:47-53`).
Distinct from `obj+0x14` = the component container (template-clone + wire tail).

### 2.5 `FUN_00bb2580` — by-name component/type materializer
Builds a component/type by name (e.g. `bb2580("CharacterManager")`) — the non-null result `bd4b20`
requires. Part of how the account tier builds CharacterManager natively (LATE-62/64).

---

## 3. The two resolution paths (do not conflate)

```
WIRE ObjectPtr (scalar) ──▶ +0x28 extractor ──▶ 005e5650(key) ──▶ node in DAT_01825258 (map A)
                                                                    └─▶ ba9420(L,idx) unboxes ──▶ Lua object
   miss ⇒ nil element ⇒ Lua list deletes it ⇒ empty getAllPlayers/LoadList ⇒ no row
   (claude_ba9420_resolver_lever.md:96-100, loadlist-empty-nil-element-deletion)

GWorld:FindObject(oid) ──▶ FUN_006481e0 ──▶ 647a30 lookup in map C (manager+0x1c) ──▶ object | "No Object for OID"
```

`ba9420` itself consults NO C++ registry and matches NO OID — it just unboxes the Lua stack slot
(`claude_ba9420_resolver_lever.md:28`). The resolution happened upstream (map A for the wire path,
map C for FindObject).

---

## 4. Two tiers of registration (same primitives, different owner)

| tier | owner node | the "OID object" | how chars/objects enter | resolves via |
|---|---|---|---|---|
| **account (char ROW)** | `TAccount` → `CharacterManager` (havable) | each `TPlayer` char | host registers char as an **owned CharacterManager having (owner≠0)** then `BackupReplicate`s it; client builds the ghost locally (MTLC) | server: `GWorld:FindObject(oid)`; then `CharacterManager+0x28` (`std::map<int,Object*>`) → `AddCharacter` → ROW |
| **world (ENTER game)** | world / `World+0x188` Computer | the controlled `TPlayer` | `NewObject(controlled)` + `NewObject(Player/CStatusPlayer/User)` + `AddComponent`×3 + `DownCall logInServerResult(arg7=OID)` | native `FindObject` on map C; checks obj+0x18 carries Player/CStatusPlayer/User |

The **enter-game leg is fully pinned** (`claude_loginserverresult_wire_spec.md:43-94`): World present,
`World+0x188` Computer present, the OID object present in map C, and that object carrying
Player/CStatusPlayer/User in `obj+0x18`. This is the DOWNSTREAM proof that a correctly-registered
object with the right comps traverses the whole path — it de-risks E3's tail.

---

## 4b. The SERVER-SIDE construct+register API — `World:CreateObject` (PINNED 2026-07-01)

**Q: what is the API entry that registers a TPlayer into GWorld?**
**A: `World:CreateObject("TPlayer")`** — server-side the shipped Lua calls
`GetEnvironment():GetNObject("ServerWorld"):CreateObject("TPlayer")` (`Res/tw_lua_decompiled/Class/Main/CStatus.lua:812`;
also `Class/Main/Player.lua:1021-1114`). This is the ONE-CALL construct **and** register: it builds the
object and files it into that world's OID→object map (map C) so `World:FindObject(oid)` resolves it.

Provenance / native binding:
- `CreateObject` is **native** — it has NO Lua definition anywhere in `Res/tw_lua_decompiled/` (the only
  `CreateObject = function` is the unrelated `RequestCreateObject` in `CMapObjectManager.lua:306`). It is a
  method inherited by `FServerWorld` (`Class/Server/FServerWorld.lua`) from a base World class, bound into
  the Lua VM as a native world method.
- Lower shell factory: **`goLua_CreateObject` = `FUN_00c48010`** (the Template/DefaultImpl path's NiObject
  shell maker; `claude_obj18_session_no_wire_writer.md:50`). `World:CreateObject` is the higher one-shot
  wrapper: construct → **`647860` OID-insert into map C** (§6) → the object is now a GWorld resident.
- Same pattern on the client side: `GWorld:CreateObject` is used by `User.lua:362`, Portal, PartyChannel,
  CMapObjectManager, InstanceMapManager, CashProductManager — all the native one-shot construct+register.

**Scope of this finding.** This pins the SERVER-side register API (what the *host* should call to make a
char a real resident — see §7 step 1). It is **distinct from** §5's client-side receiver trigger: making the
char resident server-side does not by itself make the *client* build its local ghost — that still needs the
replication envelope (§5). But it removes the guesswork on the host side: the faithful host binds
`World:CreateObject` in M-API and lets the shipped Lua construct+register the char, instead of hand-poking
`self.gworld[oid]=obj`.

---

## 5. THE OPEN UNKNOWN — the char-registration trigger (E3 blocker)

Everything above is the machinery. The one thing NOT yet pinned natively is the **trigger** that
drives the client's own `bff990`+`647860` to build+register a **char** at the account tier — the
analog of the proven TClient trigger:

```
TClient  (proven, MTLC): 0x1001 ─▶ ca4540 ─▶ ca47e0 ─▶ bff990 ─▶ (647a30 lookup / 647860 insert) ─▶ map C
CHAR     (UNKNOWN):      ??? replication envelope ─▶ ??? ─▶ bff990 ─▶ 647860 ─▶ map C ─▶ CharacterManager having
```

Established boundaries (do NOT re-walk — dead lanes):
- A **pushed wire `NewObject("TPlayer")` + tail is DEAD 4×** — the char never reaches `bff990`, only
  `TClient` does (LATE-74/142/143, FORK-A, LATE-16). The wall is the TRIGGER, not the byte format.
- The char is a **server-side resident delivered by `object:BackupReplicate(true)`**, not a pushed
  object (`char-delivery-server-side-replication-late144`, `CharacterManager.lua`
  serverCreateCharacterResult:89).

So the missing artifact = **the replication WIRE ENVELOPE** that carries a BackupReplicate'd char to
the client's native receiver so IT calls `bff990`+`647860`. Method to pin it (NORTH_STAR §4): a
**differential trace** of the client's OWN working replication (e.g. TClient's `0x1001` envelope, and
any nearby-object replication in-game) through the same seats, then emit the same envelope shape via
host M-REPL/M-CODEC on the clean E1/E2 login. The host already HAS: (1) server-side GWorld registration
(`server/src/api/host_context.py` `self.gworld[oid]=obj`), (2) object serialization (M-REPL tag-05/tag-07
BackupReplicate bytes). Only (3) the envelope is missing.

---

## 6. Structured flow model (nodes + edges)

```
# NODES  (addr : role)
bff990   0x00bff990 : NewObject / full-construct entry; find-or-insert into map C; writes pid→+0x3c, TName→+0x38, RepTime→+0x4c, bool→+0x54
647860   0x00647860 : OID-map INSERT (map C)  [~113 callers, incl. bffadc inside bff990]
647a30   0x00647a30 : OID-map LOOKUP (map C)   [called by bff990@bffa8f]
6481e0   0x006481e0 : FindObject resolver (calls 647a30) → object | "No Object for OID"
bd4b20   0x00bd4b20 : component tail; writes obj+0x18 iff non-empty comp iterator + non-null bb2580
bd2cd0   0x00bd2cd0 : obj+0x18 writer (self/RPC-routing map); AddComponent inserts here by ordinal
c024b0   0x00c024b0 : post-construct step called by bff990
bb2580   0x00bb2580 : by-name component/type materializer (e.g. "CharacterManager")
ba9420   0x00ba9420 : Lua-stack unbox of a resolved object (NOT a registry lookup)
005e5650 0x005e5650 : map-A (DAT_01825258) descriptor/string resolver (wire ObjectPtr path)
bcf510   0x00bcf510 : AddComponent RPC handler → bd2cd0
dispatch 0x00bfdb40 : key-table that dispatches to bff990
# MAPS
mapA     DAT_01825258 : string/descriptor-keyed resolver (wire ObjectPtr)
mapB     DAT_0184a90c : per-class descriptor singleton cache (NOT a registry)
mapC     manager+0x1c : GWorld OID→object table (FindObject keyspace)

# EDGES
dispatch ──▶ bff990
bff990   ──▶ 647a30 (lookup mapC)  ──miss──▶ 647860 (insert mapC)
bff990   ──▶ bd4b20 ──(comps present)──▶ writes obj+0x18 ; bd4b20 ──▶ bb2580
bff990   ──▶ c024b0
bcf510 (AddComponent) ──▶ ba9420 (unbox comp) ──▶ bd2cd0 (insert into obj+0x18 by ordinal)
GWorld:FindObject(oid) ──▶ 6481e0 ──▶ 647a30 ──▶ mapC
wire ObjectPtr ──▶ 005e5650 ──▶ mapA ──▶ ba9420
# OPEN
char_trigger ??? ──▶ (envelope) ──▶ bff990+647860  [E3 blocker — pin by differential trace]
```

---

## 7. What the host must supply (bridge to the build)

To make a char resident (E3), the host drives the client's OWN registration by delivering the
replication envelope so the client calls the section-2 primitives itself:
1. Register the char server-side as an owned CharacterManager having (`owner≠0`) — host M-API
   (`server/src/engine/api/host_context.py`, `GWorld` registry) — DONE. **Faithful API = bind
   `World:CreateObject("TPlayer")`** (§4b) so the shipped Lua constructs+registers via the real world
   method, not a direct `self.gworld[oid]=obj` poke.
2. Serialize it as a BackupReplicate object — host M-REPL (tag-05 array of tag-07) — DONE.
3. **Deliver #2 in the correct wire envelope** so the client's native receiver runs
   `bff990`+`647860` and files the char into map C under the CharacterManager relation — **the ONE
   missing piece** (§5). Then `onLoadPlayersCacheResult → LoadList → AddCharacter → ROW`
   (`D-create-leg-onloadplayers.md`, matches LATE-137 "fetched by relation, not pushed").

**Do NOT** force-patch `obj+0x18` or hand-forge the char body — both are proven dead; the host's job
is the trigger+data, the client builds (Message-Triggered Local Construction,
`message-triggered-local-construction`).
