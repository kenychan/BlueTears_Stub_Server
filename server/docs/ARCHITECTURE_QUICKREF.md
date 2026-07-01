# ARCHITECTURE_QUICKREF — faithful host, one screen

Plan at a glance. **Detail** = `architecture.md` · **rationale/prior-art** = `../../docs/superpowers/specs/2026-06-28-faithful-server-architecture.md` · **live verdict/blocker/next** = `CODEX_LATEST_STATUS.md` (top, repo root). **Update this + `architecture.md` together whenever a module, keystone, or blocker changes** (live state stays in `CODEX_LATEST_STATUS.md`, not here).

## 🏗️ HOST BUILD STATUS (2026-07-01 — the product is built + stub-free)
**Start point = `server/run_server.py`** (the modular faithful host), NOT the legacy stub. `server/src` is in
THREE tiers (map = `server/SERVER_QUICKREF.md §1`): **`engine/`** = substrate (M-LUA/M-API/M-CODEC/M-REPL/M-OBJ/M-DB) ·
**`char/`+`account/`+`charactermanager/`** = per-Lua-object host ops (mirror `ghidraRE/output/flowmodels/client/<object>.md`) ·
**`net/`** = the GSS wire transport, all byte-verified + **carved fully STUB-FREE** (login handler = real code in
`net/connection.py`; `Res/gss_stub_server_v4.py` is FROZEN LEGACY, archivable). **E1/E2 LIVE-VERIFIED** through
`run_server.py` → `UIIntro:ShowJobClassSelection`. Client-side native RE = `ghidraRE/output/flowmodels/client/` (by object,
`_MASTER.md` + `gworld.md`). Remaining = **E3** (the char replication ENVELOPE; `gworld.md §5`).

## 🔀 CURRENT LANE (LATE-137, 2026-07-01 — read FIRST; supersedes the framing below where they conflict)
**Make the LOGIN FLOW faithful, then find the missing per-char trigger from the Lua + ALL logs — chars are FETCHED by
relation, not pushed/injected.** The clean flow already reaches char-CREATION (login OK → `Client_FinishLoadingCharacter|
true|0` = count=0 → `ShowJobClassSelection`). The Lua truth (`TPlayerCache.lua`/`CharacterManager.lua`/`TAccount.lua`):
`LoadPlayers → getByRelation(accountId,"TAccount","CharacterManager") → objectList → onLoadPlayersCacheResult`;
`getByRelation`=RESOLVE-ONLY (returns chars that ALREADY exist as **havings of the account's CharacterManager**); a char
enters that relation via `serverCreateCharacterResult → GWorld:FindObject(oid):BackupReplicate(true)`. **TAccount = 4 Coms
(Session+CharacterManager[havable]+CharacterHolder+CSharedData), each with a `LuaOnClientLoad` handshake — not just a char
holder; the stub sends CSharedData EMPTY = a faithfulness gap.** PLAN: **E1** recreate the clean char-creation flow + trace
ALL channels (GAP `client_gap.log` + Lua log + `exp_dump_bytes` data-shape on the 4 Coms + `getByRelation`/`LoadList` seats)
to pin what's missing + the SHAPE + any missed Lua step → **E2** TAccount fully faithful + diff → **E3** replicate the char
so the client's OWN code builds it into the relation (`0x05` BackupObj = LEGACY candidate, parked). The wrapped-`0x03`
NewObject lane (LATE-133/134) is WRONG/abandoned. Live state = `CODEX_LATEST_STATUS.md` LATE-137.

## ✅ COMMITTED ROUTE (2026-06-29, user-directed) — FAITHFUL SERVER, no shortcuts
**We build the faithful host that the live client connects to. The scripted stub (`gss_stub_server_v4.py`) and NpSlayer injection are TEST/RE tools ONLY — their ceiling is reached** (D1: the tag-1 replication lane is dead; D2: in-client injection reaches the char-select model but injecting your way in-game = endless holes). No more forcing/patching as the product. The host runs the shipped server Lua over a real object graph and drives the client's OWN construction.

## KEYSTONE PIVOT (LATE-48/49/50) — native construction of the FULL TAccount TREE, NOT tag-1
The dead lane is **tag-1/ServerComputer replication** (`c35140`, D1 = FAIL, structurally unreachable). But the `obj+0x18` writer **`bd2cd0` has a REACHABLE caller: native construction** — the client builds `CharacterManager` by name (`bb2580("CharacterManager")`+`bd2cd0`), and `CreateLocalObject` builds a full char with `obj+0x18`. The reachable `NewObject` is **`bff990` (key03-NORMAL, NOT the dead `c35140` key03-master)**; its owner ref is read FROM THE WIRE (host-controlled), not from the dead ServerComputer (LATE-50, `../re/D-create-leg-onloadplayers.md`).

**The construction is rooted at `TAccount`, not at the chars** — the whole owner tree must be built top-down:
```
TAccount  (root account object — construct FIRST; BLOCKER 1)
 ├─ Session          (component → the login self-gate obj+0x18[Session])
 └─ CharacterManager (component → THE OWNER of the chars; materialized by-name during onLoadPlayers)
      └─ TPlayer, TPlayer, …  (the chars — leaves, in CharacterManager+0x28 std::map)
```
⇒ faithful = construct `TAccount`(+Session) → attach `CharacterManager`(owner) → construct each `TPlayer` char (key03-normal NewObject + **component tail**, owned by the manager). A char "lacks an owner" only when built without this tree. No tag-1, no injection.

## ⚠ CONSTRUCTION IS CLIENT-LOCAL — the server drives it with DATA, never by injecting objects (LATE-55, live)
**Decisive (LATE-55):** moved the stub pid base to `0x5000`; the only object that constructs is STILL the client's own `0x3e9` (TClient bootstrap), and NO stub-emitted object reaches `bff990`. ⇒ **server/host-emitted NewObjects NEVER construct — the `ca47e0`→`ca492b`→`bff990` lane processes the CLIENT'S OWN locally-built objects.** Model A (inject a char NewObject) is **DEAD** (explains ~30 sessions of failed B4 injection). Converges with D2 (`CreateLocalObject` builds locally) + s47 (`obj+0x18` filled by the client's own Lua/full-construct, not replication).
- **Client-local construct lane** = `bff990` (via `ca47e0`/`ca492b`): the client builds ITS OWN objects (e.g. `0x3e9` TClient). NOT server-injectable.
- **Server's lever = the ACCOUNT/SESSION RPC + DATA**: `onLoadPlayersCacheResult a7fd80` — the host sends the char RECORDS; the **client's OWN `a7fd80`/Lua constructs each char locally** (`obj+0x18` from its per-class construct) → `LoadList a620c0` places it in `CharacterManager+0x28` → row.
```
TAccount     ├─ Session (obj+0x18 self-gate; master-stubbable so far)
             └─ CharacterManager (bb2580 by-name) └─ TPlayer chars  ← built by the CLIENT from onLoadPlayers DATA, not injected
(ClientWorld ├─ TClient 0x3e9 = the client's OWN connection bootstrap — NOT a TAccount node, NOT a template for the char)
```
**⇒ Model B (committed):** the host drives the client's onLoadPlayers handler with faithful char records so the client builds the chars natively. Remaining wall = the create-leg char-element resolution (`005e5650` type path), reframed as "make the data drive the client's local construction", NOT "inject a NewObject".
**Persistence invariant (user-directed):** the end state needs a REAL DB-backed `TAccount` (account→CharacterManager→chars), read/written legally — only the session/master handshake is stubbable so far (in-game may need more). We cannot escape a full legal TAccount.

## ★ ROOT PATTERN — Message-Triggered Local Construction (MTLC) — live-proven (LATE-67/69), expected to RECUR for every rendered object
**The client never authors GWorld objects standalone, AND never deserializes a whole object off the wire. It builds each object LOCALLY by running its OWN construction code, TRIGGERED + parameterized by a server MESSAGE + DATA.** The packet carries a trigger/name/data — **NOT** the object's serialized bytes; pids/ids are **client-assigned** (not on the wire).
- **Live proof (LATE-67/69):** the `0x1001` challenge — payload is JUST the name `"local"` (`build_native_session_in_challenge`) — drives `ca4540`→`ca47e0`→`bff990`→`bd4b20` to **construct the Connector `0x3e9` locally** (full object, `obj+0x18` set). No object bytes on the wire; pid `0x3e9` assigned by the client.
- **Corollary (kills ~30 sessions of dead ends):** forging serialized objects on the wire is FUTILE — the client does NOT rebuild objects from wire bytes. So the **inline tag-0x07 object-value** and **hand-forged `bb0060` component tails in onLoadPlayers** were always doomed (LATE-56→59). The `bb0060` tail is the client's *local* construction payload, never an RPC-arg/wire object.
- **The faithful host's job, everywhere:** send the right **trigger MESSAGE + DATA**; the client builds. "Drive with data, not bytes."
- **Expected to RECUR:** chars, avatar, NPCs, items, world residency — each is built this way. For each: find the **trigger message + data**, do NOT serialize the object. Two tiers carry triggers: **lower-protocol** (`build_lower_gss_body(opcode,payload)`; `0x1001`/`0x1003`/`0x1006` = connection/handshake → builds connection infra) and **RPC downcalls** (`build_*_downcall`; `logInClientResult`/`onLoadPlayers`… → account/char tier). `onLoadPlayers` only RESOLVES refs (`ba9420`); the char-BUILD trigger is an earlier login-RPC, still being traced.

**Per-wall corollary:** char-select reduces to: the **TAccount tree** (account → Session + CharacterManager → chars) must be built locally, top-down, each via its own trigger message, so the **Player-bearing** char ends up with `obj+0x18` set (`bd2cd0` path) and registered → `onLoadPlayers` refs resolve → row.

## Two gates to a rendered row
- **Gate 2** (char-list slot): char `obj+0x18` empty → `getAllPlayers` skips it → no row. `obj+0x18` is written ONLY by `bd2cd0` during native construction.
- **Gate 1** (3D body): `CBody LoadingEnd` must fire ×count → `UIIntro:Show()`. Needs a loadable char body. (The list *slot* appears at `AddCharacter`, before the body.)

## Scope verdict: M0 = HEAVY (2026-06-28 LATE-29; route committed LATE-49)
Confirmed HEAVY, but the keystone is **native construction driven by a faithful owner tree**, NOT tag-1 replication. D1 proved tag-1 (`c35140`) dead; D2 proved the downstream (constructed char → `getAllPlayers` → gates → `AddCharacter` model-insert) has NO wall. So the remaining work is: host builds the owner tree → client constructs each char natively (`owner!=0`, `obj+0x18` set) → display data + body for render. Template realization (`reload_file`, Stage A — proven) feeds the native construction.

## Modules (★ = keystone) — build order M-OBJ/M-LUA/M-API → M-BOOT(owner tree) → M-REPL(native-drive)
| Module | Responsibility | Status |
|--------|----------------|--------|
| ★M-OBJ | host GWorld objects · components · `obj+0x14`/`obj+0x18` · OIDs | **built, self-test PASS** |
| ★M-LUA | run shipped server Lua (`Class`/`Template`/`CharacterManager`) in Lua 5.1 | **built, self-test PASS** |
| ★M-API | bounded goEngine natives + Template/Com construction; shipped `LoadPlayers` | **built, self-test PASS** |
| ★M-BOOT | orchestrate the **owner tree**: account → session → CharacterManager(owner) → chars | **new — the keystone now** |
| ★M-REPL | drive the client's **native** construction of that tree (reachable `bd2cd0` path, `owner!=0`); NOT tag-1 | redesign — see pivot |
| M-CODEC | wire variant codec + NetVar descriptor projection (`bef480`/`bafdc0`/`bb0060`) | partial (built + in stub) |
| M-NET | master transport + GSS/XOR/zlib crypto, framing, the login connection handler | **modular + stub-free: `server/src/net/` (crypto·framing·packets·login·connection), live-E1-verified** |
| M-DB | accounts / chars persistence | **built, self-test PASS** |
| M-WORLD / M-TICK | zone/object residency · replication tick (for in-game) | new, after construct |

## ⚠ Current keystone (M-BOOT + M-REPL native-drive)
NOT "emit a tag-1 `BackupReplicate`" (that lane is dead). The problem is: **what faithful server sequence makes the client build the owner tree natively?** The client already builds `CharacterManager` via the reachable `bb2580("CharacterManager")`+`bd2cd0` create-leg; `bff990` constructs a char given `owner!=0`. Decisive next = RE the `onLoadPlayers` native handler's per-char construction: what owner/parent it reads, and the message that drives it — then have the **host (not the stub)** supply that. Detail: `../re/D1-c35140-dispatch.md` (reframe §, lines 112-126).

## Target data flow (faithful, no injection) — TAccount tree, top-down
host (DB → shipped `LoadPlayers` builds the account→CharacterManager→char graph in its own Lua VM) drives the **post-channel login conversation** over the live transport:
1. **`TAccount` construct** (key03-normal NewObject + `Session` component tail) → login self-gate `obj+0x18[Session]` set (BLOCKER 1).
2. **`CharacterManager`** materialized by-name during `onLoadPlayersCacheResult` (`a7fd80`: `bb2580`+`bd2cd0`) → the owner.
3. **each `TPlayer` char** = key03-normal NewObject (`bff990`) + its **component tail** (`Player`/`CStatus*`/`CBody`/`CEquipment` + NetVar fields), owner ref → CharacterManager.
4. `LoadList` (`a620c0`) resolves each → `CharacterManager+0x28` (std::map) → `getAllPlayers` → `AddCharacter` → (+ display data, Gate 1 body) → **ROW** → enter game.

## Milestones
M0 HEAVY ✅ · D1 (tag-1) = FAIL ✅ · D2 (downstream of construct) = no wall ✅ → **M-BOOT: faithful owner-tree drive (current)** → M1 char constructs natively on the live client → M2 enter game with a template char → M3 char acts → M4 full 3D render. De-risk records: `derisk/D1-keystone-replication.md`, `derisk/D2-constructed-char-row.md`.

## Pointers
**construct-protocol abstraction (lanes · data flow · MTLC · packet/format · open trigger) = `CONSTRUCT_PROTOCOL.md`** · detail `architecture.md` · RE `../re/D1-c35140-dispatch.md` · live `CODEX_LATEST_STATUS.md` (top) · prior-art/refs spec §6 (module→ref index in `architecture.md`) · test loop `py server/tools/derisk_run.py`
