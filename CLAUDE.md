# CLAUDE.md — Blue Tears / Punch Monster: faithful offline server

Auto-loaded every session. **RULES live here; the how-to/paths live in linked refs — but reading the ref for the action you're doing is NON-SKIP (contract below).** Anti-drift anchors, read FIRST + re-read after compaction/resume: **`NORTH_STAR.md`**· 
**`CODEX_LATEST_STATUS.md`** (top entry only). If a thought contradicts them, re-read and eliminate rewalking, brainstorm, ask user if not confident.

## Goal (stable)

Build the faithful modular server **server / emulator** so the client's OWN mechanics construct, no force-patching. Home = **`server/`**, START POINT **`run_server.py`**. Three tiers, follow the Lua: `engine/` = substrate · `char/`+`account/`+`charactermanager/` = per-object host-ops · `net/` = wire transport. **`Res/gss_stub_server_v4.py` = FROZEN LEGACY, never edit.** NpSlayer = TEST/RE tool only. 

## §0 Discipline — never skip
- **Motion ≠ progress.** Works and result should progress from last milestone to next one, no circling. Circling = mostly artifacts, no proven progress from known milestones, stop and reposition.
- **Verify before you claim.** Always read the live logs (all 4 channels) of current fresh run / `py server/tools/derisk_run.py` verdict / the actual bytes / the function an exception came from — before naming any wall/state/result. Never pattern-match an exception code to a prior finding. 
- **Don't force or guess.** No guesswork at an undocumented hard wall, brainstorm and ask user. Never force-patch engine self-gates (`obj+0x18`). 
- **Commit checkpoints (git).** Commit before any large/experimental edit + again when a step works. `Milestone:` for breakthroughs (+ mirror to `C:\Users\keny-\Downloads\qqxj_server` after reading its PROJECT_RULES.md); else `feat:`/`checkpoint:`/`fix:` so `git log --grep='^Milestone'` stays the clean breakthrough line. Never revert to an old commit to "restore good state" without diffing + backing up current work first.
- **Brainstorm at walls with Lua** Zero online community, The whole game logic is in the decompiled Lua; the C engine only signals/inits. Brainstorm's bastion. 
- **Max simplicity and efficiency**  Write Code / MD / flow model / log / index for MACHINE consumption, simple and efficient, eliminate redundance. Basic human-readability for `NORTH_STAR.md`, `SESSION_FINDINGS_*`; full prose only for human-facing docs (milestone diaries).

## Workflow - never skip
| any live run / debug a live wall | **`NPSLAYER_CURRENT_PATCH.md`** — run mechanics, the 4 log channels, seats, deploy, active-seat/**differential-trace** method — GAP global bridge recorder and Lua channel are priority, but read all 4 channels in full current run before any claim |
| build / extend the server | **`server/SERVER_QUICKREF.md`** + the matching client flowmodel **`ghidraRE/output/flowmodels/client/<object>.md`** — the server tree is its COUNTERPART: build in that shape, keep the two in step, new work goes in the matching tier (never the frozen stub) |
| locate any function / RPC / native | **`ghidraRE/output/flowmodels/api/`** (`rpc_surface.md` wire RPCs · `native_api.md` natives + VAs · `_MASTER.md`) + `…/client/` + `gworld.md` for flow · `PROTOCOL_QUICKREF.md` wire/object dict; exact VAs in `lua_native_surface.md` / `lua_global_registrations.md` |
| RE a wall | read the 4 channels + active seats, then locate in the Lua (`LUA_QUICKREF.md` + grep `Res/tw_lua_decompiled/`). Update `LUA_QUICKREF.md` after any Lua walk.; native disasm last. After any RE walk: update `ghidraRE/output/flow_model.json` + `protocol_schema_index.json`, cite the `SESSION_FINDINGS_<date>` line, `update PROTOCOL_QUICKREF.md`(findings index) |

Optional lookup (skipping loses no rule):  `server/docs/ARCHITECTURE_QUICKREF.md`;**`PROJECT_RULES.md`** (commands, paths, Gamebryo/`Ni*` SDKs, delegation, versions) 


## Safety, mirror & workflow

- **Scope:** read/write inside this project + milestone mirror only. 
- **Client integrity:** don't modify the client binary / patch memory / install hooks unless target+purpose+rollback+anti-protection risk are understood first; back up any DLL/EXE before patching. Default proof = unmodified client + static RE + server-side logs.

Preservation context (offline nostalgia recovery of a long-discontinued game) — don't re-ask to justify.
