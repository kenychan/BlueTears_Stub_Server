"""GSS wire builders -- lower-GSS bodies, ClientWorld ops, RPC downcalls, component tails.

Byte-exact extraction (AST source copy) from Res/gss_stub_server_v4.py; build_clientworld_newobject
is the simplified clean-lane version (TClient/TAccount only). Legacy deps (native_nid, pack_variant)
are bridge-imported from Res/ until they are modularized too.
"""
from __future__ import annotations

import logging
import struct  # noqa: F401  (used by pack_f64/pack_f32)
import time  # noqa: F401  (used by build_onclientgetsrqlresult_downcall timestamp)
import sys as _sys
from pathlib import Path as _Path

_RES = _Path(__file__).resolve().parents[3] / "Res"
if str(_RES) not in _sys.path:
    _sys.path.insert(0, str(_RES))
from nid_hash import native_nid           # noqa: E402
from gss_stub_server_v3 import pack_variant  # noqa: E402

LOG = logging.getLogger("gss.net.packets")


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
FUSER_SELECT_CHARACTER_KEYS = {
    native_nid("selectCharacter"),
    native_nid("SelectCharacter"),
}
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
assert native_nid("SelectCharacter") == 0x2CECDBE5
assert native_nid("selectCharacter") == 0x3A7CE6E9

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
    "createCharacterResult": native_nid("createCharacterResult"),
    "checkCharacterNameResult": native_nid("checkCharacterNameResult"),
    "DownCallcreateCharacterResult": native_nid("DownCallcreateCharacterResult"),
    "DownCallcheckCharacterNameResult": native_nid("DownCallcheckCharacterNameResult"),
    "DownCallserverCreateCharacterResult": native_nid("DownCallserverCreateCharacterResult"),
    "serverCreateCharacterResult": native_nid("serverCreateCharacterResult"),
    "OnClientGetSRQLResult": native_nid("OnClientGetSRQLResult"),
}

assert CLIENTWORLD_METHOD_NIDS["goLua_AddComponent"] == 0x3F946AA5
assert CLIENTWORLD_METHOD_NIDS["OnLoadPlayerData"] == 0xC5B899CD
assert CLIENTWORLD_METHOD_NIDS["DownCallcreateCharacterResult"] == 0x631CB5E3
assert CLIENTWORLD_METHOD_NIDS["DownCallcheckCharacterNameResult"] == 0x9508DA80
assert CLIENTWORLD_METHOD_NIDS["DownCallserverCreateCharacterResult"] == 0xDACB2AFC
assert CLIENTWORLD_METHOD_NIDS["OnClientGetSRQLResult"] == 0xD13998D9

KEY6_CALLABLE_NAME_BY_NID = {
    value: name for name, value in CLIENTWORLD_METHOD_NIDS.items()
}
KEY6_CALLABLE_NAME_BY_NID.update({
    native_nid("SelectCharacter"): "SelectCharacter",
    native_nid("selectCharacter"): "selectCharacter",
    native_nid("checkCharacterName"): "checkCharacterName",
    native_nid("createCharacter"): "createCharacter",
    native_nid("OnServerGetSRQL"): "OnServerGetSRQL",
    native_nid("Server_FinishLoadingCharacter"): "Server_FinishLoadingCharacter",
})

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
    "createCharacterResult": native_nid("CharacterManager"),
    "checkCharacterNameResult": native_nid("CharacterManager"),
    "DownCallcreateCharacterResult": native_nid("CharacterManager"),
    "DownCallcheckCharacterNameResult": native_nid("CharacterManager"),
    "DownCallserverCreateCharacterResult": native_nid("CharacterManager"),
    "serverCreateCharacterResult": native_nid("CharacterManager"),
    "OnClientGetSRQLResult": native_nid("Player"),
}

OBJECTPTR_DESCRIPTOR_KEY = native_nid("ObjectPtr")
assert OBJECTPTR_DESCRIPTOR_KEY == 0xBF68A6A3

# ObjectPtr has two wire uses:
# - ordinary RPC handles use descriptor_key + replicated object id, no tail;
# - descriptor-backed object values currently use descriptor_key + replicated
#   object id + minimal Object(size=0) tail. The class-key/TPlayerDummy tail
#   variant is live-negative for select rows (SESSION_FINDINGS_2026-06-22 s16).
NOBJECT_WRAPPER_KEY = 0x160FF5A6
assert NOBJECT_WRAPPER_KEY == native_nid("Object")

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

def build_clientworld_newobject(
    object_pid: int,
    template_name: str,
    field_4c: int = 0,
    tname_id: int | None = None,
    is_having: bool = False,
    include_taccount_session_tail: bool = False,
    account_name: str = "codex",
) -> bytes:
    # ClientWorld key 0x03 reads: u32 field_4c, u32 object pid,
    # pack_string(template), u32 m_nTName-ish field, bool m_bIsHaving.
    # Char-tail branches (TPlayer/TPlayerDummy) were parked research lanes; the
    # faithful char path is the host-replication hook, so this builds only the
    # clean-lane control objects (TClient + faithful TAccount 4-Com tail).
    if tname_id is None:
        tname_id = native_nid(template_name)
    body = (
        field_4c.to_bytes(4, "little")
        + object_pid.to_bytes(4, "little")
        + pack_string(template_name)
        + tname_id.to_bytes(4, "little")
        + (bytes([1]) if is_having else bytes([0]))
    )
    if template_name == "TClient":
        body += build_tclient_key03_component_tail(object_pid)
    if template_name == "TAccount" and include_taccount_session_tail:
        body += build_taccount_session_key03_component_tail(
            object_pid,
            account_name=account_name,
        )
    return build_lower_gss_body(0x03, body)

def build_clientworld_linkhaving(
    parent_pid: int,
    child_pid: int,
    *,
    parent_component_self_key: int = CHARACTERHOLDER_DESCRIPTOR_KEY,
    context_or_from: int = 0,
) -> bytes:
    # ClientWorld key09 is statically byte-read in SESSION_FINDINGS_2026-06-01
    # s2/s7: four scalar reader+0x28 fields, no string slot. For character
    # rows, the parent component is TAccount.CharacterHolder.
    body = (
        pack_u32(context_or_from)
        + pack_u32(parent_pid)
        + pack_u32(parent_component_self_key)
        + pack_u32(child_pid)
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

def build_clientworld_direct_call(
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
    # Selector 0 enters the generated/native direct receiver lane in
    # 0x00bee460. Unlike selector 1, there is no Variant argc prefix.
    # Direct result receivers expect field 1 to be an ObjectPtr/self gate,
    # followed by their logical payload fields.
    if fn_name not in CLIENTWORLD_METHOD_NIDS:
        raise ValueError(f"unknown ClientWorld direct-call method: {fn_name}")
    if from_pid is None:
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
        + (0).to_bytes(2, "little", signed=False)
        + b"".join(pack_rpc_variant(arg) for arg in args)
    )
    return build_lower_gss_body(0x06, body)

def build_loginserverresult_downcall(
    dispatch_pid: int,
    session_pid: int,
    controlled_pid: int,
    *,
    target_pid: int | None = None,
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
        target_pid=target_pid,
        object_id=object_id,
    )

def build_login_downcall(
    dispatch_pid: int,
    session_pid: int,
    *,
    target_pid: int | None = None,
    object_id: int | None = None,
) -> bytes:
    # s34q Phase 0: DownCall logIn takes [Session, num, num].
    return build_clientworld_remotecall(
        dispatch_pid,
        "logIn",
        [handle_ref(session_pid), 0, 0],
        target_pid=target_pid,
        object_id=object_id,
    )

def build_loginmaster_downcall(
    dispatch_pid: int,
    session_pid: int,
    *,
    target_pid: int | None = None,
    object_id: int | None = None,
) -> bytes:
    # s34q Phase 0: DownCall logInMaster takes [Session, num, num, num|string].
    return build_clientworld_remotecall(
        dispatch_pid,
        "logInMaster",
        [handle_ref(session_pid), 0, 0, 0],
        target_pid=target_pid,
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
    object_list_by_value_arg3: bool = False,
    object_list_as_objectvalue: bool = False,
    objectvalue_row_tail: bytes | None = None,
) -> bytes:
    # Lua TPlayerCache calls session:onLoadPlayersCacheResult(from, objectList).
    # Selector-1 key6 supplies the visible self and from slots before decoded
    # wire args (PROTOCOL_QUICKREF 3.1; 0x00bee658/0x00bee660/0x00bee71a).
    # Therefore the only explicit wire arg for this call is objectList.
    _ = session_pid  # kept for call-site symmetry; self comes from key6 routing.
    if object_list_oids is None:
        object_list_oids = [player_oid] if player_oid else []
    if object_list_as_objectvalue:
        # LATE-17 diagnostic: encode the char element as an INLINE tag-0x07
        # object-value (TPlayerDummy) so the client CONSTRUCTS it via bb5f40
        # instead of resolving a (missing) bare ref. One element per oid.
        # LATE-58 TEST: append the RE'd bb0060 component tail (native object-value
        # row subset) after the className so the client's +0x2c body-applier
        # (bafdc0) consumes a well-formed tail instead of throwing on the empty
        # className-only element (LATE-56). objectvalue_row_tail = the bytes; one
        # tail shared across oids for this single-char probe.
        if objectvalue_row_tail is not None:
            list_arg = [
                ("objectvalue_with_tail", "TPlayerDummy", objectvalue_row_tail)
                for _ in object_list_oids
            ]
        else:
            list_arg = [
                handle_objectlist_objectvalue("TPlayerDummy")
                for _ in object_list_oids
            ]
    elif object_list_as_arg2 or object_list_by_value_arg3:
        # Diagnostic flag name is historical. In the corrected shape this means
        # "use the object-value list element encoding for the sole objectList arg".
        list_arg = [
            handle_objectlist_nobject_ref(oid)
            for oid in object_list_oids
        ]
    else:
        list_arg = [handle_ref(oid) for oid in object_list_oids]
    args: list = [list_arg]
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

def build_onclientgetsrqlresult_downcall(
    dispatch_pid: int,
    *,
    selected_random_quests: dict | None = None,
    timestamp: int | None = None,
    target_pid: int | None = None,
    object_id: int | None = None,
) -> bytes:
    # Player_Quest.luo: OnServerGetSRQL(pid, SRQLDate) replies:
    # DownCall(pid, "OnClientGetSRQLResult", selectedRandomQuests or {}, now).
    # The key6 prefix supplies the implicit pid/from; explicit wire args are
    # the SRQL table and timestamp.
    if selected_random_quests is None:
        selected_random_quests = {}
    if timestamp is None:
        timestamp = int(time.time())
    return build_clientworld_remotecall(
        dispatch_pid,
        "OnClientGetSRQLResult",
        [selected_random_quests, timestamp],
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

def handle_objectlist_nobject_ref(
    object_oid: int,
    wrapper_type_name: str = "Object",
) -> tuple[str, int, str]:
    # ObjectPtr object-value lane. Current best-known live path uses the
    # replicated object id as scalar, then the minimal Object(size=0) NObject
    # tail. The historical "nobject_ref" name is retained for call-site
    # stability.
    return ("objectlist_nobject_ref", int(object_oid), wrapper_type_name)

def handle_objectlist_objectvalue(
    class_name: str = "TPlayerDummy",
    packvars_count: int = 0,
) -> tuple[str, str, int]:
    # LATE-16/17: the char-select row is an INLINE object-value (wire tag 0x07)
    # in the onLoadPlayers arg3 -- NOT a bare ObjectPtr ref (tag 0x0b, which
    # bef480 RESOLVES and misses -> nil -> empty list). bef480 tag-7 path
    # (codex_bb2580_payload_semantics.c:533-550): descriptor +0x1b reads the
    # body -> 005e5650 type-lookup(class_name) -> bb5f40 CONSTRUCTS the object
    # -> descriptor +0x2c applies the body. So the element carries the
    # object-value body (className + SerializeLua "size"/packvars tail), and the
    # client builds the char on the spot.
    return ("objectvalue", class_name, packvars_count)

def pack_nobject_type2_lua_tail(type_name: str, packvars_count: int = 0) -> bytes:
    # FUN_00bafdc0 calls 0x008336f0, which resolves object+0x0c through the
    # global type registry before writer+0x58 serializes that name blob.
    # Class.lua
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
    if isinstance(value, tuple) and len(value) == 2 and value[0] == "raw":
        # FAITHFUL HOST BRIDGE (LATE-129): emit pre-serialized bytes VERBATIM. The
        # faithful host (server/src) produces the M-REPL/BackupReplicate object bytes
        # from the shipped server Lua; the stub stays pure transport and ships them
        # unchanged. value[1] = the host's bytes for this element.
        return bytes(value[1])
    if (
        isinstance(value, tuple)
        and len(value) == 3
        and value[0] == "objectvalue"
    ):
        # Inline object-value, wire tag 0x07 (LATE-20, tag-0x07 body decompile).
        # CORRECTION over LATE-16/17: the +0x1b reader (claude_tag7_objectvalue_
        # decomp.c) consumes ONLY the class identity -- a length-prefixed string --
        # then bc5650(std::string) -> 005e5650 registry lookup -> bb5f40 constructs.
        # It does NOT read an inline tag4 "size"/tag3 count tail; the prior
        # pack_nobject_type2_lua_tail(...) tail derailed +0x1b's parse so the
        # extracted className was never "TPlayerDummy" (LATE-19: bb5f40 fired
        # 10895x, never with TPlayerDummy). SerializeLua "size"/packvars state is
        # applied LATER via descriptor +0x2c, not in this header. So Candidate A =
        # class name only. (NID 0x35c6b710 is goLua-internal, never on the wire.)
        class_name = value[1]
        return bytes([0x07]) + pack_string(class_name)
    if (
        isinstance(value, tuple)
        and len(value) == 3
        and value[0] == "objectvalue_with_tail"
    ):
        # LATE-58 TEST: tag-0x07 object-value = className + the bb0060 component
        # tail. LATE-20 proved the +0x1b reader stops after the className string;
        # this appends the bytes the +0x2c body-applier (bafdc0) then consumes
        # contiguously. If bb5f40 still fires with TPlayerDummy AND the throw
        # clears/moves, the tail is being read here (disambiguates LATE-20).
        class_name = value[1]
        tail = value[2] or b""
        return bytes([0x07]) + pack_string(class_name) + tail
    if (
        isinstance(value, tuple)
        and len(value) == 3
        and value[0] == "objectlist_nobject_ref"
    ):
        object_oid = int(value[1])
        wrapper_type_name = value[2]
        # 2026-06-26 (Claude, ObjectPtr value-contract RE): the engine's ObjectPtr
        # WRITER FUN_00c533f0 emits object+0x0c = NOBJECT_WRAPPER_KEY (nid"Object"
        # =0x160FF5A6) as the stage-1 descriptor/value-identity, NOT nid"ObjectPtr".
        # The old OBJECTPTR_DESCRIPTOR_KEY mismatched the writer AND the tail (which
        # already names "Object"), so bb2580 resolved against the wrong wrapper and
        # the row scalar nil'd. Align stage-1 to what the writer emits.
        # (RPC "handle" path below is unchanged — it resolves via manager+0x1c, not bb2580.)
        return (
            bytes([0x0B])
            + NOBJECT_WRAPPER_KEY.to_bytes(4, "little", signed=False)
            + object_oid.to_bytes(4, "little", signed=False)
            + pack_nobject_type2_lua_tail(wrapper_type_name, packvars_count=0)
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
