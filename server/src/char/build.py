"""char build — host operations for the char object (Player/CStatusPlayer/User tier).

Client flowmodel counterpart: ghidraRE/output/flowmodels/client/{player,cstatusplayer,user}.md.
Builds a host char Lua object (identity NetVars + a Player component carrying m_AccountId) and
registers it in GWorld so the shipped CharacterManager:serverCreateCharacterResult's
GWorld:FindObject(oid) resolves it. `BackupReplicate(true)` serializes the object via M-CODEC and
records the bytes a transport would carry (the E3 residency payload). These are host OPERATIONS over
the engine substrate (server/src/engine/*); the object itself is a live Lua table.
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))  # server/src

from engine.codec.wire import WireBuffer  # noqa: E402


def new_char(host, oid: int, account_id: int, level: int = 1, name: str = ""):
    cls = host.lua.g[host._classname]
    c = cls["new"](cls)
    c["m_Level"] = level
    c["m_Name"] = name
    pcls = host.lua.g["HostPlayerComp"]          # the Player component (carries m_AccountId)
    player = pcls["new"](pcls)
    player["m_AccountId"] = account_id
    c["Player"] = player                          # v.Player resolves for the shipped LoadPlayers
    c["BackupReplicate"] = lambda _self, is_backup=True: backup_replicate(host, int(oid), c)
    host.ctx.gworld[int(oid)] = c                  # GWorld:FindObject(oid) resolves it
    return c


def backup_replicate(host, oid: int, obj):
    buf = WireBuffer()
    host.codec.pack_value(buf, obj)               # tag-0x07 className + Class.Pack body
    host.replicated.append((oid, buf.bytes()))
    return None
