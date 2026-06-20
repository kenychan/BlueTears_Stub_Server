"""Click the channel/enter UI once from the elevated launcher context."""

from __future__ import annotations

import argparse
import time
from run_master_ladder import (
    click_window_point,
    find_game_window,
    load_channel_click_points,
    load_channel_click_sequence,
    tap_enter,
)


def find_game_window_until(timeout: float):
    deadline = time.monotonic() + timeout
    while True:
        window = find_game_window()
        if window or time.monotonic() >= deadline:
            return window
        time.sleep(0.25)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--attempt", type=int, default=1)
    ap.add_argument("--wait-window", type=float, default=18.0)
    ap.add_argument("--pre-delay", type=float, default=3.0)
    ap.add_argument("--step-delay", type=float, default=0.8)
    args = ap.parse_args()

    window = find_game_window_until(args.wait_window)
    if not window:
        print("[click-channel] no Gamebryo Application window")
        return 2

    hwnd, rect = window
    sequence = load_channel_click_sequence()
    if sequence:
        print(
            f"[click-channel] hwnd={hwnd} rect={rect} attempt={args.attempt} "
            f"running_full_sequence={len(sequence)} pre_delay={args.pre_delay:.1f}s"
        )
        time.sleep(max(0.0, args.pre_delay))
        for idx, (label, rx, ry) in enumerate(sequence, start=1):
            window = find_game_window_until(2.0)
            if not window:
                print(f"[click-channel] window disappeared before step={idx} label={label}")
                return 3
            hwnd, rect = window
            tap_enter_after = label.endswith("confirm")
            print(
                f"[click-channel] step={idx}/{len(sequence)} label={label} "
                f"point=({rx:.4f},{ry:.4f})"
            )
            x, y = click_window_point(hwnd, rect, rx, ry)
            print(f"[click-channel] clicked screen=({x},{y}) tap_enter={int(tap_enter_after)}")
            if tap_enter_after:
                tap_enter()
            time.sleep(max(0.0, args.step_delay))
        return 0
    else:
        points = load_channel_click_points()
        rx, ry = points[(max(1, args.attempt) - 1) % len(points)]
        print(f"[click-channel] hwnd={hwnd} rect={rect} attempt={args.attempt} point=({rx:.4f},{ry:.4f})")
        tap_enter_after = True
    x, y = click_window_point(hwnd, rect, rx, ry)
    print(f"[click-channel] clicked screen=({x},{y}) tap_enter={int(tap_enter_after)}")
    if tap_enter_after:
        tap_enter()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
