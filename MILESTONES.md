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

## 2026-07-02 - Faithful Character-Creation Works (CREATE LANE) + the ~100-session post-login wall was a ONE zlib TRANSPORT bug

Status: achieved on the faithful modular host (`server/run_server.py`), unmodified
client, no injection. User-confirmed live on screen: "it created and shows the char
manager but its just empty... this def the right lane, use it as new base."

What moved (the game wall visibly moved):
- The client now runs its OWN character-creation flow against our server: after login it
  reaches the create screen, sends `checkCharacterName` + `createCharacter` (client key6
  on `self=CharacterManager 0x6c12c692`), we answer `checkCharacterNameResult` /
  `createCharacterResult(result=0=SCS_OK, reason=0)`, the client's Lua handlers run
  (`character manager>> ...Result | 0 | 0`), fires `Master_CheckCharacterNameResult SCS_OK`
  + `Master_CreateCharacterResult SCS_OK`, and DISPLAYS the char-select / CharacterManager
  screen (empty roster; "select char first" on Start). No disconnect, no error.

THE ROOT CAUSE of the long post-login wall (this is the big one):
- Every prior run disconnected right after login with `World_Disconnected`. Cause: the
  server->client direction is ONE shared zlib stream (the client uses a single persistent
  `zlib.decompressobj`, mirrored by `ZlibSyncFrameDecoder`). The old
  `build_zlib_transport_frame` did an INDEPENDENT `zlib.compress()` per frame -- a complete
  stream (BFINAL + Adler32) that ENDS the client's inflate after the login batch. So EVERY
  2nd+ post-login frame (SRQL, create ACKs, and char delivery) decoded to 0 bytes ->
  len-mismatch -> the client's native packet-validity gate `0x0115bfd0` rejects it ->
  `ca4070` report -> `0115de60` session shutdown -> socket close. NOTHING post-login could
  ever land. That is why ~100 sessions of char-delivery attempts all failed at the wire.
- FIX: `ZlibStreamFrameEncoder` -- one persistent `zlib.compressobj()` per connection,
  `Z_SYNC_FLUSH` per frame; the login batch and every later reply share it. Proven offline
  (one-shot 2nd frame -> 0 bytes; streamed -> both decode) and live (replies now dispatch
  into the client's Lua; channel holds).

Result-code detail:
- SCS_OK is `0`, not 1. Native `FUser::onCreateCharacterResult` (FUN_010b42f0) /
  `onCheckCharacterNameResult` (FUN_010b4660): `switch(result){0->SCS_OK; default->ERR_FAILED;
  8->ERR_DUPLICATED_PLAYERNAME; 9->ERR_CREATEPLAYER_BADNAME; 10->ERR_CREATEPLAYER_NOOWNERSHIP}`.

New clean base (default, no flags):
- SRQL reply SUPPRESSED (its empty-`{}`-table arg trips gate `0x0115bfd0`; re-send for
  diagnosis via `BT_SEND_SRQL_LEGACY=1`), continuous-stream encoder, create ACKs answered.

Primary recipe / artifacts:
- Run: `py Res/run_master_ladder.py --mode ordered-db-mage-select-enter --admin-command
  launch_original_stub --server-script server/run_server.py` (fresh original client).
- Files: `server/src/net/framing.py` (`ZlibStreamFrameEncoder`) + `server/src/net/connection.py`
  (shared encoder for batch + replies; checkCharacterName/createCharacter answers; SRQL default-suppress).
- Evidence: `evidence/late168_create_lane/server_create_scs_ok.log` (server side: create RPCs
  in + SCS_OK=0 ACKs out, no EOF) + `ch1_create_scs_ok.txt` (client Lua: SCS_OK + char-select).

Next milestone target:
- Deliver the created char so the roster is non-empty: after `createCharacterResult(SCS_OK)`
  the client runs `CharacterCreated -> UIIntro:UpdateCharacter -> getAllPlayers` (empty ->
  back to char-select). Send `serverCreateCharacterResult(oid, SCS_OK, info)` ->
  client `GWorld:FindObject(oid):BackupReplicate(true)` (`CharacterManager.lua:80-89`) ->
  `getAllPlayers` -> `AddCharacter` -> ROW. If "can't find created character!", the char must
  be resident in the client's GWorld at `oid` first (replication envelope / MTLC).
