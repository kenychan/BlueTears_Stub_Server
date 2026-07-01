# SERVER_QUICKREF — where things live + how the pieces connect (read to change/read fast)

The faithful host = **`server/`**: START POINT **`run_server.py`** driving **`server/src`** (the emulator — runs the
shipped server Lua over a Python engine substrate, with its OWN wire/crypto/channel transport in `server/src/net/`,
stub-free + live-E1-verified 2026-07-01). **⛔ `Res/gss_stub_server_v4.py` = FROZEN LEGACY — reference only, no more
edits;** its transport is fully migrated into `server/src/net/`. Game logic AND transport now live in `server/`.

## 1. `server/src` — the host stack, in THREE TIERS (all self-tests GREEN)

**Layout (2026-07-01 reorg — follows the Lua, mirrors the client flowmodel tree):**
```
server/
  run_server.py                    ⭐ START POINT (modular host; live-E1-verified)
  src/
    engine/                        TIER 1 — the emulator SUBSTRATE (not per-Lua-object)
      lua/runtime.py     M-LUA     runs shipped Class/Com/Template Lua over plain tables (LuaHost)
      api/host_context.py M-API    bounded goEngine natives + the GWorld registry (oid→obj, FindObject)
      codec/wire.py      M-CODEC   goLua_Pack/PackFixed (de)serialize; tag-05 array / tag-07 obj / tag-0b ref
      repl/replicate.py  M-REPL    objectList → tag-05-of-tag-07 BackupReplicate payload (+ stub_bridge.py hook)
      obj/object_model.py M-OBJ    (legacy dataclass store; folding into M-API's GWorld registry)
      db/store.py        M-DB      optional persistence (CharStore, DBNatives, SP2_LoadPlayerData)
    char/build.py                  TIER 2 — per-object HOST OPS (mirror flowmodels/client/<object>.md)
    account/account.py             │  char: new_char/backup_replicate · account: setup_account/load_account
    charactermanager/roster.py     │  roster: load_players/create_character_result/replicate (RUN shipped Lua)
    net/  (crypto/framing/packets/login/connection)   TIER 3 — the GSS wire TRANSPORT
    host.py                        FaithfulHost = the composition root (wires T1 substrate, delegates to T2)
```
- **T1 substrate** = the engine (VM/native-API/codec/repl/db). Imports: `from engine.<mod>.<file> import …`.
- **T2 per-object ops** = the parts that map to a Lua object; they RUN the shipped server Lua (they don't
  reimplement it) over T1. Each names its client-RE counterpart in `ghidraRE/output/flowmodels/client/`.
- **T3 net** = the wire transport (its own tier, sibling to the engine — NOT under engine/; it's the one
  live-E1-verified path, `run_server.py → net.* → bridged handle`).

### `FaithfulHost` (`host.py`) — the composition root; run its `__main__` demo to see the chain
- `setup_account(id, [(oid,level,name)…])` → `account.setup_account` → builds host char objects
  (`char.build.new_char`: a Lua table + a `Player` component carrying `m_AccountId`), **registers each in
  `GWorld`** (`ctx.gworld[oid]=c`), + optional M-DB persist.
- `load_players(id)` / `create_character_result(oid)` / `char_load_to_wire(id)` → `charactermanager.roster.*`
  (runs the shipped `TPlayerCache:LoadPlayers` / `CharacterManager:serverCreateCharacterResult` → BackupReplicate
  → M-REPL payload bytes).

## 2. The client-side registration facts (native — the ROW prerequisite)

The client registers a char in ITS world by the NATIVE receiver, not Lua (see `NORTH_STAR.md` §4, LATE-144):
- **`0x00bff990`** = full-construct entry; writes server pid → `object+0x3c`, calls `0x00bd4b20` (component tail) + `0x00c024b0`.
- **`0x00647860`** = the registration insert into the per-world **OID→object map** the resolver/`FindObject` reads.
- For **TClient** the trigger is the GSS handshake `0x1001 → ca4540 → ca47e0 → bff990` (MTLC). The **char analog is UNKNOWN**
  = the one open item (the replication envelope).
- **⭐ CLIENT FLOWMODEL TREE → `ghidraRE/output/flowmodels/client/`** — one file per engine object the Lua calls (organized
  by object, client-side), with **`_MASTER.md`** = the cross-object login→roster call graph. The deep one is
  **`gworld.md`**: the three maps (A `DAT_01825258` string-resolver · B `DAT_0184a90c` descriptor cache · **C `manager+0x1c`
  = the OID→object table `FindObject` reads**), the registration primitives (`bff990` construct → `647a30` lookup / `647860`
  insert into map C → `bd4b20` obj+0x18 tail + `c024b0`; `bd2cd0` obj+0x18 writer; `AddComponent`→`bcf510`), both resolution
  paths, the two tiers, nodes+edges, and §5 = the OPEN char-trigger (E3 blocker) + differential-trace plan. Each object file
  names its **server counterpart** under `server/src/<object>/` (the parallel server tree). Every claim cites its RE source.

## 3. The modular server (GO-FORWARD start point) — extraction from the legacy stub

**Start point = `server/run_server.py`** (NEW, ✅ LIVE-E1-VERIFIED 2026-07-01), NOT the legacy
`Res/gss_stub_server_v4.py`. The proven transport is extracted verbatim (byte-exact behavior) into
`server/src/net/` modules; the stub is the reference source and gets archived once the connection
handler is carved (below). Strategy = strangler-fig: extract self-contained leaves first, keep the
delicate connection handler bridged until it's moved under test, and re-verify E1 at each stage.

**Extraction tracker (stub source → module):**
| Module | Status | From `gss_stub_server_v4.py` |
|---|---|---|
| `net/crypto.py`  | ✅ done+verified | `xor_with_stream`, `load_canonical_key_stream`, `load_key_window`, `key_stream_from_*`, `header_window_is_valid`, `ServerCryptoWriter`, `RECOVERED_KEY_*` |
| `net/framing.py` | ✅ done+verified | `parse_lower_packets`, `parse_clientworld_key6_prefix`, `build_zlib_transport_frame`, `parse_zlib_transport_frames`, `ZlibSyncFrameDecoder`, `decode_channel_signin_request` |
| `net/packets.py` | ✅ done+verified | `pack_*`, `build_lower_gss_body`, `build_native_*`, `build_clientworld_issuepid/newobject/remotecall/linkhaving`, `build_tclient/taccount_*` tails, the login RPC downcalls, `handle_ref`, `pack_rpc_variant`, `apply_master_envelope` + the descriptor-key & CLIENTWORLD_* method-NID tables. **Verified: 65 shared-builder cases byte-equal to the stub, 0 mismatch** (`scratchpad/verify_packets.py`). `build_clientworld_newobject` = simplified TClient/TAccount-only clean-lane version. Deps `native_nid`, `pack_variant` bridge-imported from Res/. |
| `net/login.py`   | ✅ done+verified | `master_timing_packets` (the LATE-144 clean rung) + `select_db_mage_character` + `classify_alive_ms`. **Verified: all 10 clean-lane packets byte-equal to the stub.** MASTER_TIMING_PROBES collapsed to the single live rung (dead rungs pruned). Imports builders `from .packets`. |
| `net/connection.py` | ✅ CARVED + LIVE-E1-VERIFIED | **real code, STUB-FREE** (no more `from gss_stub_server_v4 import`). The clean E1/E2 path (sign-in full-GSS `handle` + master `handle_master_phase`: batch the 10 login packets in one zlib frame → hold + observe key6 upcalls, answer SRQL) carved out; the ~30 dead research-probe branches, the paced/per-part emit modes, the silent/heartbeat/issuepid/account-root diagnostics, and the dead pushed-`NewObject("TPlayer")` char-create branch all DROPPED. Live E1 2026-07-01 (`clean login batch sent packets=10` from `gss.net.connection` → `ShowJobClassSelection`). |
| `run_server.py`  | ✅ done + LIVE-E1-VERIFIED | `main_async` + `main` + argparse, wired to the modular `net.*`. **Live run 2026-07-01 reached `UIIntro:ShowJobClassSelection` (signInClientResult→logInClientResult→Client_FinishLoadingCharacter[true,0]) — the modular host is a proven drop-in for the stub.** |

**✅ EXTRACTION COMPLETE — the modular host is fully STUB-FREE.** Every `server/` code path is detached
from `Res/gss_stub_server_v4.py` (remaining refs are provenance comments only). The stub is FROZEN
LEGACY, retained only as the milestone-mirror reference + the harness `--server-script` default; it can
be archived. (Legacy Res deps still imported by `net/packets.py`: `gss_stub_server_v3.pack_variant`,
`nid_hash.native_nid` — small helpers, separate from v4.)

**How to live-run the modular host:** `py Res/run_master_ladder.py --mode ordered-db-mage-select-enter
--admin-command launch_original_stub --server-script server/run_server.py` (the new `--server-script`
flag points the harness at run_server.py; default stays the legacy stub). Server logger = `gss.run_server`
(entry) + `gss-v4` (bridged handle) in `Res/RE_findings/master_ladder_*_ordered-db-mage-select-enter.stderr.log`.

**Legacy module deps still bridged (Res/, small helpers — NOT the v4 stub):** `gss_stub_server_v3` (SESSION_OK,
build_signin_client_result, pack_variant), `live_character_db` (DEFAULT_DB, init_db, load_account), `nid_hash`
(native_nid).

**✅ CARVE DONE (2026-07-01):** the clean-path `handle`/`handle_master_phase` are lifted into `net/connection.py` as
real stub-free code (the ~30 dead-probe branches dropped) + live-E1-verified. `net.connection` no longer imports
`gss_stub_server_v4` — the v4 stub is fully DETACHED and can be archived (FROZEN LEGACY reference only).

**Dead lanes NOT extracted (archived in git history + the mirror):** all `build_b4_*` / `build_tplayerdummy_*` /
char-tail builders, `build_backupobj`, `build_character_replication_packets`, the 32 dead `MASTER_*_PROBES` + their
handler branches (already deleted from the stub), the `BT_*` env gates. The char path is the single
**host-replication hook** (`FaithfulHost` → `("raw",bytes)`), pending the replication ENVELOPE (§2).

### The clean E1/E2 login sequence (what the lean handler must emit — verbatim behavior)
1. `issuepid(0,1001)` · `newobject(1001,"TClient")` · `remotecall(1001,"SetControlObject",[ref(1001)])`
2. `issuepid(0,1002)` · `newobject(1002,"TAccount",session_tail=True)` · `remotecall(1002,"SetControlObject",[ref(1002)])` · `remotecall(1002,"signInClientResult",[1,1,1])`
3. `loginclientresult_downcall(1002, plr_id=1002, target=1002, oid=1002)` (LATE-138: login RPCs target the account pid, not a ghost char)
4. `onloadplayers_downcall(1002,1002, oid=1002, object_list_oids=[])` (empty roster)
5. `remotecall(1002,"Client_FinishLoadingCharacter",[True,0])` (count=0 → `ShowJobClassSelection`)
- Char path (future/E3): step 4's `object_list_oids` + a preceding replication come from `FaithfulHost` via the host hook
  — pending the replication ENVELOPE (§2).

## 4. Run / verify
- server module self-tests: `py server/src/host.py` (full chain demo), or per-module `py server/src/<mod>.py`.
- live E1: `py server/tools/derisk_run.py` (default lane = clean login) — verify UAC heartbeat first (`Res/AdminLauncher/launcher_heartbeat.txt` fresh).
- decomposition safety: `py -m py_compile Res/gss_stub_server_v4.py` after each cut; then E1.
