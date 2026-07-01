# NPSLAYER_CURRENT_PATCH.md — what `TW/Bin/NpSlayer.dll` is patching RIGHT NOW

Quick-reference for the deployed NpSlayer hook. **NpSlayer = TEST / RE tool only, never the product.**
Source: [`Res/stubs/NpSlayer_exp.c`](Res/stubs/NpSlayer_exp.c). Keep this file in sync when the patch changes.

## Current state (LATE-124)

- **Mode: CLEAN FULL-TRACE OBSERVATION — no force-inject.** All four log channels + every construct/roster/login
  seat fire, but the DLL only *reads/logs*; it does **not** modify the client (per CLAUDE.md §0: observe naturally,
  don't force engine self-gates). This is the committed default so the trace is **always on**.
- **Profile:** `EXP_TARGET_PROFILE 78` (the most comprehensive seat set — inherits the 70–77 cascade + its own).
- **Build:** `cmd //c "Res\stubs\build_exp.bat"` → `Res/stubs/build/NpSlayer_exp.dll` (expect `BUILD_EXIT=0`).
- **Deploy:** back up `TW/Bin/NpSlayer.dll` then copy the built DLL over it.
- **Deployed DLL md5:** `613846e182faf8ce65d2ab233b4c876b` (rebuild + update this line whenever the patch changes).
- **⭐ Channel 4 (C↔Lua GAP) is now ALWAYS-ON + passive** (own region near the CLIENTLOG_TAP, guard
  `g_gap_tap_done`). Installs pass-through logging wrappers on `Template`/`reload_file`/`CreateDeferedTemplate`/
  `goLua_CreateObject`/`goLua_RegistObject` at the earliest Lua seat (`VA_FUSER_UI_UPDATE_HELPER`, before
  onLoadPlayers) → **`TW/Bin/client_gap.log`** (its own file, never drowns in the 200 MB native log). Logging-only:
  no force, no realize, no restore. **This is the channel that witnesses the SILENT EmptyImpl-vs-DefaultImpl construct
  branch — READ IT EVERY RUN.**
- **⭐ GLOBAL bridge recorder (LATE-146, always-on):** on top of the 5 named wrappers, the GAP tap now sweeps
  `_G` and wraps **every global C-function** with a first-hit pass-through logger → `BRIDGE global <name>` lines in
  `client_gap.log`. So NO global C↔Lua bridge is missed, not just the construct path (live-verified: 104 globals
  wrapped, 16 fired in a clean login — `GetEnvironment`, `goLua_UnpackFixed`, `GetUIResourceManager`, `PushUIEvent`,
  `loadstring`, `include`, `Random`, …). Uses WRAPPERS not `debug.sethook` (a call-hook is per-`lua_State`; the
  construct runs on other coroutines and the hook missed it — proven live. Shared `_G` wrappers fire cross-thread).
  **Limitation:** catches GLOBAL bridges only; object **methods** (`World:CreateObject`, `obj:BackupReplicate`) live on
  metatables → pinned via RE + `ghidraRE/output/flowmodels/api/` instead.

### Force-actions — ALL gated OFF in this build (flip the gate to re-arm a probe)
| Probe / inject | What it does (a MODIFICATION) | Gate (off when) |
|---|---|---|
| `g_test5..g_test10` | force-build TPlayer / splice +0x28 / render row | all `= 0` |
| `EARLY_REALIZE` (`0x010b0490`) | `reload_file` TActor/TPlayer before onLoadPlayers | `!g_natural_parse_only` (=1 → off) |
| D2 trigger + `+0x28` splice + `FORCEUI` | build dummy, rb-splice into CharacterManager+0x28, force-paint | `!g_natural_parse_only` / `g_d2_*` |
| `B4OBJ18` obj+0x18 write (`bd2cd0`) | self-register components into obj+0x18 | `!g_natural_parse_only` (LATE-124 gate) |
| `EXP_B_DEFER_PROBE` | `CreateDeferedTemplate('TPlayerDummy')` probe | `!g_natural_parse_only` |
| `OPTION-3` enter-game force | selectCharacter on a forced char | `g_enter_game_force` (=0 → off) |
| Lua-eval seat `exp_b_lua_run(chunk)` | run an arbitrary Lua chunk in-client | only called by the probes above |

To run an ACTIVE probe again: set the relevant flag (e.g. `g_natural_parse_only = 0`) **and** the specific
`g_testN` / `g_enter_game_force`, rebuild, redeploy. Default stays clean-observation.

## The FOUR log channels (read ALL every run — CLAUDE.md SELF-DIAGNOSIS)

| # | Channel | Lands in | What it captures |
|---|---|---|---|
| 1 | **Lua** (`CLIENTLOG_TAP`) | `TW/Bin/client_internal.log` | client's own `RunLogLn_Service`/`print`/`IS_DEBUG`; `[LUALOG]`, `[Error]`+traceback; `[TAG]` lines we route via the `_CLIENTLOG_SINK` Lua global |
| 2 | **Engine-native** (`TRANSITION_DBGSTR`) | `TW/Bin/NpSlayer_exp.log` | the engine's own `OutputDebugString` |
| 3 | **Native call-tree + fault bytes** (`STACKCHAIN`/`ACCESS`) | `TW/Bin/NpSlayer_exp.log` | FPO-robust call chain on every exception/teardown (works at EBP=0); per-AV r/w + fault addr; register/stack memory = the malformed object bytes. Resolve: `py Res/stubs/resolve_stackscan.py <log> --tag=TRANSITION_EXCEPTION` |
| 4 | **C↔Lua GAP** ⭐ ALWAYS-ON · highest-priority for the construct question | `TW/Bin/client_gap.log` (its OWN file) | every C→Lua / Lua→C crossing + realize-branch on the LIVE flow: `Template(...) => Empty/Defered/Default`, `reload_file`, `CreateDeferedTemplate`, `goLua_CreateObject`, **`goLua_RegistObject(name=…)` ← fires ONLY on DefaultImpl (EmptyImpl SKIPS it, Template.lua:95)**. Witnesses the SILENT EmptyImpl-vs-DefaultImpl construct branch channels 1–3 cannot see. **Silent for the char during login→roster ⇒ char did NOT build via the Lua template path → cross-check the native `bff990` seat.** |

## Seat quick-ref (VA → what it logs)

**Construct / registration lane (the `bff990` GWorld-resident build):**
| VA | Seat | Captures |
|---|---|---|
| `0x00bff990` | `VA_KEY03_NEWOBJECT` | key-3 NewObject entry → `BFF990_CALLER` (ret/a1/a2) + `BFF990_MSG` (descriptor `param_1`, `[a1+0x28/0x54/0x04]`, 96-byte dump). `in_ECX` = FClientWorld owner |
| `0x00bb5f40` | `VA_BB5F40_ENTRY` | hollow-"Object" factory entry (`616c5054` sig = TPlayer) |
| `0x00bd2cd0` | `VA_OBJ18_REG_BD2CD0` | obj+0x18 self-map writer → `CTORDIAG bd2cd0 obj/cls38/desc` |
| `0x00c48760` | `VA_CTORDIAG_C48760` | deferred-template construct helper |
| `0x00bff767` | `VA_D2_CREATELOCAL_RET` | CreateLocalObject return (EAX = new Object*) |
| `0x00c74ed0` | `VA_C74ED0_FACTORY` | table-B typed-object factory construct (+ caller) |
| `0x00bd4cf4` | `VA_COMP_MAP_WRITE` | component-map write → `COMPWRITE obj/obj+14/obj+18` |
| `0x00bffafd` | `VA_NEWOBJ_PID_STORED` | each constructed NewObject pid (`NEWOBJ_ID`) |
| `0x00bbf785` | `VA_GATE_RESULT` | pid→slot gate find (`GATEFIND pid/slot/hit`) |

**Lower dispatch / connect:**
| VA | Seat | Captures |
|---|---|---|
| `0x00ca4540` | `VA_SESSION_DISPATCH` | lower dispatcher: u16 opcode per packet |
| `0x00ca47e0` | `VA_CONSTRUCT_WALKER` | object-tree walker (ca4540→bff990); ret = was it packet-driven |
| `0x010b0c20` | `VA_FUSER_CONNECT` | FUser::connect (READY gate) |
| `0x01081e50` | `VA_FCLIENTWORLD_CONNECT` | FClientWorld::Connect |
| `0x00c33210` | `VA_SERVERCOMPUTER_ATTACH` | ServerComputer attach (dead lane #7 — refs=0; seat proves non-firing) |

**Roster / char-select:**
| VA | Seat | Captures |
|---|---|---|
| `0x00a736e0` | `VA_ONLOADPLAYERS_WRAP` | onLoadPlayers wrapper (the roster delivery) |
| `0x00a620c0` | `VA_CHARMGR_LOADLIST` | CharacterManager LoadList + the `LOADLIST_*` family (empty/nil-deletion path) |
| `0x00a5fdf0` | `VA_CHARMGR_GETALL` | getAllPlayers (reads CharacterManager+0x28) |
| `0x010b2080` | `VA_FUSER_UPDATECHAR` | FUser::updateCharacter (the native AddCharacter fire-point) |
| `0x010b0490` | `VA_FUSER_UI_UPDATE_HELPER` | FUser UI/state helper after event (also the Lua-eval seat addr) |
| `0x010b2435` | `VA_UPDATE_ADDCHAR` | the actual AddCharacter row-add |
| `0x010b25ac` | `VA_UPDATE_HASNOCHAR` | HasNoCharacter (empty-roster branch) |

(Full seat list = the install array in `NpSlayer_exp.c` ~line 2620+; the `EXP_DUMP_*` `#define`s in the
profile-78 block ~line 2924+ enable the per-channel dumpers. `resolve_stackscan.py` decodes STACKCHAIN frames.)

## Run-op (live)
- Worker (no UAC): `schtasks /Run /TN BlueTearsElevatedLauncher`; heartbeat `Res/AdminLauncher/launcher_heartbeat.txt` (stale ~15s → `Res/start_admin_launcher.ps1` once).
- Kill lingering stub on **:5004** first. Use `py` (not `python`).
- Harness: `py Res/run_master_ladder.py --mode ordered-db-mage-select-enter --admin-command launch_original_stub` (or `py server/tools/derisk_run.py`).
- Stall = `NpSlayer_exp.log` growth stops, NOT launcher/server silence; kill only `NClient.exe` + `PunchMonster.exe`, keep the worker.
- Read protocol: read ALL 4 channels IN FULL this run BEFORE naming any wall/claim; a stale/unread channel = flying blind. Log both sides (client bytes + exact server response) on sign-in/channel/master/crypto/disconnect.

## Method (NpSlayer seats ARE hooks — use them ACTIVELY, don't just watch)
- **absence of a log ≠ proof.** For a silent drop, plant an active seat at the dispatch/validation point (intercept/inspect/modify/call any VA, no crash needed).
- **Recover the SHAPE the client needs:** seat the CONSUMER + read a VALID instance's bytes — that IS the required shape, off the client's own data.
- **DIFFERENTIAL TRACE** (the guessing-killer): drive the client's OWN working construct AND our server's FAILING attempt through the SAME seats, then diff → either FINDS a server bug (wrong bytes/order/opcode → fix the server) or PROVES the wall architectural (diverge at an init-bound/never-built dispatch).
- Mine all channels + active seats BEFORE disassembly. To witness a NEW silent bridge, add a wrapper in the GAP tap (don't force-patch). Methods (not globals) → seat by VA from `ghidraRE/output/flowmodels/api/native_api.md`.
- Decisive GAP rule: if `client_gap.log` is SILENT for the char across login→roster→select, the char did NOT build via the Lua template path → cross-check the native `bff990` seat (construct is then native-NewObject or never-triggered).
