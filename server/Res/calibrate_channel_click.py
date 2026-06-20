"""Record the real zone/channel click sequence for run_master_ladder.py."""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import json
import time
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Res" / "channel_click_calibration.json"
SEQ_OUT = ROOT / "Res" / "channel_click_sequence.json"
VK_LBUTTON = 0x01
DEFAULT_LABELS = ["zone_select", "zone_confirm", "channel_select", "channel_confirm"]

user32 = ctypes.WinDLL("user32", use_last_error=True)
try:
    user32.SetProcessDPIAware()
except Exception:
    pass


class RECT(ctypes.Structure):
    _fields_ = [
        ("left", wt.LONG),
        ("top", wt.LONG),
        ("right", wt.LONG),
        ("bottom", wt.LONG),
    ]


class POINT(ctypes.Structure):
    _fields_ = [("x", wt.LONG), ("y", wt.LONG)]


EnumWindowsProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows.argtypes = [EnumWindowsProc, wt.LPARAM]
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(RECT)]
user32.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(POINT)]
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.GetCursorPos.argtypes = [ctypes.POINTER(POINT)]
user32.GetAsyncKeyState.argtypes = [ctypes.c_int]


def find_game_window() -> tuple[int, tuple[int, int, int, int]] | None:
    found: list[tuple[int, tuple[int, int, int, int]]] = []

    @EnumWindowsProc
    def enum_proc(hwnd, _):
        if not user32.IsWindowVisible(hwnd):
            return True
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(hwnd, buf, len(buf))
        if buf.value != "Gamebryo Application":
            return True
        rect = RECT()
        if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
            return True
        origin = POINT(0, 0)
        if not user32.ClientToScreen(hwnd, ctypes.byref(origin)):
            return True
        found.append((
            int(hwnd),
            (
                int(origin.x),
                int(origin.y),
                int(origin.x + rect.right - rect.left),
                int(origin.y + rect.bottom - rect.top),
            ),
        ))
        return True

    user32.EnumWindows(enum_proc, 0)
    return found[0] if found else None


def capture_click(label: str) -> dict:
    print(f"[calibrate] Waiting for click: {label}")
    was_down = bool(user32.GetAsyncKeyState(VK_LBUTTON) & 0x8000)
    while True:
        window = find_game_window()
        down = bool(user32.GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        if down and not was_down and window:
            _, rect = window
            pt = POINT()
            user32.GetCursorPos(ctypes.byref(pt))
            left, top, right, bottom = rect
            if left <= pt.x <= right and top <= pt.y <= bottom:
                rx = (pt.x - left) / max(1, right - left)
                ry = (pt.y - top) / max(1, bottom - top)
                print(f"[calibrate] {label}=({rx:.4f},{ry:.4f}) screen=({pt.x},{pt.y})")
                return {
                    "label": label,
                    "rx": rx,
                    "ry": ry,
                    "x": int(pt.x),
                    "y": int(pt.y),
                    "rect": list(rect),
                }
        was_down = down
        time.sleep(0.02)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=len(DEFAULT_LABELS))
    ap.add_argument("--single", action="store_true", help="record only the legacy single click point")
    args = ap.parse_args()

    print("[calibrate] Waiting for Gamebryo Application window.")
    if args.single:
        labels = ["channel_confirm"]
    else:
        count = max(1, args.count)
        labels = [
            DEFAULT_LABELS[i] if i < len(DEFAULT_LABELS) else f"step{i + 1}"
            for i in range(count)
        ]
    print("[calibrate] Click each requested UI point inside the game window; Ctrl+C cancels.")
    points = [capture_click(label) for label in labels]
    recorded_at = datetime.now().isoformat(timespec="seconds")
    if points:
        legacy = dict(points[-1])
        legacy["recorded_at"] = recorded_at
        legacy["source"] = "last point from channel_click_sequence calibration"
        OUT.write_text(json.dumps(legacy, indent=2) + "\n", encoding="utf-8")
    sequence = {
        "version": 1,
        "recorded_at": recorded_at,
        "points": points,
    }
    SEQ_OUT.write_text(json.dumps(sequence, indent=2) + "\n", encoding="utf-8")
    print(f"[calibrate] saved {SEQ_OUT}")
    if points:
        print(f"[calibrate] saved legacy last point {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
