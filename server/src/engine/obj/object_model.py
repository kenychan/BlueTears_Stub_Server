"""M-OBJ — host-side GWorld object model (keystone).

The faithful host owns the *server-side* object tree that the real GIP/master owned: a
GWorld of component-bearing objects, each with a stable OID, that get replicated to the
client. The client is a replication proxy — it constructs its mirror of these objects when
they arrive over the master/tag-1 lane (see server/re/D1-c35140-dispatch.md). This module is
the host's authoritative representation of those objects, independent of the wire format
(M-CODEC) and the replication trigger (M-REPL).

Layout grounded in the RE'd client object (so host↔client stay 1:1):
  obj+0x08  class NID (FNV-ish name id; the client's `cls38`)
  obj+0x0c  OID (the replication/registry id; "pid")
  obj+0x14  component container  -> name -> Component   (the Lua-visible Com() map)
  obj+0x18  self/RPC map (key6)  -> populated on the client by native bd2cd0 at construct;
            on the host it is simply "the components that are RPC-addressable" (we mark it
            so M-REPL knows what must be present for getAllPlayers to count a row).

Char-select truth (SESSION_FINDINGS_2026-06-27):
  - TAccount has components {Session, CharacterManager, CharacterHolder, CSharedData}.
  - chars are leaves of the account's CharacterManager (getByRelation(account,"TAccount",
    "CharacterManager")); each char object carries a Player component (m_AccountId=accountId).
  - the client's CharacterManager+0x28 player list is populated natively from replication;
    getAllPlayers counts a char only if its obj+0x18 (self map) resolves (bd3660).

This module is pure host state — no wire bytes, no client patching.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterator, Optional


def class_nid(name: str) -> int:
    """goEngine class-name NID (the client's obj+0x08 `cls38`).

    Placeholder hash until the exact goLua_fix_string_for_nid algorithm is ported
    (see bc5e40 / claude_field_framing_writer_hash). Known NIDs are pinned in NID_OVERRIDES
    so host objects match the client's logged values (e.g. TPlayerDummy=0x35c6b710).
    """
    if name in NID_OVERRIDES:
        return NID_OVERRIDES[name]
    h = 0x811C9DC5
    for ch in name.encode("ascii"):
        h = ((h ^ ch) * 0x01000193) & 0xFFFFFFFF
    return h


# NIDs observed live in NpSlayer_exp.log CTORDIAG / bd2cd0 cls38 fields.
NID_OVERRIDES: dict[str, int] = {
    "TPlayerDummy": 0x35C6B710,
}


@dataclass
class Component:
    """A goEngine component (Com("Name") in the Lua templates). Holds NetVar fields."""
    name: str
    fields: dict[str, object] = field(default_factory=dict)
    rpc_addressable: bool = False  # mirrors "present in owner obj+0x18 self map"

    def set(self, key: str, value: object) -> "Component":
        self.fields[key] = value
        return self


@dataclass
class HostObject:
    """One GWorld object: class identity + OID + component container (+0x14) + self map (+0x18)."""
    class_name: str
    oid: int
    components: dict[str, Component] = field(default_factory=dict)   # obj+0x14
    self_map: dict[str, Component] = field(default_factory=dict)     # obj+0x18 (key6)

    @property
    def class_nid(self) -> int:
        return class_nid(self.class_name)

    def add_component(self, name: str, *, rpc_addressable: bool = False, **fields: object) -> Component:
        comp = Component(name=name, fields=dict(fields), rpc_addressable=rpc_addressable)
        self.components[name] = comp                # obj+0x14
        if rpc_addressable:
            self.self_map[name] = comp              # obj+0x18 (what bd2cd0 would register)
        return comp

    def com(self, name: str) -> Optional[Component]:
        return self.components.get(name)

    @property
    def is_resolvable(self) -> bool:
        """A row is counted by getAllPlayers only if its self/RPC map (obj+0x18) is non-empty
        (FUN_00bd3660 returns NULL on an empty obj+0x18 -> the char is skipped). This is the
        host-side mirror of Gate 2 — M-REPL must replicate the object so the client's bd2cd0
        populates this map."""
        return bool(self.self_map)


class CharacterManager:
    """Mirrors the client CharacterManager component. Its player list is the +0x28 std::list
    that getAllPlayers walks. On the host we hold the account's chars here so M-REPL can
    replicate them in the onLoadPlayersCacheResult shape."""

    def __init__(self) -> None:
        self.players: list[HostObject] = []   # the +0x28 list

    def add(self, char: HostObject) -> None:
        self.players.append(char)

    def get_all_players(self) -> list[HostObject]:
        """Host-side analogue of CharacterManager::getAllPlayers: only objects whose obj+0x18
        self map resolves are counted (the rest -> HasNoCharacter). Lets us assert, host-side,
        that a char we build *would* render before we ever touch the wire."""
        return [p for p in self.players if p.is_resolvable]


class GWorld:
    """The host object registry: OID -> object, with FindObject(oid) (the client's GWorld:
    FindObject the char-load paths require)."""

    def __init__(self) -> None:
        self._by_oid: dict[int, HostObject] = {}
        self._next_oid = 0x3EB  # char pids start where the stub's b4_char_pid does

    def issue_oid(self) -> int:
        oid = self._next_oid
        self._next_oid += 1
        return oid

    def create_object(self, class_name: str, oid: Optional[int] = None) -> HostObject:
        if oid is None:
            oid = self.issue_oid()
        obj = HostObject(class_name=class_name, oid=oid)
        self._by_oid[oid] = obj
        return obj

    def find_object(self, oid: int) -> Optional[HostObject]:
        return self._by_oid.get(oid)

    def __iter__(self) -> Iterator[HostObject]:
        return iter(self._by_oid.values())


def build_account_with_char(gworld: GWorld, account_id: int, *, char_name: str = "DemoWizard",
                            resolvable: bool = True) -> tuple[HostObject, HostObject]:
    """Build the faithful server-side tree: a TAccount with its 4 components, and one
    TPlayerDummy char enrolled in the account's CharacterManager, carrying a Player component
    bound to the account. Mirrors TPlayerCache:LoadPlayers / TAccount:Def.

    `resolvable=True` populates the char's obj+0x18 (what a real bd2cd0 construction yields);
    set False to model the current broken state (obj+0x18 empty -> getAllPlayers skips it)."""
    account = gworld.create_object("TAccount")
    account.add_component("Session", rpc_addressable=True)
    mgr_comp = account.add_component("CharacterManager", rpc_addressable=True)
    account.add_component("CharacterHolder")
    account.add_component("CSharedData")
    mgr = CharacterManager()
    mgr_comp.fields["_manager"] = mgr

    char = gworld.create_object("TPlayerDummy")
    char.add_component("Player", rpc_addressable=resolvable, m_AccountId=account_id)
    char.add_component("CStatus", rpc_addressable=resolvable, m_nGender=0)
    char.add_component("User", rpc_addressable=resolvable)
    char.add_component("CEquipment", m_strName=char_name)
    mgr.add(char)
    return account, char


if __name__ == "__main__":
    # Self-test: prove the host model reproduces the char-select gate logic before any wire.
    gw = GWorld()
    account, char = build_account_with_char(gw, account_id=1001, resolvable=True)
    print(f"account oid=0x{account.oid:x} class={account.class_name} "
          f"nid=0x{account.class_nid:x} comps={list(account.components)}")
    print(f"char    oid=0x{char.oid:x} class={char.class_name} "
          f"nid=0x{char.class_nid:x} resolvable={char.is_resolvable}")
    assert char.class_nid == 0x35C6B710, "TPlayerDummy NID must match the client's cls38"
    assert gw.find_object(char.oid) is char, "GWorld:FindObject must return the char"

    mgr = account.com("CharacterManager").fields["_manager"]
    rendered = mgr.get_all_players()
    print(f"getAllPlayers(resolvable char) -> {len(rendered)} row(s): "
          f"{[c.com('CEquipment').fields['m_strName'] for c in rendered]}")
    assert len(rendered) == 1, "a resolvable char must be counted (AddCharacter)"

    # Model the live-broken state: char with empty obj+0x18 -> skipped (HasNoCharacter).
    gw2 = GWorld()
    acc2, char2 = build_account_with_char(gw2, account_id=1001, resolvable=False)
    mgr2 = acc2.com("CharacterManager").fields["_manager"]
    assert mgr2.get_all_players() == [], "unresolvable char must be skipped (Gate 2)"
    print("getAllPlayers(unresolvable char) -> 0 rows (HasNoCharacter) — matches live wall")
    print("M-OBJ self-test PASSED")
