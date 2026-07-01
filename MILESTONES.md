# Milestones

## 2026-06-20 - Character Creation UI Reached

Status: achieved with runtime-assisted NpSlayer profile78 plus local stub.

What moved:
- Old channel/login/empty-select wall is past for this profile.
- The client reaches `FUser::updateCharacter`.
- The client emits `HasNoCharacter`.
- The user-visible character creation UI appears.
- After a name is typed, the client sends create-character RPCs to the local server.

Current wall:
- The server does not yet implement the create-character response/list refresh.

Primary recipe:
- `docs/milestones/2026-06-20_character_creation_ui.md`

Primary evidence:
- `evidence/2026-06-20_character_creation_ui/npslayer_profile78_charselect_milestone_20260620_151104.log`
- `evidence/2026-06-20_character_creation_ui/master_ladder_20260620_151104_ordered-b4-b5-empty-select.stderr.log`
- `evidence/2026-06-20_character_creation_ui/master_client_zsync_decode_20260620_151104.txt`

Next milestone target:
- Character creation succeeds and the character list/selection UI refreshes cleanly.


## 2026-06-30 - TPlayer Builds Locally (construct wall broken)

Status: achieved live (NpSlayer profile78 TEST5 + local stub). Retracts the prior
"roster char is architecturally un-constructable in this standalone" conclusion.

What moved:
- Root cause found: the wall was the `Lib/Template.lua:120-147` EmptyImpl/DefaultImpl
  gate, NOT a missing construct lane. On the client a TNetObject descendant (TPlayer)
  realizes as EmptyImpl (zero components, skips `goLua_RegistObject`) unless the
  hierarchy is force-loaded. That skip is why `bb2580('TPlayer')`=NULL and
  `CreateLocalObject('TPlayer')`=nil (never registered) -- symptoms, not the cause.
- Force-load -> DefaultImpl realize builds a full 25-component, registered,
  obj+0x18-bearing TPlayer locally from the client's OWN TPlayer.lua data
  (c48760 realizer + bd2cd0 obj+0x18 = full native construct).
- Validated the C<->Lua boundary: the construct is a C->Lua / Lua->C ping-pong;
  the silent EmptyImpl/DefaultImpl branch is why native-only channels missed it.

Current wall:
- The force-built TPlayer is an ORPHAN (no world owner) -> faults at teardown.

Primary recipe:
- `docs/milestones/2026-06-30_tplayer_local_construct.md`
- `docs/LUA_QUICKREF.md` (the Lua flow map)

Primary evidence:
- `evidence/2026-06-30_tplayer_local_construct/gap_log_test5.txt`
- `evidence/2026-06-30_tplayer_local_construct/native_construct_markers.txt`

Next milestone target:
- Integrate the built char into the roster (owner/OID -> GWorld/CharacterManager+0x28
  -> AddCharacter -> visible ROW), driven by the host's roster trigger.


## 2026-07-01 - Modular stub-free host + GWorld register API pinned

Status: infrastructure milestone (live-E1-verified) + RE finding. Supersedes the
LATE-111 force-build lane (that in-client injection is user-rejected drift); the
go-forward product is the faithful modular host below.

What moved (host infrastructure):
- The faithful host is now a clean 3-tier modular package, extracted STUB-FREE from
  the legacy `gss_stub_server_v4.py`. Start point = `server/run_server.py` (NOT the
  stub). Tiers: `server/src/engine/*` (substrate: M-LUA/M-API/M-CODEC/M-REPL/M-OBJ/
  M-DB), `server/src/{char,account,charactermanager}/*` (per-Lua-object host ops that
  RUN the shipped server Lua), `server/src/net/*` (the GSS wire transport).
- Login path carved out of the 1160-line `handle_master_phase` into
  `net/connection.py` as real stub-free code (dead research-probe branches dropped).
  net/{crypto,framing,packets,login} byte-verified against the stub (65 shared-builder
  cases + 10 clean-lane packets, 0 mismatch).
- LIVE E1 VERIFIED (2026-07-01): `run_server.py` reaches
  `UIIntro:ShowJobClassSelection` (signInClientResult -> logInClientResult ->
  Client_FinishLoadingCharacter[true,0]) -- a proven drop-in for the stub. The stub is
  now FROZEN LEGACY (transport reference only).

What moved (RE finding -- the GWorld register-for-TPlayer API):
- The API entry that constructs AND registers a TPlayer into the world is
  `World:CreateObject("TPlayer")` -- server-side the shipped Lua calls
  `GetEnvironment():GetNObject("ServerWorld"):CreateObject("TPlayer")`
  (`Class/Main/CStatus.lua:812`, `Class/Main/Player.lua:1021-1114`).
- `CreateObject` is NATIVE (no Lua def; inherited by FServerWorld from a base World
  class). One-shot construct -> `647860` OID-insert into the GWorld map C. Lower shell
  factory = `goLua_CreateObject` = `FUN_00c48010`. This pins the faithful SERVER-side
  register API (host M-API already exposes `GWorld:CreateObject`).
- Distinct from the still-open E3 client-side receiver trigger (the replication
  envelope, gworld.md section 5): this is what the HOST calls; the client's local
  ghost build still needs the envelope.

Primary recipe / artifacts:
- `server/SERVER_QUICKREF.md` (the 3-tier map + how to live-run the modular host)
- `docs/reference/flowmodels/client/` (the client RE flowmodel tree, one file per
  engine object + `_MASTER.md` call graph; `gworld.md` section 4b = this finding,
  section 5 = the open E3 envelope)

Next milestone target:
- E3: pin the replication WIRE ENVELOPE by differential trace of the client's own
  working replication, then emit it via host M-REPL/M-CODEC on the clean E1/E2 login so
  the client builds+registers the char locally (bff990+647860) -> ROW.
