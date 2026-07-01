# De-risk D2 — Does a *constructed* char actually render a row?

**Question (validates the target):** if a char object is genuinely constructed — components +
populated `obj+0x18` — does it produce a char-select **ROW** through the real native
`updateCharacter`/`getAllPlayers`/`AddCharacter` path, or is there a wall *beyond*
construction (binding, `Gate 1` body-load) that would also block the heavy host?

- **PASS** (AddCharacter fires + a row slot appears) → construction is the LAST wall for the
  row; the heavy host's job is exactly "construct + replicate the char" — fully de-risked.
- **FAIL** (constructed char still no row) → deeper walls; the heavy host alone is
  insufficient → investigate `Gate 1`/binding before committing to the full build.

## Method (force = TESTING PROBE only, per project rule)

Use the **proven** in-client construction (B-approach-1 Phase 1: `ClientWorld:CreateLocalObject
('TPlayerDummy')` builds a full-component char whose `obj+0x18` IS populated — resolvable by
`bb2580`/`bb5f40`, unlike forged scalars). This is a probe to answer the question; **not** the
faithful path.

1. Drive the client to a **natural** char-select where the account's `CharacterManager`
   exists (the drifted stub reaches `onLoadPlayers`/`LoadList`/finish on the deciding FUser —
   06-27 run 160415). Avoid the early-force timing wall (`bfull_diag`: forcing `ShowScreen(2)`
   before account setup → null mgr → crash).
2. At the LoadList/finish moment (where `g_last_charmgr` is captured), `CreateLocalObject
   ('TPlayerDummy')`, capture its `Object*` (binder `FUN_00c026a0`), and `push_back` it into
   `CharacterManager+0x28` (std::list recipe, 06-27 `bfull_injection_recipe`) **before**
   `getAllPlayers`. Ensure `obj+0xDC`/`obj+0xD0`|`0xD4` set (LoadList per-row gate
   `a620c0:451`).
3. Use DIAG `count=0` so `Show()` fires without waiting on the body (isolates the row from
   `Gate 1`). Keep ONE connection alive (handle the ~5s exit if it truncates the cycle).

## Pass/fail signals (from `TW/Bin/NpSlayer_exp.log`, verify THIS run)

- PASS: `updateCharacter_AddCharacter` fires (not `HasNoCharacter`) → the row slot renders
  (computer-use the client to confirm visually). Then note whether the 3D body loads
  (`Gate 1`) or `NATIVE_CRASH laststep=[prep_*]` (body-load is the next, separate wall).
- FAIL: still `HasNoCharacter` despite a populated `obj+0x18` char in `+0x28` → a resolution
  detail beyond construction (re-examine `getAllPlayers`/`bd3660` per-row gate).

## Known hazards (from 06-27)

- Timing wall: char-select forced before account/`CharacterManager` exists → null mgr crash.
- Connection wall: client exits ~5s after GSS stage1 in many runs → cycle truncated before
  `updateCharacter`. May need the stub to keep the connection past finish.
- `ItemFactory` equip silently fails at char-select → don't rely on equipped items; the row
  slot (`AddCharacter`) does not need a loaded body.

## Run-ops

Same as D1. NpSlayer/CreateLocalObject = test/logging only.

## LIVE RUN 1 — 2026-06-28 LATE-45 (Claude; probe built + fired end-to-end, result INCONCLUSIVE-by-design-gap)

Built the D2 probe in NpSlayer profile 78 (`Res/stubs/NpSlayer_exp.c`, deployed; orig DLL
backed up `TW/Bin/NpSlayer.dll.pre_d2_*.bak`). Three seats, all FIRED on a live
`ordered-db-mage-select-enter`:
- **Trigger** @ `VA_CHARMGR_LOADLIST` (one-shot): ran `ClientWorld:CreateLocalObject('TPlayerDummy')`
  in-client. `D2_OID=1, D2_HASP=1` → a real **Player-bearing** char constructed.
- **Capture** @ the construct return `0xbff767`: `c48760(arg2=0x35c6b710)` is the TPlayerDummy
  construct; its result is via the **out-param** (`*EAX`, not EAX). Captured `g_d2_char=0x36112970`,
  `obj+0x18=0x359413b8` (populated self/RPC map → construction is real, matches LATE-8b).
- **Inject** @ `getAllPlayers` entry (`0x00a5fdf0`, ECX=charmgr): hand-linked an MSVC `std::list`
  node carrying the char into `charmgr+0x28`, `newsize=1`. getAllPlayers reads the list from
  `[this+0x28]` (`MOV EDI,[EDI+0x28]` @0xa5fe40) — the **same** list, so the link target is correct.

**Result: `getAllPlayers` returned count=0 → `HasNoCharacter` (no row).** BUT the captured char's
per-row gate fields were **garbage** (`obj+0xDC=0x2473fd01, 0xD0=0xff7d15ff, 0xD4=0x4a3e28ab`), and
**this probe did NOT set them** — i.e. it skipped THIS doc's step-2 requirement ("Ensure
obj+0xDC/0xD0|0xD4 set, LoadList per-row gate a620c0:451"). So the no-row outcome is the EXPECTED
consequence of an incomplete inject, NOT yet a clean D2 verdict.

**Confounds still open (need resolving before a verdict):** (a) the per-row gate `FUN_00bd3660` /
`a620c0:451` requirements — what obj+0xDC/0xD0/0xD4 (and possibly the obj+0x18 self-map keys) must
hold for a row; (b) confirm `getAllPlayers` WALKED the node (a bd3660 seat) vs. a `std::list`
`_Mysize`-offset mismatch (I wrote `_Mysize` at list+8 per the existing GATE2REG model; unvalidated
on a non-empty list). **NEXT = RE `bd3660` + `a620c0:451` to learn the gate-field requirements, set
them on the injected char, add a bd3660 seat, re-run.** (This is the "if not, RE" branch.)

## LIVE RUN 2 — 2026-06-28 LATE-46 (Claude; std::list mechanics CRACKED via static disasm, char now in the roster)

Static-disassembled `getAllPlayers` (FUN_00a5fdf0) + its walk helper (FUN_00a69920) from
`TW/Bin/NClient_standalone.exe` (validated: `a5fe21` = `PUSH 0x1546e54`). Findings:
- **getAllPlayers reads `_Myhead` from `[this+0x28]+4`, NOT `+0`** (`a5fe40 mov edi,[edi+0x28]`;
  `a5fe46 mov ebx,[edi+4]`). MSVC `_ITERATOR_DEBUG` `std::list` layout: `list+0=_Myproxy`,
  **`list+4=_Myhead`**, `list+8=_Mysize`. The prior GATE2REG model (`head=*(list+0)`) was WRONG
  (only ever ran on empty lists, so never caught). My LIVE RUN 1 inject linked off `+0` (the
  proxy) → getAllPlayers walked the real `+4` chain, never saw the node → count=0. **= the H2
  mechanical bug, root-caused.**
- The walk helper `a69920` is a sentinel-bounded loop (`while node != _Myhead`) that calls a
  per-node callback `[ebp+0x1c]`; **no `bd3660` gate inside getAllPlayers** — it returns ALL
  nodes; the per-row gate is downstream in updateCharacter. The element value address is
  `lea eax,[ebx+edi+0xc]` (node + an offset).

**Fixes + live results (profile 78, re-run each):**
1. `_Myhead` source `+0`→`+4`: getAllPlayers NOW WALKS the node — `HasNoCharacter` STOPPED firing
   (was 1, now 0). But it **crashed** (`0xc0000005`, ECX=garbage `0x52f6270c`) reading the value
   past my 3-DWORD node → value offset is `> +8`.
2. char at `+8` AND `+0xc`, node buffer widened to 8 DWORDs (zeroed): **NO crash; `getAllPlayers`
   returned `count=1`** — the char IS in the character roster vector. BUT the element is **NULL**
   (`row0=0x00000000`), not the char. Since the char sat at `+8`/`+0xc` and the element came out
   NULL, the walker reads the value from **≥ node+0x10** (an offset still in the zeroed tail).
3. char at `+8..+0x14`: the decisive `AFTER_GETALL` cycle was **truncated** twice by the ~5s
   connection-exit hazard (stall-kill right after `getAllPlayers`, before the `0x010b211b` seat) —
   no clean `row0` captured (possibly the char now processes longer → hangs, or a deeper deref).

**STATE:** the std::list inject is mechanically cracked — a constructed CreateLocalObject char is
now walked by `getAllPlayers` and lands in the roster (`count=1`). The LAST gap is the exact
**element value offset** (`≥ node+0x10`; `lea ebx+edi+0xc` ⇒ `ebx≥4`), then whether updateCharacter's
per-row gate (`bd3660`) turns a non-null roster element into an `AddCharacter` row. **NEXT:** pin
`ebx`=`[ebp+0x24]` in `a69920` (one more static disasm of getAllPlayers' a69920 call setup) to set
the char at the exact offset, OR keep the connection alive past `getAllPlayers` to capture the
`AFTER_GETALL row0` of the `+0x10/+0x14` build, then read whether `bd3660` accepts it.

## LIVE RUN 3 — 2026-06-29 LATE-47 (Claude; the container is std::MAP not list — char now returns as row0)

The "std::list" model (RUN 2) was WRONG. Static-disasm of `getAllPlayers`+`a69920`+the crash site
proves **`CharacterManager+0x28` is `std::map<int,Object*>`** (an MSVC red-black `_Tree`), not a list
— the identical `{_Myproxy,_Myhead,_Mysize}` outer header masked it:
- The crash that RUN 2 hit (`0x00646e12`, near-null read `[0+0x15]`) **= `std::_Tree::_Inc`** —
  `_Isnil@node+0x15`, `_Right@node+8`, the go-right-then-leftmost increment. A list has no such thing.
- Walker `a69920`: `lea eax,[ebx+edi+0xc]` with **`ebx=4`** ⇒ value (the `Object*`) is at **`node+0x10`**
  = `_Myval.second`. `getAllPlayers` reads `_Myhead=*(cont+4)`, `begin=_Myhead._Left`.
- Node layout: `{_Left@0,_Parent@4,_Right@8, key@0xc, Object*@0x10, _Color@0x14, _Isnil@0x15}`.

**Fix = a proper single-node RB splice** (NpSlayer profile 78): `N` = the one black root whose three
child links are the nil sentinel `S=_Myhead`; `S._Left/_Parent/_Right` all → `N`; `key=0x3eb`,
`value=char`; `_Mysize=1`. **LIVE RESULT — clean, no crash, no truncation-of-getAllPlayers:**
`getAllPlayers` returns `count=1`, **`row0 = 0x35937c68 = the constructed char`** (vector
`begin..end` = 4 bytes = one `Object*`); `HasNoCharacter` did NOT fire; updateCharacter takes the
**has-character path** (`fuser+0x8c=1`). ⇒ **a constructed char IS accepted into the roster vector that
updateCharacter consumes** — the std::list/map mechanics are fully solved.

**REMAINING GAP (only truncation):** the ~5s connection-exit stall-killed the cycle right after the
`AFTER_GETALL` seat (`0x010b211b`), BEFORE `AddCharacter` (`0x010b2435`). So whether the per-row
processing (two `bd3660` gates at `0x010b21bb`/`0x010b226a` + the row-add) turns `row0=char` into an
`AddCharacter` ROW is not yet captured (`AddCharacter=0`, but unobserved-past-the-kill, not rejected).
**NEXT:** static-mapped the loop `0x010b211b→0x010b2435` (two `bd3660` calls feed `ebx`/`edi`; the
real work is at `0x010b22ed+`, `cmp [edi+0x4c],[esi+0x68]`). Add seats at the post-gate branch +
`AddCharacter`, and/or keep the connection alive past getAllPlayers, to read the row outcome.

## LIVE RUN 4 — 2026-06-29 LATE-48 (Claude; gate chain traversed to AddCharacter, but NO VISIBLE ROW — user-observed)

Found that `VA_UPDATE_ADDCHAR` was **not even seated** in profile 78 (only AFTER_GETALL/HASNOCHAR
were) — so the prior `AddCharacter=0` was a false negative. Added 3 trace seats: `0x010b227f`
(rowgate2, EDI = 2nd `bd3660` result), `0x010b232f` (addgate, `cmp [esi+0x74],0`), `0x010b23be`
(resolve, EAX = `bc5860` value-registry result), plus the real `0x010b2435` AddCharacter seat.

**LIVE — the constructed char traverses the WHOLE row-add data path:**
- `AFTER_GETALL count=1 row0=char` (RB splice still correct).
- `rowgate2` (`0x010b227f`): **EDI non-zero** (`0x114af160`) → `bd3660` accepted the char; falls through.
- `addgate` (`0x010b232f`): **`[esi+0x74]` non-zero** → does NOT skip; proceeds.
- `resolve` (`0x010b23be`): **`bc5860` returned a REAL object** (`EAX=0x03fa73c4`, not nil) → the
  value-registry-miss hypothesis is DISPROVEN; the resolve succeeds.
- `AddCharacter` (`0x010b2435`) **fired** with `ESI=char`.

**BUT — user watching the live client: NO char appeared in the row (empty list).** So reaching
AddCharacter is necessary, NOT sufficient. Static disasm of `bc6020` shows it is a
**`std::map::operator[]` insert** (find via `bc8e20`; allocate+insert via `bc5650`/`bc7350`) into the
registry map at `[0x184a2b0]+0x78`, keyed by `0x148f9dc` — i.e. AddCharacter **registers the char into
the char-select MODEL; it does not render a widget.**

**HONEST VERDICT:** the DATA path (construct → RB-map → 2× bd3660 gates → resolve → model-insert) has
**no wall** — a constructed CreateLocalObject char is accepted end-to-end into the char-select model.
The VISIBLE ROW is a **separate render** the model-insert alone does not trigger. Two open causes (not
yet distinguished): (a) the row widget needs the char's DISPLAY DATA (name/level/appearance) which a
bare `CreateLocalObject('TPlayerDummy')` lacks (empty NetVars; the "2nd-layer descriptor +0x2c data"
this doc flagged) → an empty/skipped widget; (b) the char-select SCREEN does not re-render after a late
model insert, and/or the ~8s connection-death tore down before any repaint. **NEXT (decisive, pick
one):** (A) disasm the post-loop widget-build (`0x010b24ee+`) to see what char fields it reads, populate
the char's name/level, re-test; (B) keep the connection alive past updateCharacter to rule out
render-timing; OR (C) per `milestones.md`, treat the char-LIST row as the structurally-hardest path and
pivot to M1 (enter-game via the control-object `c5a880` lane, which HAS a reachable client construct).
