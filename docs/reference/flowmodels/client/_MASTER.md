# CLIENT flowmodels — MASTER (how the engine objects call each other, following the Lua)

This folder = the **client-side** flowmodel tree: one file per **engine object** the Lua calls into,
holding the NATIVE functions that back that object's Lua methods (what the client's C actually does).
The **server-side** implementation of the same objects lives in the parallel tree `server/src/<object>/`
(see `server/SERVER_QUICKREF.md`). Split rule (user, 2026-07-01): **client-invoked → this tree;
server-invoked (GWorld/ServerCom/server handlers) → the server tree.** Organized **by engine object**.

Each per-object file follows the `gworld.md` template: Lua surface → native functions (cited) →
nodes/edges → open unknowns. Grounded in `LUA_QUICKREF.md` + `flow_model.json` + the `claude_*.md` RE.

---

## The flow (login → roster → enter), by engine object

```
                         ┌─────────────────────────── CLIENT process ───────────────────────────┐
 connect      TClient ─▶ clientworld.md  (FUser::connect → FClientWorld::Connect → ca47e0 → bff990)
                         │   builds TClient(Connector) LOCALLY, registers in GWorld OID table
                         ▼
 sign-in      Session ─▶ session.md   signInClientResult → user:onSignInResult          [Session.lua:10]
                         ▼
 login        Session ─▶ session.md   logInClientResult  → user:onLogInResult           [Session.lua:117]
                         │   TAccount + its 4 Coms built by-name (bb2580): see taccount.md
                         ▼
 roster fetch TPlayerCache ─▶ tplayercache.md  LoadPlayers → getByRelation → onLoadPlayersCacheResult
                         │      getByRelation = RESOLVE-ONLY (native 01109fa0); resolves chars that ALREADY
                         │      exist as havings of the account's CharacterManager (does NOT construct)
                         ▼
 resolve      CharacterManager ─▶ charactermanager.md  each objectList elem → ba9420 resolve
                         │      unresolved → nil → Lua deletes slot → empty roster → count=0
                         ▼
 count gate   Session ─▶ session.md   Client_FinishLoadingCharacter(result,count) → user:onFinishLoadingCharacter
                         │      count==0 → UIIntro:ShowJobClassSelection (creation screen — E1 END, reached ✅)
                         │      count>=1 → char-SELECT roster                              ← needs a resident char
                         ▼
 row paint    UIIntro ─▶ uiintro.md   AddCharacter(event) → row (reads CStatus level/job, CBody preview)
                         └───────────────────────────────────────────────────────────────┘

  SERVER side (the host emits these — server/src tree):
    charactermanager (server): serverCreateCharacterResult → GWorld:FindObject(oid):BackupReplicate(true)
    ← THE char-residency trigger (server→client push). The client then builds the char LOCALLY (MTLC)
      via clientworld/bff990 and files it into GWorld's OID table under the CharacterManager relation.
    OPEN UNKNOWN = the replication WIRE ENVELOPE (gworld.md §5).
```

## Object index (client tree)

| file | engine object | client role (Lua methods → native) | server counterpart |
|---|---|---|---|
| [gworld.md](gworld.md) | **GWorld** (native world singleton) | `FindObject` resolve on the OID table; the registration primitives (`bff990`/`647860`/`bd4b20`/`bd2cd0`) | `server/src/world/` |
| clientworld.md | ClientWorld (native client world) | `Connect`, `CreateLocalObject`, `FindObject` — the LOCAL build path | — (client-only) |
| session.md | Session (Com of TAccount) | `signInClientResult`/`logInClientResult`/`Client_FinishLoadingCharacter` client handlers | `server/src/session/` |
| taccount.md | TAccount (4 Coms) | built by-name (`bb2580`); Session+CharacterManager+CharacterHolder+CSharedData | `server/src/account/` |
| charactermanager.md | CharacterManager (havable Com) | `getAllPlayers`/`AddCharacter`/`LoadList`(a620c0)/ba9420 resolve | `server/src/charactermanager/` |
| tplayercache.md | TPlayerCache | `LoadPlayers`/`getByRelation`/`onLoadPlayersCacheResult` | `server/src/charactermanager/` (fetch) |
| player.md | Player (char Com) | `LuaOnClientLoad`→`ClientGetSRQL`; identity `m_AccountId`/`m_name` | `server/src/char/` |
| cstatusplayer.md | CStatusPlayer (char Com) | `GetAttributeValue`/`GetJobClass`/`m_nGender` (ROW columns) | `server/src/char/` |
| user.md | User (controlled-player Com) | in-game identity — **DROPPED for the ROW** (LATE-134 disconnect) | `server/src/char/` |
| csharedata.md | CSharedData (OwnerOnly Com) | `LuaOnClientLoad` from `GWorld.m_SharedData` (client-local) | `server/src/account/` |
| uiintro.md | UIIntro | `AddCharacter` row paint; `CreateTUIControlObject` 3D preview | — (client UI) |
| template.md | template system (globals) | `goLua_CreateObject`/`goLua_RegistObject`/`reload_file`/`Template`/`DefaultImpl`/`EmptyImpl` | `server/src/template/` |
| object.md | Object methods (any object) | `AddComponent`/`GetBaseComponent`/`GetCom`; `BackupReplicate`/`ForceReplicate` | `server/src/repl/`, `server/src/obj/` |
| env.md | globals | `GetNObject`(72×)/`GetEnvironment`(72×)/`GetNID` — pervasive native accessors | `server/src/api/` |

## Notes
- Objects appearing on BOTH sides (Session, CharacterManager, Player…) are **Lua CLASSES with client
  AND server methods** — the same .lua file, split by which process invokes each method. The native
  primitive underneath (build/register/replicate) is shared; the client file documents the client
  handlers, the server tree implements the server ones.
- Only `gworld.md` is deep so far (it's the residency crux). The rest are the enumerated surface —
  populated with Lua signature + call site + native mapping where established, `OPEN` where not.
