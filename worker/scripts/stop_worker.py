#!/usr/bin/env python3
from __future__ import annotations

import os
import re
import subprocess
import sys
import time
from pathlib import Path


def _run(cmd: list[str], use_sudo: bool = False, capture: bool = False):
    if use_sudo and os.geteuid() != 0:
        cmd = ["sudo", *cmd]
    return subprocess.run(cmd, check=False, text=True, capture_output=capture)


def module_loaded(name: str) -> bool:
    try:
        data = Path("/proc/modules").read_text(encoding="utf-8")
    except Exception:
        return False
    return any(line.split()[0] == name for line in data.splitlines() if line.strip())


def unload_module(name: str) -> None:
    if not module_loaded(name):
        return
    _run(["rmmod", name], use_sudo=True)


def _find_pids_by_pattern(pattern: str) -> list[int]:
    """用 pgrep -f 按命令行匹配，pattern 已做正则转义，避免误杀."""
    result = _run(["pgrep", "-f", pattern], capture=True)
    if result.returncode != 0 or not result.stdout:
        return []
    pids: list[int] = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            pids.append(int(line))
        except ValueError:
            continue
    return pids


def _send_signal(pid: int, sig: int, use_sudo: bool = False) -> bool:
    """发送信号，无权限时用 sudo。"""
    try:
        os.kill(pid, sig)
        return True
    except ProcessLookupError:
        return True
    except PermissionError:
        if use_sudo:
            r = _run(["kill", f"-{sig}", str(pid)], use_sudo=True, capture=True)
            return r.returncode == 0
        return False


def _stop_pid(pid: int, name: str, use_sudo: bool = False) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return True
    except PermissionError:
        use_sudo = True

    print(f"Stopping {name} pid={pid}")
    if not _send_signal(pid, 15, use_sudo):
        print(f"[ERROR] Cannot send SIGTERM to {name} pid={pid} (try: sudo python stop_worker.py)", file=sys.stderr)
        return False

    for _ in range(10):
        try:
            os.kill(pid, 0)
            time.sleep(0.5)
        except ProcessLookupError:
            print(f"{name} stopped (pid={pid})")
            return True
        except PermissionError:
            time.sleep(0.5)
            continue

    print(f"Force killing {name} pid={pid}")
    _send_signal(pid, 9, use_sudo)
    return True


def stop_process(pid_file: Path, name: str, patterns: list[str]) -> None:
    """patterns: 按顺序尝试，直到找到 PID（先绝对路径，再相对路径等）."""
    pids: list[int] = []

    if pid_file.exists():
        try:
            pids.append(int(pid_file.read_text(encoding="utf-8").strip()))
        except Exception:
            pass

    if not pids:
        for pattern in patterns:
            pids = _find_pids_by_pattern(pattern)
            if pids:
                break

    if not pids:
        print(f"{name} process not found")
        pid_file.unlink(missing_ok=True)
        return

    for pid in pids:
        _stop_pid(pid, name)

    pid_file.unlink(missing_ok=True)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    scripts_dir = Path(__file__).resolve().parent
    pid_file = scripts_dir / ".worker.pid"
    worker_bin = (root / "build" / "worker" / "worker").resolve()
    # 先按绝对路径匹配；若进程用相对路径启动则用 build/worker/worker 兜底
    patterns = [
        re.escape(str(worker_bin)),
        re.escape("build/worker/worker"),
    ]

    stop_process(pid_file, "worker", patterns)

    # 先卸载依赖者，再卸载被依赖者（通常逆序）
    unload_module("cpu_stat_collector")
    unload_module("softirq_collector")

    return 0


if __name__ == "__main__":
    sys.exit(main())
