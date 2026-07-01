# Milestones

Long-standing goal (30+ sessions): a character **ROW** in char-select → **enter game** →
char acts → ... Faithful, via the real client flow (no force-patching the engine self-gates).

## State

- **M0 — set scope (DONE = HEAVY).** Faithful construction needs the host; the scripted stub
  can't construct the char (LATE-29).
- **M-LUA feasibility (DONE = GREEN, LATE-36).** The shipped goEngine shared Lua RUNS server-side
  in a standalone Lua 5.1 VM (`lupa.lua51`): `Lib/Class.lua` loads unmodified; class define →
  instantiate → Init → NetVar default → field get/set → method dispatch → `isa()` all pass over
  PLAIN TABLES (the metamethods `__goLua_index`/`newindex`/`classindex` are shipped Lua). The host
  owes only a bounded native shim (≈6 plumbing + ~15 domain verbs); even wire `Pack` is
  shipped-Lua-driven. ⇒ HEAVY = "run the shipped Lua over M-OBJ", not a logic reimplementation.
  Module: `server/src/lua/runtime.py` (self-test GREEN: `py server/src/lua/runtime.py`).
  **NEXT:** bridge native base classes NObject/Computer/Component to M-OBJ HostObject `.new`, wire
  domain natives, run real `TPlayerCache:LoadPlayers` → char objectList.
- **M-API LoadPlayers (DONE = GREEN, LATE-37).** The REAL shipped `TPlayerCache:LoadPlayers`
  server-RPC runs end-to-end on the host: domain natives (`GetEnvironment`/`GetNObject`/
  `GWorld:FindObject`/`getByRelationImpl`) wired over a registry of live Lua objects →
  `getByRelation`→`Callback`→callback→`session:onLoadPlayersCacheResult` delivers a 2-char
  objectList. Module `server/src/api/host_context.py` (self-test GREEN). **THE SEAM:**
  `onLoadPlayersCacheResult` is a goEngine RPC (no Lua def) — server-side it marshals the objectList
  to the client, whose NATIVE onLoadPlayers handler builds the rows. ⇒ host now produces a real
  server objectList; the remaining crux is M-REPL/M-CODEC (marshal via shipped `Class.Pack`) + the
  LIVE test of whether the client's native handler constructs a ROW from FAITHFUL bytes.
- **Host emit stack COMPLETE (LATE-39→42, all self-tests GREEN).** M-OBJ+M-LUA+M-API+M-CODEC+M-REPL +
  the Template/Com object-construction layer. ⚠ TWO serializers: M-CODEC tag-variant (writer `bb7e80`
  pinned) = RPC args; the construct/replication lane uses the `bb0060` descriptor-keyed component
  format (`component_tail`, partly already in the stub). Faithful FULL char = a large package-boot
  grind (Gate-1 render only). DECISIVE blocker = the construct LANE (D1) — gated on the UAC live test
  + a strategy call (replication-lane vs enter-game/avatar).
- **M-DB persistence (DONE = GREEN, LATE-44).** `server/src/db/store.py`: `CharStore` (JSON-backed
  account→chars + per-char `xData` blob) + `DBNatives` installing the shipped
  `DBCommandExecute(pid,"SP2_LoadPlayerData",{uid},cb)` native (`Session.lua:194`; result-set shape
  `result[1][1].xData`). FaithfulHost gains optional `db=` (persist on `setup_account`, `load_account`
  rehydrates from disk). Suite 8/8 GREEN. Optional infra — does NOT move the row/construct milestone.
- **De-risk D1 — keystone replication (DONE = FAIL).** The tag-1/ServerComputer construct lane
  (`c35140`) is structurally unreachable on a normal connection (`c33210`/`c33720` refs=0, never
  fire live; only flag0 `bfe230`). `re/D1-c35140-dispatch.md`. ⇒ M-REPL-as-tag-1 is dead.
- **Construct-primitive RE (DONE, this session).** The ONE reachable full-construct is the
  **control-object path `c5a880`** (TClient ctor + 18× `bd2cd0` obj+0x18 writes) +
  `SetControlObject`→`DAT_0184a90c` residency, on the flag0 path. In-client force (CreateLocalObject
  / GWorld:CreateObject / goLua_AddComponent) all walled. `re/D-c5a880-control-construct.md`,
  `re/D-obj18-writers.md`.

  **⇒ LADDER RE-ORDER:** the char-LIST row has no reachable client-local construct (needs the dead
  tag-1 lane); the **AVATAR / enter-game does** (c5a880). Target enter-game-with-a-template-char
  FIRST (matches the long-standing "…→ enter game with a template char →…" wording).

- **M1 — ENTER GAME with a template char** (was M3). Goal: host drives the client's control-object
  construction (`bfe230`→`bc05c0`→`c5a880` + SetControlObject residency) for a template TPlayer
  avatar → resolvable, GWorld-resident, obj+0x18-populated → world entry.
  - **⚠ NOT yet shown char-drivable.** "Re-aim SetControlObject at the char" = a re-tread (LATE-24,
    negative). `c5a880` is m_ControlObj-specific and its trigger is unmapped. **Prereq RE (bounded):
    what triggers `c5a880`, and is it control-object-identity-bound (a char can't ride it → wall) or
    drivable for a chosen object?** `re/D-c5a880-control-construct.md`. Until answered, do not build.
- **M2 — char acts in world.** Continuous replication / movement / interaction.
- **M3 — char-select ROW** (deferred; structurally hardest). Needs server-replicated non-control
  char objects the client mirrors — no reachable lane yet (tag-1 dead). Revisit after the avatar
  construct primitive is built (it may generalize).
- **M4 — full 3D render.** `Gate 1`: `CBody` body-load → `Show`. Needs a loadable body.

## Notes

- M1 milestone = mirror to `qqxj_server` + `Milestone:` commit (PROJECT_RULES §19).
- Each milestone: verify primary `NpSlayer_exp.log` evidence THIS run before claiming.
