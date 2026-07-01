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
NPSLAYER_EXP_LOG = ROOT / "TW" / "Bin" / "NpSlayer_exp.log"
RESULT_RE = re.compile(r"\[master-ladder-result\]\s+rung=(\S+)\s+alive_ms=(\d+)\s+class=(\S+)\s+event=(\S+)")
CLICK_LOG_RE = re.compile(
    r"(?:UI_TEXT.*(?:伺服|頻道|確認|Server|Channel|Confirm))"
)
LIVE_LOG_RE = re.compile(
    r"(CODEX_|UI_|UI_TEXT|CHARMGR|LOADLIST|POSTFINISH|FINISH|"
    r"Server_Finish|Master_Finish|SESSION_E0|FUSER_STATE|EXCEPTION|"
    r"LSR_GATE|LOGIN_THROW_SITE|TAG0B|OBJLIST|ONLOAD|transition|"
    r"create|select)",
    re.IGNORECASE,
)
GSS_CHANNEL_LIST_SENT_RE = re.compile(r"sent full-gss stage2 0x1003\+0x1004")
GSS_CHANNEL_SELECTED_RE = re.compile(r"decoded 0x4005")
GAME_CLIENT_IMAGES = ("NClient.exe", "PunchMonster.exe")
CLIENT_KILL_STABLE_SECONDS = 2.0
CLIENT_KILL_TIMEOUT_SECONDS = 8.0

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
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
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(RECT)]
user32.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(POINT)]
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.SetForegroundWindow.argtypes = [wt.HWND]
user32.ShowWindow.argtypes = [wt.HWND, ctypes.c_int]
user32.BringWindowToTop.argtypes = [wt.HWND]
user32.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
user32.mouse_event.argtypes = [wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, ctypes.c_size_t]
user32.keybd_event.argtypes = [wt.BYTE, wt.BYTE, wt.DWORD, ctypes.c_size_t]
kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
kernel32.QueryFullProcessImageNameW.restype = wt.BOOL
kernel32.CloseHandle.argtypes = [wt.HANDLE]

SW_RESTORE = 9
VK_RETURN = 0x0D
KEYEVENTF_KEYUP = 0x0002
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000


def queue_admin(command: str) -> None:
    subprocess.run(
        ["powershell.exe", "-ExecutionPolicy", "Bypass", "-File", str(QUEUE_SCRIPT), command],
        cwd=ROOT,
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def append_npslayer_marker(text: str) -> None:
    try:
        NPSLAYER_EXP_LOG.parent.mkdir(parents=True, exist_ok=True)
        with NPSLAYER_EXP_LOG.open("a", encoding="utf-8", errors="replace") as f:
            f.write(text.rstrip() + "\n")
    except OSError:
            pass


def taskkill_images(image_names: tuple[str, ...]) -> None:
    for name in image_names:
        subprocess.run(
            ["taskkill", "/F", "/IM", name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )


def kill_game() -> None:
    queue_admin("kill_game")
    taskkill_images((*GAME_CLIENT_IMAGES, "GameMon.des", "GameGuard.des", "ncclient.exe"))
    wait_for_game_clients_gone()


def kill_client_only() -> None:
    # The game often runs elevated after the launcher handoff, so local taskkill
    # can fail silently. Queue the elevated worker too, while preserving it.
    # NClient can hand off into PunchMonster after a tiny gap, so require a
    # stable quiet window instead of trusting the first empty process list.
    queue_admin("kill_game")
    if wait_for_game_clients_gone():
        append_npslayer_marker("=== CODEX_CLIENT_KILL_CONFIRMED ===")
    else:
        append_npslayer_marker("=== CODEX_CLIENT_KILL_STILL_RUNNING ===")


def is_process_running(image_name: str) -> bool:
    proc = subprocess.run(
        ["tasklist", "/FI", f"IMAGENAME eq {image_name}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=False,
    )
    return image_name.lower() in proc.stdout.lower()


def is_game_client_running() -> bool:
    return any(is_process_running(name) for name in GAME_CLIENT_IMAGES)


def wait_for_game_clients_gone(
    timeout: float = CLIENT_KILL_TIMEOUT_SECONDS,
    stable_seconds: float = CLIENT_KILL_STABLE_SECONDS,
) -> bool:
    deadline = time.time() + timeout
    quiet_since: float | None = None
    next_admin_queue = 0.0
    while time.time() < deadline:
        now = time.time()
        if now >= next_admin_queue:
            queue_admin("kill_game")
            next_admin_queue = now + 1.0
        taskkill_images(GAME_CLIENT_IMAGES)
        if is_game_client_running():
            quiet_since = None
        else:
            if quiet_since is None:
                quiet_since = now
            if now - quiet_since >= stable_seconds:
                return True
        time.sleep(0.25)
    return not is_game_client_running()


def wait_for_client_launch(timeout: float = 30.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if is_game_client_running():
            print("[launch-check] game client is running")
            append_npslayer_marker("=== CODEX_LAUNCH_CHECK seen_game_client=1 ===")
            return True
        time.sleep(0.5)
    print("[launch-check] game client was not observed after queueing launch")
    append_npslayer_marker("=== CODEX_LAUNCH_CHECK seen_game_client=0 ===")
    return False


class NpSlayerLiveMonitor:
    """Non-blocking NpSlayer tailer used while the client is alive."""

    def __init__(self, stall_seconds: float):
        self.stall_seconds = stall_seconds
        self.last_growth = time.time()
        self.pos = 0
        if NPSLAYER_EXP_LOG.exists():
            try:
                self.pos = NPSLAYER_EXP_LOG.stat().st_size
            except OSError:
                self.pos = 0
        print(
            f"[observe] live NpSlayer tail enabled; "
            f"stall_kill={stall_seconds:.1f}s"
        )

    def poll(self) -> str | None:
        if NPSLAYER_EXP_LOG.exists():
            try:
                with NPSLAYER_EXP_LOG.open("r", encoding="utf-8", errors="replace") as f:
                    f.seek(self.pos)
                    chunk = f.read()
                    self.pos = f.tell()
            except OSError:
                chunk = ""
            if chunk:
                self.last_growth = time.time()
                for line in chunk.splitlines():
                    if LIVE_LOG_RE.search(line):
                        print(f"[npslayer] {line[:220]}")

        if not is_game_client_running():
            print("[observe] game client is no longer running")
            append_npslayer_marker("=== CODEX_OBSERVE_END reason=client_exit ===")
            return "client_exit"

        if self.stall_seconds > 0 and time.time() - self.last_growth >= self.stall_seconds:
            print(
                f"[observe] no NpSlayer growth for {self.stall_seconds:.1f}s; "
                "killing client, keeping launcher"
            )
            append_npslayer_marker(
                f"=== CODEX_STALL_KILL no_npslayer_growth={self.stall_seconds:.1f}s ==="
            )
            kill_client_only()
            append_npslayer_marker("=== CODEX_OBSERVE_END reason=stall_kill ===")
            return "stall_kill"

        return None


def observe_npslayer_live(duration: float, stall_seconds: float) -> str:
    """Tail useful NpSlayer lines and kill the client if its log stalls."""
    deadline = time.time() + max(0.0, duration)
    monitor = NpSlayerLiveMonitor(stall_seconds)
    while time.time() < deadline:
        event = monitor.poll()
        if event is not None:
            return event

        time.sleep(0.25)

    append_npslayer_marker("=== CODEX_OBSERVE_END reason=duration_elapsed ===")
    return "duration_elapsed"


def find_game_window() -> tuple[int, tuple[int, int, int, int]] | None:
    found_known: list[tuple[int, tuple[int, int, int, int]]] = []
    found_unknown: list[tuple[int, tuple[int, int, int, int]]] = []

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
        item = (
            int(hwnd),
            (
                int(origin.x),
                int(origin.y),
                int(origin.x + rect.right - rect.left),
                int(origin.y + rect.bottom - rect.top),
            ),
        )
        _pid, image_name = describe_game_window(int(hwnd))
        if image_name in GAME_CLIENT_IMAGES:
            found_known.append(item)
        else:
            found_unknown.append(item)
        return True

    user32.EnumWindows(enum_proc, 0)
    if found_known:
        return found_known[0]
    return found_unknown[0] if found_unknown else None


def get_window_pid(hwnd: int) -> int:
    pid = wt.DWORD(0)
    user32.GetWindowThreadProcessId(wt.HWND(hwnd), ctypes.byref(pid))
    return int(pid.value)


def get_process_image_name(pid: int) -> str:
    if pid <= 0:
        return ""
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return ""
    try:
        size = wt.DWORD(1024)
        buf = ctypes.create_unicode_buffer(size.value)
        if not kernel32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size)):
            return ""
        return Path(buf.value).name
    finally:
        kernel32.CloseHandle(handle)


def describe_game_window(hwnd: int) -> tuple[int, str]:
    pid = get_window_pid(hwnd)
    return pid, get_process_image_name(pid)


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


def channel_clicker(stop: threading.Event, mode: str, stub_log: Path | None = None) -> None:
    fallback_delays = (18.0, 30.0, 44.0)
    # Unattended first-run clients can close the GSS socket ~3.5s after the
    # 0x1003+0x1004 server list if no UI click happens. Start the recorded
    # zone/channel sequence early, then keep bounded retries for slower loads.
    ready_delays = (1.0, 5.0, 13.0, 25.0)
    start = time.time()
    next_timed = 0
    queued = 0
    last_queue_at = 0.0
    ready_at: float | None = None
    channel_selected = False
    log_pos = 0
    stub_pos = 0
    if NPSLAYER_EXP_LOG.exists():
        try:
            log_pos = NPSLAYER_EXP_LOG.stat().st_size
        except OSError:
            log_pos = 0
    if stub_log is not None and stub_log.exists():
        try:
            stub_pos = stub_log.stat().st_size
        except OSError:
            stub_pos = 0

    def queue_click(reason: str) -> None:
        nonlocal queued, last_queue_at
        now = time.time()
        if now - last_queue_at < 3.0:
            return
        queued += 1
        last_queue_at = now
        print(
            f"[channel-clicker] queue=elevated_click_channel "
            f"attempt={queued} reason={reason}"
        )
        append_npslayer_marker(
            f"=== CODEX_CLICK_QUEUE attempt={queued} reason={reason} ==="
        )
        queue_admin("click_channel")

    while not stop.is_set():
        elapsed = time.time() - start
        if stub_log is not None and stub_log.exists():
            try:
                with stub_log.open("r", encoding="utf-8", errors="replace") as f:
                    f.seek(stub_pos)
                    chunk = f.read()
                    stub_pos = f.tell()
            except OSError:
                chunk = ""
            if chunk:
                for line in chunk.splitlines():
                    if GSS_CHANNEL_SELECTED_RE.search(line):
                        channel_selected = True
                        append_npslayer_marker(
                            "=== CODEX_CLICK_DISARMED reason=server_decoded_0x4005 ==="
                        )
                        break
                    if ready_at is None and GSS_CHANNEL_LIST_SENT_RE.search(line):
                        ready_at = time.time()
                        next_timed = 0
                        append_npslayer_marker(
                            "=== CODEX_CLICK_ARMED reason=server_sent_0x1003_0x1004 ==="
                        )

        if channel_selected:
            time.sleep(0.25)
            continue

        if mode in ("timed", "both"):
            if ready_at is None:
                delays = fallback_delays
                base_elapsed = elapsed
            else:
                delays = ready_delays
                base_elapsed = time.time() - ready_at
            if next_timed < len(delays) and base_elapsed >= delays[next_timed]:
                next_timed += 1
                if ready_at is None:
                    queue_click(f"fallback_timed_{elapsed:.1f}s")
                else:
                    queue_click(f"server_ready_plus_{base_elapsed:.1f}s")

        if mode in ("log", "both") and NPSLAYER_EXP_LOG.exists():
            try:
                with NPSLAYER_EXP_LOG.open("r", encoding="utf-8", errors="replace") as f:
                    f.seek(log_pos)
                    chunk = f.read()
                    log_pos = f.tell()
            except OSError:
                chunk = ""
            if chunk:
                for line in chunk.splitlines():
                    if CLICK_LOG_RE.search(line):
                        marker = line.strip()[:96].replace(" ", "_")
                        queue_click(f"log_{marker}")
                        break

        time.sleep(0.25)


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
    click_mode: str,
    admin_command: str,
    observe_seconds: float,
    stall_kill_seconds: float,
    leave_running_on_result: bool,
    server_script: str = str(ROOT / "Res" / "gss_stub_server_v4.py"),
) -> dict:
    rung = probe
    is_enter_game_probe = rung == "ordered-db-mage-select-enter"
    if is_enter_game_probe and observe_seconds <= 0:
        observe_seconds = 90.0
        leave_running_on_result = True
    stub_stdout = FINDINGS / f"master_ladder_{stamp}_{rung}.stdout.log"
    stub_stderr = FINDINGS / f"master_ladder_{stamp}_{rung}.stderr.log"
    stub_cmd = [
        str(PYTHON), str(server_script),
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
    append_npslayer_marker(
        f"=== CODEX_RUN_START stamp={stamp} rung={rung} "
        f"time={datetime.now().isoformat(timespec='seconds')} ==="
    )
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
            click_thread = threading.Thread(
                target=channel_clicker,
                args=(click_stop, click_mode, stub_stderr),
                daemon=True,
            )
            click_thread.start()

        queue_admin(admin_command)
        client_seen = wait_for_client_launch(timeout=30.0)
        live_monitor = NpSlayerLiveMonitor(stall_kill_seconds) if client_seen else None
        deadline = time.time() + 150.0
        found = None
        observe_event = None
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
                if live_monitor is not None:
                    observe_event = live_monitor.poll()
                    if observe_event is not None:
                        break
                time.sleep(0.5)
        finally:
            click_stop.set()
            if click_thread is not None:
                click_thread.join(timeout=1.0)
            if found is not None and observe_seconds > 0:
                print(f"[observe] result seen; leaving client/stub alive for {observe_seconds:.1f}s")
                observe_event = observe_npslayer_live(observe_seconds, stall_kill_seconds)
            if observe_event == "stall_kill":
                stop_proc(stub)
            elif not (found is not None and leave_running_on_result):
                kill_game()
                stop_proc(stub)
            else:
                print(f"[observe] leaving client/stub running; stub_pid={stub.pid}")

    if found is None:
        event = observe_event or "no_master_ladder_result"
        append_npslayer_marker(
            f"=== CODEX_RUN_END stamp={stamp} rung={rung} "
            f"result=timeout event={event} ==="
        )
        return {
            "rung": rung,
            "result": "timeout",
            "alive_ms": None,
            "class": "error",
            "event": event,
            "stderr": str(stub_stderr),
        }
    line, alive_ms, klass, event, _text = found
    append_npslayer_marker(
        f"=== CODEX_RUN_END stamp={stamp} rung={rung} "
        f"result=ok class={klass} event={event} alive_ms={alive_ms} ==="
    )
    return {
        "rung": rung,
        "result": "ok",
        "alive_ms": alive_ms,
        "class": klass,
        "event": event,
        "observe_event": observe_event,
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
            "ordered-enter-oneplayer-tplayer",
            "ordered-enter-componented-tplayer",
            "ordered-db-mage-select-enter",
            "silent-master",
        ),
        default="ladder",
    )
    ap.add_argument("--no-click-channel", action="store_true")
    ap.add_argument(
        "--click-mode",
        choices=("timed", "log", "both"),
        default="both",
        help="queue the elevated channel click sequence on timers, NpSlayer log markers, or both",
    )
    ap.add_argument(
        "--observe-seconds",
        type=float,
        default=0.0,
        help="after a result marker, keep the client/stub alive this many seconds",
    )
    ap.add_argument(
        "--stall-kill-seconds",
        type=float,
        default=8.0,
        help="during observation, kill the client if NpSlayer log has no growth",
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
    ap.add_argument(
        "--server-script",
        default=str(ROOT / "Res" / "gss_stub_server_v4.py"),
        help="server entry point to launch (default = legacy stub; use server/run_server.py for the modular host)",
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
    elif args.mode == "ordered-enter-oneplayer-tplayer":
        probes = ["ordered-enter-oneplayer-tplayer"]
        summary_path = FINDINGS / f"master_ordered_enter_oneplayer_tplayer_{stamp}_summary.log"
    elif args.mode == "ordered-enter-componented-tplayer":
        probes = ["ordered-enter-componented-tplayer"]
        summary_path = FINDINGS / f"master_ordered_enter_componented_tplayer_{stamp}_summary.log"
    elif args.mode == "ordered-db-mage-select-enter":
        probes = ["ordered-db-mage-select-enter"]
        summary_path = FINDINGS / f"master_ordered_db_mage_select_enter_{stamp}_summary.log"
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
            args.click_mode,
            args.admin_command,
            args.observe_seconds,
            args.stall_kill_seconds,
            args.leave_running_on_result,
            args.server_script,
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
