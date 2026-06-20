# 2026-06-20 Character Creation UI Milestone

## Result

The client reached the empty character creation UI from the local stub path.
After the user typed the name `keny`, the client sent create-character packets.
The remaining wall is server-side: the stub has no create-character response yet.

## What Made This Work

The milestone used NpSlayer profile78:

- capture the real `CharacterManager` from `CharacterManager::LoadList`
- skip only the bad `Session::UpdateCharacterCount` bookkeeping call when `Session+0xe0` is null
- fill missing `FUser+0x84` with the captured `CharacterManager`
- let `FUser::updateCharacter` enumerate the empty list and emit `HasNoCharacter`

This does not modify `NClient.exe` on disk. It is a runtime-assisted hook path,
not yet the clean server-only path.

## Files Used

Server/stub:

- `server/Res/gss_stub_server_v4.py`
- `server/Res/gss_stub_server_v3.py`
- `server/Res/live_character_db.py`
- `server/Res/nid_hash.py`
- `server/Res/run_master_ladder.py`
- `server/Res/RE_findings/gss_key_window_20260525.json`
- `server/Res/RE_findings/xor_keystream_full_CANONICAL.hex`

Client hook:

- `client_crack/stubs/NpSlayer_exp.c`
- `client_crack/stubs/NpSlayer_exp.def`
- `client_crack/stubs/build_exp.bat`
- `client_crack/bin/NpSlayer.dll`

## Hashes

Milestone active hook DLL:

```text
MD5  FCE6DF56D4A864AADAF974FFE9353500  client_crack/bin/NpSlayer.dll
MD5  FCE6DF56D4A864AADAF974FFE9353500  client_crack/bin/NpSlayer_profile78_charselect_milestone_20260620.dll
```

Rollback/reference:

```text
MD5  B9B7C285365FD63BC27B3F7A8D2F7B3B  client_crack/bin/NpSlayer_known_good.dll
```

## Reproduce From A Plain Original Client

Assume:

- plain client root: `C:\Users\keny-\Downloads\qqxj`
- mirror root: `C:\Users\keny-\Downloads\qqxj_server`
- Conda Python: `C:\Users\keny-\anaconda3\python.exe`

1. Back up the plain client's current hook DLL:

```powershell
Copy-Item C:\Users\keny-\Downloads\qqxj\TW\Bin\NpSlayer.dll `
  C:\Users\keny-\Downloads\qqxj\TW\Bin\NpSlayer_before_profile78.dll
```

2. Install the milestone files:

```powershell
powershell -ExecutionPolicy Bypass -File `
  C:\Users\keny-\Downloads\qqxj_server\tools\install_to_client.ps1 `
  -ClientRoot C:\Users\keny-\Downloads\qqxj
```

3. Compile-check the stub scripts:

```powershell
& C:\Users\keny-\anaconda3\python.exe -m py_compile `
  C:\Users\keny-\Downloads\qqxj\Res\gss_stub_server_v4.py `
  C:\Users\keny-\Downloads\qqxj\Res\run_master_ladder.py
```

4. Run the milestone probe:

```powershell
cd C:\Users\keny-\Downloads\qqxj
& C:\Users\keny-\anaconda3\python.exe Res\run_master_ladder.py `
  --mode ordered-b4-b5-empty-select `
  --observe-seconds 45 `
  --leave-running-on-result
```

Expected behavior:

- zone/channel automation reaches channel selection
- after login transition, the empty character creation UI appears
- typing a name and submitting sends create-character RPCs
- the client eventually reports server connection off because create response is not implemented yet

## Evidence Anchors

NpSlayer milestone log:

- `LOADLIST_CAPTURE_ESI_CHARMGR` at line 1814
- `FUSER84_GUARD write` at lines 1891-1893
- `FUser::updateCharacter` at line 1894
- `updateCharacter_HasNoCharacter` at line 1907

Server observe log:

- master socket held open after result at line 35
- extra post-UI client master packets at lines 39, 46, and 48
- client reset after unanswered create attempt at line 57

Decoded client create stream:

- packet 3 carries typed name `keny`
- packet 4 carries `CreatePlayerInfo` and appearance template strings
- client->server master zlib uses one continuous sync-flush stream, not independent `zlib.decompress()` frames

## Current Next Step

Recover and implement the server response to the CharacterManager create-character
RPC, then refresh the character list/selection UI.
