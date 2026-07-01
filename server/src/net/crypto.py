"""GSS wire cipher — keystream load + XOR + the server-side crypto writer.

Byte-exact extraction from Res/gss_stub_server_v4.py (the proven transport). The client sends a
4-byte clear header + XOR-transformed bodies keyed by a recovered keystream window; the server
replies under the same header, advancing a cursor through the window.
"""
from __future__ import annotations

import asyncio
import json
import logging
from pathlib import Path

LOG = logging.getLogger("gss.net.crypto")

# Repo root = .../qqxj ; this module lives at server/src/net/crypto.py -> parents[3].
_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_KEY_WINDOW = _ROOT / "Res" / "RE_findings" / "gss_key_window_20260525.json"
DEFAULT_CANONICAL_KEYSTREAM = _ROOT / "Res" / "RE_findings" / "xor_keystream_full_CANONICAL.hex"

RECOVERED_KEY_START = 0x31
RECOVERED_KEY_BYTES = bytes.fromhex(
    "e9c594430aa35769315b7e2bb1b593293062470564d6efdbc79b0f078f92ec52"
    "a30da529e6b8fbd0214cdb8c33e9c5ebb364c1c18e0ef0838af66a9ae46c8bc7"
    "a48b83a95fbec4ce7ba46978701863ef77d03cdf686e62dbe947f3c9ceb6ee53"
    "ebaf10d8542c0d0cd1cc11ff978d79d6911eddcb366b86e98bad30c44ed81ca0"
    "861ecd455c9688a27132a8db6705cc64691a5c15a739a32ff8840dfc7dc0aab0"
    "17efacde44759a86a6d48c667fe05bfbac88d4c04d85da9b9682a9c1a12ffdba"
    "5d524d01e73dd9ccc0ece67a12f7ecb1d2d9e74df80e2eb7d44385bfa0e7b04f"
    "ae39d99aab8dae964d3397c67c"
)
RECOVERED_KEY_RANGES = [
    (RECOVERED_KEY_START, RECOVERED_KEY_BYTES),
    (
        0x19C,
        bytes.fromhex(
            "325b15941ca9716e3d84aa16ca4cd79eaa999d0509fe1cca48c7c8d60443"
            "7ba4d502d98cee8ddb8fc13c43684339f3482e679a488d27215da51caedc"
            "2f20d8b20492a526d685340e432e289453bd5b5cc09486e289e0b97d5298"
            "db57cec8eedc0e3382245d7b6a86768a201cc463a768107fc768471e064"
            "6ddba8d4bca56ff47"
        ),
    ),
]


def xor_with_stream(data: bytes, stream: bytes) -> bytes:
    if len(data) > len(stream):
        raise ValueError(f"response {len(data)} exceeds recovered stream {len(stream)}")
    return bytes(b ^ stream[i] for i, b in enumerate(data))


def load_canonical_key_stream(path: Path = DEFAULT_CANONICAL_KEYSTREAM) -> dict[int, int]:
    if not path.exists():
        return {}
    hex_text = "".join(
        line.strip()
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    )
    try:
        blob = bytes.fromhex(hex_text)
    except ValueError as exc:
        LOG.warning("failed to parse canonical XOR key stream %s: %s", path, exc)
        return {}
    return {i: b for i, b in enumerate(blob)}


def load_key_window(path: Path = DEFAULT_KEY_WINDOW) -> dict[int, int]:
    embedded: dict[int, int] = {}
    for start, blob in RECOVERED_KEY_RANGES:
        embedded.update({start + i: b for i, b in enumerate(blob)})
    if not path.exists():
        embedded.update(load_canonical_key_stream())
        return embedded
    raw = json.loads(path.read_text())
    embedded.update({int(k): int(v) for k, v in raw.get("bytes", {}).items()})
    # The full deterministic key buffer is required to decode master client
    # headers outside the old cracked ranges, e.g. observed 0x0261..0x04b1.
    embedded.update(load_canonical_key_stream())
    return embedded


def key_stream_from_header(header: bytes, length: int, key_window: dict[int, int]) -> bytes | None:
    if not key_window:
        return None
    start = int.from_bytes(header[:2], "little")
    out = bytearray()
    for i in range(length):
        value = key_window.get(start + i)
        if value is None:
            return None
        out.append(value)
    return bytes(out)


def key_stream_from_window(header: bytes, cursor: int, length: int, key_window: dict[int, int]) -> bytes | None:
    start = int.from_bytes(header[:2], "little")
    end = int.from_bytes(header[2:4], "little")
    if end <= start:
        return None
    window = end - start
    out = bytearray()
    for i in range(length):
        key_index = start + ((cursor - start + i) % window)
        value = key_window.get(key_index)
        if value is None:
            return None
        out.append(value)
    return bytes(out)


def key_stream_from_cursor(cursor: int, length: int, key_window: dict[int, int]) -> bytes | None:
    out = bytearray()
    for i in range(length):
        value = key_window.get(cursor + i)
        if value is None:
            return None
        out.append(value)
    return bytes(out)


def header_window_is_valid(header: bytes) -> bool:
    start = int.from_bytes(header[:2], "little")
    end = int.from_bytes(header[2:4], "little")
    return end >= start + 0x80 and end < 0x500


class ServerCryptoWriter:
    def __init__(self, writer: asyncio.StreamWriter, header: bytes, key_window: dict[int, int]) -> None:
        self.writer = writer
        self.header = header
        self.key_window = key_window
        self.cursor = int.from_bytes(header[:2], "little")
        self.start = int.from_bytes(header[:2], "little")
        self.end = int.from_bytes(header[2:4], "little")
        self.sent_header = False

    async def send(self, clear: bytes) -> None:
        stream = key_stream_from_window(self.header, self.cursor, len(clear), self.key_window)
        if stream is None:
            raise ValueError(
                f"missing server key stream at cursor=0x{self.cursor:x} len={len(clear)} "
                f"for header={self.header.hex(' ')}"
            )
        LOG.info(
            "[wire] master_xor_send cursor=0x%x clear_len=%d window=[0x%x..0x%x]",
            self.cursor, len(clear), self.start, self.end,
        )
        packet = bytearray()
        if not self.sent_header:
            packet += self.header
            self.sent_header = True
        packet += xor_with_stream(clear, stream)
        LOG.info(
            "WIRE: server->client len=%d bytes=%s",
            len(packet),
            bytes(packet).hex(" "),
        )
        self.writer.write(packet)
        await self.writer.drain()
        if self.end > self.start:
            window = self.end - self.start
            self.cursor = self.start + ((self.cursor - self.start + len(clear)) % window)
