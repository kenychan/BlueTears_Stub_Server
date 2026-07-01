# CONSTRUCT_PROTOCOL — how the client builds a rendered object (general abstraction)

The reusable model behind char-select (and every later rendered object: avatar, NPCs,
items, world residency). Abstracts the **construct lanes + message/data flow** that turn a
server-side record into a client GWorld object + a row. Low-level wire bytes live in
`../../PROTOCOL_QUICKREF.md`; live verdict/blocker in `../../CODEX_LATEST_STATUS.md`;
narrative evidence in `../re/D-create-leg-onloadplayers.md`. **Update this whenever a lane,
trigger, or format is proven/falsified.**

---

## 1. Object model (the node tree — types are SETTLED)

```
ClientWorld (DAT_0184da00, ONE singleton; all transports feed its object/GWorld map)
 ├─ TClient  (Connector)         = the CONNECTION object        — client-LOCAL, built by GSS 0x1001
 └─ TAccount (root account obj)  = the ACCOUNT                  — built top-down at login
      ├─ Session          (component)  → login self-gate obj+0x18[Session]
      └─ CharacterManager (component)  → OWNER of the chars; materialized by-name (bb2580) in a7fd80
           └─ TPlayer, TPlayer, …      = the CHARS (each a TActor; comps Player/CStatus*/User/CBody/CEquipment)
                                          leaves in CharacterManager+0x28 (std::map<int,Object*>)
```
Three distinct node kinds — never conflate: **TClient**(Connector)=connection ≠
**TAccount**(Session+CharacterManager)=account ≠ **TPlayer**(TActor)=char.

**The roster char is `TPlayer`, NOT `TPlayerDummy`.** `TPlayerDummy` is referenced in exactly ONE
place (its own template def) in the real client — it was a lighter TEST stand-in (8 visual
components, no DB) used by D2/the stub. `TPlayer` (57 refs) is the real char: DB-backed (`DBCom`,
`CStatusPlayer.m_DBVersion`), with `CInventory`/`CSkillBook`/`CItemManager`/`CEquipment`/… It is
REQUIRED (DBCom = the persistence invariant; the dummy has none). The construct lane (§2) is
class-agnostic (`bb5f40` builds any className via dynamic `bb2580`), so it builds `TPlayer` the
same way — the difference is PAYLOAD, not lane. The host sends the **owner-replicated component
subset** (per each component's `m_StoCReplicateType`: `NormalReplicate`+`OwnerOnly` reach the
owner; `NotReplicate` e.g. `CPlayerDbFilter` is skipped), each with correct `bb0060` field-framing
— the heavier-component framing is the genuine B4 work for the faithful char.

---

## 2. Construct lanes — per-transport `[obj+0x640]` key→handler tables

Construction is **dispatched per network "computer"**: each transport installs its own
`[obj+0x640]` key→handler table in its ctor (insert helper `FUN_00930590`). A wire object
node carries a `key`; the receiving computer looks up `[obj+0x640][key]` and calls it. The
common tail of every construct is **`bd4b20`** (component builder) → **`bd2cd0`** (writes
`obj+0x18`, the self/RPC map = "this object is real/registered").

Each table is a red-black map keyed by the **wire opcode** (insert `930590`, find `930680`).
Opcode→key: `0x03` NewObject = **key3**, `0x05` BackupObj = **key5**, `0x06` RemoteCall = key6.

| Table | Installer | Owner | key3 = NewObject(0x03) | key5 = BackupObj(0x05) | Status (LATE-90) |
|-------|-----------|-------|------------------------|------------------------|------------------|
| **A** | `bfd800` (`0xbfdb40` install) ← FClientWorld ctor `0109d1c0` | **FClientWorld** = client's LOCAL world | `bff990` → `bd4b20` | *(absent)* | **LIVE** — builds the Connector `0x3e9` |
| **B** | `c74640`/`c74774` | **ServerComputer** (master-replication receiver; built by `bfe230`) | `c74ed0` → **`bb5f40` type-factory** + `bd4b20` | *(absent)* | **LIVE** lane, but DORMANT at char-select (untriggered) |
| **C** | `c59580` | a network computer | (key3 present) | **`c5c770`** → `bd3660` gate → `bd4b20` (components) → `bb2580` | **LIVE** lane, DORMANT (untriggered) |
| **(tag-1)** | `c32290` table; `c33210` ServerComputer-attach *registers* `c35140` | **tag-1 ServerComputer** (legacy) | `c35140` | *(absent)* | **DEAD** — `c33210`/`c33720`/`c35140` all 0 refs (D1) |

**Reading (LATE-90):** `0x03` (→`bff990`/`c74ed0`) and `0x05` BackupObj (→`c5c770`) are BOTH
**LIVE** typed+component construct lanes — neither uses the dead `c35140` (a separate legacy
table). At char-select only table A actually FIRES (for the Connector), because the host only
ever sends a construct message the FClientWorld manager consumes. Tables B/C (the ServerComputer
/ replication receivers) are live machinery that **no current message targets**. ⇒ the char build
is a master-lane `0x03`/`0x05` construct message **targeting the ServerComputer manager (table
B/C)** — the open piece is the manager-target resolution (see §6).

---

## 3. The two transports + message/data flow

```
GSS socket (port 5004)         — XOR crypto only; lower-protocol triggers (build_lower_gss_body)
  client→ 0x4009 sign-in  →  server→ 0x1001 challenge(name="local")  ⇒ FClientWorld builds TClient 0x3e9 LOCALLY
  client→ 0x4002 response →  server→ 0x1003 result + 0x1004 master-update
  client→ 0x4005 channel  →  server→ 0x1006 sign-in result (master_id, channel_id, host, port)
  GSS idles/closes

Master socket (port from 0x1006) — XOR + zlib(type-4); NESL replication + RPC downcalls
  …login conversation… signInClientResult → logInClientResult(accountId,plrId,type)
  SERVER (our host runs the shipped Lua): TPlayerCache:LoadPlayers
        → getByRelation(accountId,"TAccount","CharacterManager")  [native getByRelationImpl: DB query]
        → objectList (the account's char objects, server-side)
        → session:onLoadPlayersCacheResult(from, objectList)       [native Session RPC, NID 0xcd7b8836]
  CLIENT receives onLoadPlayersCacheResult → handler a7fd80 (see §4)
```

`PlayerManager::createObject` (`b8f750`) is the **in-game** player path (chat/channel/market/
monster) — NOT char-select. The char roster is `CharacterManager`, fed by the cache lane above.

---

## 4. The RESOLVE/BUILD split (the crux)

`onLoadPlayersCacheResult` handler **`a7fd80` only RESOLVES + REGISTERS — it does NOT build chars:**
```
a7fd80: bb2580("CharacterManager")→bb5f40  [materialize the CONTAINER]  +  bd2cd0 [its obj+0x18]
        for each elem in objectList:  bd3660 (type gate)  →  resolve OID  →  bc6020 (map insert = AddCharacter)
        (no bff990, no per-char bb5f40 — the chars must ALREADY exist in GWorld)
```
`logInServerResult` (`a81020`) likewise **bails** if the char OID is absent ("No Object for
OID" / "No COM: Player/CStatusPlayer/User"). ⇒ **the char must be BUILT (registered in GWorld
+ its components, obj+0x18 set) BEFORE either resolves it.** Resolve uses the value/object
registry `0x0184a2b0` (via `bc5860`); a miss → nil → element deleted → empty list → no row.

**Two gates to a rendered row:** Gate-2 = char `obj+0x18` set (else `getAllPlayers` skips it,
written only by `bd2cd0` during real construction) → `AddCharacter` row slot. Gate-1 = `CBody
LoadingEnd` ×count → 3D body shows.

---

## 5. Construct MECHANISM — Message-Triggered Local Construction (MTLC)

**The client never deserializes a whole object off the wire and never accepts an injected
NewObject. It builds each object LOCALLY by running its OWN construct code, TRIGGERED +
parameterized by a server MESSAGE + DATA.** pids/ids are client-assigned, NOT on the wire.

- **Live proof:** `0x1001` payload = just the name `"local"` → `ca4540`→`ca47e0`→`bff990`→
  `bd4b20` builds the Connector locally (full object, `obj+0x18` set). No object bytes on wire.
- **Dead by this model:** injecting a serialized char NewObject (Model A), inline tag-0x07
  object-values, hand-forged `bb0060` component tails as RPC args. (~30 sessions; LATE-55→59.)
- **The host's job everywhere:** send the right **trigger message + data**; the client builds.
  Recurs for chars, avatar, NPCs, items, world residency. Per object: find the trigger
  message + data; do NOT serialize the object.

### Packet/data formats (abstract; bytes in PROTOCOL_QUICKREF §2–§3)
- Every lower packet: `u32 body_len | u8 0 | u16 opcode | 9× pad | body`. Build only via
  `build_lower_gss_body(opcode,body)`.
- Construct/replication opcodes (master lane): `0x02` IssuePID, `0x03` NewObject
  (`u32 pid | u32 namelen | ASCII name`), `0x05` BackupObj, `0x06` RemoteCall (key6),
  `0x09` LinkHaving (`u32 parent | u32 child | u32 slotlen | ASCII slot`).
- key6 RemoteCall prefix: `from_pid | target_pid | object_id | self_key | callable_key |
  u16 selector | Variant argc | Variant args`.
- Component tail (a constructed object's LOCAL build payload, NOT a wire object): read by the
  component reader (vtable `+0x1c`, e.g. `bede50`): `u16 u16 u16 → obj+0x20/22/24`, `u8 flag →
  +0x26`, optional OID-list if flag≠0, then **field payload via `bb0060`** — descriptor-driven
  (walks `descriptor+0x28`; ONE value per replicated-mode field, widths from `.bss`, NO field
  count on the wire), then end-marker. Values may be zero; only COUNT + per-field WIDTH must match.

---

## 6. The CORE TENSION (why this is hard) + the OPEN frontier

**Tension:** a char is a SERVER object. Server objects normally reach the client via
ServerComputer/NESL replication — but that construct (`c35140`) is **dead** on this client. The
only LIVE construct (`bff990`/FClientWorld) builds the client's OWN local objects. So the char
must be built **client-LOCALLY (MTLC), as if it were the client's own object, from server
data** — exactly the committed model. Persistence invariant (user): the end state still needs a
real DB-backed `TAccount`; only the session/master handshake is stubbable.

**THE RECIPE (LATE-90/91) — WHERE + WHAT:**
- **WHERE (target = the ServerComputer's NetSession):** the `+0x640` construct table is embedded
  per-Computer in its `CL_NetSession_Impl` (`ca5320`). `DispatchMessage` = `ca4540` is a
  PER-SESSION vtable method (slot +0x28). Manager selection is **STRUCTURAL** — *which Computer's
  session processes the stream* — NOT a target-pid in the packet. `bff990` = key3 of FClientWorld's
  session (GSS lane, `DAT_0184da00`) → builds a bare "Object" (HOLLOW). **`c74ed0` = key3 of the
  ServerComputer's session** (master lane; ServerComputer built by `bfe230`→`bb5f40("ServerComputer")`
  at `holder+0x188`, session embedded at `+0xd0`) → `bb5f40` type-factory → typed TPlayer. So:
  deliver the construct **over the master lane so the ServerComputer session's DispatchMessage runs.**
- **WHAT (0x03 NewObject envelope):** `u32 field_A(→obj+0x4c) | u32 pid(→obj+0x3c) | string
  className="TPlayer" (=u32 len+bytes) | u32 ctx(→obj+0x38)` then the **component tail on the same
  stream** (`bd4b20`, flag=0). Per component: `u32 key | u16×3 header | u8 flag(=0) | bb0060
  field-payload`. `bb0060` is descriptor-driven (one value per replicated-mode field, widths from
  the `.bss` descriptor — **dump live to lock**). Send TPlayer's owner-replicated component subset.
- This explains B4: the same `0x03` was right; it just hit `bff990` (FClientWorld session, hollow)
  instead of `c74ed0` (ServerComputer session). The envelope was never the bug — the **session** was.

**OPEN (the precise remaining gap): routing the construct to the ServerComputer session.** LATE-78
showed the stub's master batch BYPASSES `ca4540` — only the GSS session's DispatchMessage ever fired
(opcode `0x1001`); the ServerComputer session's DispatchMessage NEVER ran ⇒ `c74ed0` dormant, and the
stub's "NewObject" lands on a hollow-object path instead.
**Method to close it:** (a) PROBE — seat `ca4540` logging ECX (session `this`) + opcode for EVERY
invocation; confirm whether the ServerComputer session's DispatchMessage ever fires. (b) RE the master
receive path (`01158950` worker queue → `holder+0x188` ServerComputer → `+0xd0` session DispatchMessage)
to find what routes a master packet there + why the stub's batch bypasses it. (c) Deliver the `0x03`
TPlayer construct on that lane. Downstream (resolve → AddCharacter → row) is de-risked (D2).
