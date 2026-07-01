# CLAUDE.md — Blue Tears / Punch Monster: faithful offline server

Auto-loaded every session. **RULES live here; the how-to/paths live in linked refs — but reading the ref for the action you're doing is NON-SKIP (contract below).** Anti-drift anchors, read FIRST + re-read after compaction/resume: **`NORTH_STAR.md`** (proven route + DEAD lanes never re-walk + the one next step) · **`CODEX_LATEST_STATUS.md`** (live state / blocker / next-step, top entry only). If a thought contradicts them, re-read instead of re-deriving.

## Goal (stable)

Build the faithful offline **server / emulator** so the client's OWN mechanics construct + render the char — NOT by force-patching. Home = **`server/`**, START POINT **`run_server.py`** (the modular host). Three tiers, follow the Lua: `engine/` = substrate · `char/`+`account/`+`charactermanager/` = per-object host-ops · `net/` = wire transport. **⛔ `Res/gss_stub_server_v4.py` = FROZEN LEGACY, never edit.** NpSlayer = TEST/RE tool only. Ladder: char row shows → enter game → char acts → render.

## §0 Discipline — never skip

- **Motion ≠ progress.** Before acting, ask what NEW decision-changing result it yields toward a milestone; re-establishing the known yields none. Meta-work must serve a concrete next action. A turn that's mostly artifacts with no new result = circling: stop, state the one blocker, run the decisive test.
- **Verify before you claim.** Read the PRIMARY evidence THIS run — the live logs (all 4 channels) / `py server/tools/derisk_run.py` verdict / the actual bytes / the function an exception came from — before naming any wall/state/result. Never pattern-match an exception code to a prior finding. A stale log ≠ running the test.
- **Don't force or guess.** At an undocumented hard wall where the next move is guesswork, STOP and surface. The faithful host drives the engine's OWN construction — never force-patch engine self-gates (`obj+0x18`); they exist because the engine needs real objects.
- **⭐ LUA-FIRST IS LAW.** The whole flow (login→creation→select, construct, roster, RPC handlers) is in the decompiled Lua; the C engine only signals/inits. BEFORE any native RE: read the Lua (`LUA_QUICKREF.md` + grep `Res/tw_lua_decompiled/` — NOT `extracted_named_v4`) + read all 4 live channels in full. Native disasm is the LAST resort. Update `LUA_QUICKREF.md` after any Lua walk.
- **Commit checkpoints (git).** Commit before any large/experimental edit + again when a step works. `Milestone:` for breakthroughs (+ mirror to `qqxj_server`); else `feat:`/`checkpoint:`/`fix:` so `git log --grep='^Milestone'` stays the clean breakthrough line. Never revert to an old commit to "restore good state" without diffing + backing up current work first.
- **Brainstorm at walls; keep context fresh.** Zero online community — reason from the client + open-source engine refs. Checkpoint + compact by ~50% context; re-read the anchors after.

## Non-skip reads — read the ref for the action you're doing (this IS the anti-drift loop)

| Doing… | MUST read first (non-skip) |
|---|---|
| any live run / debug a live wall | **`NPSLAYER_CURRENT_PATCH.md`** — run mechanics, the 4 log channels (incl. the always-on GAP global bridge recorder), seats, deploy, active-seat/**differential-trace** method — AND read all 4 channels in full THIS run before any claim |
| build / extend the server | **`server/SERVER_QUICKREF.md`** + the matching client flowmodel **`ghidraRE/output/flowmodels/client/<object>.md`** — the server tree is its COUNTERPART: build in that shape, keep the two in step, new work goes in the matching tier (never the frozen stub) |
| locate any function / RPC / native | **`ghidraRE/output/flowmodels/api/`** (`rpc_surface.md` wire RPCs · `native_api.md` natives + VAs · `_MASTER.md`) + `…/client/` + `gworld.md` for flow · `PROTOCOL_QUICKREF.md` wire/object dict; exact VAs in `lua_native_surface.md` / `lua_global_registrations.md` |
| RE a wall | the LUA-FIRST chain (above), THEN the 4 channels + active seats; native disasm last. After any RE walk: update `ghidraRE/output/flow_model.json` + `protocol_schema_index.json`, cite the `SESSION_FINDINGS_<date>` line |
| engine / `Ni*` / mesh / render / body-load | `gamebryo-v3/` · `gamebryo-v2/` SDKs (see `PROJECT_RULES.md`), NOT the binary |
| commit a milestone | mirror to `qqxj_server` (rules below) |

Optional lookup (skipping loses no rule): **`PROJECT_RULES.md`** (commands, paths, Gamebryo/`Ni*` SDKs, delegation, versions) · `server/docs/ARCHITECTURE_QUICKREF.md`.

## Machine-facing docs — MAX efficiency (rule)

Any MD / flow model / log / index written for MACHINE consumption (the flowmodels, the api map, generated indexes, seat/channel logs) = **max density**: terse, structured, tables/keys over prose, no padding — just enough human readability to skim. Only human-facing docs (this file, `NORTH_STAR.md`, `SESSION_FINDINGS_*`, milestone diaries) get readable prose. Don't pretty-write machine refs; pack them for efficiency.

## Safety, mirror & workflow

- **Scope:** read/write inside this project only. The SOLE external write target = `C:\Users\keny-\Downloads\qqxj_server` — the clean milestone mirror: breakthrough-reproducing artifacts ONLY (host/stub code, hooks + build script + built DLL + hash, curated logs, recipes — no dumps/caches/client assets), self-contained per milestone (sync ALL current tools, never leave a stale one), `MILESTONES.md` APPEND-ONLY (never rewrite old entries). **Don't edit the parent `~/Downloads/CLAUDE.md`** — only the in-project rules.
- **Client integrity:** don't modify the client binary / patch memory / install hooks unless target+purpose+rollback+anti-protection risk are understood first; back up any DLL/EXE before patching. Default proof = unmodified client + static RE + server-side logs.
- **Workflow:** the architect reserves its budget for hard architecture / protocol-binary analysis / decisions; delegate bounded, compact-result mechanical work (see `PROJECT_RULES.md`). Targeted instrumentation + bounded runs over blind testing; simplest code that works. At each milestone: log + `Milestone:` commit + mirror, then drive the next.

Preservation context (offline nostalgia recovery of a long-discontinued game) — don't re-ask to justify.
