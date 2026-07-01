"""The clean E1/E2 login sequence — the one live lane (LATE-144 decomposition).

Byte-exact extraction from Res/gss_stub_server_v4.py: master_timing_packets emits the
faithful clean login (TClient control object + faithful 4-Com TAccount + login RPCs +
empty onLoadPlayers -> Client_FinishLoadingCharacter[True,0] -> UIIntro:ShowJobClassSelection).

Char delivery (E3) is NOT here: it is the HOST-REPLICATION hook (server/src FaithfulHost emits
the BackupReplicate bytes, shipped via pack_rpc_variant's ("raw",bytes) path) — pending the
replication ENVELOPE (NORTH_STAR §4). The legacy stub carried ~30 dead research rungs; those are
pruned in this modular start point, so MASTER_TIMING_PROBES is the single live rung.
"""
from __future__ import annotations

from .packets import (
    build_clientworld_issuepid,
    build_clientworld_newobject,
    build_clientworld_remotecall,
    build_loginclientresult_downcall,
    build_onloadplayers_downcall,
    handle_ref,
)

# The one live rung; the ~30 dead research rungs from the legacy stub are pruned here.
MASTER_DB_MAGE_SELECT_ENTER_PROBES = ("ordered-db-mage-select-enter",)
MASTER_TIMING_PROBES = MASTER_DB_MAGE_SELECT_ENTER_PROBES


def master_timing_packets(
    rung: str,
    account_pid: int = 1001,
    account_name: str = "codex",
    bundle: dict | None = None,
) -> list[bytes]:
    if rung not in MASTER_TIMING_PROBES:
        raise ValueError(f"not a master timing probe: {rung}")
    # ── CLEAN E1/E2 LANE (LATE-144 decomposition) — the ONE live rung ────────────────
    # Faithful clean login: TClient(control) + faithful TAccount(4-Com) + login RPCs,
    # ZERO chars -> logInClientResult -> empty onLoadPlayers (count=0) ->
    # Client_FinishLoadingCharacter[True,0] -> UIIntro:ShowJobClassSelection (E1/E2,
    # LATE-138/140). Char delivery (E3) is the HOST-REPLICATION hook: server/src
    # FaithfulHost emits the BackupReplicate bytes, shipped verbatim via
    # pack_rpc_variant's ("raw",bytes) path — pending the replication ENVELOPE
    # (NORTH_STAR §4).
    if rung in MASTER_DB_MAGE_SELECT_ENTER_PROBES:
        client_pid = account_pid                  # 1001 = TClient (control object)
        session_account_pid = account_pid + 1     # 1002 = TAccount (login target; LATE-138)
        return [
            build_clientworld_issuepid(0, client_pid),
            build_clientworld_newobject(client_pid, "TClient"),
            build_clientworld_remotecall(
                client_pid, "SetControlObject", [handle_ref(client_pid)]),
            build_clientworld_issuepid(0, session_account_pid),
            build_clientworld_newobject(
                session_account_pid, "TAccount",
                include_taccount_session_tail=True, account_name=account_name),
            build_clientworld_remotecall(
                session_account_pid, "SetControlObject", [handle_ref(session_account_pid)]),
            build_clientworld_remotecall(
                session_account_pid, "signInClientResult", [1, 1, 1]),
            build_loginclientresult_downcall(
                session_account_pid, plr_id=session_account_pid,
                target_pid=session_account_pid, object_id=session_account_pid),
            build_onloadplayers_downcall(
                session_account_pid, session_account_pid,
                target_pid=session_account_pid, object_id=session_account_pid,
                object_list_oids=[]),
            build_clientworld_remotecall(
                session_account_pid, "Client_FinishLoadingCharacter", [True, 0],
                target_pid=session_account_pid, object_id=session_account_pid),
        ]
    raise ValueError(
        f"rung {rung!r} is a pruned research lane; only "
        f"ordered-db-mage-select-enter (clean E1/E2 login) is supported"
    )


def classify_alive_ms(alive_ms: int) -> str:
    if alive_ms < 2000:
        return "fast"
    if alive_ms >= 50000:
        return "idle"
    return "mid"


def select_db_mage_character(bundle: dict) -> dict:
    selected = next(
        (
            char for char in bundle["characters"]
            if char.get("job_class") == "JobClassMagician"
        ),
        bundle["characters"][0] if bundle["characters"] else None,
    )
    if selected is None:
        raise ValueError("DB mage select/enter probe requires one character")
    return selected
