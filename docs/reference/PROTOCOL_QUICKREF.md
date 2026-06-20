# PROTOCOL_QUICKREF — read this first, every session

If you are about to change `Res/gss_stub_server_v4.py` or any Ghidra/protocol
file and you have not read this file in the current session, **stop and read
it first**. Most regressions in this project are agents skipping this file.

If anything in this file contradicts your memory of the protocol, **this file
wins** until a newer dated entry overrides a specific line. Don't argue with
it from context.

---

## 1. Connection flow (the only one)

```
GSS socket (port 5004)
    client  -> 4-byte XOR window header + 128-byte XOR-encrypted 0x4009 sign-in body
    server  -> 4-byte XOR window header (9c 01 1c 02) then XOR-encrypted:
                  0x1001 SessionInChallenge
    client  -> 0x4002 challenge response (20 bytes total: 16-byte head + u32 body)
    server  -> 0x1003 SessionInResult + 0x1004 MasterChannelUpdate (concatenated)
    client  -> 0x4005 channel sign-in (28 bytes total: 16-byte head + u32+u32+u32 body)
    server  -> 0x1006 SignInResult (body: u8 success + u32 master_id + u32 channel_id +
                                          pack_string(host) + pack_string(port))
    GSS socket closes / idles

Master socket (port from 0x1006 — same 5004 in stub)
    client  -> 4-byte XOR window header (per-master-session, observed e.g. 6d 03 4a 04)
    server  -> 4-byte XOR window header (use 9c 01 1c 02 to reuse recovered keys)
    server  -> XOR-encrypted ( zlib-frame ( one-or-more lower-packets ) )
    client  -> XOR-encrypted ( zlib-frame ( client lower-packets ) )
    ...     -> NESL replication continues here
```

Master uses **both** the XOR crypto layer (type-2) **and** the zlib transport
layer (type-4). GSS uses only XOR. Confirmed by `FUN_0115ebc0` decompile in
`ghidraRE/output/decompiled_layer_chain_ctor.c` plus the runtime evidence
that the client emits a `valid_crypto_window=True` 4-byte header on master
(see `Res/RE_findings/gss_v4_clear_raw_heartbeat_20260528_041348.err.log`).

---

## 2. Lower-packet 16-byte head (every NESL packet, every GSS packet)

```
offset  size  field
0       u32   body_len            -- length of the body, NOT total
4       u8    reserved (0)
5       u16   opcode              -- the ONLY field every dispatcher reads
7       9     padding (zero)
16      ...   body
```

Build with `build_lower_gss_body(opcode, body)`. Never assemble heads by hand.

---

## 3. The eight opcodes you will use

| opcode | name           | body shape | byte-proven? |
|--------|----------------|-----|--------------|
| 0x02   | IssuePID       | `u32 pid` | by analogy |
| 0x03   | NewObject      | `u32 pid + u32 namelen + ASCII name` | **yes** (FUN_00bd5f80) |
| 0x05   | BackupObj      | `u32 pid + u32 namelen + ASCII name + pack_variant(dict)` | inferred |
| 0x06   | RemoteCall     | `u32 pid + u32 fnlen + ASCII fn + u32 argc + Variant args` | **min-size yes** (≥0x14) |
| 0x09   | LinkHaving     | `u32 parent + u32 child + u32 slotlen + ASCII slot` | inferred |
| 0x0d   | OobToClient    | `u8 subtype + payload` | yes |
| 0x0f   | EncryptToClient| TerSafe wrap (NOT yet byte-proven; diagnostic only) | partial |
| 0x12   | Heartbeat      | empty | yes — **but it's client→server, not server→client** |

GSS opcodes 0x1001 / 0x1003 / 0x1004 / 0x1006 / 0x4002 / 0x4005 / 0x4009 use
the same head but different bodies; see `SESSION_FINDINGS_2026-05-27.md` §5,
§9, §19, §21.

### 3.1 ClientWorld key6 RemoteCall prefix (2026-06-01)

For the current master ClientWorld manager, opcode `0x06` uses the recovered
five-scalar key6 prefix before Variant argc/args:

```
u32 from_pid | u32 target_pid | u32 object_id | u32 self_key |
u32 callable_key | u16 selector | Variant argc | Variant args...
```

Confirmed correction (2026-06-03 s3): profile6 runtime evidence in
`SESSION_FINDINGS_2026-06-03.md` s1/s3 shows the third scalar is looked up in
the live `manager+0x1c` object-pid map. With `object_id=0x3e9`, the
`0x00c00a48` lookup returned non-null (`EAX=0x2984b1a0`). Field 4 remains the
method/callable key (`native_nid(method)`); the same run did **not** reach
`0x00bee460`, so the current wall is downstream of field2.

For server->client key6 calls, the stub default is `from_pid = 0`. The recipe
already allowed `from=0`, and `SESSION_FINDINGS_2026-06-02.md` s8 proves the
selector-1 route gate computes its mask from field0/from while field4 remains
the callable key. Manager pid `1001` maps to route mask `0x40`; `0` maps to
`0x10` and avoids the mask-0x40 hard-false edge before Lua dispatch.

`callable_key` is **not** zero. Static evidence shows native send helpers feed
the `FUN_00bc6020(method_name)` output into scalar field 4, and key6 later uses
that value in `FUN_00bc5860` before `FUN_00ba9640` tests the Lua callable. The
visible `PUSH 0` after scalar field 4 in native send helpers is the selector
writer, not field 4. In the stub, default `callable_key = method_nid`.
Evidence: `SESSION_FINDINGS_2026-06-01.md` §44.

---

## 4. XOR crypto layer (type-2) — what you must know

- Window header is `u16 start | u16 end` little-endian. `end > start + 0x80`
  and `end < 0x500` for validity (`header_window_is_valid()`).
- Each side advances a cursor through its window for each outgoing byte.
- The recovered key buffer covers two ranges only:
  - `0x31..0xf9` (232 bytes) — from sign-in known-plaintext crack
  - `0x19c..0x290` (244 bytes) — from a later known-plaintext crack
- Server response header `9c 01 1c 02` selects window `0x019c..0x021c`. Use
  this on both GSS and master sockets — same global key buffer.
- **The cursor wraps MODULARLY at `(end - start)` bytes** (`end` exclusive),
  re-indexing `key_buf[start .. end)` forever. For `9c 01 1c 02` that is
  exactly 128 keys (`0x019c..0x021c`), ALL of which are recovered (they sit
  inside the `0x19c..0x290` range). **So burst length through the XOR layer
  is UNBOUNDED for this window** — any size decrypts cleanly.
- CORRECTION (2026-05-29, Claude): an earlier version of this section claimed
  "~244 usable bytes per cursor pass; burst exceeds that and produces garbage
  past byte ~244." **That was wrong.** Byte-level static proof: recv-decode
  slot `0x01169750` (inner loop `0x011697c0`) and send-encode slot
  `0x01169820` (inner loop `0x011698b0`) in
  `ghidraRE/output/crypto_header_edges.md` both do `key = key_buf[cursor];
  cursor++; if (cursor == end) cursor = start`. The stub already mirrors this
  (`gss_stub_server_v4.py:458`, `:548`). If the 12 KB character burst fails,
  it is NOT XOR key exhaustion — look at zlib / lower-validity / protocol.
  Full write-up: `SESSION_FINDINGS_2026-05-29.md` §3.

---

## 5. Master zlib layer (type-4) — what you must know

- Frame format: `u32 raw_len | u32 compressed_len | zlib.compress(raw)`
- Implemented by `build_zlib_transport_frame(clear)`.
- One frame may contain multiple concatenated lower packets.
- Frame goes **inside** the XOR-encrypted byte stream on master, **not**
  outside it.

---

## 6. The seven things every agent gets wrong

When you find yourself reaching for any of these, stop and re-read.

1. **"Master is clear zlib, no XOR."** Wrong. Master has XOR *and* zlib.
   The client emits a 4-byte XOR window header on master — proven in
   `Res/RE_findings/gss_v4_clear_raw_heartbeat_20260528_041348.err.log`.

2. **"Heartbeat (0x12) is a good first server probe."** Wrong. Heartbeat
   is client→server (`TcDProto::keep_alive`). The first valid server master
   packet is `IssuePID` (0x02).

3. **"Session/Channel/World PIDs are hardcoded constants."** Wrong. The
   client resolves them by template-name registration via `GetNObject(name)`.
   The server picks the numeric PID and replicates the object under that
   PID — any unique integer works.

4. **"CStatusPlayer has a flat stat dict (Level=, HP=, ...)."** Wrong.
   Stats live in `m_modificationList` as `{m_attrName, m_value, m_name,
   m_suppierid}` records. Base values are modifications with `suppierid=0`.

5. **"Use TPlayer for the character-select screen."** Wrong. Use
   `TPlayerDummy` for select-screen replication. `TPlayer` is for after the
   player clicks "Enter Game" and `User:Start(nil)` fires.

6. **"The XOR key window caps a burst at ~244 bytes."** Wrong (corrected
   2026-05-29). The window wraps modularly at `(end - start)`; for
   `9c 01 1c 02` that is 128 keys, all recovered, so any-length burst
   decrypts cleanly. XOR is NOT a burst-size limit. See §4 and
   `SESSION_FINDINGS_2026-05-29.md` §3. (If a big burst still fails, the
   cause is zlib/validity/protocol, and probing one packet at a time is
   still useful for *isolating* which packet the client rejects — just not
   for "key budget" reasons.)

7. **"BackupObj / LinkHaving body shapes are byte-proven."** Partly resolved
   (2026-05-29). The **wire Variant codec is now byte-proven** from
   `FUN_00bef480` (tags 1=bool/3=number-as-double/4=string-u32len/5=table-u16count/
   7=object/11=handle) — see `SESSION_FINDINGS_2026-05-29.md` §9.1.
   **Correction (2026-06-02, Codex):** tag `0x0b` ObjectPtr handle is not
   `tag + u32 pid`; static slot recovery shows
   `tag 0x0b + u32 descriptor_key + u32 object_id`, where the ObjectPtr
   descriptor key candidate is `native_nid("ObjectPtr") = 0xbf68a6a3`.
   Evidence: `SESSION_FINDINGS_2026-06-02.md` s22,
   `ghidraRE/output/codex_objectptr_descriptor_wire.md:539-620`, and
   `ghidraRE/output/codex_objectptr_descriptor_slots.c:569-668`.
   And opcodes 0x02/0x03/0x05/0x06/0x09 have **registered handlers** on the
   goEngine NObject managers (NewObject=`0x00c74ed0`, BackupObj=`0x00c5c770`;
   §9.2), so they dispatch rather than throw. Still NOT byte-read: what those
   handlers *require* (NewObject remainder bytes; which NetVar keys BackupObj
   demands) — decompile `0x00c74ed0` / `0x00c5c770` to finish. If a replication
   packet crashes with zlib/XOR OK, suspect the NetVar-key expectations there.

---

## 7. Required reading order if you compress / resume / forget

1. This file (`PROTOCOL_QUICKREF.md`).
2. `SESSION_HANDOFF_<latest>.md` (active task + TODO).
3. `CODEX_LATEST_STATUS.md` (≤30 lines — what the last session left).
4. The section of `SESSION_FINDINGS_*.md` you specifically need. Don't read
   them end-to-end every session; grep for the keyword.

If your change touches the wire format, you re-read §1-§5 above. No
exceptions.

---

## 8. Source-of-truth precedence

```
PROTOCOL_QUICKREF.md          (this file, single page, authoritative)
   ↑ overrides
SESSION_FINDINGS_2026-05-28.md  (newer findings)
   ↑ overrides
SESSION_FINDINGS_2026-05-27.md  (Claude's static RE walk, §49-§51)
   ↑ overrides
ghidraRE/output/PROTOCOL_FINDINGS.md  (original)
```

To change this file, you must have **byte-level runtime or static evidence**
that contradicts an existing line. Quote the file path and line number of
the evidence in your edit.

---

## 9. The post-channel close cascade (added 2026-05-28 evening)

After multiple rounds of investigation, here's what's true about the
34ms close-after-account-root we've been chasing.

> CORRECTION (2026-05-29, Claude): the chain in §9.1 is NOT a "close
> cascade" — it is the NORMAL per-tick network receive-queue drain.
> `FUN_00ca5590` (§9.5 called it a no-op "state-copy function") actually
> calls the queue consumer `FUN_0115b0b0` (`LEA ECX,[ESI+0x48]; CALL
> 0x0115b0b0` at `0x00ca55a0`) and only THEN copies goEngine timing floats.
> The full receive pipeline (IOCP -> XOR -> zlib -> extractor -> validity
> `0x0115bfd0` -> `FUN_01158950` -> queue insert `FUN_011580f0` -> drain
> `FUN_0115b0b0` -> `packet->vtable[+0x0c]` -> opcode dispatcher
> `FUN_00ca4540`) is mapped in `SESSION_FINDINGS_2026-05-29.md` §4. The
> "invalid packet detected!" `__CxxThrowException` in `FUN_00ca4540` fires
> only when the opcode is missing from the handler hash table at
> `owner+0x620` (that is the old `crt_error` mode). Treat §9.1/§9.5 below as
> superseded by findings §4.

### 9.1 The full close path (proven by Stalker + Ghidra decompiles)

```
goEngine::client_tick   (FUN_01030870, runs every game frame)
  └─ FUN_00bc1a70   (CONDITIONAL — only calls observer-notify under
                     specific state conditions; not unconditional)
       └─ FUN_011298e0   (observer dispatcher: iterates list at
                          DAT_0181d7b4 / DAT_0181d7b8, CALL EDX)
            └─ FUN_00ca5590   (one observer — references "goEngine"
                               string at 0x01556028, triggers close
                               cascade)
                 └─ FUN_0115b0b0   (thread-safe queue pump,
                                    uses EnterCS/LeaveCS via
                                    DAT_01480300/04)
                      └─ FUN_0115a910 (uses DAT_0148030c/014801f4)
                      └─ FUN_01158410 → FUN_01155dc0 (worker)
                 └─ ... eventual close via socket teardown ...
                      └─ FUN_011672e0 → ws2_32!closesocket
```

### 9.2 The IAT slots 0x0148030c / 0x014801f4 are NOT the bridge

Earlier I had a wrong hypothesis that these IAT slots were the bridge
into the injected DLL. They're not.

Evidence from `ghidraRE/output/close_decision_chain.md`:

| IAT VA | runtime value | apparent role |
|---|---|---|
| `0x01480300` | `0x03d66918` | `EnterCriticalSection` (proven §2) |
| `0x01480304` | `0x03d66924` | `LeaveCriticalSection` (proven §2) |
| `0x0148030c` | `0x03d6693c` | another sync primitive, same heap region |
| `0x014801f4` | `0x03d66630` | another sync primitive, same heap region |

**All four IAT slots in this group point into the same `~0x03d66xxx`
heap region.** That's the signature of anti-cheat IAT hijacking: the
injected DLL (TerSafe or similar) overwrote the IAT to redirect every
Win32 sync-primitive call through a heap trampoline that ultimately
chains to the real API. CALL through these slots is normal sync-prim
work, NOT a bridge to close-enforcement code.

Conclusion: the close decision is **purely app-side**. The injected
DLL is only acting as a thunk wrapper around Win32 sync calls.

### 9.3 What triggers the close is conditional inside FUN_00bc1a70

Per `ghidraRE/output/close_decision_chain.md` lines 60-168,
FUN_00bc1a70 is a large function with many branches. The
observer-notify call at `0x00bc1b00` (which triggers the close cascade)
is reached **only** if specific conditional jumps evaluate to certain
values. The branch gating uses these state variables:

```
DAT_01825df0   (state flag, multiple read+write sites)
DAT_01826008   (state field, read+write)
DAT_01826018   (state field, read+write)
DAT_0182601c   (state field, read+write)
DAT_01826020   (state field, read+write)
DAT_01826024   (state field, read+write)
DAT_0183e12c   (state variable, read multiple times)
```

These are the state machine that decides whether to fire close-notify
each tick. The values of these DATs after master-socket-open determine
whether the client decides to close.

### 9.4 The "goEngine" string is a state key, NOT specific to close

`"goEngine"` at `0x01556028` is referenced by **9 different functions**
across the engine. It's used as a state-key string in many places.
The close callback `FUN_00ca5590` checking it doesn't mean the close
is goEngine-specific — it means the callback uses goEngine as a key
to look up its target state.

### 9.5 What we don't yet know (CORRECTED 2026-05-28 evening after re-read)

**Update: my earlier read of FUN_00bc1a70 and FUN_00ca5590 was wrong.**
After actually reading the decompiles in detail:

- **FUN_00bc1a70 is the game's per-tick entity iteration with
  profiling instrumentation.** It calls FUN_011298e0 only when
  `param_13 != 0` (a char flag set by `client_tick`). The DAT
  state variables (DAT_01826008, DAT_01826018, etc.) are timing
  accumulators, NOT a close-decision state machine.
- **FUN_00ca5590 is NOT a close callback.** It's a state-copying
  function that compares `this->m_name` against `"goEngine"` and
  copies internal state to output parameters when matched.
  Zero close logic in its body.
- **The closesocket backtrace shows these functions only because
  they were on the stack** when a separate code path triggered the
  close via a Windows window-message → injected DLL WndProc →
  closesocket. The close did NOT come through this chain.

**Confirmed by silent-master A/B test (20:03)**:
- Test A (open master, send NOTHING): client stayed connected 60s,
  no close.
- Test B (open master, send account-root): client reset 290ms
  after frame.
- **The close is reactive to our packet contents**, not a tick-based
  timeout. Specifically, ~290ms = ~10 game ticks worth of time for
  the network receive worker to process our 43-byte frame, post a
  window message, and the message loop to dispatch it.

**The real gap** (§51.5 unfinished work resurfaces): the receive
handler for NESL opcode 0x03 (NewObject) on the master socket is
unidentified. We have:

- The byte-proven send-side stats inspector (`FUN_00bd5f80`, §49.5).
- The receive path up through queue insert (`0x115bfd0` validity
  → `0x1158950` dispatch wrapper → `0x011580f0` insert →
  `0x01155560` queue ADD).
- NO byte-proven receive-side handler for opcode 0x03.

What rejects our packet must be downstream of `0x01155560` (queue
ADD), invoked by some consumer/worker thread we haven't located.

### 9.6 What we still need to recover statically

To diagnose the 290ms close, we need:

1. **The packet-class construction site.** Where in the receive
   path does the binary turn raw bytes into a per-opcode packet
   object with a class-specific vtable? Per §51, this happens via
   normal CALL (not scaled-index dispatch — that was ruled out).
   Suspect functions: inside `FUN_01158950`'s body before queue
   insertion, or inside FUN_00ca4540 (the GSS-side validator, which
   may share construction logic with master).
2. **The opcode 0x03 packet class.** Once construction site is
   found, find which vtable is selected for opcode 0x03. The
   handler is `vtable+N` for some N (likely a Process() / Execute()
   slot).
3. **What the NewObject handler validates** beyond pid + namelen +
   ASCII. §49.5 noted "body[+0x08+N..] remainder (likely zero or
   initial state — needs runtime test)." Maybe required initial
   state bytes follow the template name.

### 9.7 Implications for next protocol moves

- **Do NOT chase FUN_00bc1a70 / FUN_00ca5590 further.** They're not
  the close logic.
- **Do NOT instrument the per-tick path.** The close is triggered
  by network receive processing, not by client_tick.
- **DO statically recover the master receive dispatch and the
  opcode-0x03 packet class.** That's the real gap.

### 9.6 Implications for next protocol moves

- **Do NOT chase the "injected DLL bridge"** — it's not the close
  enforcer. (Banked finding; saves a future round.)
- **Do NOT change protocol bytes** based on hypotheses about what
  the client expects until the silent-master A/B says whether the
  close is reactive or independent.
- **DO investigate FUN_00bc1a70's branch logic** as the next static
  step after the A/B. The condition under which observer-notify
  fires IS the close trigger.
