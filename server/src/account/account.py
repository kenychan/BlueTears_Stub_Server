"""account — host operations for the TAccount tier (account + CharacterManager children + persist).

Client flowmodel counterpart: ghidraRE/output/flowmodels/client/taccount.md. Builds the account's
CharacterManager children (via char.build.new_char), registers the account->children relation the
shipped getByRelation resolves, and (with M-DB) persists/rehydrates each char so a fresh host reloads
the account from disk. Host OPERATIONS over the engine substrate.
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))  # server/src


def setup_account(host, account_id: int, char_specs: list[tuple]):
    """char_specs = list of (oid, level, name). Builds the account's CharacterManager children.
    With M-DB attached, also persists each char (xData = its M-CODEC body)."""
    from char.build import new_char
    chars = [new_char(host, oid, account_id, lvl, nm) for (oid, lvl, nm) in char_specs]
    host.ctx.add_account(account_id, chars)
    if host.db is not None:
        host.db.add_account(account_id)
        for (oid, lvl, nm), c in zip(char_specs, chars):
            blob = host.codec.pack(c, is_backup=True).bytes()   # the serialized player blob
            host.db.add_char(account_id, name=nm, level=lvl, oid=oid,
                             class_name=host._classname, xdata=blob)
        host.db.save()
    return chars


def load_account(host, account_id: int):
    """Rehydrate an account's chars from M-DB into live host objects (stateful reload)."""
    if host.db is None:
        raise RuntimeError("load_account requires a CharStore (db=)")
    from char.build import new_char
    recs = host.db.get_chars(account_id)
    chars = [new_char(host, r["oid"], account_id, r["level"], r["name"]) for r in recs]
    host.ctx.add_account(account_id, chars)
    return chars
