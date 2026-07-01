#!/usr/bin/env py
"""Automated de-risk run + verdict for the HEAVY-host build loop.

One command does the whole "observe" half of a build->test iteration:
  pre-flight (kill lingering stub, check launcher) -> run the live harness
  -> analyze the fresh NpSlayer_exp.log delta -> print a crisp PASS/FAIL verdict.

Decisive signals (see server/re/D1-c35140-dispatch.md):
  EARLY_REALIZE        template realized before onLoadPlayers
  c35140 HIT           the char reached the tag-1 master construct lane (vs armed-only)
  char obj+0x18 build  a bd2cd0 CTORDIAG for a char NID OUTSIDE the realize chunk
  AddCharacter         a row was projected (vs HasNoCharacter = empty list)

Usage:
  py server/tools/derisk_run.py                 # live run + analyze + verdict
  py server/tools/derisk_run.py --analyze FILE  # analyze an existing fresh-log slice (no run)
  py server/tools/derisk_run.py --since N        # analyze the live log tail from line N (no run)
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LOG = os.path.join(REPO, "TW", "Bin", "NpSlayer_exp.log")
HARNESS = ["py", "Res/run_master_ladder.py", "--mode",
           "ordered-db-mage-select-enter", "--admin-command", "launch_original_stub"]

CHAR_NIDS = {"35c6b710"}  # TPlayerDummy (extend as we learn TPlayer=0x9f4a5bcd etc.)


def preflight() -> None:
    # best-effort: kill a stub still listening on 5004 (OSError-10048 guard)
    try:
        out = subprocess.run(["netstat", "-ano"], capture_output=True, text=True).stdout
        pids = {ln.split()[-1] for ln in out.splitlines() if ":5004" in ln and "LISTEN" in ln.upper()}
        for pid in pids:
            subprocess.run(["taskkill", "/F", "/PID", pid], capture_output=True)
            print(f"[preflight] killed lingering stub pid={pid} on :5004")
    except Exception as e:  # noqa: BLE001
        print(f"[preflight] stub-port check skipped: {e}")
    hb = os.path.join(REPO, "Res", "AdminLauncher", "launcher_heartbeat.txt")
    if os.path.exists(hb):
        import time
        age = time.time() - os.path.getmtime(hb)
        state = "FRESH" if age <= 15 else f"STALE ({age:.0f}s)"
        print(f"[preflight] launcher heartbeat: {state} -- {open(hb).read().strip()}")
        if age > 15:
            print("[preflight] WARNING: launcher stale -> game may not launch. "
                  "Run Res/start_admin_launcher.ps1 (pops UAC) first.")
    else:
        print("[preflight] WARNING: no launcher heartbeat -> run Res/start_admin_launcher.ps1")


def analyze(lines: list[str]) -> dict:
    text_idx = list(enumerate(lines))
    early_start = next((i for i, l in text_idx if "EARLY_REALIZE start" in l), None)
    early_done = next((i for i, l in text_idx if "EARLY_REALIZE done" in l), None)
    realize_lo = early_start if early_start is not None else -1
    realize_hi = early_done if early_done is not None else -1

    c35140_hit = sum(1 for l in lines
                     if ("c35140" in l or "TAG1_KEY03" in l) and "armed int3" not in l
                     and ("REACHED" in l or "HIT" in l or "TAG1_KEY03 hit" in l))
    bff990_hit = sum(1 for l in lines if "REACHED 0x00bff990" in l)

    ctor = re.compile(r"CTORDIAG bd2cd0 obj=0x([0-9a-f]+) cls38=0x([0-9a-f]+)")
    char_builds_in_realize = 0
    char_builds_wire = 0
    for i, l in text_idx:
        m = ctor.search(l)
        if not m:
            continue
        nid = m.group(2)
        if nid in CHAR_NIDS:
            if realize_lo <= i <= realize_hi:
                char_builds_in_realize += 1
            else:
                char_builds_wire += 1

    add_char = sum(1 for l in lines if re.search(r"AddCharacter|updateCharacter_AddChar", l))
    has_no = sum(1 for l in lines if re.search(r"HasNoCharacter|updateCharacter_HasNo", l))
    loadlist_empty = sum(1 for l in lines if "LOADLIST_EMPTY" in l)

    return {
        "lines": len(lines),
        "early_realize": early_start is not None and early_done is not None,
        "c35140_hit": c35140_hit,
        "bff990_hit": bff990_hit,
        "char_build_realize_chunk": char_builds_in_realize,
        "char_build_wire": char_builds_wire,
        "AddCharacter": add_char,
        "HasNoCharacter": has_no,
        "LOADLIST_EMPTY": loadlist_empty,
    }


def verdict(a: dict) -> None:
    print("\n=== DE-RISK VERDICT " + "=" * 40)
    for k in ("lines", "early_realize", "c35140_hit", "bff990_hit",
              "char_build_realize_chunk", "char_build_wire",
              "AddCharacter", "HasNoCharacter", "LOADLIST_EMPTY"):
        print(f"  {k:24} = {a[k]}")
    constructed = a["char_build_wire"] > 0 or a["c35140_hit"] > 0
    row = a["AddCharacter"] > 0
    print("-" * 60)
    if row and constructed:
        print("  RESULT: *** MILESTONE *** char constructed AND row projected.")
    elif constructed:
        print("  RESULT: char CONSTRUCTED via the wire (c35140/obj+0x18) but no row yet "
              "-> next gate (binding / body-load).")
    else:
        print("  RESULT: construction wall HOLDS -- char never reached the tag-1 construct "
              "lane (c35140=0 hits / no wire obj+0x18). "
              f"{'HasNoCharacter' if a['HasNoCharacter'] else 'no row'} -> still HEAVY-host-blocked.")
    print("=" * 60)


def run_live() -> list[str]:
    preflight()
    base = sum(1 for _ in open(LOG, encoding="latin-1")) if os.path.exists(LOG) else 0
    print(f"[run] baseline log lines={base}; launching harness ...")
    subprocess.run(HARNESS, cwd=REPO)
    with open(LOG, encoding="latin-1") as f:
        all_lines = f.read().splitlines()
    fresh = all_lines[base:]
    print(f"[run] fresh lines={len(fresh)}")
    return fresh


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--analyze", metavar="FILE", help="analyze an existing fresh-log slice")
    ap.add_argument("--since", type=int, help="analyze the live log tail from this line number")
    args = ap.parse_args()
    if args.analyze:
        lines = open(args.analyze, encoding="latin-1").read().splitlines()
    elif args.since is not None:
        lines = open(LOG, encoding="latin-1").read().splitlines()[args.since:]
    else:
        lines = run_live()
    verdict(analyze(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
