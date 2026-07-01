#!/usr/bin/env python3
"""The faithful offline server — modular entry point (GO-FORWARD start point).

This REPLACES Res/gss_stub_server_v4.py as the start point. The transport is modular under
server/src/net/ (crypto, framing, packets, login) + the connection seam (net.connection, still
strangler-fig-bridged to the legacy handler until the lean carve). Game logic lives in server/src
(the FaithfulHost / M-* stack). Behavior is byte-identical to the legacy stub for the clean E1/E2
login lane (`ordered-db-mage-select-enter`); this file mirrors the stub's main/main_async exactly,
swapping the monolith imports for the modular ones.
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import sys
from pathlib import Path

# server/src on the path for the net.* package; Res/ for the remaining legacy DB dep.
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE / "src"))
sys.path.insert(0, str(_HERE.parent / "Res"))

from net.connection import handle                       # noqa: E402
from net.crypto import DEFAULT_KEY_WINDOW, load_key_window  # noqa: E402
from net.login import MASTER_TIMING_PROBES              # noqa: E402
from live_character_db import DEFAULT_DB, init_db, load_account  # noqa: E402

LOG = logging.getLogger("gss.run_server")


async def main_async(args: argparse.Namespace) -> None:
    root = Path(args.root).resolve()
    db_path = Path(args.db).resolve()
    if args.init_db or args.full_gss:
        init_db(db_path, reset=args.reset_db)
        bundle = load_account(db_path, args.account)
        LOG.info("live DB ready at %s account=%s characters=%d",
                 db_path, args.account, len(bundle["characters"]))
    key_window = load_key_window(Path(args.key_window))
    LOG.info("loaded recovered key-window bytes=%d from %s", len(key_window), args.key_window)
    server = await asyncio.start_server(
        lambda r, w: handle(
            r,
            w,
            root,
            args.fixed_header,
            bytes.fromhex(args.response_header) if args.response_header else None,
            key_window,
            args.native_1006,
            args.native_1004_after,
            args.master,
            args.channel,
            args.signin_host,
            args.signin_port,
            args.full_gss,
            args.signin_wait,
            db_path,
            args.account,
            args.character_burst,
            args.character_template,
            args.finish_session_pid,
            not args.omit_finish_loading,
            not args.omit_signin_result,
            args.master_probe,
            args.master_crypto,
            args.master_envelope,
            args.hold_after_result_seconds,
        ),
        args.host,
        args.port,
    )
    LOG.info("listening on %s:%d root=%s", args.host, args.port, root)
    async with server:
        await server.serve_forever()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5004)
    ap.add_argument("--root", default=str(Path(__file__).resolve().parents[1]))
    ap.add_argument("--key-window", default=str(DEFAULT_KEY_WINDOW))
    ap.add_argument("--fixed-header", action="store_true",
                    help="diagnostic mode that reuses the 20260525_175851 header/stream")
    ap.add_argument("--response-header", default=None,
                    help="diagnostic 4-byte hex header override, e.g. 9c01b804")
    ap.add_argument("--native-1006", action="store_true",
                    help="send native lower GSS opcode 0x1006 sign-in result instead of high-level RemoteCall")
    ap.add_argument("--native-1004-after", action="store_true",
                    help="after native 0x1006, append one native 0x1004 master/channel update on the same receive stream")
    ap.add_argument("--master", default="master1")
    ap.add_argument("--channel", default="channel1")
    ap.add_argument("--signin-host", default="127.0.0.1",
                    help="endpoint host string returned in 0x1006 for the master connection")
    ap.add_argument("--signin-port", default="5004",
                    help="endpoint port string returned in 0x1006 for the master connection")
    ap.add_argument("--full-gss", action="store_true",
                    help="run the recovered 0x4009 -> 0x1001/0x1003/0x1004 -> 0x1006 flow")
    ap.add_argument("--signin-wait", type=float, default=120.0,
                    help="seconds to wait for the client's server-select/sign-in follow-up before continuing to log only")
    ap.add_argument("--db", default=str(DEFAULT_DB))
    ap.add_argument("--account", default="demo")
    ap.add_argument("--init-db", action="store_true",
                    help="create/update the local live SQLite database and exit only if --serve is not requested")
    ap.add_argument("--reset-db", action="store_true",
                    help="delete and recreate the local live SQLite database before serving")
    ap.add_argument("--character-burst", action="store_true",
                    help="after 0x1006, send inferred NESL account/character replication on the master socket")
    ap.add_argument("--character-template", default="TPlayerDummy",
                    help="template name for character NewObject packets during --character-burst")
    ap.add_argument("--finish-session-pid", type=int, default=None,
                    help="override target PID for Client_FinishLoadingCharacter; default uses the account Session m_pid")
    ap.add_argument("--omit-finish-loading", action="store_true",
                    help="diagnostic only: omit Client_FinishLoadingCharacter after character replication")
    ap.add_argument("--omit-signin-result", action="store_true",
                    help="diagnostic only: omit master Session.signInClientResult after character replication")
    ap.add_argument("--master-probe",
                    choices=("none", "issuepid", "account-root", "heartbeat", "silent", *MASTER_TIMING_PROBES),
                    default="none",
                    help="diagnostic only: ladder/diag probes run no-Frida master close-timing; heartbeat is client->server and kept for A/B")
    ap.add_argument("--master-crypto", choices=("xor", "none"), default="xor",
                    help="master transport mode: xor is protocol default above type-4 zlib; none is a likely-wrong A/B diagnostic")
    ap.add_argument("--master-envelope", choices=("raw", "oob2", "encrypt", "oob2-encrypt"), default="raw",
                    help="envelope applied inside the master zlib frame; oob2 follows Claude section 49.4")
    ap.add_argument("--hold-after-result-seconds", type=float, default=0.0,
                    help="diagnostic/logging only: keep master socket open after ladder result")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    # === PROTOCOL ASSERTION GATES ===
    # Per PROTOCOL_QUICKREF.md sections 1 and 6:
    # master requires XOR + zlib. Clear-zlib master is forbidden until
    # byte-proven runtime evidence overrides it.
    if args.master_crypto == "none":
        raise SystemExit(
            "ABORT: master_crypto='none' is the historical wrong default. "
            "Master REQUIRES type-2 XOR above type-4 zlib "
            "(PROTOCOL_QUICKREF.md section 1 layer chain, section 6.1). "
            "Client emits XOR window header on master "
            "(see gss_v4_clear_raw_heartbeat_20260528_041348.err.log). "
            "Use --master-crypto xor. If you have new evidence that overrides "
            "this, update PROTOCOL_QUICKREF.md FIRST and remove this assertion."
        )

    if args.character_template == "TPlayer" and args.full_gss:
        # TPlayer is the in-world template. For character-select replication
        # use TPlayerDummy.
        raise SystemExit(
            "ABORT: character_template='TPlayer' is wrong for character-select "
            "screen replication. Use TPlayerDummy (PROTOCOL_QUICKREF.md "
            "section 6.5). TPlayer is for after the user clicks Enter Game "
            "and User:Start fires."
        )
    # === END GATES ===
    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=logging.DEBUG if args.verbose else logging.INFO,
    )
    try:
        asyncio.run(main_async(args))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
