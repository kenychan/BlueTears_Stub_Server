"""Per-client GSS connection handler — the clean E1/E2 login lane, carved stub-free.

Real code (no longer bridged to Res/gss_stub_server_v4.py). The ~30 dead research-probe branches,
the paced/per-part emit strategies, the silent/heartbeat/issuepid/account-root diagnostic modes, and
the dead pushed-`NewObject("TPlayer")` char-create branch are all DROPPED — only the clean
`ordered-db-mage-select-enter` login path remains (LATE-144). Behavior is verified by the live E1 test
reaching `UIIntro:ShowJobClassSelection`.

Two sockets:
  sign-in socket → `handle` full-GSS flow: 0x1001 challenge → 0x1003+0x1004 → 0x1006 → observe.
  master  socket → `handle_master_phase`: one atomic zlib frame carrying the clean login packets
                   (net.login) → hold the socket open + observe the client's key6 upcalls.
"""
from __future__ import annotations

import asyncio
import logging
import os
import sys as _sys
import time
from pathlib import Path
from pathlib import Path as _Path

_RES = _Path(__file__).resolve().parents[3] / "Res"
if str(_RES) not in _sys.path:
    _sys.path.insert(0, str(_RES))

from nid_hash import native_nid              # noqa: E402
from live_character_db import load_account   # noqa: E402

from .crypto import (                        # noqa: E402
    ServerCryptoWriter,
    header_window_is_valid,
    key_stream_from_window,
    xor_with_stream,
)
from .framing import (                       # noqa: E402
    ZlibStreamFrameEncoder,
    ZlibSyncFrameDecoder,
    build_zlib_transport_frame,
    decode_channel_signin_request,
    parse_clientworld_key6_prefix,
    parse_lower_packets,
)
from .packets import (                       # noqa: E402
    KEY6_CALLABLE_NAME_BY_NID,
    SESSION_DESCRIPTOR_KEY,
    apply_master_envelope,
    build_clientworld_remotecall,
    build_native_master_channel_update,
    build_native_session_in_challenge,
    build_native_session_in_result,
    build_native_signin_result,
    build_onclientgetsrqlresult_downcall,
)
from .login import master_timing_packets     # noqa: E402

LOG = logging.getLogger("gss.net.connection")

# Master-phase crypto header (recovered trace login_trace_20260525_175851).
DEFAULT_FULL_GSS_RESPONSE_HEADER = bytes.fromhex("9c 01 1c 02")


def expected_signin_body(root: Path) -> bytes:
    """The known-plaintext 0x4009 sign-in body for our local launch args (128 bytes). XORing the
    client's cipher body against this recovers the keystream window."""
    bin_path = str(root / "TW" / "Bin") + "\\"
    path_bytes = bin_path.encode("ascii")
    version = b"001863_40.8.5"
    provider = b"Authenticator"
    token = b"dummy"
    local_key = bytes([1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0])
    body = bytearray()
    body += (112).to_bytes(4, "little")
    body += b"\x00"
    body += (0x4009).to_bytes(2, "little")
    body += b"\x00" * 9
    body += len(token).to_bytes(4, "little") + token
    body += len(local_key).to_bytes(4, "little") + local_key
    body += (0).to_bytes(4, "little")
    body += (0).to_bytes(4, "little")
    body += len(version).to_bytes(4, "little") + version
    body += len(path_bytes).to_bytes(4, "little") + path_bytes
    body += len(provider).to_bytes(4, "little") + provider
    if len(body) != 128:
        raise ValueError(f"expected 128-byte sign-in body, got {len(body)}")
    return bytes(body)


def log_client_bytes(peer, label: str, data: bytes) -> None:
    LOG.info("[%s] client->server %s len=%d bytes=%s", peer, label, len(data), data.hex(" "))


async def handle_master_phase(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    peer: object,
    client_header: bytes,
    response_header: bytes,
    key_window: dict[int, int],
    db_path: Path,
    account_name: str,
    master_envelope: str,
    hold_after_result_seconds: float,
) -> None:
    """The master login phase: send the clean E1/E2 login packets as one atomic zlib frame under the
    XOR crypto header, then hold the socket open and observe the client's key6 upcalls (answer
    OnServerGetSRQL = normal char-select data; note Server_FinishLoadingCharacter)."""
    master_accept_at = time.perf_counter()
    client_start = int.from_bytes(client_header[:2], "little")
    client_end = int.from_bytes(client_header[2:4], "little")
    LOG.info(
        "[%s] master-phase initial4=%s start=0x%x end=0x%x valid_crypto_window=%s envelope=%s",
        peer, client_header.hex(" "), client_start, client_end,
        header_window_is_valid(client_header), master_envelope,
    )
    # master transport = type-2 XOR over type-4 zlib; send the crypto header first.
    writer.write(response_header)
    await writer.drain()
    LOG.info("WIRE: server->client len=%d bytes=%s", len(response_header), response_header.hex(" "))
    LOG.info("[%s] sent master-phase server crypto header=%s", peer, response_header.hex(" "))
    crypto = ServerCryptoWriter(writer, response_header, key_window)
    crypto.sent_header = True

    client_recv_cursor = client_start
    client_zlib_decoder = ZlibSyncFrameDecoder()
    # LATE-168: the server->client direction is ONE shared zlib stream (the client uses one
    # persistent decompressobj). Every frame — the login batch AND every later reply — must
    # continue this SAME stream, or the client's inflate ends and rejects the next frame.
    server_zlib_encoder = ZlibStreamFrameEncoder()

    # The clean E1/E2 login: TClient(control) + faithful TAccount(4-Com) + login RPCs + empty roster
    # -> Client_FinishLoadingCharacter[true,0] -> ShowJobClassSelection. All lower packets in ONE frame.
    packets = master_timing_packets("ordered-db-mage-select-enter", account_name=account_name)
    inner_burst = b"".join(packets)
    burst = apply_master_envelope(inner_burst, master_envelope)
    zlib_frame = server_zlib_encoder.frame(burst)
    try:
        await crypto.send(zlib_frame)
    except (ConnectionResetError, BrokenPipeError, OSError) as exc:
        LOG.info("[master-result] [%s] alive_ms=%d event=batch_send_error error=%s",
                 peer, int((time.perf_counter() - master_accept_at) * 1000), exc)
        return
    LOG.info(
        "[master] [%s] clean login batch sent packets=%d inner_bytes=%d frame_bytes=%d bodies=%s",
        peer, len(packets), len(inner_burst), len(zlib_frame),
        [{"opcode": int.from_bytes(p[5:7], "little"), "body": p[16:].hex(" ")}
         for p in packets if len(p) >= 16],
    )

    answered_srql = False
    answered_finish = False
    answered_checkname = False
    answered_create = False

    async def send_master_inner(label: str, response: bytes) -> None:
        frame = server_zlib_encoder.frame(apply_master_envelope(response, master_envelope))
        LOG.info("[%s] responding %s inner_len=%d lower_packets=%s", peer, label, len(response),
                 [{"opcode": op, "body": b.hex(" ")} for op, b in parse_lower_packets(response)])
        await crypto.send(frame)

    # Keep the socket open while the client processes login -> ShowJobClassSelection, decoding its
    # key6 upcalls. OnServerGetSRQL (first char's char-select data) -> answer; Server_Finish -> note.
    hold_until = time.perf_counter() + max(hold_after_result_seconds, 5.0)
    while time.perf_counter() < hold_until:
        timeout = max(0.1, min(5.0, hold_until - time.perf_counter()))
        try:
            data = await asyncio.wait_for(reader.read(4096), timeout=timeout)
        except asyncio.TimeoutError:
            continue
        except (ConnectionResetError, OSError) as exc:
            LOG.info("[%s] master recv error=%r", peer, exc)
            return
        if not data:
            LOG.info("[%s] master EOF alive_ms=%d", peer,
                     int((time.perf_counter() - master_accept_at) * 1000))
            return
        log_client_bytes(peer, "master clean", data)
        stream = key_stream_from_window(client_header, client_recv_cursor, len(data), key_window)
        if stream is not None:
            clear = xor_with_stream(data, stream)
            for _raw_len, _inflated_len, lower_packets, status in client_zlib_decoder.feed(clear):
                if status != "ok":
                    continue
                for opcode, body in lower_packets:
                    if opcode != 0x06:
                        continue
                    prefix = parse_clientworld_key6_prefix(body)
                    if not prefix:
                        continue
                    LOG.info(
                        "[%s] client key6 from=%d target=%d object=%d self=0x%08x "
                        "callable=0x%08x(%s) selector=0x%04x",
                        peer, prefix["from_pid"], prefix["target_pid"], prefix["object_id"],
                        prefix["self_key"], prefix["callable_key"],
                        KEY6_CALLABLE_NAME_BY_NID.get(prefix["callable_key"], "unknown"),
                        prefix["selector"],
                    )
                    if (prefix["callable_key"] == native_nid("OnServerGetSRQL")
                            and prefix["self_key"] == native_nid("Player") and not answered_srql):
                        dispatch_pid = prefix["object_id"] or prefix["from_pid"]
                        if os.environ.get("BT_SEND_SRQL_LEGACY") == "1":
                            # LATE-167: the SRQL reply's empty-{}-table arg fails the client's native
                            # validity gate 0x0115bfd0 -> ca4070 report -> session shutdown -> close.
                            # DEFAULT = suppress (the new clean base — channel holds into create).
                            # This flag re-sends the malformed reply ONLY to diagnose the gate.
                            await send_master_inner(
                                "Player.OnClientGetSRQLResult (empty SRQL) [LEGACY malformed]",
                                build_onclientgetsrqlresult_downcall(dispatch_pid),
                            )
                        else:
                            LOG.info("[%s] OnServerGetSRQL observed; SRQL reply SUPPRESSED "
                                     "(clean base — malformed {}-arg trips validity gate 0x0115bfd0)", peer)
                        answered_srql = True
                    if (prefix["callable_key"] == native_nid("checkCharacterName")
                            and not answered_checkname):
                        # LATE-168: the client drives its OWN faithful create flow. Answer
                        # checkCharacterName -> checkCharacterNameResult(result=SCS_OK=1, reason=0)
                        # on CharacterManager. Scalar args (like the WORKING signInClientResult
                        # [1,1,1]) pass the validity gate; only the SRQL {}-table arg failed.
                        dispatch_pid = prefix["object_id"] or prefix["from_pid"]
                        await send_master_inner(
                            "CharacterManager.checkCharacterNameResult (SCS_OK=0)",
                            build_clientworld_remotecall(
                                dispatch_pid, "checkCharacterNameResult", [0, 0],
                                object_id=prefix["object_id"],
                            ),
                        )
                        answered_checkname = True
                    if (prefix["callable_key"] == native_nid("createCharacter")
                            and not answered_create):
                        # LATE-168: createCharacter -> createCharacterResult(SCS_OK=1, reason=0).
                        # serverCreateCharacterResult(oid)+GWorld:FindObject(oid):BackupReplicate
                        # (the actual char delivery) is the NEXT layer once these results land.
                        dispatch_pid = prefix["object_id"] or prefix["from_pid"]
                        await send_master_inner(
                            "CharacterManager.createCharacterResult (SCS_OK=0)",
                            build_clientworld_remotecall(
                                dispatch_pid, "createCharacterResult", [0, 0],
                                object_id=prefix["object_id"],
                            ),
                        )
                        answered_create = True
                    if (prefix["callable_key"] == native_nid("Server_FinishLoadingCharacter")
                            and prefix["self_key"] == SESSION_DESCRIPTOR_KEY and not answered_finish):
                        LOG.info("[%s] observed Session.Server_FinishLoadingCharacter; no echo", peer)
                        answered_finish = True
        if client_end > client_start:
            window = client_end - client_start
            client_recv_cursor = client_start + ((client_recv_cursor - client_start + len(data)) % window)
    LOG.info("[master-result] [%s] alive_ms=%d event=hold_complete", peer,
             int((time.perf_counter() - master_accept_at) * 1000))


async def handle(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    root: Path,
    fixed_header: bool,
    response_header_override: bytes | None,
    key_window: dict[int, int],
    native_1006: bool,
    native_1004_after: bool,
    master: str,
    channel: str,
    signin_host: str,
    signin_port: str,
    full_gss: bool,
    signin_wait: float,
    db_path: Path,
    account_name: str,
    character_burst: bool,
    character_template: str,
    finish_session_pid: int | None,
    finish_loading: bool,
    signin_result: bool,
    master_probe: str,
    master_crypto: str,
    master_envelope: str,
    hold_after_result_seconds: float,
) -> None:
    """Per-client dispatcher. The signature matches run_server's call; the clean lane uses only the
    full-GSS sign-in flow + the master phase. The diagnostic-only params (fixed_header, native_1006,
    native_1004_after, character_*, finish_*, signin_result, master_probe, master_crypto) are retained
    for call-compat but unused — their branches were the dead lanes dropped in the carve."""
    peer = writer.get_extra_info("peername")
    LOG.info("[%s] connected", peer)
    try:
        header = await reader.readexactly(4)
        try:
            cipher_body = await asyncio.wait_for(reader.readexactly(128), timeout=2.0)
        except asyncio.TimeoutError:
            # master socket: no 128-byte sign-in body -> the master login phase.
            await handle_master_phase(
                reader, writer, peer, header,
                response_header_override or DEFAULT_FULL_GSS_RESPONSE_HEADER,
                key_window, db_path, account_name, master_envelope, hold_after_result_seconds,
            )
            return

        # sign-in socket: recover the keystream window from the known-plaintext body.
        plain = expected_signin_body(root)
        stream = bytes(c ^ p for c, p in zip(cipher_body, plain))
        LOG.info("[%s] first header=%s stream=%s", peer, header.hex(" "), stream[:32].hex(" "))
        log_client_bytes(peer, "initial sign-in header+body", header + cipher_body)
        live_start = int.from_bytes(header[:2], "little")
        client_key_window = dict(key_window)
        server_key_window = dict(key_window)
        for i, value in enumerate(stream):
            client_key_window.setdefault(live_start + i, value)  # canonical wins on conflict

        # full-GSS sign-in: 0x1001 challenge -> 0x1003+0x1004 master/channel -> 0x1006 sign-in result.
        response_header = response_header_override or DEFAULT_FULL_GSS_RESPONSE_HEADER
        crypto = ServerCryptoWriter(writer, response_header, server_key_window)
        bundle = load_account(db_path, account_name)
        account_id = int(bundle["account"]["id"])
        stage1 = build_native_session_in_challenge()
        await crypto.send(stage1)
        LOG.info("[%s] sent full-gss stage1 0x1001 bytes=%d chars=%d",
                 peer, len(stage1), len(bundle["characters"]))

        challenge_reply = b""
        try:
            challenge_reply = await asyncio.wait_for(reader.read(4096), timeout=10.0)
        except asyncio.TimeoutError:
            LOG.info("[%s] no 0x4002 challenge-response within 10s; leaving open", peer)
        if challenge_reply:
            log_client_bytes(peer, "0x4002 challenge follow-up", challenge_reply)
            client_cursor = live_start + len(cipher_body)
            follow_stream = key_stream_from_window(header, client_cursor, len(challenge_reply), client_key_window)
            if follow_stream is not None:
                clear = xor_with_stream(challenge_reply, follow_stream)
                LOG.info("[%s] decoded challenge follow-up clear=%s packets=%s",
                         peer, clear[:128].hex(" "), parse_lower_packets(clear))
            stage2 = build_native_session_in_result(account_id) + build_native_master_channel_update(master, channel)
            await crypto.send(stage2)
            LOG.info("[%s] sent full-gss stage2 0x1003+0x1004 bytes=%d", peer, len(stage2))

        try:
            data = await asyncio.wait_for(reader.read(4096), timeout=signin_wait)
        except asyncio.TimeoutError:
            LOG.info("[%s] no post-channel/sign-in packet within %.1fs", peer, signin_wait)
            data = b""
        if data:
            log_client_bytes(peer, "post-channel/sign-in", data)
            sign_master_id = 1
            sign_channel_id = 1
            post_cursor = live_start + len(cipher_body) + len(challenge_reply)
            post_stream = key_stream_from_window(header, post_cursor, len(data), client_key_window)
            if post_stream is not None:
                clear = xor_with_stream(data, post_stream)
                packets = parse_lower_packets(clear)
                LOG.info("[%s] decoded post-channel/sign-in clear=%s packets=%s",
                         peer, clear[:128].hex(" "), packets)
                for opcode, body in packets:
                    if opcode == 0x4005:
                        decoded = decode_channel_signin_request(body)
                        if decoded is not None:
                            _token, sign_master_id, sign_channel_id = decoded
                            LOG.info("[%s] decoded 0x4005 master_id=%d channel_id=%d",
                                     peer, sign_master_id, sign_channel_id)
            sign_in = build_native_signin_result(signin_host, signin_port, sign_master_id, sign_channel_id)
            await crypto.send(sign_in)
            LOG.info("[%s] sent full-gss stage3 0x1006 bytes=%d master_id=%d channel_id=%d endpoint=%s:%s",
                     peer, len(sign_in), sign_master_id, sign_channel_id, signin_host, signin_port)

        while True:
            data = await reader.read(4096)
            if not data:
                LOG.info("[%s] EOF", peer)
                return
            log_client_bytes(peer, "post-gss follow-up", data)
    except asyncio.IncompleteReadError as e:
        LOG.info("[%s] disconnected early: partial=%d", peer, len(e.partial))
    except (ConnectionError, OSError) as e:
        LOG.info("[%s] connection closed/reset: %s", peer, e)
    finally:
        writer.close()
        try:
            await writer.wait_closed()
        except (ConnectionError, OSError):
            pass


__all__ = ["handle", "handle_master_phase"]
