#!/usr/bin/env python3
"""Known-plaintext GSS bootstrap probe for the original QQXJ client.

This is intentionally small and evidence-driven:
- the client sends a 4-byte clear header plus a 128-byte XOR-transformed
  Authenticator_WebSessionKey sign-in body;
- the sign-in body contents are known for our local launch args;
- XORing cipher body with expected plaintext recovers enough keystream to send
  a short signInClientResult response under the same 4-byte header.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import struct
import sys
import time
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gss_stub_server_v3 import SESSION_OK, build_signin_client_result, pack_variant
from live_character_db import DEFAULT_DB, init_db, load_account
from nid_hash import native_nid

LOG = logging.getLogger("gss-v4")
DEFAULT_KEY_WINDOW = Path(__file__).resolve().parent / "RE_findings" / "gss_key_window_20260525.json"
DEFAULT_CANONICAL_KEYSTREAM = Path(__file__).resolve().parent / "RE_findings" / "xor_keystream_full_CANONICAL.hex"
DEFAULT_FULL_GSS_RESPONSE_HEADER = bytes.fromhex("9c 01 1c 02")
MASTER_LADDER_PROBES = ("ladder-t0", "ladder-t1", "ladder-t2", "ladder-t3", "ladder-t4")
MASTER_ISSUEPID_DIAG_PROBES = ("diag-d1", "diag-d2", "diag-d3")
MASTER_LADDER_TWOU32_PROBES = ("ladder2-t1", "ladder2-t2", "ladder2-t3", "ladder2-t4")
MASTER_ORDERED_EMPTY_PROBES = ("ordered-empty", "ordered-empty-challenge")
MASTER_TCLIENT_WAIT_PROBES = ("tclient-wait-connect",)
MASTER_TCLIENT_BATCH_PROBES = ("tclient-batch-wait-connect",)
MASTER_CONNECTOR_HANDSHAKE_PROBES = ("connector-connect-probe",)
MASTER_ORDERED_EMPTY_BATCH_PROBES = ("ordered-empty-batch",)
MASTER_ORDERED_LOGINRESULT_PROBES = ("ordered-empty-loginresult",)
MASTER_ORDERED_LOGINSEQ_PROBES = ("ordered-empty-loginseq",)
MASTER_ORDERED_LOGINSEQ_SESSIONTAIL_PROBES = ("ordered-empty-loginseq-sessiontail",)
MASTER_ORDERED_LOGINCLIENTRESULT_SESSIONTAIL_PROBES = (
    "ordered-empty-loginclientresult-sessiontail",
)
MASTER_ORDERED_LOGINCLIENTRESULT_ONLOADPLAYERS_PROBES = (
    "ordered-empty-loginclientresult-onloadplayers",
)
MASTER_ORDERED_ONLOADPLAYERDATA_PROBES = (
    "ordered-empty-onloadplayerdata-loginclientresult",
)
MASTER_ORDERED_ONLOADPLAYERDATA_PACED_PROBES = (
    "ordered-empty-onloadplayerdata-paced-loginclientresult",
)
MASTER_ORDERED_B4_ONLOADPLAYERDATA_PACED_PROBES = (
    "ordered-b4-onloadplayerdata-paced-loginclientresult",
)
MASTER_ORDERED_B4_B5_NO_ONLOAD_PROBES = (
    "ordered-b4-b5-no-onload",
)
MASTER_ORDERED_B4_B5_PACED_AFTER_LOGIN_PROBES = (
    "ordered-b4-b5-paced-after-loginclientresult",
)
MASTER_ORDERED_B4_B5_EMPTY_SELECT_PROBES = (
    "ordered-b4-b5-empty-select",
)
MASTER_ORDERED_B4_B5_FINISH0_PROBES = (
    "ordered-b4-b5-finish0",
)
MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_PROBES = (
    "ordered-b4-b5-prelogin-finish",
)
MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_OBJECTLIST_PROBES = (
    "ordered-b4-b5-prelogin-finish-objectlist",
)
MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_FINISH_PROBES = (
    "ordered-b4-b5-prelogin-double-onload-finish",
)
MASTER_ORDERED_B4_B5_PRELOGIN_ONLOAD_LISTARG_FINISH_PROBES = (
    "ordered-b4-b5-prelogin-onload-listarg-finish",
)
MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_ARG3_FINISH_PROBES = (
    "ordered-b4-b5-prelogin-double-onload-arg3-finish",
)
MASTER_TACCOUNT_DIRECT_FINISH_PROBES = (
    "ordered-taccount-direct-finish-empty",
)
MASTER_ONLOADPLAYERS_PROBES = ("ordered-empty-onloadplayers",)
MASTER_LOGINRPC_BOOTSTRAP_PROBES = ("ordered-empty-loginrpc-bootstrap",)
MASTER_LOGINRPC_BOOTSTRAP_GOLUAADD_PROBES = ("ordered-empty-loginrpc-bootstrap-goluaadd",)
MASTER_TIMING_PROBES = (
    MASTER_LADDER_PROBES
    + MASTER_ISSUEPID_DIAG_PROBES
    + MASTER_LADDER_TWOU32_PROBES
    + MASTER_ORDERED_EMPTY_PROBES
    + MASTER_TCLIENT_WAIT_PROBES
    + MASTER_TCLIENT_BATCH_PROBES
    + MASTER_CONNECTOR_HANDSHAKE_PROBES
    + MASTER_ORDERED_EMPTY_BATCH_PROBES
    + MASTER_ORDERED_LOGINRESULT_PROBES
    + MASTER_ORDERED_LOGINSEQ_PROBES
    + MASTER_ORDERED_LOGINSEQ_SESSIONTAIL_PROBES
    + MASTER_ORDERED_LOGINCLIENTRESULT_SESSIONTAIL_PROBES
    + MASTER_ORDERED_LOGINCLIENTRESULT_ONLOADPLAYERS_PROBES
    + MASTER_ORDERED_ONLOADPLAYERDATA_PROBES
    + MASTER_ORDERED_ONLOADPLAYERDATA_PACED_PROBES
    + MASTER_ORDERED_B4_ONLOADPLAYERDATA_PACED_PROBES
    + MASTER_ORDERED_B4_B5_NO_ONLOAD_PROBES
    + MASTER_ORDERED_B4_B5_PACED_AFTER_LOGIN_PROBES
    + MASTER_ORDERED_B4_B5_EMPTY_SELECT_PROBES
    + MASTER_ORDERED_B4_B5_FINISH0_PROBES
    + MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_PROBES
    + MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_OBJECTLIST_PROBES
    + MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_FINISH_PROBES
    + MASTER_ORDERED_B4_B5_PRELOGIN_ONLOAD_LISTARG_FINISH_PROBES
    + MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_ARG3_FINISH_PROBES
    + MASTER_TACCOUNT_DIRECT_FINISH_PROBES
    + MASTER_ONLOADPLAYERS_PROBES
    + MASTER_LOGINRPC_BOOTSTRAP_PROBES
    + MASTER_LOGINRPC_BOOTSTRAP_GOLUAADD_PROBES
)
MASTER_LADDER_IDLE_SECONDS = 65.0
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


def log_client_bytes(peer, label: str, data: bytes) -> None:
    LOG.info(
        "[%s] client->server %s len=%d bytes=%s",
        peer,
        label,
        len(data),
        data.hex(" "),
    )

FIXED_RESPONSE_HEADER = bytes.fromhex("31 00 e4 00")

# Recovered from trace login_trace_20260525_175851.log:
# header 31 00 e4 00, known plaintext 0x4009 sign-in body, transformed body.
# This is key_buffer[0x31+1 .. 0x31+128] because the client increments the
# rolling index before XORing each body byte.
FIXED_RESPONSE_STREAM = bytes.fromhex(
    "e9c594430aa35769315b7e2bb1b593293062470564d6efdbc79b0f078f92"
    "ec52a30da529e6b8fbd0214cdb8c33e9c5ebb364c1c18e0ef0838af66a9a"
    "e46c8bc7a48b83a95fbec4ce7ba46978701863ef77d03cdf686e62dbe947"
    "f3c9ceb6ee53ebaf10d8542c0d0cd1cc11ff978d79d6911eddcb366b86e"
    "98bad30c411ff978d"
)


def expected_signin_body(root: Path) -> bytes:
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


def xor_with_stream(data: bytes, stream: bytes) -> bytes:
    if len(data) > len(stream):
        raise ValueError(f"response {len(data)} exceeds recovered stream {len(stream)}")
    return bytes(b ^ stream[i] for i, b in enumerate(data))


def pack_string(value: str) -> bytes:
    data = value.encode("ascii")
    return len(data).to_bytes(4, "little") + data


def build_lower_gss_body(opcode: int, payload: bytes) -> bytes:
    body = bytearray()
    body += len(payload).to_bytes(4, "little")
    body += b"\x00"
    body += opcode.to_bytes(2, "little")
    body += b"\x00" * 9
    body += payload
    return bytes(body)


def build_native_session_in_challenge(challenge: str = "local") -> bytes:
    payload = bytearray()
    payload += b"\x01"
    payload += (0).to_bytes(4, "little")
    payload += (0).to_bytes(4, "little")
    payload += pack_string(challenge)
    return build_lower_gss_body(0x1001, bytes(payload))


def build_native_session_in_result(account_id: int = 1) -> bytes:
    payload = bytearray()
    payload += b"\x01"
    payload += (0).to_bytes(4, "little")  # errorCode
    payload += (0).to_bytes(4, "little")  # field2
    payload += account_id.to_bytes(4, "little")
    payload += (0).to_bytes(4, "little")  # serverCount
    payload += (0).to_bytes(8, "little")
    payload += (0).to_bytes(4, "little")
    return build_lower_gss_body(0x1003, bytes(payload))


def build_native_signin_result(host: str, port: str, master_id: int = 1, channel_id: int = 1) -> bytes:
    # GSSConnector::onSignInResult (0x1006) reads:
    # bool-like success, u32-like field, u32/count/id-like field, endpoint host, endpoint port.
    # The two strings feed FUser::connect -> FClientWorld::Connect, not the UI labels.
    payload = bytearray()
    payload += b"\x01"
    payload += master_id.to_bytes(4, "little")
    payload += channel_id.to_bytes(4, "little")
    payload += pack_string(host)
    payload += pack_string(port)
    return build_lower_gss_body(0x1006, bytes(payload))


def build_native_master_channel_update(master: str, channel: str, master_id: int = 1, channel_id: int = 1) -> bytes:
    # Section 19 exact schema: header string, masters, then channels.
    payload = bytearray()
    payload += pack_string(master)
    payload += (1).to_bytes(4, "little")
    payload += master_id.to_bytes(4, "little")
    payload += pack_string(master)
    payload += b"\x00"
    payload += (0).to_bytes(4, "little")
    payload += (1).to_bytes(4, "little")
    payload += channel_id.to_bytes(4, "little")
    payload += pack_string(channel)
    payload += (0).to_bytes(4, "little")
    payload += (0).to_bytes(4, "little")
    payload += (100).to_bytes(4, "little")
    payload += (0).to_bytes(4, "little")
    payload += b"\x00"
    payload += pack_string("")
    return build_lower_gss_body(0x1004, bytes(payload))


def build_issuepid(new_pid: int) -> bytes:
    return build_lower_gss_body(0x02, new_pid.to_bytes(4, "little"))


def build_clientworld_issuepid(first_value: int, manager_pid: int) -> bytes:
    body = first_value.to_bytes(4, "little") + manager_pid.to_bytes(4, "little")
    return build_lower_gss_body(0x02, body)


def build_bogus_opcode_control(manager_pid: int) -> bytes:
    body = (0).to_bytes(4, "little") + manager_pid.to_bytes(4, "little")
    return build_lower_gss_body(0x6E6E, body)


def build_newobject(new_pid: int, template_name: str) -> bytes:
    name = template_name.encode("ascii")
    body = new_pid.to_bytes(4, "little") + len(name).to_bytes(4, "little") + name
    return build_lower_gss_body(0x03, body)


SERVERCOM_DESCRIPTOR_KEY = native_nid("ServerCom")
CONNECTOR_DESCRIPTOR_KEY = native_nid("Connector")
SESSION_DESCRIPTOR_KEY = native_nid("Session")
CHANNELCONTEXT_DESCRIPTOR_KEY = native_nid("ChannelContext")
CHARACTERMANAGER_DESCRIPTOR_KEY = native_nid("CharacterManager")
CHARACTERHOLDER_DESCRIPTOR_KEY = native_nid("CharacterHolder")
CSHAREDDATA_DESCRIPTOR_KEY = native_nid("CSharedData")
PLAYER_DESCRIPTOR_KEY = native_nid("Player")
CSTATUSPLAYER_DESCRIPTOR_KEY = native_nid("CStatusPlayer")
USER_DESCRIPTOR_KEY = native_nid("User")
CPRIVATESTATUS_DESCRIPTOR_KEY = native_nid("CPrivateStatus")
PHYSICS_DESCRIPTOR_KEY = native_nid("Physics")
CBODY_DESCRIPTOR_KEY = native_nid("CBody")
PLAYERCONTROLLER_DESCRIPTOR_KEY = native_nid("PlayerController")
CEQUIPMENT_DESCRIPTOR_KEY = native_nid("CEquipment")
CSKILLBOOK_DESCRIPTOR_KEY = native_nid("CSkillBook")
assert SERVERCOM_DESCRIPTOR_KEY == 0x52F62708
assert CONNECTOR_DESCRIPTOR_KEY == 0x7D1FEC2E
assert SESSION_DESCRIPTOR_KEY == 0xDB2C74E0
assert CHANNELCONTEXT_DESCRIPTOR_KEY == 0x9419399D
assert CHARACTERMANAGER_DESCRIPTOR_KEY == 0x6C12C692
assert CHARACTERHOLDER_DESCRIPTOR_KEY == 0x90D6E0AC
assert CSHAREDDATA_DESCRIPTOR_KEY == 0x6B27291D
assert PLAYER_DESCRIPTOR_KEY == 0x5D2C8910
assert CSTATUSPLAYER_DESCRIPTOR_KEY == 0x5833CD58
assert USER_DESCRIPTOR_KEY == 0xCB16687B
assert CPRIVATESTATUS_DESCRIPTOR_KEY == 0xC37AC904
assert PHYSICS_DESCRIPTOR_KEY == 0x72247545
assert CBODY_DESCRIPTOR_KEY == 0xA31BB7C7
assert PLAYERCONTROLLER_DESCRIPTOR_KEY == 0xFF044DB3
assert CEQUIPMENT_DESCRIPTOR_KEY == 0xF79809EB
assert CSKILLBOOK_DESCRIPTOR_KEY == 0xFBBE74DE


def pack_u32(value: int) -> bytes:
    return int(value).to_bytes(4, "little", signed=False)


def pack_u64(value: int) -> bytes:
    return int(value).to_bytes(8, "little", signed=False)


def pack_f64(value: float) -> bytes:
    return struct.pack("<d", float(value))


def pack_f32(value: float) -> bytes:
    return struct.pack("<f", float(value))


def pack_nipoint3(x: float, y: float, z: float) -> bytes:
    return pack_f32(x) + pack_f32(y) + pack_f32(z)


def pack_u16(value: int) -> bytes:
    return int(value).to_bytes(2, "little", signed=False)


def pack_s16(value: int) -> bytes:
    return int(value).to_bytes(2, "little", signed=True)


def pack_bool(value: bool) -> bytes:
    return b"\x01" if value else b"\x00"


def pack_lua_fixed_nil() -> bytes:
    return b"\x00"


def pack_lua_fixed_number(value: float) -> bytes:
    return b"\x03" + pack_f64(value)


def pack_lua_fixed_string(value: str) -> bytes:
    return b"\x04" + pack_string(value)


def pack_lua_fixed_value(value) -> bytes:
    # Class.lua PackVars use the same RawPack tag family as RPC Variants.
    # Existing nil/number/string helpers are kept for byte-identical old tails.
    return pack_variant(value)


def pack_room_id(value: int = 0, room: int = 0) -> bytes:
    # Profile46 (2026-06-17) shows User.m_RoomID consumes two dwords.
    return pack_u32(value) + pack_u32(room)


def pack_empty_descriptor_container() -> bytes:
    # Profile45 CStatusPlayer proved bb0060 descriptor containers consume
    # a short zero count on this key03 path, not BackupObj's u32 hash count.
    return pack_u16(0)


def pack_empty_hash() -> bytes:
    # Hash/list typed fields use their type reader; an empty container is count=0.
    return pack_u32(0)


def pack_empty_uintlist() -> bytes:
    # s20260619_uintlist_reader_slot2c: nested uintList reader reads a u16
    # element count from reader vslot +0x40, then loops that many uint values.
    return pack_u16(0)


def build_component_instance_tail(
    field_bytes: bytes,
    include_type2_trailer: bool = True,
    lua_fixed_tail: bytes = b"",
    is_havable_header: bool = False,
) -> bytes:
    # SESSION_FINDINGS_2026-06-03 s11: component +0x1c reads 3 shorts,
    # a false optional-list byte, then bridge-filtered descriptor fields.
    # SESSION_FINDINGS_2026-06-17 s21: descriptor type 2 then reads a
    # five-call trailer: reader +0x6c, +0x44, +0x6c, +0x48, +0x50.
    # The old marker-only B4 candidate let Player consume the following
    # CStatus key as the second +0x6c blob length and raised the runtime error.
    # Profile45 then proved bb0060 calls Class:Unpack after that trailer; for
    # Player the generic Lua PackVars tail consumed five one-byte fixed values.
    # field_bytes is already typed; live s6/s7 proved widths are not all u32.
    type2_trailer = (
        pack_u32(0) + b"\x00" + pack_u32(0) + b"\x00" + pack_f64(0.0)
        if include_type2_trailer
        else b""
    )
    # FUN_00bede50 true +0x26 branch reads reader slot +0x28 immediately
    # after the fixed header. The slot resolves to FUN_01130120, a u32 count.
    # A zero count preserves/rebuilds the native list while skipping elements.
    havable_list_payload = pack_u32(0) if is_havable_header else b""
    return (
        pack_s16(0)
        + pack_s16(0)
        + pack_s16(0)
        + (b"\x01" if is_havable_header else b"\x00")
        + havable_list_payload
        + field_bytes
        + type2_trailer
        + lua_fixed_tail
    )


def build_component_base_header_only() -> bytes:
    # Use only for components whose live key03 path has no active field body.
    return pack_s16(0) + pack_s16(0) + pack_s16(0) + b"\x00"


def build_component_lua_type2_tail(type_name: str) -> bytes:
    # 2026-06-18 s30: type-2 NObject/Lua tails are not all-zero blobs.
    # The first blob is the class/type name, then Class.lua Pack emits
    # a keyed "size" value even when PackVars is empty.
    return (
        pack_s16(0)
        + pack_s16(0)
        + pack_s16(0)
        + b"\x00"
        + pack_nobject_type2_lua_tail(type_name, packvars_count=0)
    )


def build_tclient_key03_component_tail(object_pid: int) -> bytes:
    # SESSION_FINDINGS_2026-06-03 s12: base ServerCom is read before
    # TClient Connector in the recovered native component order.
    # SESSION_FINDINGS_2026-06-05 s6/s7: RawRead trace proved an empty
    # ServerCom instance is exactly 40 zero bytes; the earlier compact
    # [u32,u32]+empty-blob shape throws in ServerCom slot +0x1c and prevents
    # the outer component loop from reaching Connector.
    servercom_instance = b"\x00" * 40
    # Diagnostic padding from Claude's sandbox, not a final protocol recipe:
    # use profile11/profile8 logging to trim Connector's exact slot +0x1c
    # layout after proving the loop reaches it.
    connector_instance = b"\x00" * 80
    return (
        pack_u32(SERVERCOM_DESCRIPTOR_KEY)
        + servercom_instance
        + pack_u32(CONNECTOR_DESCRIPTOR_KEY)
        + connector_instance
    )


def build_channelcontext_default_fields(channel_id: int = 1) -> bytes:
    # Profile73 B4DESC dump proves ChannelContext.key bb0060 order. The
    # wire carries raw field values only, in descriptor walk order; no
    # per-field keys. ChannelContext.lua says CHGCHTYPE_DEFAULT=0.
    return b"".join([
        pack_string(""),                 # m_zoneID: String
        pack_u32(0),                     # m_type: CHGCHTYPE_DEFAULT
        pack_u32(0),                     # m_player_oid: OID
        pack_nipoint3(0.0, 0.0, 0.0),    # m_return_point: NiPoint3
        pack_string(""),                 # m_mode: String
        pack_bool(False),                # m_summon: bool
        pack_u32(0),                     # m_clear_count: int
        pack_string(""),                 # m_difficulty: String
        pack_nipoint3(0.0, 0.0, 0.0),    # m_point: NiPoint3
        pack_string(""),                 # m_map_name: String
        pack_u32(channel_id),            # m_return_channel_number: int
        pack_string(""),                 # m_return_map_name: String
        pack_string(""),                 # m_position: String
        pack_string(""),                 # m_themeName: String
        pack_empty_uintlist(),           # m_playerlist: uintList
        pack_u32(channel_id),            # m_field_channel_number: int
    ])


def build_taccount_session_fields(
    account_pid: int,
    account_name: str = "codex",
    account_id: int = 1,
    channel_id: int = 1,
) -> bytes:
    # Profile73 B4DESC Session.key active fields, in bb0060 walk order.
    # Mode-0 fields (NID/ObjectPtr/m_channel_number) are deliberately skipped.
    return b"".join([
        build_channelcontext_default_fields(channel_id),  # m_channel_ctx
        pack_bool(False),              # m_IsHavable
        pack_u16(0),                   # m_StoCReplicateType
        pack_u32(0),                   # m_chatserver_port
        pack_u32(0),                   # m_gender
        pack_u32(0),                   # m_subscriber
        pack_u32(0),                   # m_pcRoom
        pack_u32(0),                   # m_iCompanyAlliance
        pack_u32(0),                   # m_chatserver_addr
        pack_u16(0),                   # m_StoMReplicateType
        pack_string(account_name),     # m_account_name
        pack_u32(account_pid),         # m_pid
        pack_u32(account_pid),         # m_id
        pack_u32(0),                   # m_iUserType
        pack_u32(0),                   # m_bluedia
        pack_u32(account_id),          # m_account_id
        pack_u16(0),                   # m_MtoSReplicateType
    ])


def build_taccount_charactermanager_fields() -> bytes:
    # 2026-06-19 profile73 after the full Session payload: CharacterManager
    # slot-7 bb0060 reads these active fields before its type-2 trailer.
    # 2026-06-19 profile76: CharacterManager ctor creates native list at
    # +0x28, but generic component read clears it and only rebuilds it when
    # m_IsHavable (+0x26) is true. LoadList requires the rebuilt list.
    return b"".join([
        pack_bool(True),               # m_IsHavable
        pack_u16(0),                   # m_StoMReplicateType
        pack_u32(0),                   # m_bAllowPK
        pack_u16(0),                   # m_MtoSReplicateType
        pack_u16(0),                   # m_StoCReplicateType
    ])


def build_taccount_characterholder_fields() -> bytes:
    # Profile73 after CharacterManager: CharacterHolder bb0060 reads the
    # inherited component fields only, in this order.
    return b"".join([
        pack_bool(False),              # m_IsHavable
        pack_u16(0),                   # m_StoMReplicateType
        pack_u16(2),                   # m_MtoSReplicateType = NotReplicate
        pack_u16(0),                   # m_StoCReplicateType
    ])


def build_taccount_session_key03_component_tail(
    account_pid: int = 1001,
    account_name: str = "codex",
    account_id: int = 1,
    channel_id: int = 1,
) -> bytes:
    # 2026-06-19 (Claude): the key03 component loop (FUN_00bd4b20, called once
    # from the NewObject receiver FUN_00bff990) is COUNT-driven from TAccount's
    # descriptor, not wire-terminated. profile71 s1 proved Session-only (body
    # 0x24) overflows even though the cursor reached body-end after Session, and
    # s2 proved a u32 0 terminator is consumed as the next component key -> the
    # loop expects a FIXED number of (key + base-header) component entries.
    #
    # TAccount = Template("TAccount","TNetObject"). The owner client receives
    # every StoC-replicated component. TNetObject's only component, ServerCom,
    # is m_StoCReplicateType=NotReplicate (suppressed -> that is why the trace
    # starts at Session, not ServerCom). The four TAccount components all
    # replicate StoC to the owner (default replicates; you only override to
    # suppress, as ServerCom does; CSharedData is OwnerOnly = the owner client):
    #   Session, CharacterManager, CharacterHolder, CSharedData (declaration
    #   order = descriptor order = loop read order).
    # All four are logic-only components (no Lua serializable fields beyond the
    # 3-short + bool replicate-type base header), so each is key(4)+header(7).
    #
    # Each NewObject packet has its own reader buffer (begin/end), so sending
    # too MANY components is contained to this packet, while too FEW overflows
    # past end -> packet read overflow! -> teardown. The live profile71 TRACERD
    # log reports exactly how many entries the loop consumes; trim if <4.
    # 2026-06-20 (Claude): Codex's full 4-component-with-fields tail (below) is the
    # confirmed-correct serialization — it removes BOTH the overflow AND the earlier
    # "CharacterManager throw" (which profile73 proved was really Session's 17 missing field
    # bytes being mis-read, not a CharacterManager-specific fault). The current wall is now
    # downstream in CharacterManager::LoadList. See CLAUDE_FINDINGS_2026-06-19_taccount_4component.md
    # for the (now-superseded) 4x base-header analysis.
    # 2026-06-19 correction: later profile73 B4DESC snapshots prove the
    # Session component itself has 17 active fields in this owner key03 mode.
    # The compact 0x45 tail reaches Session.m_channel_ctx, then consumes the
    # following CharacterManager key as field bytes and tears down. The Session
    # component therefore needs its active descriptor payload before the next
    # component key.
    return (
        pack_u32(SESSION_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_taccount_session_fields(
                account_pid,
                account_name=account_name,
                account_id=account_id,
                channel_id=channel_id,
            )
        )
        + pack_u32(CHARACTERMANAGER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_taccount_charactermanager_fields(),
            is_havable_header=True,
        )
        + pack_u32(CHARACTERHOLDER_DESCRIPTOR_KEY)
        + build_component_instance_tail(build_taccount_characterholder_fields())
        + pack_u32(CSHAREDDATA_DESCRIPTOR_KEY)
        # Profile73 latest CSharedData trace enters the generic type-2 trailer
        # immediately after the base component header; no m_Data field read was
        # observed in this owner key03 mode.
        + build_component_instance_tail(b"")
    )


def build_b4_player_fields(char_pid: int, account_id: int = 1, permanent_id: int = 10001) -> bytes:
    # Live profile42 B4 bucket dump (2026-06-17): Player has 29 fields total;
    # key03 flag=0 consumes the 14 active mode {1,2,3,4} fields below in
    # descriptor order (Player has modes 1 and 4 in the active subset).
    return b"".join([
        pack_u32(account_id),         # m_AccountId: uint
        pack_bool(False),             # m_IsHavable: bool
        pack_string("Codex"),         # m_name: String
        pack_u16(0),                  # m_StoCReplicateType: ushort
        pack_u16(0),                  # m_StoMReplicateType: ushort
        pack_u32(permanent_id),       # m_PermanentId: uint
        pack_u32(0),                  # m_DeleteRemainTime: uint, mode4
        pack_u32(char_pid),           # m_pid: PID
        pack_u64(0),                  # m_LogoutTime: int64, mode4
        pack_u64(0),                  # m_DeleteRequestTime: int64
        pack_bool(False),             # m_bIsDeleted: bool
        pack_u16(0),                  # m_MtoSReplicateType: ushort
        pack_u32(0),                  # m_AasPlayTime: uint
        pack_bool(True),              # m_AasIsAdult: bool
    ])


def build_b4_cstatusplayer_fields() -> bytes:
    # Live profile42 B4 bucket dump: CStatusPlayer has 24 fields total;
    # key03 flag=0 consumes the 16 active mode {1,2,3,4} fields below in
    # descriptor order (CStatusPlayer has modes 1 and 4 in the active subset).
    # Profile45 showed the empty descriptor container readers consume u16
    # counts here, not the generic BackupObj Variant/hash u32 count.
    return b"".join([
        pack_u32(0),                  # m_tBuddyRelationBreak: uint
        pack_bool(False),             # m_IsHavable: bool
        pack_empty_descriptor_container(), # m_changePercentMap
        pack_u32(0),                  # m_changeID: uint
        pack_u16(0),                  # m_StoCReplicateType: ushort
        pack_empty_descriptor_container(), # m_valueRawMap
        pack_empty_descriptor_container(), # m_valueMinMap
        pack_empty_descriptor_container(), # m_changeRollbackMap
        pack_u32(0),                  # m_uiLastLevelUpTime: uint
        pack_u16(0),                  # m_StoMReplicateType: ushort
        pack_string(""),              # m_strNameForWatcher: minimal empty String
        pack_u32(0),                  # m_nJobClass: NID
        pack_u32(0),                  # m_nGender: NID
        pack_s16(0),                  # m_iNumModifiers: short
        pack_empty_descriptor_container(), # m_valueMaxMap
        pack_u16(0),                  # m_MtoSReplicateType: ushort
    ])


def build_b4_user_fields(char_pid: int) -> bytes:
    # Live profile42 B4 bucket dump: User has 11 fields total;
    # key03 flag=0 consumes the 8 active mode {1,2,3,4} fields below in
    # descriptor order (User's active subset is mode 1 only).
    return b"".join([
        pack_bool(False),             # m_IsHavable: bool
        pack_string("Intro_whiteCastle"), # m_SaveRoomName: String
        pack_u16(0),                  # m_MtoSReplicateType: ushort
        pack_room_id(0, 0),           # m_RoomID: RoomID, two dwords
        pack_u32(char_pid),           # m_PlayerPID: PID
        pack_u16(0),                  # m_StoCReplicateType: ushort
        pack_string("Codex"),         # m_PlayerName: String
        pack_u16(0),                  # m_StoMReplicateType: ushort
    ])


def build_b4_user_lua_fixed_tail(
    room_back: str = "",
    room_back_pos: str = "",
    room_index: int = 0,
    prev_map_name: str = "",
    ins_unit_num: int = 0,
) -> bytes:
    # Profile48 live key-string dump proves the first four fixed reads:
    # m_InsUnitNum, m_PrevMapName, m_RoomBack, m_RoomBackPos.
    # Class.lua sorts PackVars lexically; User.lua adds m_SelectedRoomIndex.
    return b"".join([
        pack_lua_fixed_number(float(ins_unit_num)),
        pack_lua_fixed_string(prev_map_name),
        pack_lua_fixed_string(room_back),
        pack_lua_fixed_string(room_back_pos),
        pack_lua_fixed_number(float(room_index)),
    ])


def build_b4_cprivatestatus_fields() -> bytes:
    # Profile51 fulltails shows bb0060 iterates CPrivateStatus native fields
    # in descriptor-bucket order, not registration order. The reader consumes
    # 35 bytes before the fixed Lua tail: pt3, u32, bool, u32, u16, u16,
    # u32, u32, u16, then the shared 18-byte B4 trailer.
    return b"".join([
        pack_nipoint3(0.0, 0.0, 0.0), # m_return_position
        pack_u32(0),                  # m_LogMode bucket field
        pack_bool(False),             # bool bucket field
        pack_u32(0),                  # mSobTm bucket field
        pack_u16(0),                  # user-ish replicate type bucket field
        pack_u16(0),                  # user-ish replicate type bucket field
        pack_u32(0),                  # m_PTime
        pack_u32(0),                  # mLsLvUpTm
        pack_u16(0),                  # user-ish replicate type bucket field
    ])


def build_b4_cprivatestatus_lua_fixed_tail() -> bytes:
    # Runtime-valid profile51 slice from the 10:26 keystr run. The client
    # consumed these exact bytes as CPrivateStatus goLua_UnpackFixed #15..#108
    # and reached RECHECK at cursor 0x247. In that old packet the tail
    # accidentally included the following User bytes; here those bytes are
    # intentionally isolated as CPrivateStatus fixed-value payload and the real
    # User component key follows after this component.
    tail = bytes.fromhex(
        "0500000300000000000000000300000000000000000500000400000000030000"
        "000000000000050200040200000049440100040a00000043726561746554696d"
        "65010005000005000005000005000005030004040000005a6f6e650300000000"
        "0000000004050000005468656d650300000000000000000405000000576f726c"
        "6403000000000000000005020004060000006d5f4e616d650400000000040700"
        "00006d5f496e6465780300000000000000000500000300000000000000000300"
        "0000000000000005000005000005000005000005000005000001000101050100"
        "040800000043617264426f6f6b010101000100010000000500007b6816cb0000"
        "00000000000011000000496e74726f5f7768697465436173746c650000000000"
        "0000000000eb030000000005000000436f646578000000000000000000000000"
        "00"
    )
    assert len(tail) == 0x141
    return tail


def build_b4_physics_fields() -> bytes:
    # s20260618_s19: Physics callback 0x008499d0 registers these twelve
    # fields. TPlayerDummy.lua sets m_eType=Physics.PhyObj_Player and the
    # intro map spawn position; unspecified fields stay zero/false.
    # s20260618_s20: profile51 live cursor evidence shows the Physics native
    # field region is 78 bytes before the shared type-2 trailer. The previous
    # 73-byte payload made the reader steal five bytes from the trailer/Lua tail
    # and corrupt the following CBody descriptor key.
    pos = (-228.431, 11.4057, 39.2735)
    return b"".join([
        pack_u32(16),                 # m_eType: Physics.PhyObj_Player
        pack_f32(0.0),                # m_fDirection
        pack_nipoint3(*pos),          # m_kPosition
        pack_nipoint3(0.0, 0.0, 0.0), # m_kVelocity
        pack_nipoint3(*pos),          # m_kSavePosition
        pack_nipoint3(0.0, 0.0, 0.0), # m_kCollisionCenter
        pack_nipoint3(0.0, 0.0, 0.0), # m_kCollisionExtend
        pack_bool(False),             # m_bCollidee
        pack_bool(False),             # m_bIgnore
        pack_bool(False),             # m_bFixPosition
        pack_bool(False),             # m_bAttachOnTerrain
        pack_bool(False),             # m_bSetPhysics
        b"\x00" * 5,                  # live-proven trailing native field gap
    ])


def build_b4_equipment_container_template() -> dict:
    page = {"SelectedPage": 1, 1: {}, 2: {}}
    return {
        "Armor": dict(page),
        "Weapon": dict(page),
        "Casual": dict(page),
    }


def build_b4_cequipment_lua_fixed_tail() -> bytes:
    # CEquipment inherits CItemContainer and then merges its own NetVars plus
    # two ReplicateVars. Sorted lexical PackVars gives 22 unique keys.
    init_container = build_b4_equipment_container_template()
    scale = {
        "Head": 0,
        "Body": 0,
        "ArmWidth": 0,
        "ArmHeight": 0,
        "LegWidth": 0,
        "LegHeight": 0,
    }
    return b"".join([
        pack_lua_fixed_string("Casual"),           # m_ArmorOrCasual
        pack_lua_fixed_value({}),                  # m_BonusAppliedSetems
        pack_lua_fixed_number(63),                 # m_CasOptNm
        pack_lua_fixed_value(init_container),      # m_Container
        pack_lua_fixed_value({}),                  # m_ContainerEx
        pack_lua_fixed_string(""),                 # m_CurTransformation
        pack_lua_fixed_string(""),                 # m_CurrentVehicle
        pack_lua_fixed_value({"Hair": ""}),        # m_HairCasual
        pack_lua_fixed_value({"r": 255, "g": 255, "b": 255, "a": 255}),
        pack_lua_fixed_number(0.75),               # m_HeadSize
        pack_lua_fixed_value(init_container),      # m_InitContainer
        pack_lua_fixed_value({}),                  # m_Modifiers
        pack_lua_fixed_value(scale),               # m_Scale
        pack_lua_fixed_number(0.5),                # m_ScaleByAge
        pack_lua_fixed_value({}),                  # m_SetemModifiers
        pack_lua_fixed_string(""),                 # m_SkinColorCasual
        pack_lua_fixed_value({}),                  # m_TabSize
        pack_lua_fixed_string(""),                 # m_strEye
        pack_lua_fixed_string(""),                 # m_strEyebrow
        pack_lua_fixed_string(""),                 # m_strHairStyle
        pack_lua_fixed_string(""),                 # m_strMouth
        pack_lua_fixed_string(""),                 # m_strNose
    ])


def build_b4_physics_lua_fixed_tail() -> bytes:
    return b"".join([
        pack_lua_fixed_value({}),                  # m_kEvolutionColCenter
        pack_lua_fixed_value({}),                  # m_kEvolutionColExtend
    ])


def build_b4_cbody_lua_fixed_tail() -> bytes:
    # CBody.lua NetVars sorted lexically by Class.lua PackVars:
    # m_CharacterDescriptionTable, m_LoadingEndEffects, m_Weapon.
    return b"".join([
        pack_lua_fixed_value({}),                  # m_CharacterDescriptionTable
        pack_lua_fixed_value({}),                  # m_LoadingEndEffects
        pack_lua_fixed_string(""),                 # m_Weapon
    ])


def build_b4_playercontroller_lua_fixed_tail() -> bytes:
    # PlayerController has no own NetVars, but inherits ActorController's
    # four NetVars. TPlayerDummy overrides m_RegenDuration to 1.
    return b"".join([
        pack_lua_fixed_value(False),               # m_ActionDamaged
        pack_lua_fixed_number(1),                  # m_FloatUpLevel
        pack_lua_fixed_number(1),                  # m_RegenDuration
        pack_lua_fixed_string(""),                 # m_TransformOne
    ])


def build_b4_cskillbook_lua_fixed_tail() -> bytes:
    # CSkillBook has one Lua NetVar and one ReplicateVar, sorted lexically.
    return b"".join([
        pack_lua_fixed_value({}),                  # m_AttributeLevel
        pack_lua_fixed_number(0),                  # m_SkillBookLevel
    ])


def build_b4_cskillbook_fields() -> bytes:
    # s20260618_s19: CSkillBook callback 0x008a02d0 registers one native
    # descriptor-container field, m_SkillLevelHash (helper 0x0076d2e0).
    return pack_empty_descriptor_container()


def build_tplayerdummy_b4_component_tail_full(char_pid: int) -> bytes:
    # Full TPlayerDummy.lua component order for later row/body rendering work:
    # Player, CStatusPlayer, CPrivateStatus, User, Physics, CBody,
    # PlayerController, CEquipment. CSkillBook belongs to the full in-world
    # player path, not the character-select dummy.
    return b"".join([
        pack_u32(PLAYER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_player_fields(char_pid),
            lua_fixed_tail=pack_lua_fixed_nil() * 5,
        ),
        pack_u32(CSTATUSPLAYER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_cstatusplayer_fields(),
            # Profile45 cstatuswidth: after corrected descriptor fields,
            # CStatusPlayer reaches Class:Unpack and consumes nine one-byte
            # fixed Lua values before the next component key.
            lua_fixed_tail=pack_lua_fixed_nil() * 9,
        ),
        pack_u32(CPRIVATESTATUS_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_cprivatestatus_fields(),
            lua_fixed_tail=build_b4_cprivatestatus_lua_fixed_tail(),
        ),
        pack_u32(USER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_user_fields(char_pid),
            lua_fixed_tail=build_b4_user_lua_fixed_tail(),
        ),
        pack_u32(PHYSICS_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_physics_fields(),
            lua_fixed_tail=build_b4_physics_lua_fixed_tail(),
        ),
        pack_u32(CBODY_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            b"",
            lua_fixed_tail=build_b4_cbody_lua_fixed_tail(),
        ),
        pack_u32(PLAYERCONTROLLER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            b"",
            lua_fixed_tail=build_b4_playercontroller_lua_fixed_tail(),
        ),
        pack_u32(CEQUIPMENT_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            b"",
            lua_fixed_tail=build_b4_cequipment_lua_fixed_tail(),
        ),
    ])


def build_tplayerdummy_b4_component_tail(char_pid: int) -> bytes:
    # Minimal tail is the live-proven char-select bootstrap: profile61 reached
    # all B5 RPCs and finish with this 1027-byte batch. The full component tail
    # is preserved above, but profile61 20260618_111033 regressed after
    # logInClientResult with the full tail before onLoadPlayersCacheResult.
    return b"".join([
        pack_u32(PLAYER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_player_fields(char_pid),
            lua_fixed_tail=pack_lua_fixed_nil() * 5,
        ),
        pack_u32(CSTATUSPLAYER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_cstatusplayer_fields(),
            lua_fixed_tail=pack_lua_fixed_nil() * 9,
        ),
        pack_u32(USER_DESCRIPTOR_KEY)
        + build_component_instance_tail(
            build_b4_user_fields(char_pid),
            lua_fixed_tail=build_b4_user_lua_fixed_tail(),
        ),
    ])


def build_clientworld_newobject(
    object_pid: int,
    template_name: str,
    field_4c: int = 0,
    tname_id: int | None = None,
    is_having: bool = False,
    include_taccount_session_tail: bool = False,
    include_tplayerdummy_b4_tail: bool = False,
    account_name: str = "codex",
) -> bytes:
    # ClientWorld key 0x03 reads: u32 field_4c, u32 object pid,
    # pack_string(template), u32 m_nTName-ish field, bool m_bIsHaving.
    if tname_id is None:
        tname_id = native_nid(template_name)
    body = (
        field_4c.to_bytes(4, "little")
        + object_pid.to_bytes(4, "little")
        + pack_string(template_name)
        + tname_id.to_bytes(4, "little")
        + (b"\x01" if is_having else b"\x00")
    )
    if template_name == "TClient":
        body += build_tclient_key03_component_tail(object_pid)
    if template_name == "TAccount" and include_taccount_session_tail:
        body += build_taccount_session_key03_component_tail(
            object_pid,
            account_name=account_name,
        )
    if template_name == "TPlayerDummy" and include_tplayerdummy_b4_tail:
        body += build_tplayerdummy_b4_component_tail(object_pid)
    return build_lower_gss_body(0x03, body)


def build_backupobj(target_pid: int, component_name: str, netvar_dict: dict) -> bytes:
    name = component_name.encode("ascii")
    body = (
        target_pid.to_bytes(4, "little")
        + len(name).to_bytes(4, "little")
        + name
        + pack_variant(netvar_dict)
    )
    return build_lower_gss_body(0x05, body)


def build_linkhaving(parent_pid: int, child_pid: int, slot_name: str = "") -> bytes:
    name = slot_name.encode("ascii")
    body = (
        parent_pid.to_bytes(4, "little")
        + child_pid.to_bytes(4, "little")
        + len(name).to_bytes(4, "little")
        + name
    )
    return build_lower_gss_body(0x09, body)


def build_remotecall(target_pid: int, fn_name: str, args: list) -> bytes:
    name = fn_name.encode("ascii")
    body = (
        target_pid.to_bytes(4, "little")
        + len(name).to_bytes(4, "little")
        + name
        + len(args).to_bytes(4, "little")
        + b"".join(pack_rpc_variant(arg) for arg in args)
    )
    return build_lower_gss_body(0x06, body)


CLIENTWORLD_METHOD_NIDS = {
    "SetControlObject": 0xEAE76408,
    "connect": native_nid("connect"),
    "challenge": 0x031E9CA4,
    "response": native_nid("response"),
    "Client_FinishLoadingCharacter": 0xFF39796A,
    "signInClientResult": 0xEE6FF30D,
    "AddComponent": native_nid("AddComponent"),
    "goLua_AddComponent": native_nid("goLua_AddComponent"),
    "logInClientResult": native_nid("logInClientResult"),
    "logIn": native_nid("logIn"),
    "logInMaster": native_nid("logInMaster"),
    "logInServerResult": native_nid("logInServerResult"),
    "OnLoadPlayerData": native_nid("OnLoadPlayerData"),
    "onLoadPlayersCacheResult": native_nid("onLoadPlayersCacheResult"),
}

assert CLIENTWORLD_METHOD_NIDS["goLua_AddComponent"] == 0x3F946AA5
assert CLIENTWORLD_METHOD_NIDS["OnLoadPlayerData"] == 0xC5B899CD

CLIENTWORLD_SELF_KEYS = {
    "SetControlObject": 0x00000002,
    "connect": native_nid("Connector"),
    "challenge": native_nid("Connector"),
    "response": native_nid("Connector"),
    "Client_FinishLoadingCharacter": native_nid("Session"),
    "signInClientResult": native_nid("Session"),
    "logInClientResult": native_nid("Session"),
    "logIn": native_nid("Session"),
    "logInMaster": native_nid("Session"),
    "logInServerResult": native_nid("Session"),
    "OnLoadPlayerData": native_nid("Session"),
    "onLoadPlayersCacheResult": native_nid("Session"),
}

OBJECTPTR_DESCRIPTOR_KEY = native_nid("ObjectPtr")
assert OBJECTPTR_DESCRIPTOR_KEY == 0xBF68A6A3

# s20260618 ObjectPtr objectList correction: runtime traces show the live
# wrapper advertises object+0x0c key 0x160ff5a6, which native_nid maps to
# "Object".  The descriptor body is type-2: bb0060 reads this resolved type
# blob, then Lua Pack("size", n) key/value.
# Keep this local to onLoadPlayers objectList; generic RPC self/object handles
# still use the proven descriptor+pid scalar form.
NOBJECT_WRAPPER_KEY = 0x160FF5A6
assert NOBJECT_WRAPPER_KEY == native_nid("Object")


def build_clientworld_remotecall(
    manager_pid: int,
    fn_name: str,
    args: list,
    *,
    from_pid: int | None = None,
    target_pid: int | None = None,
    object_id: int | None = None,
    self_key: int | None = None,
    context: int | None = None,
) -> bytes:
    # ClientWorld key6 reads five scalar prefix fields, then a u16 call-mode
    # selector. Selector 1 enters the Variant argc/args branch in 0x00bee460.
    # 2026-06-03 s1/s2: profile6 showed scalar field2 is looked up in the
    # manager+0x1c object-pid map, so it must be the target object id. Field4
    # remains the callable-name key resolved through 0x00bc5860.
    if fn_name not in CLIENTWORLD_METHOD_NIDS:
        raise ValueError(f"unknown ClientWorld RemoteCall method: {fn_name}")
    if from_pid is None:
        # Server->client key6 uses field0/from before Lua in the selector-1
        # route gate. The recipe permits from=0, and s8 shows this keeps the
        # route mask at 0x10 instead of manager_pid's 0x40 hard-false lane.
        from_pid = 0
    if target_pid is None:
        target_pid = manager_pid
    if object_id is None:
        object_id = target_pid
    if self_key is None:
        self_key = CLIENTWORLD_SELF_KEYS[fn_name]
    if context is None:
        context = CLIENTWORLD_METHOD_NIDS[fn_name]
    body = (
        from_pid.to_bytes(4, "little", signed=False)
        + target_pid.to_bytes(4, "little", signed=False)
        + object_id.to_bytes(4, "little", signed=False)
        + self_key.to_bytes(4, "little", signed=False)
        + context.to_bytes(4, "little", signed=False)
        + (1).to_bytes(2, "little", signed=False)
        + pack_variant(len(args))
        + b"".join(pack_rpc_variant(arg) for arg in args)
    )
    return build_lower_gss_body(0x06, body)


def build_loginserverresult_downcall(
    dispatch_pid: int,
    session_pid: int,
    controlled_pid: int,
    *,
    object_id: int | None = None,
) -> bytes:
    # s34l: DownCall logInServerResult takes 8 args. Arg1 is the Session
    # ObjectPtr used as RPC "this"; arg7 is the controlled-object OID.
    return build_clientworld_remotecall(
        dispatch_pid,
        "logInServerResult",
        [
            handle_ref(session_pid),
            0,
            1,  # truthy numeric bool, not Variant tag bool
            0,
            0,
            0,
            controlled_pid,
            0,
        ],
        object_id=object_id,
    )


def build_login_downcall(
    dispatch_pid: int,
    session_pid: int,
    *,
    object_id: int | None = None,
) -> bytes:
    # s34q Phase 0: DownCall logIn takes [Session, num, num].
    return build_clientworld_remotecall(
        dispatch_pid,
        "logIn",
        [handle_ref(session_pid), 0, 0],
        object_id=object_id,
    )


def build_loginmaster_downcall(
    dispatch_pid: int,
    session_pid: int,
    *,
    object_id: int | None = None,
) -> bytes:
    # s34q Phase 0: DownCall logInMaster takes [Session, num, num, num|string].
    return build_clientworld_remotecall(
        dispatch_pid,
        "logInMaster",
        [handle_ref(session_pid), 0, 0, 0],
        object_id=object_id,
    )


def build_onloadplayers_downcall(
    dispatch_pid: int,
    session_pid: int,
    *,
    player_oid: int = 0,
    target_pid: int | None = None,
    object_id: int | None = None,
    object_list_oids: list[int] | None = None,
    object_list_as_arg2: bool = False,
) -> bytes:
    # Lua TPlayerCache calls session:onLoadPlayersCacheResult(from, objectList).
    # Static wrapper 0x00a736e0 reads arg1=ObjectPtr, arg2=number->OID, then
    # converts arg3 through the DAT_0184c144 list helper before FUN_00a7fd80.
    # Profile69-deep proved replacing arg2 with a table is consumed by the
    # number lane and still reaches CharacterManager::LoadList with an empty list.
    if object_list_as_arg2:
        if object_list_oids is None:
            object_list_oids = [player_oid] if player_oid else []
        args: list = [
            handle_ref(session_pid),
            [handle_objectlist_nobject_ref() for _ in object_list_oids],
        ]
    else:
        args = [handle_ref(session_pid), player_oid]
    if object_list_oids is not None and not object_list_as_arg2:
        # Claude 2026-06-19: inline-materialize is a dead end (Object=Playerless,
        # TPlayerDummy=NULL). Reference the already-replicated real char by plain
        # handle; trace whether bb5f40 resolves the live pid -> Player-bearing row.
        args.append([handle_ref(oid) for oid in object_list_oids])
    return build_clientworld_remotecall(
        dispatch_pid,
        "onLoadPlayersCacheResult",
        args,
        target_pid=target_pid,
        object_id=object_id,
    )


def build_onloadplayerdata_downcall(
    dispatch_pid: int,
    session_pid: int,
    controlled_pid: int,
    *,
    target_pid: int | None = None,
    object_id: int | None = None,
    pid_client: int | None = None,
    master_id: int = 1,
    channel_num: int = 1,
    data_type: int = 1,
    uid: int = 1,
) -> bytes:
    # SESSION_FINDINGS_2026-06-17 s9/s10: generated receiver 0x00a76440
    # extracts self + 6 args and calls Session::OnLoadPlayerData(0x00a80f30).
    # It is registered by base name only, not DownCallOnLoadPlayerData.
    if pid_client is None:
        pid_client = session_pid
    return build_clientworld_remotecall(
        dispatch_pid,
        "OnLoadPlayerData",
        [
            handle_ref(session_pid),
            pid_client,
            master_id,
            channel_num,
            controlled_pid,
            data_type,
            uid,
        ],
        target_pid=target_pid,
        object_id=object_id,
    )


def build_loginclientresult_downcall(
    dispatch_pid: int,
    *,
    result: int = 1,
    master: int = 1,
    channel: int = 1,
    account_id: int = 1,
    plr_id: int = 1,
    user_type: int = 1,
    target_pid: int | None = None,
    object_id: int | None = None,
) -> bytes:
    # Claude s53 / charselect B345: Session.logInClientResult takes
    # result, master, channel, accountId, plrId, type. The key6 prefix
    # carries the implicit from/self routing fields.
    return build_clientworld_remotecall(
        dispatch_pid,
        "logInClientResult",
        [result, master, channel, account_id, plr_id, user_type],
        target_pid=target_pid,
        object_id=object_id,
    )


def build_addcomponent_downcall(
    receiver_pid: int,
    carrier_pid: int,
    target_pid: int,
    component_pid: int,
    *,
    carrier_self_key: int = native_nid("Connector"),
    use_golua_callable: bool = False,
) -> bytes:
    # PROTOCOL_QUICKREF.md section 3.1 gives the key6 envelope.
    # Claude s45: selector-1 key6 RPC dispatch is Lua-mediated, so the live
    # probe uses goLua_AddComponent instead of the plain C++ AddComponent NID.
    callable_key = (
        CLIENTWORLD_METHOD_NIDS["goLua_AddComponent"]
        if use_golua_callable
        else CLIENTWORLD_METHOD_NIDS["AddComponent"]
    )
    return build_clientworld_remotecall(
        receiver_pid,
        "AddComponent",
        [handle_ref(target_pid), handle_ref(component_pid)],
        target_pid=receiver_pid,
        object_id=carrier_pid,
        self_key=carrier_self_key,
        context=callable_key,
    )


def handle_ref(pid: int) -> tuple[str, int]:
    return ("handle", pid)


def handle_objectlist_nobject_ref(wrapper_type_name: str = "TPlayerDummy") -> tuple[str, str]:
    # Claude 2026-06-19: live run proved the "Object" wrapper materializes but is
    # Player-less (AFTER_GETALL count=0). The objectList row must materialize the
    # real character class so getAllPlayers counts it. Try the char class itself.
    return ("objectlist_nobject_ref", wrapper_type_name)


def pack_nobject_type2_lua_tail(type_name: str, packvars_count: int = 0) -> bytes:
    # FUN_00bafdc0 calls 0x008336f0, which resolves object+0x0c through the
    # global type registry before writer+0x58 serializes that name blob. Live
    # wrapper object+0x0c is 0x160ff5a6 == native_nid("Object"). Class.lua
    # Pack() then emits goLua_Pack(source, "size", #PackVars); goLua_Pack
    # writes the key as tag 4/string and the count as tag 3/double.
    return (
        pack_string(type_name)
        + b"\x04"
        + pack_string("size")
        + b"\x03"
        + pack_f64(float(packvars_count))
    )


def pack_rpc_variant(value) -> bytes:
    if (
        isinstance(value, tuple)
        and len(value) == 2
        and value[0] == "objectlist_nobject_ref"
    ):
        return (
            bytes([0x0B])
            + OBJECTPTR_DESCRIPTOR_KEY.to_bytes(4, "little", signed=False)
            + native_nid(value[1]).to_bytes(4, "little", signed=False)
            + pack_nobject_type2_lua_tail(value[1], packvars_count=0)
        )
    if (
        isinstance(value, tuple)
        and len(value) == 2
        and value[0] == "handle"
        and isinstance(value[1], int)
    ):
        return (
            bytes([0x0B])
            + OBJECTPTR_DESCRIPTOR_KEY.to_bytes(4, "little", signed=False)
            + int(value[1]).to_bytes(4, "little", signed=False)
        )
    if isinstance(value, (list, tuple)):
        out = bytearray([5])
        out.extend(len(value).to_bytes(2, "little"))
        for i, item in enumerate(value, 1):
            out.extend(pack_variant(i))
            out.extend(pack_rpc_variant(item))
        return bytes(out)
    if isinstance(value, dict):
        out = bytearray([5])
        out.extend(len(value).to_bytes(2, "little"))
        for key, item in value.items():
            out.extend(pack_rpc_variant(key))
            out.extend(pack_rpc_variant(item))
        return bytes(out)
    return pack_variant(value)


def build_heartbeat() -> bytes:
    return build_lower_gss_body(0x12, b"")


def build_oob_to_client(subtype: int, payload: bytes) -> bytes:
    # FClientWorld::OnOobFromServer reads u8 subtype, then asks the packet for
    # the remaining body size before copying that many bytes. Subtype 2 routes
    # the copied buffer to TcDProto::OnDProtoData.
    if not 0 <= subtype <= 0xff:
        raise ValueError(f"OobToClient subtype out of range: {subtype}")
    body = bytes([subtype]) + payload
    return build_lower_gss_body(0x0d, body)


def build_encrypt_to_client(payload: bytes) -> bytes:
    # TcDProto::PacketFilterOnRecv special-cases opcode 0x0f and passes the
    # body cursor to OnResultGetData(mode=1). The exact TerSafe transform is
    # not recovered, so this is diagnostic until a trace proves it is needed.
    return build_lower_gss_body(0x0f, payload)


def apply_master_envelope(clear_packets: bytes, envelope: str) -> bytes:
    if envelope == "raw":
        return clear_packets
    if envelope == "oob2":
        return build_oob_to_client(2, clear_packets)
    if envelope == "encrypt":
        return build_encrypt_to_client(clear_packets)
    if envelope == "oob2-encrypt":
        return build_oob_to_client(2, build_encrypt_to_client(clear_packets))
    raise ValueError(f"unsupported master envelope: {envelope}")


class PidRegistry:
    """Stub-side PID allocator; role names are documentation, not wire constants."""

    def __init__(self, start: int = 1001) -> None:
        self.next_pid = start
        self.roles: dict[str, int] = {}

    def alloc(self, role: str) -> int:
        if role in self.roles:
            return self.roles[role]
        pid = self.next_pid
        self.next_pid += 1
        self.roles[role] = pid
        return pid

    def get(self, role: str) -> int | None:
        return self.roles.get(role)


def argb_to_tuple(value: int) -> tuple:
    return (
        "NiColorA",
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF,
        (value >> 24) & 0xFF,
    )


def build_status_modifications(char: dict) -> list[dict]:
    stats = dict(char.get("stats", {}))
    for key in ("Level", "Exp", "ClassLevel"):
        field = {
            "Level": "level",
            "Exp": "exp",
            "ClassLevel": "class_level",
        }[key]
        stats.setdefault(key, char.get(field, 0))
    for key in ("HP", "MP", "HPMax", "MPMax"):
        field = {
            "HP": "hp",
            "MP": "mp",
            "HPMax": "hp_max",
            "MPMax": "mp_max",
        }[key]
        value = char.get(field)
        if value is not None:
            stats.setdefault(key, value)
    return [
        {
            "m_attrName": name,
            "m_value": value,
            "m_name": "Base",
            "m_suppierid": 0,
        }
        for name, value in stats.items()
    ]


def build_character_replication_packets(
    bundle: dict,
    template_name: str = "TPlayerDummy",
    finish_session_pid: int | None = None,
    finish_loading: bool = True,
    signin_result: bool = True,
    master_name: str = "master1",
    channel_name: str = "channel1",
) -> list[bytes]:
    # Per PROTOCOL_QUICKREF.md section 6.4: CStatusPlayer NetVars do NOT
    # contain flat stat keys. Stats live in m_modificationList records.
    _FORBIDDEN_FLAT_STATS = {
        "Level", "Exp", "HP", "MP", "HPMax", "MPMax",
        "MinPhysicalAttackPower", "MaxPhysicalAttackPower",
        "MinMagicalAttackPower", "MaxMagicalAttackPower",
        "IcePower", "FirePower", "LightningPower", "DarkPower",
    }

    account = bundle["account"]
    chars = bundle["characters"]
    pids = PidRegistry()
    account_pid = pids.alloc("account:TAccount")
    packets: list[bytes] = [
        build_clientworld_issuepid(0, account_pid),
        build_clientworld_newobject(account_pid, "TAccount"),
        build_backupobj(account_pid, "Session", {
            "m_pid": account_pid,
            "m_AccountId": account["id"],
            "m_account_name": account["name"],
            "m_gender": account.get("gender", 0),
        }),
    ]
    for index, char in enumerate(chars, 1):
        char_pid = pids.alloc(f"character:{char['permanent_id']}:{template_name}")
        status_modifications = build_status_modifications(char)
        status_netvars = {
            "m_DBVersion": char.get("db_version", 64),
            "m_nJobClass": char["job_class"],
            "m_nSubClass": char["sub_class"],
            "m_AccountId": char["account_id"],
            "m_PermanentId": char["permanent_id"],
            "m_pid": char_pid,
            "m_AttributeLevel": {},
            "m_MaxValues": {},
            "m_periodicModifications": [],
            "m_modificationList": status_modifications,
        }
        forbidden = _FORBIDDEN_FLAT_STATS & set(status_netvars.keys())
        if forbidden:
            raise AssertionError(
                f"CStatusPlayer NetVars contain forbidden flat stat keys: "
                f"{sorted(forbidden)}. Stats belong in m_modificationList as "
                f"{{m_attrName, m_value, m_name, m_suppierid}} records "
                f"(PROTOCOL_QUICKREF.md section 6.4)."
            )
        packets += [
            build_clientworld_issuepid(0, char_pid),
            build_clientworld_newobject(char_pid, template_name),
            build_linkhaving(account_pid, char_pid, ""),
            build_backupobj(char_pid, "Player", {
                "m_PlayerName": char["name"],
                "m_name": char["name"],
                "m_AccountId": char["account_id"],
                "m_PermanentId": char["permanent_id"],
                "m_pid": char_pid,
                "m_bNeedBackupReplicateTPlayer": True,
            }),
            build_backupobj(char_pid, "CStatusPlayer", status_netvars),
            build_backupobj(char_pid, "Physics", {
                "m_kPosition": ("NiPoint3", char["pos_x"], char["pos_y"], char["pos_z"]),
                "m_kDirection": ("NiPoint3", char["dir_x"], char["dir_y"], char["dir_z"]),
            }),
            build_backupobj(char_pid, "User", {
                "m_RoomBack": char.get("room_back", ""),
                "m_RoomBackPos": char.get("room_back_pos", ""),
                "m_SelectedRoomIndex": char.get("room_index", 0),
                "m_SaveRoomName": char.get("map_name", "Intro_whiteCastle"),
            }),
            build_backupobj(char_pid, "CEquipment", {
                "m_strHairStyle": char.get("hair_style", ""),
                "m_strEyebrow": char.get("eyebrow", ""),
                "m_strEye": char.get("eye", ""),
                "m_strMouth": char.get("mouth", ""),
                "m_strNose": char.get("nose", "Default_Nose"),
                "m_HairColorCasual": argb_to_tuple(int(char.get("hair_color_argb") or 0)),
                "m_SkinColorCasual": char.get("skin_color", ""),
                "m_Scale": {
                    "Head": char.get("scale_head", 0),
                    "Body": char.get("body_size", 0.5),
                    "ArmWidth": char.get("scale_arm_width", 0),
                    "ArmHeight": char.get("scale_arm_height", 0),
                    "LegWidth": char.get("scale_leg_width", 0),
                    "LegHeight": char.get("scale_leg_height", 0),
                },
                "m_HeadSize": char.get("head_size", 0.7),
                "m_ScaleByAge": char.get("scale_by_age", 0.5),
                "m_ArmorOrCasual": char.get("armor_or_casual", "Casual"),
                "m_Container": {},
            }),
            build_backupobj(char_pid, "CSkillBook", {
                "m_SkillBookLevel": char.get("skill_book_level", char.get("level", 1)),
                "m_SkillLevelHash": char.get("skills", {}),
            }),
        ]
    if finish_loading:
        target_pid = finish_session_pid if finish_session_pid is not None else account_pid
        packets.append(build_remotecall(target_pid, "Client_FinishLoadingCharacter", [True, 0]))
    if signin_result:
        target_pid = finish_session_pid if finish_session_pid is not None else account_pid
        packets.append(build_remotecall(target_pid, "signInClientResult", [0, True, master_name, channel_name]))
    return packets


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


def load_key_window(path: Path) -> dict[int, int]:
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


def build_zlib_transport_frame(clear: bytes) -> bytes:
    compressed = zlib.compress(clear)
    return len(clear).to_bytes(4, "little") + len(compressed).to_bytes(4, "little") + compressed


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


def master_timing_packets(
    rung: str,
    account_pid: int = 1001,
    account_name: str = "codex",
) -> list[bytes]:
    if rung not in MASTER_TIMING_PROBES:
        raise ValueError(f"not a master timing probe: {rung}")
    if rung == "diag-d1":
        return [build_clientworld_issuepid(0, account_pid)]
    if rung == "diag-d2":
        return [build_issuepid(account_pid)]
    if rung == "diag-d3":
        return [build_bogus_opcode_control(account_pid)]
    if rung in MASTER_CONNECTOR_HANDSHAKE_PROBES:
        # PROTOCOL_QUICKREF.md section 3.1 defines the key6 RemoteCall
        # envelope. Claude s36 proves Connector.connect reads one scalar via
        # FUN_00bab7b0 before Connector::connect(0x00a91210). This bounded
        # probe intentionally skips SetControlObject/key2 and only asks which
        # native Connector gate blocks.
        client_pid = account_pid
        return [
            build_clientworld_issuepid(0, client_pid),
            build_clientworld_newobject(client_pid, "TClient"),
            build_clientworld_remotecall(
                client_pid,
                "connect",
                [1],
            ),
        ]
    if rung in MASTER_LOGINRPC_BOOTSTRAP_PROBES or rung in MASTER_LOGINRPC_BOOTSTRAP_GOLUAADD_PROBES:
        # Claude s40 live-validation recipe:
        # NewObject(controlled=TAccount), NewObject(Session),
        # AddComponent(controlled, Session), then Session login RPC chain.
        # The AddComponent call is carried through the already-routable
        # TClient/Connector lane proven by connector-connect-probe.
        carrier_pid = account_pid
        controlled_pid = account_pid + 1
        session_pid = account_pid + 2
        return [
            build_clientworld_issuepid(0, carrier_pid),
            build_clientworld_newobject(carrier_pid, "TClient"),
            build_clientworld_issuepid(0, controlled_pid),
            build_clientworld_newobject(controlled_pid, "TAccount"),
            build_clientworld_issuepid(0, session_pid),
            build_clientworld_newobject(session_pid, "Session"),
            build_addcomponent_downcall(
                session_pid,
                carrier_pid,
                controlled_pid,
                session_pid,
                use_golua_callable=rung in MASTER_LOGINRPC_BOOTSTRAP_GOLUAADD_PROBES,
            ),
            build_onloadplayers_downcall(
                session_pid,
                session_pid,
                object_id=controlled_pid,
            ),
            build_login_downcall(
                session_pid,
                session_pid,
                object_id=controlled_pid,
            ),
            build_loginmaster_downcall(
                session_pid,
                session_pid,
                object_id=controlled_pid,
            ),
            build_loginserverresult_downcall(
                session_pid,
                session_pid,
                controlled_pid,
                object_id=controlled_pid,
            ),
        ]
    if rung in MASTER_TACCOUNT_DIRECT_FINISH_PROBES:
        # master_replication_recipe.md section 7.1 / 8: shortest empty
        # character-select path. TAccount plants the Session component, then
        # Client_FinishLoadingCharacter is sent as a Session RPC with argc=2.
        account_obj_pid = account_pid
        return [
            build_clientworld_issuepid(0, account_obj_pid),
            build_clientworld_newobject(
                account_obj_pid,
                "TAccount",
                include_taccount_session_tail=True,
                account_name=account_name,
            ),
            build_clientworld_remotecall(
                account_obj_pid,
                "SetControlObject",
                [handle_ref(account_obj_pid)],
            ),
            build_clientworld_remotecall(
                account_obj_pid,
                "Client_FinishLoadingCharacter",
                [True, 0],
            ),
        ]
    if (
        rung in MASTER_ORDERED_EMPTY_PROBES
        or rung in MASTER_TCLIENT_WAIT_PROBES
        or rung in MASTER_TCLIENT_BATCH_PROBES
        or rung in MASTER_ORDERED_EMPTY_BATCH_PROBES
        or rung in MASTER_ORDERED_LOGINRESULT_PROBES
        or rung in MASTER_ORDERED_LOGINSEQ_PROBES
        or rung in MASTER_ORDERED_LOGINSEQ_SESSIONTAIL_PROBES
        or rung in MASTER_ORDERED_LOGINCLIENTRESULT_SESSIONTAIL_PROBES
        or rung in MASTER_ORDERED_LOGINCLIENTRESULT_ONLOADPLAYERS_PROBES
        or rung in MASTER_ORDERED_ONLOADPLAYERDATA_PROBES
        or rung in MASTER_ORDERED_ONLOADPLAYERDATA_PACED_PROBES
        or rung in MASTER_ORDERED_B4_ONLOADPLAYERDATA_PACED_PROBES
        or rung in MASTER_ORDERED_B4_B5_NO_ONLOAD_PROBES
        or rung in MASTER_ORDERED_B4_B5_PACED_AFTER_LOGIN_PROBES
        or rung in MASTER_ORDERED_B4_B5_EMPTY_SELECT_PROBES
        or rung in MASTER_ORDERED_B4_B5_FINISH0_PROBES
        or rung in MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_PROBES
        or rung in MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_OBJECTLIST_PROBES
        or rung in MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_FINISH_PROBES
        or rung in MASTER_ORDERED_B4_B5_PRELOGIN_ONLOAD_LISTARG_FINISH_PROBES
        or rung in MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_ARG3_FINISH_PROBES
        or rung in MASTER_ONLOADPLAYERS_PROBES
        or rung in MASTER_LOGINRPC_BOOTSTRAP_PROBES
    ):
        client_pid = account_pid
        session_account_pid = account_pid + 1
        b4_char_pid = account_pid + 2
        is_b4_onload = rung in MASTER_ORDERED_B4_ONLOADPLAYERDATA_PACED_PROBES
        is_b4_no_onload = rung in MASTER_ORDERED_B4_B5_NO_ONLOAD_PROBES
        is_b4_paced_after_login = rung in MASTER_ORDERED_B4_B5_PACED_AFTER_LOGIN_PROBES
        is_b4_empty_select = rung in MASTER_ORDERED_B4_B5_EMPTY_SELECT_PROBES
        is_b4_finish0 = rung in MASTER_ORDERED_B4_B5_FINISH0_PROBES
        is_b4_prelogin_finish = rung in MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_PROBES
        is_b4_prelogin_finish_objectlist = (
            rung in MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_OBJECTLIST_PROBES
        )
        is_b4_prelogin_double_onload_finish = (
            rung in MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_FINISH_PROBES
        )
        is_b4_prelogin_onload_listarg_finish = (
            rung in MASTER_ORDERED_B4_B5_PRELOGIN_ONLOAD_LISTARG_FINISH_PROBES
        )
        is_b4_prelogin_double_onload_arg3_finish = (
            rung in MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_ARG3_FINISH_PROBES
        )
        is_b4_character = (
            is_b4_onload
            or is_b4_no_onload
            or is_b4_paced_after_login
            or is_b4_finish0
            or is_b4_prelogin_finish
            or is_b4_prelogin_finish_objectlist
            or is_b4_prelogin_double_onload_finish
            or is_b4_prelogin_onload_listarg_finish
            or is_b4_prelogin_double_onload_arg3_finish
        )
        # 2026-06-19 (Claude) REPRODUCTION MODE: treat empty-select as a with-character b4 path
        # to reproduce last night's "channel -> reading char list -> transition wallpaper" run
        # (which staged TPlayerDummy + delivered the char). Revert this `or is_b4_empty_select`
        # (and the two player_oid/finish lines below) to restore the pure no-character base.
        is_b4 = is_b4_character
        is_b5_login_flow = is_b4_character or is_b4_empty_select
        include_taccount_session_tail = (
            rung in MASTER_ORDERED_LOGINSEQ_SESSIONTAIL_PROBES
            or rung in MASTER_ORDERED_LOGINCLIENTRESULT_SESSIONTAIL_PROBES
            or rung in MASTER_ORDERED_LOGINCLIENTRESULT_ONLOADPLAYERS_PROBES
            or rung in MASTER_ORDERED_ONLOADPLAYERDATA_PROBES
            or rung in MASTER_ORDERED_ONLOADPLAYERDATA_PACED_PROBES
            or is_b5_login_flow
            or rung in MASTER_ONLOADPLAYERS_PROBES
        )
        # Full state-order candidate from master_replication_recipe.md:
        # TClient control -> TAccount/Session -> Session RPCs -> empty select.
        packets = [
            build_clientworld_issuepid(0, client_pid),
            build_clientworld_newobject(client_pid, "TClient"),
            build_clientworld_remotecall(
                client_pid,
                "SetControlObject",
                [handle_ref(client_pid)],
            ),
        ]
        if rung in MASTER_TCLIENT_WAIT_PROBES or rung in MASTER_TCLIENT_BATCH_PROBES:
            return packets
        if rung == "ordered-empty-challenge":
            # TClient owns Connector; Connector.challenge(seq) is static-proven
            # to JumpUpCall response(0). See codex_connector_challenge_static.
            packets.append(
                build_clientworld_remotecall(
                    client_pid,
                    "challenge",
                    [0],
                )
            )
        packets.extend([
            build_clientworld_issuepid(0, session_account_pid),
            build_clientworld_newobject(
                session_account_pid,
                "TAccount",
                include_taccount_session_tail=include_taccount_session_tail,
                account_name=account_name,
            ),
            build_clientworld_remotecall(
                session_account_pid,
                "SetControlObject",
                [handle_ref(session_account_pid)],
            ),
            build_clientworld_remotecall(
                session_account_pid,
                "signInClientResult",
                [1, 1, 1],
            ),
        ])
        if is_b4:
            packets.extend([
                build_clientworld_issuepid(0, b4_char_pid),
                build_clientworld_newobject(
                    b4_char_pid,
                    "TPlayerDummy",
                    include_tplayerdummy_b4_tail=True,
                ),
            ])
        if (
            rung in MASTER_ORDERED_LOGINCLIENTRESULT_SESSIONTAIL_PROBES
            or rung in MASTER_ORDERED_LOGINCLIENTRESULT_ONLOADPLAYERS_PROBES
            or rung in MASTER_ORDERED_ONLOADPLAYERDATA_PROBES
            or rung in MASTER_ORDERED_ONLOADPLAYERDATA_PACED_PROBES
            or (
                is_b5_login_flow
                and not is_b4_prelogin_finish
                and not is_b4_prelogin_double_onload_finish
                and not is_b4_prelogin_onload_listarg_finish
                and not is_b4_prelogin_double_onload_arg3_finish
            )
        ):
            if (
                rung in MASTER_ORDERED_ONLOADPLAYERDATA_PROBES
                or rung in MASTER_ORDERED_ONLOADPLAYERDATA_PACED_PROBES
                or is_b4_onload
            ):
                packets.append(
                    build_onloadplayerdata_downcall(
                        session_account_pid,
                        session_account_pid,
                        b4_char_pid if is_b4_onload else client_pid,
                        target_pid=b4_char_pid if is_b4_onload else None,
                        object_id=session_account_pid,
                    )
                )
            packets.append(
                build_loginclientresult_downcall(
                    session_account_pid,
                    plr_id=b4_char_pid if is_b4 else 1,
                    target_pid=b4_char_pid if is_b4 else None,
                    object_id=session_account_pid if is_b4 else None,
                )
            )
        if rung in MASTER_ORDERED_LOGINRESULT_PROBES:
            packets.append(
                build_loginserverresult_downcall(
                    session_account_pid,
                    session_account_pid,
                    client_pid,
                )
            )
        if (
            rung in MASTER_ONLOADPLAYERS_PROBES
            or rung in MASTER_ORDERED_LOGINCLIENTRESULT_ONLOADPLAYERS_PROBES
            or is_b5_login_flow
        ):
            onload_object_list_oids = None
            if is_b4_prelogin_finish_objectlist:
                onload_object_list_oids = [b4_char_pid]
            elif is_b4_empty_select:
                # 2026-06-19 profile75: the generated wrapper reads an indirect
                # arg3 objectList before CharacterManager::LoadList. Empty-select
                # must send an empty table here, not omit the slot.
                onload_object_list_oids = []
            packets.append(
                build_onloadplayers_downcall(
                    session_account_pid,
                    session_account_pid,
                    player_oid=b4_char_pid if is_b4 else 0,
                    target_pid=b4_char_pid if is_b4 else None,
                    object_id=session_account_pid if is_b4 else None,
                    object_list_oids=onload_object_list_oids,
                )
            )
            if (
                is_b4_prelogin_double_onload_finish
                or is_b4_prelogin_onload_listarg_finish
                or is_b4_prelogin_double_onload_arg3_finish
            ):
                # Profile69 proved the first onLoadPlayersCacheResult takes
                # the CharacterManager creation leg only. Repeating the same
                # already-valid downcall after that registration should enter
                # the existing-manager LoadList leg at 0x00a620c0.
                packets.append(
                    build_onloadplayers_downcall(
                        session_account_pid,
                        session_account_pid,
                        player_oid=b4_char_pid,
                        target_pid=b4_char_pid,
                        object_id=session_account_pid,
                        object_list_oids=[b4_char_pid],
                        object_list_as_arg2=is_b4_prelogin_onload_listarg_finish,
                    )
                )
        if rung in MASTER_ORDERED_LOGINSEQ_PROBES or rung in MASTER_ORDERED_LOGINSEQ_SESSIONTAIL_PROBES:
            packets.extend([
                build_login_downcall(session_account_pid, session_account_pid),
                build_loginmaster_downcall(session_account_pid, session_account_pid),
                build_loginserverresult_downcall(
                    session_account_pid,
                    session_account_pid,
                    client_pid,
                ),
            ])
        packets.extend([
            build_clientworld_remotecall(
                session_account_pid,
                "Client_FinishLoadingCharacter",
                [True, 0 if is_b4_finish0 else 1 if is_b4 else 0],  # REPRO: char finish (was 0 for empty_select)
                target_pid=b4_char_pid if is_b4 else None,
                object_id=session_account_pid if is_b4 else None,
            ),
        ])
        return packets

    packets: list[bytes] = []
    rung_index = int(rung.rsplit("t", 1)[1])
    use_twou32_issuepid = rung in MASTER_LADDER_TWOU32_PROBES
    if rung_index >= 1:
        if use_twou32_issuepid:
            packets.append(build_clientworld_issuepid(0, account_pid))
        else:
            packets.append(build_issuepid(account_pid))
    if rung_index >= 2:
        if use_twou32_issuepid:
            packets.append(build_clientworld_newobject(account_pid, "TAccount"))
        else:
            packets.append(build_newobject(account_pid, "TAccount"))
    if rung_index >= 3:
        packets.append(
            build_clientworld_remotecall(
                account_pid,
                "SetControlObject",
                [handle_ref(account_pid)],
            )
        )
    if rung_index >= 4:
        packets.append(
            build_clientworld_remotecall(
                account_pid,
                "Client_FinishLoadingCharacter",
                [True, 0],
            )
        )
    return packets


def classify_alive_ms(alive_ms: int) -> str:
    if alive_ms < 2000:
        return "fast"
    if alive_ms >= 50000:
        return "idle"
    return "mid"


async def handle_master_phase(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    peer: object,
    client_header: bytes,
    response_header: bytes,
    key_window: dict[int, int],
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
    master_name: str,
    channel_name: str,
    hold_after_result_seconds: float,
) -> None:
    master_accept_at = time.perf_counter()
    # Master layer chain is [type-1 base, type-2 XOR, type-4 zlib]. If this
    # is reached with clear master traffic, the stub is bypassing a required
    # client layer.
    assert master_crypto == "xor", (
        f"handle_master_phase invoked with master_crypto={master_crypto!r}; "
        "must be 'xor'. See PROTOCOL_QUICKREF.md section 1 + section 6.1."
    )
    assert master_envelope in ("raw", "oob2", "encrypt", "oob2-encrypt"), (
        f"unknown master_envelope={master_envelope!r}"
    )

    client_start = int.from_bytes(client_header[:2], "little")
    client_end = int.from_bytes(client_header[2:4], "little")
    LOG.info(
        "[%s] master-phase initial4=%s start=0x%x end=0x%x "
        "valid_crypto_window=%s crypto=%s envelope=%s",
        peer,
        client_header.hex(" "),
        client_start,
        client_end,
        header_window_is_valid(client_header),
        master_crypto,
        master_envelope,
    )

    crypto: ServerCryptoWriter | None = None
    if master_crypto == "xor":
        writer.write(response_header)
        await writer.drain()
        LOG.info(
            "WIRE: server->client len=%d bytes=%s",
            len(response_header),
            response_header.hex(" "),
        )
        LOG.info("[%s] sent master-phase server crypto header=%s", peer, response_header.hex(" "))
        crypto = ServerCryptoWriter(writer, response_header, key_window)
        crypto.sent_header = True
    else:
        LOG.info(
            "[%s] master-phase uses diagnostic clear type-4 zlib transport; "
            "no XOR header sent",
            peer,
        )

    client_recv_cursor = client_start
    client_zlib_decoder = ZlibSyncFrameDecoder()

    if master_probe in MASTER_TIMING_PROBES:
        packets = master_timing_packets(master_probe, account_name=account_name)
        total_inner_bytes = 0
        total_zlib_clear_bytes = 0
        total_frame_bytes = 0
        if not packets:
            LOG.info(
                "[master-ladder] [%s] rung=%s sent no lower NESL packets "
                "(crypto header only); measuring master close timing",
                peer,
                master_probe,
            )
        if (
            master_probe in MASTER_ORDERED_ONLOADPLAYERDATA_PACED_PROBES
            or master_probe in MASTER_ORDERED_B4_ONLOADPLAYERDATA_PACED_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_PACED_AFTER_LOGIN_PROBES
        ):
            # Timing-only isolation. The lower-packet bytes stay identical to
            # the corresponding batch mode; only the zlib-frame boundary moves.
            if master_probe in MASTER_ORDERED_B4_B5_PACED_AFTER_LOGIN_PROBES:
                phase2_count = 2
                phase1_label = "phase1_through_loginclientresult"
                wait_label = "logInClientResult"
                phase2_label = "phase2_onloadplayers_finish"
                recv_error_event = "paced_recv_error_after_loginclientresult"
            else:
                # s12: OnLoadPlayerData reaches a client-side logInServerResult
                # emitter. Do not answer with logInClientResult in the same
                # frame; first give the client a chance to send that upstream
                # result.
                phase2_count = (
                    3 if master_probe in MASTER_ORDERED_B4_ONLOADPLAYERDATA_PACED_PROBES else 2
                )
                phase1_label = "phase1_through_onloadplayerdata"
                wait_label = "OnLoadPlayerData"
                phase2_label = "phase2_loginclientresult_finish"
                recv_error_event = "paced_recv_error_after_onload"
            phase1_packets = packets[:-phase2_count]
            phase2_packets = packets[-phase2_count:]

            async def send_paced_phase(label: str, phase_packets: list[bytes]) -> None:
                inner = b"".join(phase_packets)
                burst = apply_master_envelope(inner, master_envelope)
                zlib_frame = build_zlib_transport_frame(burst)
                if crypto is not None:
                    await crypto.send(zlib_frame)
                else:
                    writer.write(zlib_frame)
                    await writer.drain()
                    LOG.info("WIRE: server->client len=%d bytes=%s", len(zlib_frame), zlib_frame.hex(" "))
                LOG.info(
                    "[master-ladder] [%s] rung=%s %s packets=%d inner_bytes=%d "
                    "zlib_clear_bytes=%d frame_bytes=%d bodies=%s",
                    peer,
                    master_probe,
                    label,
                    len(phase_packets),
                    len(inner),
                    len(burst),
                    len(zlib_frame),
                    [
                        {
                            "opcode": int.from_bytes(packet[5:7], "little"),
                            "body": packet[16:].hex(" "),
                        }
                        for packet in phase_packets
                        if len(packet) >= 16
                    ],
                )

            try:
                await send_paced_phase(phase1_label, phase1_packets)
            except (ConnectionResetError, BrokenPipeError, OSError) as exc:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=paced_phase1_send_error error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    exc,
                )
                return

            try:
                data = await asyncio.wait_for(reader.read(4096), timeout=5.0)
            except asyncio.TimeoutError:
                data = b""
                LOG.info(
                    "[master-ladder] [%s] rung=%s no client bytes after "
                    "%s within 5s; sending phase2 anyway",
                    peer,
                    master_probe,
                    wait_label,
                )
            except (ConnectionResetError, OSError) as exc:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=%s error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    recv_error_event,
                    exc,
                )
                return

            if data:
                log_client_bytes(peer, f"master {master_probe} after {wait_label}", data)
                stream = key_stream_from_window(client_header, client_recv_cursor, len(data), key_window)
                if stream is not None:
                    clear = xor_with_stream(data, stream)
                    LOG.info(
                        "[%s] master %s clear after %s=%s frames=%s",
                        peer,
                        master_probe,
                        wait_label,
                        clear.hex(" "),
                        parse_zlib_transport_frames(clear),
                    )
                else:
                    LOG.info(
                        "[%s] master %s missing client key bytes after %s "
                        "cursor=0x%x len=%d",
                        peer,
                        master_probe,
                        wait_label,
                        client_recv_cursor,
                        len(data),
                    )
                client_recv_cursor += len(data)

            try:
                await send_paced_phase(phase2_label, phase2_packets)
            except (ConnectionResetError, BrokenPipeError, OSError) as exc:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=paced_phase2_send_error error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    exc,
                )
                return

            try:
                data = await asyncio.wait_for(reader.read(4096), timeout=15.0)
            except asyncio.TimeoutError:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=no_client_bytes_after_paced_tail_15s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                )
                return
            except (ConnectionResetError, OSError) as exc:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=paced_recv_error_after_tail error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    exc,
                )
                return
            if data == b"":
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=eof_after_paced_tail",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                )
                return
            log_client_bytes(peer, f"master {master_probe} after paced tail", data)
            stream = key_stream_from_window(client_header, client_recv_cursor, len(data), key_window)
            if stream is not None:
                clear = xor_with_stream(data, stream)
                LOG.info(
                    "[%s] master %s clear after paced tail=%s frames=%s",
                    peer,
                    master_probe,
                    clear.hex(" "),
                    parse_zlib_transport_frames(clear),
                )
            alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
            LOG.info(
                "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                "event=client_bytes_after_paced_tail",
                master_probe,
                alive_ms,
                classify_alive_ms(alive_ms),
            )
            return
        if (
            master_probe in MASTER_TCLIENT_BATCH_PROBES
            or master_probe in MASTER_CONNECTOR_HANDSHAKE_PROBES
            or master_probe in MASTER_ORDERED_EMPTY_BATCH_PROBES
            or master_probe in MASTER_ORDERED_LOGINRESULT_PROBES
            or master_probe in MASTER_ORDERED_LOGINSEQ_PROBES
            or master_probe in MASTER_ORDERED_LOGINSEQ_SESSIONTAIL_PROBES
            or master_probe in MASTER_ORDERED_LOGINCLIENTRESULT_SESSIONTAIL_PROBES
            or master_probe in MASTER_ORDERED_LOGINCLIENTRESULT_ONLOADPLAYERS_PROBES
            or master_probe in MASTER_ORDERED_ONLOADPLAYERDATA_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_NO_ONLOAD_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_EMPTY_SELECT_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_FINISH0_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_PRELOGIN_FINISH_OBJECTLIST_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_FINISH_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_PRELOGIN_ONLOAD_LISTARG_FINISH_PROBES
            or master_probe in MASTER_ORDERED_B4_B5_PRELOGIN_DOUBLE_ONLOAD_ARG3_FINISH_PROBES
            or master_probe in MASTER_TACCOUNT_DIRECT_FINISH_PROBES
            or master_probe in MASTER_ONLOADPLAYERS_PROBES
            or master_probe in MASTER_LOGINRPC_BOOTSTRAP_PROBES
            or master_probe in MASTER_LOGINRPC_BOOTSTRAP_GOLUAADD_PROBES
        ):
            # PROTOCOL_QUICKREF.md section 5: one master zlib frame may carry
            # multiple concatenated lower packets. This probe keeps the exact
            # lower-packet bytes but sends them as one atomic frame.
            inner_burst = b"".join(packets)
            burst = apply_master_envelope(inner_burst, master_envelope)
            zlib_frame = build_zlib_transport_frame(burst)
            try:
                if crypto is not None:
                    await crypto.send(zlib_frame)
                else:
                    writer.write(zlib_frame)
                    await writer.drain()
                    LOG.info(
                        "WIRE: server->client len=%d bytes=%s",
                        len(zlib_frame),
                        zlib_frame.hex(" "),
                    )
            except (ConnectionResetError, BrokenPipeError, OSError) as exc:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=batch_send_error error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    exc,
                )
                return
            total_inner_bytes = len(inner_burst)
            total_zlib_clear_bytes = len(burst)
            total_frame_bytes = len(zlib_frame)
            LOG.info(
                "[master-ladder] [%s] rung=%s batch_packets=%d "
                "inner_bytes=%d zlib_clear_bytes=%d frame_bytes=%d bodies=%s",
                peer,
                master_probe,
                len(packets),
                total_inner_bytes,
                total_zlib_clear_bytes,
                total_frame_bytes,
                [
                    {
                        "opcode": int.from_bytes(packet[5:7], "little"),
                        "body": packet[16:].hex(" "),
                    }
                    for packet in packets
                    if len(packet) >= 16
                ],
            )
            try:
                data = await asyncio.wait_for(reader.read(4096), timeout=5.0)
            except asyncio.TimeoutError:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                if (
                    master_probe in MASTER_ORDERED_EMPTY_BATCH_PROBES
                    or master_probe in MASTER_ORDERED_B4_B5_EMPTY_SELECT_PROBES
                ):
                    LOG.info(
                        "[master-ladder] [%s] rung=%s no client bytes within 5s; "
                        "holding for 25s more to distinguish loading-screen progress from silence",
                        peer,
                        master_probe,
                    )
                    try:
                        data = await asyncio.wait_for(reader.read(4096), timeout=25.0)
                    except asyncio.TimeoutError:
                        alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                        LOG.info(
                            "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                            "event=no_client_bytes_after_batch_30s",
                            master_probe,
                            alive_ms,
                            classify_alive_ms(alive_ms),
                        )
                        return
                    except (ConnectionResetError, OSError) as exc:
                        alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                        LOG.info(
                            "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                            "event=batch_recv_error_after_5s error=%s",
                            master_probe,
                            alive_ms,
                            classify_alive_ms(alive_ms),
                            exc,
                        )
                        return
                else:
                    LOG.info(
                        "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                        "event=no_client_bytes_after_batch",
                        master_probe,
                        alive_ms,
                        classify_alive_ms(alive_ms),
                    )
                    return
            except (ConnectionResetError, OSError) as exc:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=batch_recv_error error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    exc,
                )
                return
            if data == b"":
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=eof_after_batch",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                )
                return
            log_client_bytes(peer, f"master {master_probe} after batch", data)
            if master_crypto == "xor":
                stream = key_stream_from_window(
                    client_header,
                    client_recv_cursor,
                    len(data),
                    key_window,
                )
                if stream is not None:
                    clear = xor_with_stream(data, stream)
                    LOG.info(
                        "[%s] master %s clear after batch=%s frames=%s",
                        peer,
                        master_probe,
                        clear.hex(" "),
                        parse_zlib_transport_frames(clear),
                    )
                    LOG.info(
                        "[%s] master %s zsync after batch=%s",
                        peer,
                        master_probe,
                        client_zlib_decoder.feed(clear),
                    )
                else:
                    LOG.info(
                        "[%s] master %s missing client key bytes "
                        "cursor=0x%x len=%d",
                        peer,
                        master_probe,
                        client_recv_cursor,
                        len(data),
                    )
            if client_end > client_start:
                window = client_end - client_start
                client_recv_cursor = client_start + (
                    (client_recv_cursor - client_start + len(data)) % window
                )
            alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
            LOG.info(
                "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                "event=client_bytes_after_batch",
                master_probe,
                alive_ms,
                classify_alive_ms(alive_ms),
            )
            if hold_after_result_seconds > 0:
                hold_until = time.perf_counter() + hold_after_result_seconds
                LOG.info(
                    "[%s] holding master socket after result for %.1fs",
                    peer,
                    hold_after_result_seconds,
                )
                while time.perf_counter() < hold_until:
                    timeout = max(0.1, min(5.0, hold_until - time.perf_counter()))
                    try:
                        data = await asyncio.wait_for(reader.read(4096), timeout=timeout)
                    except asyncio.TimeoutError:
                        LOG.info("[%s] hold-after-result still open", peer)
                        continue
                    except (ConnectionResetError, OSError) as exc:
                        LOG.info("[%s] hold-after-result recv error=%r", peer, exc)
                        return
                    if not data:
                        LOG.info("[%s] hold-after-result EOF", peer)
                        return
                    log_client_bytes(peer, f"master {master_probe} hold after result", data)
                    stream = key_stream_from_window(
                        client_header,
                        client_recv_cursor,
                        len(data),
                        key_window,
                    )
                    if stream is not None:
                        clear = xor_with_stream(data, stream)
                        LOG.info(
                            "[%s] master %s hold clear=%s frames=%s",
                            peer,
                            master_probe,
                            clear.hex(" "),
                            parse_zlib_transport_frames(clear),
                        )
                        LOG.info(
                            "[%s] master %s hold zsync=%s",
                            peer,
                            master_probe,
                            client_zlib_decoder.feed(clear),
                        )
                    else:
                        LOG.info(
                            "[%s] master %s hold missing client key bytes cursor=0x%x len=%d",
                            peer,
                            master_probe,
                            client_recv_cursor,
                            len(data),
                        )
                    if client_end > client_start:
                        window = client_end - client_start
                        client_recv_cursor = client_start + (
                            (client_recv_cursor - client_start + len(data)) % window
                        )
                LOG.info("[%s] hold-after-result ended", peer)
            return
        for idx, clear_packet in enumerate(packets, start=1):
            burst = apply_master_envelope(clear_packet, master_envelope)
            zlib_frame = build_zlib_transport_frame(burst)
            try:
                if crypto is not None:
                    await crypto.send(zlib_frame)
                else:
                    writer.write(zlib_frame)
                    await writer.drain()
                    LOG.info(
                        "WIRE: server->client len=%d bytes=%s",
                        len(zlib_frame),
                        zlib_frame.hex(" "),
                    )
            except (ConnectionResetError, BrokenPipeError, OSError) as exc:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=send_error error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    exc,
                )
                return
            total_inner_bytes += len(clear_packet)
            total_zlib_clear_bytes += len(burst)
            total_frame_bytes += len(zlib_frame)
            opcode = int.from_bytes(clear_packet[5:7], "little") if len(clear_packet) >= 7 else -1
            LOG.info(
                "[master-ladder] [%s] rung=%s part=%d/%d opcode=0x%04x "
                "inner_bytes=%d zlib_clear_bytes=%d frame_bytes=%d body=%s",
                peer,
                master_probe,
                idx,
                len(packets),
                opcode,
                len(clear_packet),
                len(burst),
                len(zlib_frame),
                clear_packet[16:].hex(" ") if len(clear_packet) >= 16 else "",
            )
            if master_probe in MASTER_ORDERED_EMPTY_PROBES or master_probe in MASTER_TCLIENT_WAIT_PROBES:
                is_tclient_wait_final = (
                    master_probe in MASTER_TCLIENT_WAIT_PROBES and idx == len(packets)
                )
                read_timeout = 5.0 if is_tclient_wait_final else 0.25
                timed_out = False
                try:
                    data = await asyncio.wait_for(reader.read(4096), timeout=read_timeout)
                except asyncio.TimeoutError:
                    timed_out = True
                    data = b""
                except (ConnectionResetError, OSError) as exc:
                    alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                    LOG.info(
                        "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                        "event=recv_error_after_part%d error=%s",
                        master_probe,
                        alive_ms,
                        classify_alive_ms(alive_ms),
                        idx,
                        exc,
                    )
                    return
                if data == b"":
                    if not timed_out or writer.is_closing():
                        alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                        LOG.info(
                            "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                            "event=eof_after_part%d",
                            master_probe,
                            alive_ms,
                            classify_alive_ms(alive_ms),
                            idx,
                        )
                        return
                    if is_tclient_wait_final:
                        alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                        LOG.info(
                            "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                            "event=no_client_bytes_after_part%d",
                            master_probe,
                            alive_ms,
                            classify_alive_ms(alive_ms),
                            idx,
                        )
                        return
                else:
                    log_client_bytes(peer, f"master {master_probe} after part {idx}", data)
                    if master_crypto == "xor":
                        stream = key_stream_from_window(
                            client_header,
                            client_recv_cursor,
                            len(data),
                            key_window,
                        )
                        if stream is not None:
                            clear = xor_with_stream(data, stream)
                            LOG.info(
                                "[%s] master %s clear after part %d=%s frames=%s",
                                peer,
                                master_probe,
                                idx,
                                clear.hex(" "),
                                parse_zlib_transport_frames(clear),
                            )
                        else:
                            LOG.info(
                                "[%s] master %s missing client key bytes "
                                "cursor=0x%x len=%d",
                                peer,
                                master_probe,
                                client_recv_cursor,
                                len(data),
                            )
                    client_recv_cursor += len(data)
                    if is_tclient_wait_final:
                        alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                        LOG.info(
                            "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                            "event=client_bytes_after_part%d",
                            master_probe,
                            alive_ms,
                            classify_alive_ms(alive_ms),
                            idx,
                        )
                        return
        LOG.info(
            "[master-ladder] [%s] rung=%s send_complete packets=%d "
            "inner_bytes=%d zlib_clear_bytes=%d frame_bytes=%d",
            peer,
            master_probe,
            len(packets),
            total_inner_bytes,
            total_zlib_clear_bytes,
            total_frame_bytes,
        )
    elif master_probe == "silent":
        LOG.info(
            "[%s] master-phase silent probe active; sent crypto header only, "
            "holding socket open and logging client bytes",
            peer,
        )
    elif character_burst or master_probe != "none":
        bundle = load_account(db_path, account_name)
        if master_probe == "heartbeat":
            packets = [build_heartbeat()]
        elif master_probe == "issuepid":
            packets = [build_clientworld_issuepid(0, 1001)]
        elif master_probe == "account-root":
            account_pid = 1001
            packets = [
                build_clientworld_issuepid(0, account_pid),
                build_clientworld_newobject(account_pid, "TAccount"),
                build_backupobj(account_pid, "Session", {
                    "m_pid": account_pid,
                    "m_AccountId": 1,
                    "m_account_name": "demo",
                    "m_gender": 1,
                }),
                build_remotecall(
                    account_pid,
                    "signInClientResult",
                    [0, True, master_name, channel_name],
                ),
            ]
        else:
            packets = build_character_replication_packets(
                bundle,
                character_template,
                finish_session_pid,
                finish_loading,
                signin_result,
                master_name,
                channel_name,
            )
        if master_probe == "account-root":
            total_inner_bytes = 0
            total_zlib_clear_bytes = 0
            total_frame_bytes = 0
            for idx, clear_packet in enumerate(packets, start=1):
                burst = apply_master_envelope(clear_packet, master_envelope)
                zlib_frame = build_zlib_transport_frame(burst)
                if crypto is not None:
                    await crypto.send(zlib_frame)
                else:
                    writer.write(zlib_frame)
                    await writer.drain()
                total_inner_bytes += len(clear_packet)
                total_zlib_clear_bytes += len(burst)
                total_frame_bytes += len(zlib_frame)
                LOG.info(
                    "[master] [%s] sent account-root part=%d/%d "
                    "crypto=%s envelope=%s inner_packets=1 inner_bytes=%d "
                    "zlib_clear_bytes=%d frame_bytes=%d",
                    peer,
                    idx,
                    len(packets),
                    master_crypto,
                    master_envelope,
                    len(clear_packet),
                    len(burst),
                    len(zlib_frame),
                )
            LOG.info(
                "[%s] sent master-phase zlib-framed account-root probe "
                "crypto=%s envelope=%s inner_packets=%d inner_bytes=%d "
                "zlib_clear_bytes=%d frame_bytes=%d template=%s "
                "finish_loading=%s signin_result=%s finish_session_pid=%s",
                peer,
                master_crypto,
                master_envelope,
                len(packets),
                total_inner_bytes,
                total_zlib_clear_bytes,
                total_frame_bytes,
                character_template,
                finish_loading,
                signin_result,
                finish_session_pid,
            )
        else:
            inner_burst = b"".join(packets)
            burst = apply_master_envelope(inner_burst, master_envelope)
            zlib_frame = build_zlib_transport_frame(burst)
            if crypto is not None:
                await crypto.send(zlib_frame)
            else:
                writer.write(zlib_frame)
                await writer.drain()
            LOG.info(
                "[%s] sent master-phase zlib-framed %s "
                "crypto=%s envelope=%s inner_packets=%d inner_bytes=%d "
                "zlib_clear_bytes=%d frame_bytes=%d template=%s "
                "finish_loading=%s signin_result=%s finish_session_pid=%s",
                peer,
                f"{master_probe} probe" if master_probe != "none" else "character replication",
                master_crypto,
                master_envelope,
                len(packets),
                len(inner_burst),
                len(burst),
                len(zlib_frame),
                character_template,
                finish_loading,
                signin_result,
                finish_session_pid,
            )
        if finish_loading and finish_session_pid is None:
            LOG.info(
                "[%s] Client_FinishLoadingCharacter target defaults to account Session m_pid "
                "from the replicated TAccount/Session object",
                peer,
            )
    else:
        LOG.info("[%s] master-phase character replication disabled; logging only", peer)

    cursor = client_recv_cursor
    clear_pending = bytearray(client_header if master_crypto == "none" else b"")
    while True:
        try:
            data = await asyncio.wait_for(reader.read(4096), timeout=30.0)
        except asyncio.TimeoutError:
            if master_probe in MASTER_TIMING_PROBES:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                if alive_ms >= int(MASTER_LADDER_IDLE_SECONDS * 1000):
                    LOG.info(
                        "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                        "event=server_timeout_still_open",
                        master_probe,
                        alive_ms,
                        classify_alive_ms(alive_ms),
                    )
                    return
            LOG.info("[%s] no master-phase payload within 30s; keeping protocol state unguessed", peer)
            continue
        except (ConnectionResetError, OSError) as exc:
            if master_probe in MASTER_TIMING_PROBES:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s "
                    "event=recv_error error=%s",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                    repr(exc),
                )
            return
        if not data:
            if master_probe in MASTER_TIMING_PROBES:
                alive_ms = int((time.perf_counter() - master_accept_at) * 1000)
                LOG.info(
                    "[master-ladder-result] rung=%s alive_ms=%d class=%s event=eof",
                    master_probe,
                    alive_ms,
                    classify_alive_ms(alive_ms),
                )
            LOG.info("[%s] master-phase EOF", peer)
            return

        log_client_bytes(peer, "master-phase payload", data)
        if master_crypto == "none":
            clear_pending.extend(data)
            zlib_frames = parse_zlib_transport_frames(bytes(clear_pending))
            if zlib_frames:
                LOG.info(
                    "[%s] decoded clear master-phase zlib frames=%s",
                    peer,
                    [(raw_len, packets) for raw_len, _inflated, packets in zlib_frames],
                )
        else:
            stream = key_stream_from_window(client_header, cursor, len(data), key_window)
            if stream is None:
                LOG.info(
                    "[%s] master-phase payload not decoded: missing key bytes at cursor=0x%x len=%d",
                    peer,
                    cursor,
                    len(data),
                )
            else:
                clear = xor_with_stream(data, stream)
                LOG.info(
                    "[%s] decoded master-phase clear=%s packets=%s",
                    peer,
                    clear[:128].hex(" "),
                    parse_lower_packets(clear),
                )
                zlib_frames = parse_zlib_transport_frames(clear)
                if zlib_frames:
                    LOG.info(
                        "[%s] decoded master-phase zlib frames=%s",
                        peer,
                        [(raw_len, packets) for raw_len, _inflated, packets in zlib_frames],
                    )
                LOG.info(
                    "[%s] decoded master-phase zsync frames=%s",
                    peer,
                    client_zlib_decoder.feed(clear),
                )
            if client_end > client_start:
                window = client_end - client_start
                cursor = client_start + ((cursor - client_start + len(data)) % window)


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
    peer = writer.get_extra_info("peername")
    LOG.info("[%s] connected", peer)
    try:
        header = await reader.readexactly(4)
        try:
            cipher_body = await asyncio.wait_for(reader.readexactly(128), timeout=2.0)
        except asyncio.TimeoutError:
            await handle_master_phase(
                reader,
                writer,
                peer,
                header,
                response_header_override or DEFAULT_FULL_GSS_RESPONSE_HEADER,
                key_window,
                db_path,
                account_name,
                character_burst,
                character_template,
                finish_session_pid,
                finish_loading,
                signin_result,
                master_probe,
                master_crypto,
                master_envelope,
                master,
                channel,
                hold_after_result_seconds,
            )
            return

        plain = expected_signin_body(root)
        stream = bytes(c ^ p for c, p in zip(cipher_body, plain))
        LOG.info("[%s] first header=%s stream=%s", peer, header.hex(" "), stream[:32].hex(" "))
        log_client_bytes(peer, "initial sign-in header+body", header + cipher_body)
        live_start = int.from_bytes(header[:2], "little")
        client_key_window = dict(key_window)
        server_key_window = dict(key_window)
        live_conflicts = 0
        live_filled = 0
        for i, value in enumerate(stream):
            key_index = live_start + i
            existing = client_key_window.get(key_index)
            if existing is None:
                client_key_window[key_index] = value
                live_filled += 1
            elif existing != value:
                # The sign-in plaintext is not byte-stable across all runs; the
                # canonical key stream wins when it already has this index.
                live_conflicts += 1
        LOG.debug("[%s] merged live key stream start=0x%x len=%d",
                  peer, live_start, len(stream))
        if live_conflicts or live_filled:
            LOG.info(
                "[%s] live key stream merge filled=%d conflicts_preserved_canonical=%d",
                peer,
                live_filled,
                live_conflicts,
            )
        if cipher_body == plain:
            LOG.warning("[%s] first body was plaintext, not transformed", peer)
        if (cipher_body[0] ^ stream[0]) != 0x70 or (cipher_body[5] ^ stream[5]) != 0x09:
            LOG.warning("[%s] known-plaintext check did not match expected sign-in", peer)

        if full_gss:
            response_header = response_header_override or DEFAULT_FULL_GSS_RESPONSE_HEADER
            crypto = ServerCryptoWriter(writer, response_header, server_key_window)
            bundle = load_account(db_path, account_name)
            account_id = int(bundle["account"]["id"])
            stage1 = build_native_session_in_challenge()
            await crypto.send(stage1)
            LOG.info(
                "[%s] sent full-gss stage1 0x1001 bytes=%d header=%s chars=%d",
                peer, len(stage1), response_header.hex(" "), len(bundle["characters"]),
            )

            challenge_reply = b""
            try:
                challenge_reply = await asyncio.wait_for(reader.read(4096), timeout=10.0)
            except asyncio.TimeoutError:
                LOG.info(
                    "[%s] no 0x4002 challenge-response within 10s; "
                    "leaving connection open for client retry",
                    peer,
                )
            if challenge_reply:
                log_client_bytes(peer, "0x4002 challenge follow-up", challenge_reply)
                client_cursor = live_start + len(cipher_body)
                follow_stream = key_stream_from_window(header, client_cursor, len(challenge_reply), client_key_window)
                if follow_stream is not None:
                    clear = xor_with_stream(challenge_reply, follow_stream)
                    LOG.info("[%s] decoded challenge follow-up clear=%s packets=%s",
                             peer, clear[:128].hex(" "), parse_lower_packets(clear))
                else:
                    LOG.info("[%s] challenge follow-up not decoded: missing client key bytes at cursor=0x%x",
                             peer, client_cursor)
                stage2 = b"".join([
                    build_native_session_in_result(account_id),
                    build_native_master_channel_update(master, channel),
                ])
                await crypto.send(stage2)
                LOG.info(
                    "[%s] sent full-gss stage2 0x1003+0x1004 bytes=%d",
                    peer,
                    len(stage2),
                )

            try:
                data = await asyncio.wait_for(reader.read(4096), timeout=signin_wait)
            except asyncio.TimeoutError:
                LOG.info(
                    "[%s] no post-channel/sign-in packet within %.1fs; leaving connection open for logging",
                    peer,
                    signin_wait,
                )
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
                                request_token, sign_master_id, sign_channel_id = decoded
                                LOG.info(
                                    "[%s] decoded 0x4005 token=%d master_id=%d channel_id=%d",
                                    peer,
                                    request_token,
                                    sign_master_id,
                                    sign_channel_id,
                                )
                else:
                    LOG.info("[%s] post-channel packet not decoded: missing client key bytes at cursor=0x%x",
                             peer, post_cursor)
                sign_in = build_native_signin_result(
                    signin_host,
                    signin_port,
                    sign_master_id,
                    sign_channel_id,
                )
                await crypto.send(sign_in)
                LOG.info(
                    "[%s] sent full-gss stage3 0x1006 bytes=%d master_id=%d channel_id=%d endpoint=%s:%s",
                    peer,
                    len(sign_in),
                    sign_master_id,
                    sign_channel_id,
                    signin_host,
                    signin_port,
                )

            while True:
                data = await reader.read(4096)
                if not data:
                    LOG.info("[%s] EOF", peer)
                    return
                log_client_bytes(peer, "post-gss follow-up", data)

        if native_1006:
            response = build_native_signin_result(signin_host, signin_port)
            LOG.info("[%s] using native lower GSS 0x1006 response body=%s",
                     peer, response.hex(" "))
        else:
            # Diagnostic only: this is still a high-level RPC body, not the full
            # CL_NetSession lower packet envelope. The client validator reads a
            # lower opcode at body +5 before GSS handlers can see the payload.
            response = build_signin_client_result(SESSION_OK, "master1", "channel1")
        if response_header_override is not None:
            response_header = response_header_override
            response_stream = key_stream_from_header(response_header, len(response), server_key_window)
            if response_stream is None:
                raise ValueError(f"no recovered key stream for response header {response_header.hex(' ')}")
        elif fixed_header:
            # Diagnostic only. This regressed to a normal connection cut-off in
            # later testing, so the default stays with the live client header.
            response_header = FIXED_RESPONSE_HEADER
            response_stream = FIXED_RESPONSE_STREAM
        else:
            response_header = header
            response_stream = key_stream_from_header(header, len(response), server_key_window)
            if response_stream is None:
                response_stream = stream
                LOG.warning("[%s] no complete recovered key window for header=%s; using known-plaintext stream",
                            peer, header.hex(" "))
        packet = bytearray(response_header + xor_with_stream(response, response_stream))
        if native_1004_after:
            stream_start = int.from_bytes(response_header[:2], "little") + len(response)
            channel_update = build_native_master_channel_update(master, channel)
            channel_stream = key_stream_from_cursor(stream_start, len(channel_update), server_key_window)
            if channel_stream is None:
                LOG.warning("[%s] skipped native lower GSS 0x1004: missing key stream at 0x%x len=%d",
                            peer, stream_start, len(channel_update))
            else:
                packet += xor_with_stream(channel_update, channel_stream)
                LOG.info("[%s] appended native lower GSS 0x1004 body=%s stream_start=0x%x",
                         peer, channel_update.hex(" "), stream_start)
        writer.write(packet)
        await writer.drain()
        LOG.info("[%s] sent response len=%d total_tcp=%d header=%s native_1006=%s native_1004_after=%s",
                 peer, len(response), len(packet), response_header.hex(" "), native_1006,
                 native_1004_after)

        while True:
            data = await reader.read(4096)
            if not data:
                LOG.info("[%s] EOF", peer)
                return
            log_client_bytes(peer, "post-response follow-up", data)
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
