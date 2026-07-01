# Faithful host architecture (Option-2 HEAVY)

Working architecture doc for the host — **the authoritative going-forward layout**, updated as
the build proceeds. One-screen version: `ARCHITECTURE_QUICKREF.md` (keep the two in sync). Full
rationale/prior-art: the brainstorm spec
`docs/superpowers/specs/2026-06-28-faithful-server-architecture.md`. Live verdict/blocker/next:
`CODEX_LATEST_STATUS.md` (top) — not duplicated here.

## ★ ROOT PATTERN — Message-Triggered Local Construction (MTLC) — live-proven, recurs for every rendered object

**The client never authors GWorld objects standalone, and it never deserializes a whole object off
the wire. It builds each object LOCALLY (its own construction code) TRIGGERED + parameterized by a
server MESSAGE + DATA.** The wire carries a trigger/name/data — NOT the serialized object; pids/ids
are CLIENT-assigned.

- **Live proof (LATE-67/69, 2026-06-29):** the `0x1001` "challenge" packet — payload is only the
  name `"local"` (`build_native_session_in_challenge`) — drives `ca4540`→`ca47e0`→`bff990`→`bd4b20`
  and the client **constructs the Connector `0x3e9` locally** (full object, `obj+0x18` set). No
  object bytes on the wire; the pid `0x3e9` is assigned by the client, not carried.
- **Why this is THE pattern (not a one-off):** it is the single explanation behind ~30 sessions of
  dead ends. The client is a replication proxy that BUILDS, not deserializes. Therefore forging
  serialized objects on the wire — the inline tag-0x07 object-value, the hand-forged `bb0060`
  component tail stuffed into an `onLoadPlayers` arg (LATE-56→59) — is structurally futile. The
  `bb0060` component tail is the LOCAL construction payload, never a wire/RPC-arg object.
- **The faithful host's job, everywhere:** send the right **trigger message + data**; the client
  builds. "Drive with DATA, not bytes." Expected to recur for chars, avatar, NPCs, items, world
  residency — for each, find the *trigger message + data*, do not serialize the object.
- **Two trigger tiers:** (1) **lower-protocol** `build_lower_gss_body(opcode, payload)` —
  `0x1001`/`0x1003`/`0x1006` connection+handshake, reaches the `ca4540`→`ca47e0`→`bff990` construct
  lane, builds connection infra (the Connector); (2) **RPC downcalls** `build_*_downcall` —
  `logInClientResult`/`onLoadPlayers`/… the account+char tier. `onLoadPlayers` only RESOLVES refs
  (`ba9420`, LATE-65), so the char-BUILD trigger is an earlier login-RPC (under trace).

## Root cause (one invariant behind every wall)

The client is a **replication proxy**. Every char-select/enter-game wall reduces to: a
GWorld-resident, **Player-bearing** object must be **built locally by the client when triggered by
the right server message** (MTLC pattern above) — the client never authors it standalone, nor
rebuilds it from wire bytes.

- `Gate 2` (immediate, char-list): char `obj+0x18` empty → `getAllPlayers` skips → no row.
  `obj+0x18` written only by `FUN_00bd2cd0` during native construction.
- `Gate 1` (downstream, 3D render): `CBody` `LoadingEnd` must fire `count` times for
  `UIIntro:Show()`. Needs a real loadable char body. (The list *slot* appears at
  `AddCharacter`, before the body.)

## ✅ COMMITTED ROUTE — FAITHFUL SERVER, no shortcuts (2026-06-29 LATE-49, user-directed)

We build the faithful host that the **live client connects to**. The scripted stub
(`gss_stub_server_v4.py`) and NpSlayer injection/forcing are **TEST/RE tools ONLY — their ceiling
is reached.** D1 proved the tag-1 replication lane dead; D2 proved that in-client injection reaches
the char-select model but **injecting your way in-game is endless holes** (continuous replication of
the avatar + every nearby object, frame after frame — not injectable). No more forcing/patching as
the product. The host runs the shipped server Lua over a real object graph and drives the client's
OWN construction.

## M0 verdict = HEAVY (2026-06-28 LATE-29) — keystone PIVOTED (LATE-48/49)

HEAVY confirmed. But the keystone is **native construction driven by a faithful owner tree**, NOT
master/tag-1 replication:

- **Dead lane:** tag-1/ServerComputer replication (`c35140`) — de-risk D1 = FAIL, structurally
  unreachable on a normal connection (`c33210`/`c33720` ServerComputer attach: refs=0, no
  pointer-table installs, not on the connect/login tree — triple agreement).
- **Reachable lane:** the `obj+0x18` writer **`bd2cd0` has a reachable caller — native
  construction.** The client already builds its `CharacterManager` this way
  (`bb2580("CharacterManager")`+`bd2cd0`, LATE-27); `CreateLocalObject('TPlayerDummy')` builds a full
  char with `obj+0x18` populated (D2). The reachable `NewObject` `bff990` fails ONLY for `owner==0`.
- **The reframe (the owner) — the tree is rooted at `TAccount`, not the chars:**
  ```
  TAccount  (root account object — construct FIRST; BLOCKER 1)
   ├─ Session          (component → login self-gate obj+0x18[Session])
   └─ CharacterManager (component → THE OWNER of the chars; materialized by-name @onLoadPlayers)
        └─ TPlayer chars  (leaves, in CharacterManager+0x28)
  ```
  A char "lacks an owner" only when built without this tree. The reachable construct lane is
  **`bff990` (key03-NORMAL)**, NOT the dead `c35140` (key03-master); `bff990`'s owner ref is read
  FROM THE WIRE (host-controlled), not from the dead ServerComputer (LATE-50,
  `../re/D-create-leg-onloadplayers.md`).
- **Components must replicate.** Client TNetObject templates are `FakeObjectDef` stubs (methods, NO
  `Com(...)`), so each object's components ship as a **component tail** in its NewObject (the
  `bb0060`/`bafdc0` descriptor-keyed format) — they are not built from the type name alone.

⇒ build the host that, **as the live server**, drives the client to construct the full **TAccount
tree** top-down (TAccount+Session → CharacterManager → chars). Template realization (`reload_file`,
Stage A — proven) feeds the native construction.

## D2 result (downstream de-risked, 2026-06-29 LATE-48)

A constructed char (`obj+0x18` populated), placed in `CharacterManager+0x28` (which is a
`std::map<int,Object*>`, not a list), traverses the WHOLE char-select data path with no wall:
`getAllPlayers` returns it → both `bd3660` per-row gates accept it → the `bc5860` value-registry
resolve succeeds → `AddCharacter` (`bc6020`, a `std::map::operator[]` insert) registers it. The only
remaining row work is **display data** (name/level/appearance — empty on a bare construct) + the
screen render (Gate 1 body). ⇒ construction + a faithful owner tree is the job; the model/row
machinery already accepts a real char.

## Node tree + which lane builds each node (LATE-53, live-anchored — DO NOT conflate the two lanes)

Two distinct construction lanes; anchoring every finding to a node + lane is what keeps us out of loops:

- **KEY-3 NewObject lane** (`bff990`, dispatched via the **key→handler table @`0xbfdb40`**, key3→`bff990`;
  NOT `bbf750`/`0x183e0e0` — that's off-path, LATE-53). The UNIVERSAL "construct a replicated GWorld
  object from the wire" lane: type via `bb5f40` → component tail via `bd4b20` → `obj+0x18` via `bd2cd0`.
  Wire/channel-driven. It builds objects on BOTH branches below.
- **ACCOUNT/SESSION RPC lane** (`onLoadPlayersCacheResult a7fd80` → `bb2580` by-name → `LoadList a620c0`):
  materializes/places TAccount-tree nodes — `CharacterManager` by-name, then `LoadList` **places** each
  already-constructed char into `CharacterManager+0x28`.

```
ClientWorld (FClientWorld)             [channel handshake — the stub already drives this]
 ├─ control object pid 0x3e9 (KEY-3)   ← the legal key-3 NewObject we observed live (cls38=0,
 │                                        preceded by ChannelContext parse) = m_ControlObj.
 │                                        NOT a TAccount node — ENVELOPE template only.
 └─ ChannelContext
TAccount (account root)                [login/session]
 ├─ Session            (obj+0x18 self-gate — master-stubbable so far; in-game may need more)
 └─ CharacterManager   (bb2580 by-name)
      └─ TPlayer chars  (KEY-3 NewObject to CONSTRUCT the Player-bearing obj + LoadList to PLACE it)
```

⇒ **`0x3e9` is a ClientWorld/Channel node, not a TAccount node.** It is a template for the **key-3
NewObject ENVELOPE + dispatch ONLY**; the char reuses that envelope but with its OWN class (`TPlayer`),
its OWN owner (`CharacterManager`, by-name materialized), and its OWN component tail. Capturing `0x3e9`'s
wire bytes = capturing the proven construct envelope, NOT the char's tree placement.

## Persistence invariant (user-directed, LATE-53): the faithful server needs a REAL TAccount in the DB

The end state is a **legally read/written `TAccount`** (account → `CharacterManager` → chars) persisted in
**M-DB** — we cannot escape a full legal TAccount. The ONLY stubbable part so far is the **session/master
handshake** (and in-game may still require more than char-select does). Everything in the TAccount tree
must be real and DB-backed; the key-3 envelope is just the transport that constructs those nodes
client-side.

## Modules (10) — keystones marked ★

| Module   | Responsibility | Reuse / status |
|----------|----------------|----------------|
| M-NET    | Master transport + GSS/XOR/zlib crypto, framing | reuse `gss_stub_server_v4.py` transport |
| ★M-CODEC | Wire variant codec + NetVar descriptor projection (`bef480`/`bafdc0`/`bb0060`) | keystone; partial in stub |
| M-DB     | Persistence (accounts, chars) | reuse `Res/live_character_db.py` |
| ★M-OBJ   | Host GWorld object model: objects, components, `obj+0x14`/`obj+0x18`, OIDs | keystone; new |
| M-WORLD  | Zone/world container, object residency | new (minimal first) |
| M-TICK   | Scheduler / replication tick | new (minimal first) |
| ★M-REPL  | Master/tag-1 replication → client `c35140`→`c48760`→`bd2cd0`. **Prereq: get the client to do the flag1 ServerComputer attach (`c33210`) — `Connect`/`bfe230` (flag0) never sets up the tag-1 lane** (see blocker below) | keystone; the core unknown (de-risk D1) |
| M-API    | Bounded goEngine native API surface the shipped Lua calls | new |
| M-LUA    | Run shipped server Lua (`TPlayerCache`/`CharacterManager`) | runtime TBD (LuaJIT?) |
| M-BOOT   | Orchestration: wire the modules, account→char-load→replicate flow | new |

**Keystone order:** M-OBJ (host object) → M-REPL (replicate it onto the client) → M-CODEC
(emit the bytes faithfully). M-LUA/M-API come once construction is proven.

## Current keystone (M-BOOT owner-tree + M-REPL native-drive)

NOT "emit a tag-1 `BackupReplicate`" — that lane is dead (above). The keystone is now: **what
faithful server sequence makes the live client build the owner tree natively?** The pieces are
reachable — the client builds `CharacterManager` via `bb2580("CharacterManager")`+`bd2cd0`, and
`bff990` constructs a char given `owner!=0`. The host (`server/src`, all module self-tests GREEN)
already realizes the account→CharacterManager→char object graph in its own Lua VM (shipped
`TPlayerCache:LoadPlayers`); it has just **never been wired as the live server** — the client still
talks to the scripted stub.

So the work is the integration we've never done: **make the faithful host the live server for the
char delivery** (the stub stays only as wire/crypto/channel transport; the host supplies the object
graph + the construction-driving messages at `onLoadPlayers`). Decisive next step = RE the client's
native `onLoadPlayers` per-char construction (the create-leg): what owner/parent it reads, and the
exact message that drives it — then have the host emit that so the char constructs with `owner!=0`
via the reachable `bd2cd0` path. RE state: `../re/D1-c35140-dispatch.md` (reframe §, lines 112-126);
the downstream is de-risked (`derisk/D2-constructed-char-row.md`).

## Target data flow (char-select row)

```
DB (M-DB) → load account chars → host GWorld Player objects (M-OBJ)
  → master replication (M-REPL/M-CODEC; needs the tag-1/ServerComputer lane attached FIRST)
  → client constructs via c35140→c48760→bd2cd0
  → char obj+0x18 populated → CharacterManager+0x28 → getAllPlayers counts it
  → AddCharacter → (Gate 1: CBody body-load) → UIIntro:Show → ROW
```

## Runtime (decision D1 — to re-confirm after de-risks)

Heavy host = stateful replication server. Likely split: Python (orchestration + transport,
reuse the stub) + embedded Lua (LuaJIT) for the shipped server Lua + a native/C hot path for
the codec/GWorld if perf demands. **Not decided yet** — the de-risks (D1/D2) may narrow it.
See `decisions.md`.

## Reference architectures (which ref for which module — borrow patterns, not code)

Index only; full rationale + caveats in spec §6. **Borrow patterns, not code** (different language + protocol).

| Reference | Engine | Borrow for |
|-----------|--------|-----------|
| ★ **OpenDAoC / Dawn of Light** (DAoC, WAR) | Gamebryo/NetImmerse — **our family** | sim core: ECS → M-OBJ/M-TICK; DOLSharp NIF parser → the Gamebryo asset layer (Gate 1 body-load) |
| **OpenMW / TES3MP** | NetImmerse lineage | the overall shape — "run the original game's own Lua" → M-LUA |
| **Moongate** (Ultima Online, .NET) | n/a | the Lua-logic + delta-sync boundary → M-API + M-REPL |
| L2J_Server / MapleStory | Unreal-2 / non-Gamebryo | era/genre + gameplay-feel sanity checks only (M3/M4) |

**⚠ The keystone is irreducibly ours.** Every entry above shares only the *engine base*, never the netcode — each studio bolted its own replication middleware on top (Mythic's for DAoC/WAR; **goEngine/Firenze for us**). The wire/construct lane (M0 · M-CODEC · M-REPL) is in no Gamebryo emulator — don't go looking for our protocol in their code.
