"""charactermanager — host operations for the CharacterManager / TPlayerCache roster tier.

Client flowmodel counterpart: ghidraRE/output/flowmodels/client/{charactermanager,tplayercache}.md.
Runs the SHIPPED server Lua (the host does NOT reimplement these — it executes the real classes):
  load_players            -> shipped TPlayerCache:LoadPlayers -> live objectList
  create_character_result -> shipped CharacterManager:serverCreateCharacterResult -> BackupReplicate
  replicate / char_load_to_wire -> the faithful M-REPL replication payload (tag-05 of tag-07)
Host OPERATIONS: they wire a capture session + drive the shipped RPCs over the engine substrate.
"""
from __future__ import annotations


def load_players(host, account_id: int):
    """Run the shipped TPlayerCache:LoadPlayers; returns the live Lua objectList."""
    captured = {}
    session = host.ctx._newtab()
    session["onLoadPlayersCacheResult"] = lambda _s, _f, ol: captured.__setitem__("ol", ol)
    host.tpc["LoadPlayers"](host.tpc, 0, account_id, session)
    return captured.get("ol")


def create_character_result(host, oid: int, info=None):
    """Run the shipped CharacterManager:serverCreateCharacterResult(oid, result, info).
    With info=nil it does GWorld:FindObject(oid) -> object:BackupReplicate(true) -> return."""
    host.cm["serverCreateCharacterResult"](host.cm, 0, oid, True, info)


def replicate(host, objectlist) -> bytes:
    """Serialize a live objectList into the faithful replication payload (M-REPL)."""
    return host.repl.onloadplayers_payload(objectlist)


def char_load_to_wire(host, account_id: int) -> bytes:
    """Full chain: LoadPlayers (shipped Lua) -> objectList -> faithful replication bytes."""
    return replicate(host, load_players(host, account_id))
