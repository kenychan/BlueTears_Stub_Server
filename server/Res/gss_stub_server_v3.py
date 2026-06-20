#!/usr/bin/env python3
"""
gss_stub_server_v3.py — RPC-aware Punch Monster TW (BlueTears) stub server.

v3 builds on v2 with:
  - **Variant serialization** matching the wire format recovered from
    FUN_00bef480 (the C++ variant deserializer used by LuaRemoteCall).
  - **RemoteCall RPC envelopes** matching the Lua Component:UpCall /
    DownCall API recovered from `Lua/Lib/Component.luo`.
  - **Session protocol replies** matching `Lua/Class/Main/Session.luo`.

WIRE FORMAT
-----------

Per packet on the wire:
    [u16 length][XOR-encrypted body of `length` bytes]

The initial 4-byte connection header is sent ONCE by the server before any
encrypted packet:
    [u16 key_start][u16 key_end]
This sets the recv-side cipher window. After receiving it, the client
mirrors the window on its send side.

XOR cipher (from FUN_01074af0/FUN_010748b0):
    for each byte:
      key_idx = (key_idx + 1) % (key_end - key_start)
      byte ^= key_buffer[key_start + key_idx]

The key_buffer is at GSSConnector_subobj + 0x1c on the client; harvest
with `dump_xor_key.py` from a live signed-in session.

NESL Variant on the wire (recovered from FUN_00bef480):
    +0  u8 type_tag
    +1  per-type payload:
        tag 1 (bool)     : u8                  (0 or 1)
        tag 3 (number)   : IEEE 754 double     (8 bytes)
        tag 4 (string)   : u32 length + bytes
        tag 5 (table)    : u16 count + N x [key_variant, value_variant]
        tag 7 (object)   : string class_name + fields (impl-dependent)
        tag 11 (handle)  : u32

RemoteCall RPC packet body (recovered from Component:UpCall + RawPack):
    The packet body sent over the wire as the contents of a `RemoteCall`
    transport opcode is itself a sequence of variants representing:
        [target_pid : number]
        [fn_name    : string]
        [arg_count  : number]
        [arg_1, arg_2, ..., arg_N : variants]

USAGE
-----
    # 1. Harvest cipher key (one-time, from live client):
    python dump_xor_key.py --pid <NClient_pid> --out key.bin

    # 2. Redirect host:
    echo "127.0.0.1 gss.dlonline.com.tw" >> hosts

    # 3. Run stub:
    python gss_stub_server_v3.py --key-blob key.bin --port 5004
"""

from __future__ import annotations

import argparse
import asyncio
import logging
import struct
from pathlib import Path

LOG = logging.getLogger("gss")


# ---------------------------------------------------------------------------
# NESL Variant wire codec.
# ---------------------------------------------------------------------------

TAG_NULL    = 0
TAG_BOOL    = 1
TAG_NUM     = 3
TAG_STRING  = 4
TAG_TABLE   = 5
TAG_OBJECT  = 7
TAG_HANDLE  = 11


def pack_variant(value) -> bytes:
    """Serialize a Python value to the NESL wire-Variant format.
    The exact rules are inferred from FUN_00bef480 + RawPack:
      - None       → tag 0 (null), no payload
      - bool       → tag 1 + u8
      - int/float  → tag 3 + f64
      - str        → tag 4 + u32 len + utf-8 bytes
      - dict/list  → tag 5 + u16 count + recursive entries
    """
    if value is None:
        return bytes([TAG_NULL])
    if isinstance(value, bool):
        return bytes([TAG_BOOL, 1 if value else 0])
    if isinstance(value, (int, float)):
        return bytes([TAG_NUM]) + struct.pack("<d", float(value))
    if isinstance(value, (bytes, bytearray)):
        return bytes([TAG_STRING]) + struct.pack("<I", len(value)) + bytes(value)
    if isinstance(value, str):
        data = value.encode("utf-8")
        return bytes([TAG_STRING]) + struct.pack("<I", len(data)) + data
    if isinstance(value, (list, tuple)):
        out = bytearray([TAG_TABLE])
        out.extend(struct.pack("<H", len(value)))
        for i, v in enumerate(value, 1):
            out.extend(pack_variant(i))   # 1-based key (Lua convention)
            out.extend(pack_variant(v))
        return bytes(out)
    if isinstance(value, dict):
        out = bytearray([TAG_TABLE])
        out.extend(struct.pack("<H", len(value)))
        for k, v in value.items():
            out.extend(pack_variant(k))
            out.extend(pack_variant(v))
        return bytes(out)
    raise TypeError(f"unhandled variant value: {value!r}")


def unpack_variant(buf: bytes, off: int = 0) -> tuple[object, int]:
    """Read one Variant starting at buf[off]; return (value, new_off)."""
    if off >= len(buf):
        raise ValueError("variant: buffer truncated")
    tag = buf[off]
    off += 1
    if tag == TAG_NULL:
        return None, off
    if tag == TAG_BOOL:
        return bool(buf[off]), off + 1
    if tag == TAG_NUM:
        (v,) = struct.unpack_from("<d", buf, off)
        return v, off + 8
    if tag == TAG_STRING:
        (n,) = struct.unpack_from("<I", buf, off)
        off += 4
        return buf[off:off+n].decode("utf-8", errors="replace"), off + n
    if tag == TAG_TABLE:
        (n,) = struct.unpack_from("<H", buf, off)
        off += 2
        out = {}
        for _ in range(n):
            k, off = unpack_variant(buf, off)
            v, off = unpack_variant(buf, off)
            out[k] = v
        return out, off
    if tag == TAG_OBJECT:
        # Implementation simplification: read a string class name and
        # then leave the body to the caller to parse based on class.
        class_name, off = unpack_variant(buf, off)
        return ("<object>", class_name), off
    if tag == TAG_HANDLE:
        (h,) = struct.unpack_from("<I", buf, off)
        return ("<handle>", h), off + 4
    raise ValueError(f"variant: unknown tag {tag}")


# ---------------------------------------------------------------------------
# RemoteCall RPC envelope.
# ---------------------------------------------------------------------------

class RemoteCallPacket:
    """A RemoteCall RPC packet body.

    The body is a sequence of variants:
        [target_pid][fn_name][arg_count][arg_1]...[arg_N]
    The arg_count variant is a number; the args are typed.
    """

    def __init__(self, target_pid: int, fn_name: str, args: list) -> None:
        self.target_pid = target_pid
        self.fn_name = fn_name
        self.args = args

    def to_bytes(self) -> bytes:
        out = bytearray()
        out.extend(pack_variant(self.target_pid))
        out.extend(pack_variant(self.fn_name))
        out.extend(pack_variant(len(self.args)))
        for a in self.args:
            out.extend(pack_variant(a))
        return bytes(out)

    @classmethod
    def from_bytes(cls, buf: bytes) -> "RemoteCallPacket":
        off = 0
        target_pid, off = unpack_variant(buf, off)
        fn_name,    off = unpack_variant(buf, off)
        arg_count,  off = unpack_variant(buf, off)
        try:
            n = int(arg_count)
        except (TypeError, ValueError):
            n = 0
        args = []
        for _ in range(n):
            v, off = unpack_variant(buf, off)
            args.append(v)
        if isinstance(target_pid, float):
            target_pid = int(target_pid)
        return cls(target_pid, str(fn_name), args)

    def __repr__(self) -> str:
        return (f"RemoteCall(target_pid={self.target_pid}, fn={self.fn_name!r}, "
                f"args={self.args!r})")


# ---------------------------------------------------------------------------
# XOR cipher (rolling key window).
# ---------------------------------------------------------------------------

class XorStream:
    def __init__(self, key_buffer: bytes, key_start: int, key_end: int) -> None:
        if key_end <= key_start:
            raise ValueError(f"bad window: {key_start}..{key_end}")
        if key_end > len(key_buffer):
            raise ValueError(f"window {key_end} exceeds key size {len(key_buffer)}")
        self.key = key_buffer
        self.start = key_start
        self.end = key_end
        self.window = key_end - key_start
        self.idx = 0

    def transform(self, data: bytes) -> bytes:
        out = bytearray(len(data))
        for i, b in enumerate(data):
            self.idx = (self.idx + 1) % self.window
            out[i] = b ^ self.key[self.start + self.idx]
        return bytes(out)


# ---------------------------------------------------------------------------
# Session-level Response Builders (from Lua/Class/Main/Session.luo).
# ---------------------------------------------------------------------------

# Stub PID assignments — real values are server-generated.
PID_SESSION   = 1
PID_GSS       = 2
PID_USER      = 3
PID_CLIENT    = 100   # the client's own PID

# Session result codes — opaque on the wire. Common conventions:
SESSION_OK              = 0
SESSION_FAIL_BAD_LOGIN  = 1


def build_signin_client_result(result: int, master: str = "master1",
                                channel: str = "channel1") -> bytes:
    """Server → client: Session.signInClientResult(from, result, master, channel)
    `from` is the source PID (PID_SESSION), supplied by the RPC framework
    so we don't pack it in the args list."""
    rpc = RemoteCallPacket(
        target_pid=PID_SESSION,                # invoke on the Session object
        fn_name="signInClientResult",
        args=[result, master, channel],
    )
    return rpc.to_bytes()


def build_login_client_result(result: int, master: str, channel: str,
                              account_id: int, player_id: int) -> bytes:
    rpc = RemoteCallPacket(
        target_pid=PID_SESSION,
        fn_name="logInClientResult",
        args=[result, master, channel, account_id, player_id],
    )
    return rpc.to_bytes()


def build_signout_client_result() -> bytes:
    rpc = RemoteCallPacket(
        target_pid=PID_SESSION,
        fn_name="signOutClientResult",
        args=[],
    )
    return rpc.to_bytes()


# ---------------------------------------------------------------------------
# Connection.
# ---------------------------------------------------------------------------

class GssConnection:
    def __init__(self, reader: asyncio.StreamReader,
                 writer: asyncio.StreamWriter,
                 key_buffer: bytes | None) -> None:
        self.reader = reader
        self.writer = writer
        self.key_buffer = key_buffer
        self.recv_cipher: XorStream | None = None
        self.send_cipher: XorStream | None = None
        self.peer = writer.get_extra_info("peername")

    async def run(self) -> None:
        LOG.info("[%s] connection opened", self.peer)
        try:
            await self._send_initial_header()
            await self._loop()
        except (asyncio.IncompleteReadError, ConnectionResetError) as e:
            LOG.info("[%s] disconnected: %s", self.peer, e)
        except Exception:
            LOG.exception("[%s] connection error", self.peer)
        finally:
            self.writer.close()

    async def _send_initial_header(self) -> None:
        if self.key_buffer is None:
            LOG.warning("[%s] no cipher key — running plaintext (client WILL reject)", self.peer)
            key_start, key_end = 0, 1
        else:
            key_start, key_end = 0, min(0x100, len(self.key_buffer))
        header = struct.pack("<HH", key_start, key_end)
        self.writer.write(header)
        await self.writer.drain()
        LOG.info("[%s] sent initial header: key window [%#x..%#x)",
                 self.peer, key_start, key_end)
        if self.key_buffer is not None:
            self.recv_cipher = XorStream(self.key_buffer, key_start, key_end)
            self.send_cipher = XorStream(self.key_buffer, key_start, key_end)

    async def _read_packet(self) -> bytes | None:
        try:
            length_bytes = await self.reader.readexactly(2)
        except asyncio.IncompleteReadError:
            return None
        if self.recv_cipher:
            length_bytes = self.recv_cipher.transform(length_bytes)
        (length,) = struct.unpack("<H", length_bytes)
        if length == 0:
            return b""
        body = await self.reader.readexactly(length)
        if self.recv_cipher:
            body = self.recv_cipher.transform(body)
        return body

    async def _send_packet(self, body: bytes) -> None:
        if len(body) > 0xFFFF:
            raise ValueError(f"packet body too large: {len(body)}")
        framed = struct.pack("<H", len(body)) + body
        if self.send_cipher:
            framed = self.send_cipher.transform(framed)
        self.writer.write(framed)
        await self.writer.drain()

    async def _loop(self) -> None:
        while True:
            body = await self._read_packet()
            if body is None:
                LOG.info("[%s] EOF", self.peer)
                return
            LOG.debug("[%s] recv %d bytes: %s",
                      self.peer, len(body), body[:64].hex())
            await self._dispatch(body)

    async def _dispatch(self, body: bytes) -> None:
        """Parse the incoming packet as a RemoteCall RPC and respond."""
        try:
            rpc = RemoteCallPacket.from_bytes(body)
            LOG.info("[%s] RPC %r", self.peer, rpc)
        except Exception as e:
            LOG.warning("[%s] could not parse RPC body: %s", self.peer, e)
            LOG.warning("[%s] raw body: %s", self.peer, body.hex())
            return

        handler = self._rpc_handlers.get(rpc.fn_name)
        if handler is None:
            LOG.info("[%s] TODO: no handler for %r", self.peer, rpc.fn_name)
            return
        await handler(self, rpc)

    # Map of fn_name → handler coroutine. Handlers are bound methods.
    _rpc_handlers: dict[str, "asyncio.coroutines.Coroutine"] = {}

    @classmethod
    def register(cls, fn_name: str):
        """Decorator to register a handler for a server-side RPC."""
        def wrap(method):
            cls._rpc_handlers[fn_name] = method
            return method
        return wrap


# ---------------------------------------------------------------------------
# Handlers — what the client calls on the server.
# ---------------------------------------------------------------------------

@GssConnection.register("signInClient")
async def _handle_signin_client(self, rpc: RemoteCallPacket) -> None:
    LOG.info("[%s] client signing in: args=%r", self.peer, rpc.args)
    resp = build_signin_client_result(SESSION_OK, "master1", "channel1")
    await self._send_packet(resp)
    LOG.info("[%s] sent signInClientResult OK", self.peer)


@GssConnection.register("logInClient")
async def _handle_login_client(self, rpc: RemoteCallPacket) -> None:
    LOG.info("[%s] client logging in: args=%r", self.peer, rpc.args)
    resp = build_login_client_result(SESSION_OK, "master1", "channel1",
                                      account_id=1001, player_id=1)
    await self._send_packet(resp)


@GssConnection.register("signOutClient")
async def _handle_signout_client(self, rpc: RemoteCallPacket) -> None:
    LOG.info("[%s] client signing out", self.peer)
    resp = build_signout_client_result()
    await self._send_packet(resp)


@GssConnection.register("connect")
async def _handle_connect(self, rpc: RemoteCallPacket) -> None:
    """GSS:connect(...) — client wants to initiate connection."""
    LOG.info("[%s] GSS:connect args=%r", self.peer, rpc.args)
    # Reply by firing signInClientResult OK
    resp = build_signin_client_result(SESSION_OK, "master1", "channel1")
    await self._send_packet(resp)


# ---------------------------------------------------------------------------
# Entrypoint.
# ---------------------------------------------------------------------------

async def main_async(args: argparse.Namespace) -> None:
    key_buffer: bytes | None = None
    if args.key_blob:
        key_buffer = Path(args.key_blob).read_bytes()
        LOG.info("loaded cipher key: %d bytes", len(key_buffer))

    server = await asyncio.start_server(
        lambda r, w: GssConnection(r, w, key_buffer).run(),
        host=args.host,
        port=args.port,
    )
    addrs = ", ".join(str(s.getsockname()) for s in server.sockets)
    LOG.info("listening on %s", addrs)
    async with server:
        await server.serve_forever()


def main() -> None:
    ap = argparse.ArgumentParser(description="GSS stub server v3 (RPC-aware)")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5044)
    ap.add_argument("--key-blob", default=None,
                    help="path to dumped XOR key buffer (from dump_xor_key.py)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
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
