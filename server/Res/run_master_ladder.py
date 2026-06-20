#!/usr/bin/env python3
"""Run Claude's no-Frida master-connect close-timing ladder."""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import json
import re
import subprocess
import threading
import time
from datetime import datetime
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PYTHON = Path(r"C:\Users\keny-\anaconda3\python.exe")
FINDINGS = ROOT / "Res" / "RE_findings"
QUEUE_SCRIPT = ROOT / "Res" / "queue_admin_command.ps1"
CHANNEL_CLICK_CALIBRATION = ROOT / "Res" / "channel_click_calibration.json"
CHANNEL_CLICK_SEQUENCE = ROOT / "Res" / "channel_click_sequence.json"
RESULT_RE = re.compile(r"\[master-ladder-result\]\s+rung=(\S+)\s+alive_ms=(\d+)\s+class=(\S+)\s+event=(\S+)")

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
user32.SetForegroundWindow.argtypes = [wt.HWND]
user32.ShowWindow.argtypes = [wt.HWND, ctypes.c_int]
user32.BringWindowToTop.argtypes = [wt.HWND]
user32.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
user32.mouse_event.argtypes = [wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, ctypes.c_size_t]
user32.keybd_event.argtypes = [wt.BYTE, wt.BYTE, wt.DWORD, ctypes.c_size_t]

SW_RESTORE = 9
VK_RETURN = 0x0D
KEYEVENTF_KEYUP = 0x0002
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004


def queue_admin(command: str) -> None:
    subprocess.run(
        ["powershell.exe", "-ExecutionPolicy", "Bypass", "-File", str(QUEUE_SCRIPT), command],
        cwd=ROOT,
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def kill_game() -> None:
    queue_admin("kill_game")
    for name in ("NClient.exe", "GameMon.des", "GameGuard.des", "ncclient.exe"):
        subprocess.run(
            ["taskkill", "/F", "/IM", name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )


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


def tap_enter() -> None:
    user32.keybd_event(VK_RETURN, 0, 0, 0)
    time.sleep(0.03)
    user32.keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0)


def click_window_point(hwnd: int, rect: tuple[int, int, int, int], rx: float, ry: float) -> tuple[int, int]:
    left, top, right, bottom = rect
    x = int(left + (right - left) * rx)
    y = int(top + (bottom - top) * ry)
    user32.ShowWindow(wt.HWND(hwnd), SW_RESTORE)
    time.sleep(0.03)
    user32.BringWindowToTop(wt.HWND(hwnd))
    time.sleep(0.03)
    user32.SetForegroundWindow(wt.HWND(hwnd))
    time.sleep(0.03)
    user32.SetCursorPos(x, y)
    time.sleep(0.03)
    user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    time.sleep(0.03)
    user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    return x, y


def load_channel_click_points() -> list[tuple[float, float]]:
    points: list[tuple[float, float]] = []
    if CHANNEL_CLICK_CALIBRATION.exists():
        try:
            data = json.loads(CHANNEL_CLICK_CALIBRATION.read_text(encoding="utf-8"))
            rx = float(data["rx"])
            ry = float(data["ry"])
            if 0.0 <= rx <= 1.0 and 0.0 <= ry <= 1.0:
                points.append((rx, ry))
                print(f"[channel-clicker] learned point=({rx:.4f},{ry:.4f}) from {CHANNEL_CLICK_CALIBRATION.name}")
        except Exception as exc:
            print(f"[channel-clicker] ignored bad calibration: {exc}")
    points.extend([(0.50, 0.76), (0.70, 0.76), (0.74, 0.84)])
    return points


def load_channel_click_sequence() -> list[tuple[str, float, float]]:
    if not CHANNEL_CLICK_SEQUENCE.exists():
        return []
    try:
        data = json.loads(CHANNEL_CLICK_SEQUENCE.read_text(encoding="utf-8"))
    except Exception as exc:
        print(f"[channel-clicker] ignored bad sequence calibration: {exc}")
        return []
    out: list[tuple[str, float, float]] = []
    for idx, item in enumerate(data.get("points", [])):
        try:
            label = str(item.get("label") or f"step{idx + 1}")
            rx = float(item["rx"])
            ry = float(item["ry"])
        except Exception:
            continue
        if 0.0 <= rx <= 1.0 and 0.0 <= ry <= 1.0:
            out.append((label, rx, ry))
    if out:
        print(f"[channel-clicker] loaded {len(out)} sequence points from {CHANNEL_CLICK_SEQUENCE.name}")
    return out


def channel_clicker(stop: threading.Event) -> None:
    delays = (12.0, 22.0, 32.0)
    last = 0.0
    for attempt, delay in enumerate(delays, start=1):
        time.sleep(max(0.0, delay - last))
        last = delay
        if stop.is_set():
            return
        print(f"[channel-clicker] queue=elevated_click_channel full-sequence attempt={attempt}")
        queue_admin("click_channel")


def read_result(path: Path, rung: str) -> tuple[str, int, str, str, str] | None:
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    for line in text.splitlines():
        match = RESULT_RE.search(line)
        if match and match.group(1) == rung:
            return line, int(match.group(2)), match.group(3), match.group(4), text
    return None


def read_silent_result(path: Path) -> tuple[str, int | None, str, str, str] | None:
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    markers = (
        ("client->server master-phase payload", "client_master_payload"),
        ("decoded master-phase clear=", "decoded_master_clear"),
        ("decoded master-phase zlib frames=", "decoded_master_zlib"),
        ("master-phase payload not decoded", "master_payload_not_decoded"),
        ("no master-phase payload within 30s", "no_master_payload_30s"),
        ("master-phase EOF", "master_eof"),
    )
    for line in reversed(text.splitlines()):
        for marker, event in markers:
            if marker in line:
                return line, None, "silent", event, text
    return None


def stop_proc(proc: subprocess.Popen[str]) -> None:
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)


def run_probe(
    probe: str,
    stamp: str,
    click_channel: bool,
    admin_command: str,
    observe_seconds: float,
    leave_running_on_result: bool,
) -> dict:
    rung = probe
    stub_stdout = FINDINGS / f"master_ladder_{stamp}_{rung}.stdout.log"
    stub_stderr = FINDINGS / f"master_ladder_{stamp}_{rung}.stderr.log"
    stub_cmd = [
        str(PYTHON), str(ROOT / "Res" / "gss_stub_server_v4.py"),
        "--full-gss",
        "--master-probe", rung,
        "--master-crypto", "xor",
        "--master-envelope", "raw",
        "--character-template", "TPlayerDummy",
        "--signin-wait", "120",
        "-v",
    ]
    hold_seconds = 0.0
    if observe_seconds > 0:
        hold_seconds = max(observe_seconds + 30.0, 60.0)
    if leave_running_on_result:
        hold_seconds = max(hold_seconds, 300.0)
    if hold_seconds > 0:
        stub_cmd.extend(["--hold-after-result-seconds", f"{hold_seconds:.1f}"])

    print(f"[rung] {rung}")
    print(f"[stub] {' '.join(stub_cmd)}")
    kill_game()
    time.sleep(1.0)
    with stub_stdout.open("w", encoding="utf-8", errors="replace") as so, stub_stderr.open("w", encoding="utf-8", errors="replace") as se:
        stub = subprocess.Popen(stub_cmd, cwd=ROOT, stdout=so, stderr=se, text=True)
        time.sleep(1.5)
        if stub.poll() is not None:
            return {
                "rung": rung,
                "result": "stub-exit",
                "alive_ms": None,
                "class": "error",
                "event": f"stub_exit_{stub.returncode}",
                "stderr": str(stub_stderr),
            }

        click_stop = threading.Event()
        click_thread: threading.Thread | None = None
        if click_channel:
            click_thread = threading.Thread(target=channel_clicker, args=(click_stop,), daemon=True)
            click_thread.start()

        queue_admin(admin_command)
        deadline = time.time() + 150.0
        found = None
        try:
            while time.time() < deadline:
                if rung == "silent":
                    found = read_silent_result(stub_stderr)
                else:
                    found = read_result(stub_stderr, rung)
                if found is not None:
                    break
                if stub.poll() is not None:
                    break
                time.sleep(0.5)
        finally:
            click_stop.set()
            if click_thread is not None:
                click_thread.join(timeout=1.0)
            if found is not None and observe_seconds > 0:
                print(f"[observe] result seen; leaving client/stub alive for {observe_seconds:.1f}s")
                time.sleep(observe_seconds)
            if not (found is not None and leave_running_on_result):
                kill_game()
                stop_proc(stub)
            else:
                print(f"[observe] leaving client/stub running; stub_pid={stub.pid}")

    if found is None:
        return {
            "rung": rung,
            "result": "timeout",
            "alive_ms": None,
            "class": "error",
            "event": "no_master_ladder_result",
            "stderr": str(stub_stderr),
        }
    line, alive_ms, klass, event, _text = found
    return {
        "rung": rung,
        "result": "ok",
        "alive_ms": alive_ms,
        "class": klass,
        "event": event,
        "line": line,
        "stderr": str(stub_stderr),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--stop", type=int, default=4)
    ap.add_argument(
        "--mode",
        choices=(
            "ladder",
            "issuepid-diagnostics",
            "corrected-ladder",
            "ordered-empty",
            "ordered-empty-challenge",
            "tclient-wait-connect",
            "tclient-batch-wait-connect",
            "connector-connect-probe",
            "ordered-empty-batch",
            "ordered-empty-loginresult",
            "ordered-empty-loginseq",
            "ordered-empty-loginseq-sessiontail",
            "ordered-empty-loginclientresult-sessiontail",
            "ordered-empty-loginclientresult-onloadplayers",
            "ordered-empty-onloadplayerdata-loginclientresult",
            "ordered-empty-onloadplayerdata-paced-loginclientresult",
            "ordered-b4-onloadplayerdata-paced-loginclientresult",
            "ordered-b4-b5-no-onload",
            "ordered-b4-b5-paced-after-loginclientresult",
            "ordered-b4-b5-empty-select",
            "ordered-b4-b5-finish0",
            "ordered-b4-b5-prelogin-finish",
            "ordered-b4-b5-prelogin-finish-objectlist",
            "ordered-b4-b5-prelogin-double-onload-finish",
            "ordered-b4-b5-prelogin-onload-listarg-finish",
            "ordered-b4-b5-prelogin-double-onload-arg3-finish",
            "ordered-taccount-direct-finish-empty",
            "ordered-empty-onloadplayers",
            "ordered-empty-loginrpc-bootstrap",
            "ordered-empty-loginrpc-bootstrap-goluaadd",
            "silent-master",
        ),
        default="ladder",
    )
    ap.add_argument("--no-click-channel", action="store_true")
    ap.add_argument(
        "--observe-seconds",
        type=float,
        default=0.0,
        help="after a result marker, keep the client/stub alive this many seconds",
    )
    ap.add_argument(
        "--leave-running-on-result",
        action="store_true",
        help="do not kill the client or stub after a result marker",
    )
    ap.add_argument(
        "--admin-command",
        choices=("launch_original_stub", "trace_original_login", "trace_key6_gate", "trace_key6_min", "trace_login_bootstrap"),
        default="launch_original_stub",
        help="admin launcher command used to start the client",
    )
    args = ap.parse_args()

    FINDINGS.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    summary_path = FINDINGS / f"master_ladder_{stamp}_summary.log"
    if args.mode == "issuepid-diagnostics":
        probes = ["diag-d2", "diag-d1", "diag-d3"]
        summary_path = FINDINGS / f"master_issuepid_diag_{stamp}_summary.log"
    elif args.mode == "corrected-ladder":
        probes = ["ladder2-t2", "ladder2-t3", "ladder2-t4"]
        summary_path = FINDINGS / f"master_ladder2_{stamp}_summary.log"
    elif args.mode == "ordered-empty":
        probes = ["ordered-empty"]
        summary_path = FINDINGS / f"master_ordered_empty_{stamp}_summary.log"
    elif args.mode == "ordered-empty-challenge":
        probes = ["ordered-empty-challenge"]
        summary_path = FINDINGS / f"master_ordered_empty_challenge_{stamp}_summary.log"
    elif args.mode == "tclient-wait-connect":
        probes = ["tclient-wait-connect"]
        summary_path = FINDINGS / f"master_tclient_wait_connect_{stamp}_summary.log"
    elif args.mode == "tclient-batch-wait-connect":
        probes = ["tclient-batch-wait-connect"]
        summary_path = FINDINGS / f"master_tclient_batch_wait_connect_{stamp}_summary.log"
    elif args.mode == "connector-connect-probe":
        probes = ["connector-connect-probe"]
        summary_path = FINDINGS / f"master_connector_connect_probe_{stamp}_summary.log"
    elif args.mode == "ordered-empty-batch":
        probes = ["ordered-empty-batch"]
        summary_path = FINDINGS / f"master_ordered_empty_batch_{stamp}_summary.log"
    elif args.mode == "ordered-empty-loginresult":
        probes = ["ordered-empty-loginresult"]
        summary_path = FINDINGS / f"master_ordered_empty_loginresult_{stamp}_summary.log"
    elif args.mode == "ordered-empty-loginseq":
        probes = ["ordered-empty-loginseq"]
        summary_path = FINDINGS / f"master_ordered_empty_loginseq_{stamp}_summary.log"
    elif args.mode == "ordered-empty-loginseq-sessiontail":
        probes = ["ordered-empty-loginseq-sessiontail"]
        summary_path = FINDINGS / f"master_ordered_empty_loginseq_sessiontail_{stamp}_summary.log"
    elif args.mode == "ordered-empty-loginclientresult-sessiontail":
        probes = ["ordered-empty-loginclientresult-sessiontail"]
        summary_path = FINDINGS / f"master_ordered_empty_loginclientresult_sessiontail_{stamp}_summary.log"
    elif args.mode == "ordered-empty-loginclientresult-onloadplayers":
        probes = ["ordered-empty-loginclientresult-onloadplayers"]
        summary_path = FINDINGS / f"master_ordered_empty_loginclientresult_onloadplayers_{stamp}_summary.log"
    elif args.mode == "ordered-empty-onloadplayerdata-loginclientresult":
        probes = ["ordered-empty-onloadplayerdata-loginclientresult"]
        summary_path = FINDINGS / f"master_ordered_empty_onloadplayerdata_loginclientresult_{stamp}_summary.log"
    elif args.mode == "ordered-empty-onloadplayerdata-paced-loginclientresult":
        probes = ["ordered-empty-onloadplayerdata-paced-loginclientresult"]
        summary_path = FINDINGS / f"master_ordered_empty_onloadplayerdata_paced_loginclientresult_{stamp}_summary.log"
    elif args.mode == "ordered-b4-onloadplayerdata-paced-loginclientresult":
        probes = ["ordered-b4-onloadplayerdata-paced-loginclientresult"]
        summary_path = FINDINGS / f"master_ordered_b4_onloadplayerdata_paced_loginclientresult_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-no-onload":
        probes = ["ordered-b4-b5-no-onload"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_no_onload_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-paced-after-loginclientresult":
        probes = ["ordered-b4-b5-paced-after-loginclientresult"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_paced_after_loginclientresult_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-empty-select":
        probes = ["ordered-b4-b5-empty-select"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_empty_select_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-finish0":
        probes = ["ordered-b4-b5-finish0"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_finish0_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-prelogin-finish":
        probes = ["ordered-b4-b5-prelogin-finish"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_prelogin_finish_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-prelogin-finish-objectlist":
        probes = ["ordered-b4-b5-prelogin-finish-objectlist"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_prelogin_finish_objectlist_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-prelogin-double-onload-finish":
        probes = ["ordered-b4-b5-prelogin-double-onload-finish"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_prelogin_double_onload_finish_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-prelogin-onload-listarg-finish":
        probes = ["ordered-b4-b5-prelogin-onload-listarg-finish"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_prelogin_onload_listarg_finish_{stamp}_summary.log"
    elif args.mode == "ordered-b4-b5-prelogin-double-onload-arg3-finish":
        probes = ["ordered-b4-b5-prelogin-double-onload-arg3-finish"]
        summary_path = FINDINGS / f"master_ordered_b4_b5_prelogin_double_onload_arg3_finish_{stamp}_summary.log"
    elif args.mode == "ordered-taccount-direct-finish-empty":
        probes = ["ordered-taccount-direct-finish-empty"]
        summary_path = FINDINGS / f"master_ordered_taccount_direct_finish_empty_{stamp}_summary.log"
    elif args.mode == "ordered-empty-onloadplayers":
        probes = ["ordered-empty-onloadplayers"]
        summary_path = FINDINGS / f"master_ordered_empty_onloadplayers_{stamp}_summary.log"
    elif args.mode == "ordered-empty-loginrpc-bootstrap":
        probes = ["ordered-empty-loginrpc-bootstrap"]
        summary_path = FINDINGS / f"master_ordered_empty_loginrpc_bootstrap_{stamp}_summary.log"
    elif args.mode == "ordered-empty-loginrpc-bootstrap-goluaadd":
        probes = ["ordered-empty-loginrpc-bootstrap-goluaadd"]
        summary_path = FINDINGS / f"master_ordered_empty_loginrpc_bootstrap_goluaadd_{stamp}_summary.log"
    elif args.mode == "silent-master":
        probes = ["silent"]
        summary_path = FINDINGS / f"master_silent_{stamp}_summary.log"
    else:
        probes = [f"ladder-t{i}" for i in range(args.start, args.stop + 1)]
    results = []
    for probe in probes:
        result = run_probe(
            probe,
            stamp,
            not args.no_click_channel,
            args.admin_command,
            args.observe_seconds,
            args.leave_running_on_result,
        )
        results.append(result)
        print(f"[result] {result}")

    with summary_path.open("w", encoding="utf-8", errors="replace") as f:
        for result in results:
            f.write(str(result) + "\n")
    print(f"[summary] {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
