# De-risk D1 — Can inbound replication drive the construction lane?

**Question (make-or-break for the whole HEAVY host):** can a *server-emitted* master/tag-1
replication message make the client run `c35140`→`c48760`→`CreateDeferedTemplate`→`bd2cd0`
and construct ONE Player-bearing object (writing its `obj+0x18`) — or is that lane reachable
**only** by the client's own control-object machinery (`SetControlObject`→`0x3e9`), never by
inbound replication?

- **PASS** → the host's core mechanism works; building M-OBJ/M-REPL/M-CODEC is justified.
- **FAIL** → inbound replication cannot trigger client construction; the heavy host as
  designed won't produce the row → re-think (surface to user).

## What we already know (primary evidence)

- Two manager vtables (`codex_master_dispatch_owner_static.md`): self/control `0x01557270`
  (`NewObject`→`bff990`, needs `owner!=0`) vs **replicated `0x01558658`**
  (`c35cb0`/`c35140`/`c33cc0`/`c5a880`).
- `c35140`→`c48760` @`0x00c48914` pushes Lua `CreateDeferedTemplate` (06-27
  `emushape_foundation`). With the template realized (`reload_file`, Stage A PASS),
  `CreateDeferedTemplate` returns true.
- Scripted ClientWorld `NewObject` (`build_clientworld_newobject`) reaches tag-2 `bff990`,
  **never** `c35140` (LATE-24 + LATE-29 log: `c35140` armed, 0 hits).
- ⇒ the open question is the **wire envelope/dispatch that reaches the *replicated* vtable's
  `c35140`**, which scripted ClientWorld packets do not use.

## Plan

1. **RE (static):** find `c35140`'s caller(s) and how the **replicated-manager vtable
   `0x01558658`** is selected. Trace the master dispatch from the wire: which opcode/key and
   object-state routes a replicated object to `c35cb0`/`c35140` (vs the self/control
   `0x01557270`/`bff990`). Identify the minimal wire shape `c35140` consumes. Output →
   `server/re/` + `ghidraRE/output/`.
2. **Build the minimal emitter** in the stub transport (test harness only): emit that
   master-replication envelope for one `TPlayerDummy`/Player object.
3. **Live test:** EARLY_REALIZE the template (already in NpSlayer) → send the emitter →
   observe with the armed seats: `TAG1_KEY03`(c35140 HIT?), `CTORDIAG_C48760`
   (CreateDeferedTemplate ret), `OBJ18_REG_BD2CD0`/`OBJ18_WRITE_BD2D4D` (obj+0x18 written for
   the char pid?), then `CharacterManager+0x28` size + `AddCharacter`/`HasNoCharacter`.

## Pass/fail signals (from `TW/Bin/NpSlayer_exp.log`, verify THIS run)

- PASS: `c35140` **HIT** for the char + `bd2cd0` writes `obj+0x18` for the char pid +
  char enters `CharacterManager+0x28`.
- PARTIAL: `c35140` HIT but `CreateDeferedTemplate` false / no `bd2cd0` → template/realization
  ordering or a 2nd gate.
- FAIL: `c35140` still 0 hits regardless of envelope → lane is client-control-only.

## Run-ops

Kill lingering stub (port 5004) before launch. Stall-kill only NClient/PunchMonster; keep
the worker (`schtasks /Run /TN BlueTearsElevatedLauncher`). Use `py`. NpSlayer = test/logging
only.
