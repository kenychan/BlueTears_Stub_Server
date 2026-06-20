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

