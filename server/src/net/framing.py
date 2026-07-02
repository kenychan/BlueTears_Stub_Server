"""GSS framing — lower packet parse + zlib transport frame (de)coders.

Byte-exact extraction from Res/gss_stub_server_v4.py. The clear stream is a sequence of
16-byte-header lower-GSS packets; the client->server type-4 channel wraps them in zlib frames
that share one zlib stream (ZlibSyncFrameDecoder).
"""
from __future__ import annotations

import zlib


def parse_lower_packets(clear: bytes) -> list[tuple[int, bytes]]:
    packets: list[tuple[int, bytes]] = []
    off = 0
    while off + 16 <= len(clear):
        body_len = int.from_bytes(clear[off:off + 4], "little")
        total = 16 + body_len
        if body_len < 0 or off + total > len(clear):
            break
        opcode = int.from_bytes(clear[off + 5:off + 7], "little")
        packets.append((opcode, clear[off + 16:off + total]))
        off += total
    return packets


def parse_clientworld_key6_prefix(body: bytes) -> dict[str, int] | None:
    if len(body) < 22:
        return None
    return {
        "from_pid": int.from_bytes(body[0:4], "little"),
        "target_pid": int.from_bytes(body[4:8], "little"),
        "object_id": int.from_bytes(body[8:12], "little"),
        "self_key": int.from_bytes(body[12:16], "little"),
        "callable_key": int.from_bytes(body[16:20], "little"),
        "selector": int.from_bytes(body[20:22], "little"),
    }


def build_zlib_transport_frame(clear: bytes) -> bytes:
    compressed = zlib.compress(clear)
    return len(clear).to_bytes(4, "little") + len(compressed).to_bytes(4, "little") + compressed


class ZlibStreamFrameEncoder:
    """Encode server->client type-4 zlib frames that share ONE zlib stream — the send-side
    mirror of ZlibSyncFrameDecoder. LATE-168 root cause: the client inflates the whole
    server->client direction with a single persistent decompressobj, so every frame after the
    first must CONTINUE the same deflate stream (Z_SYNC_FLUSH). An independent zlib.compress()
    per frame (build_zlib_transport_frame) ends the stream (BFINAL + Adler32); the client's
    decompressobj then yields nothing for the next frame -> len-mismatch -> the native validity
    gate 0x0115bfd0 rejects it -> ca4070 -> session shutdown. Proven offline: with one-shot
    compress the 2nd frame decodes to 0 bytes; with this encoder both frames decode. This is why
    NO post-login server->client reply (SRQL, create-results, char delivery) ever landed."""

    def __init__(self) -> None:
        self._obj = zlib.compressobj()

    def frame(self, clear: bytes) -> bytes:
        comp = self._obj.compress(clear) + self._obj.flush(zlib.Z_SYNC_FLUSH)
        return len(clear).to_bytes(4, "little") + len(comp).to_bytes(4, "little") + comp


def parse_zlib_transport_frames(clear: bytes) -> list[tuple[int, bytes, list[tuple[int, bytes]]]]:
    frames: list[tuple[int, bytes, list[tuple[int, bytes]]]] = []
    off = 0
    while off + 8 <= len(clear):
        raw_len = int.from_bytes(clear[off:off + 4], "little")
        compressed_len = int.from_bytes(clear[off + 4:off + 8], "little")
        frame_end = off + 8 + compressed_len
        if raw_len <= 0 or compressed_len <= 0 or frame_end > len(clear):
            break
        try:
            inflated = zlib.decompress(clear[off + 8:frame_end])
        except zlib.error:
            break
        if len(inflated) != raw_len:
            break
        frames.append((raw_len, inflated, parse_lower_packets(inflated)))
        off = frame_end
    return frames


class ZlibSyncFrameDecoder:
    """Decode client->server type-4 zlib frames that share one zlib stream."""

    def __init__(self) -> None:
        self._obj = zlib.decompressobj()

    def feed(self, clear: bytes) -> list[tuple[int, int, list[tuple[int, bytes]], str]]:
        frames: list[tuple[int, int, list[tuple[int, bytes]], str]] = []
        off = 0
        while off + 8 <= len(clear):
            raw_len = int.from_bytes(clear[off:off + 4], "little")
            compressed_len = int.from_bytes(clear[off + 4:off + 8], "little")
            frame_end = off + 8 + compressed_len
            if raw_len <= 0 or compressed_len <= 0 or frame_end > len(clear):
                frames.append((raw_len, 0, [], "invalid-frame-header"))
                break
            try:
                inflated = self._obj.decompress(clear[off + 8:frame_end])
            except zlib.error as exc:
                frames.append((raw_len, 0, [], f"zlib-error:{exc}"))
                break
            status = "ok" if len(inflated) == raw_len else f"len-mismatch:{len(inflated)}"
            frames.append((raw_len, len(inflated), parse_lower_packets(inflated), status))
            off = frame_end
        return frames


def decode_channel_signin_request(body: bytes) -> tuple[int, int, int] | None:
    if len(body) < 12:
        return None
    request_token = int.from_bytes(body[0:4], "little")
    master_id = int.from_bytes(body[4:8], "little")
    channel_id = int.from_bytes(body[8:12], "little")
    return request_token, master_id, channel_id
