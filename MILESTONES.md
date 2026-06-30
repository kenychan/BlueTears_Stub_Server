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
