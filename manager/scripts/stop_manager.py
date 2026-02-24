#!/usr/bin/env python3
from __future__ import annotations

import argparse
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
    """发送信号，无权限时用 sudo。返回是否发送成功或进程已不存在."""
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
        print(f"[ERROR] Cannot send SIGTERM to {name} pid={pid} (try run with sudo)", file=sys.stderr)
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


def stop_mysql_server() -> None:
    candidates = ["mysqld", "mysql", "mariadb"]
    for svc in candidates:
        ret = _run(["systemctl", "is-active", "--quiet", svc], use_sudo=True, capture=True)
        if ret.returncode == 0:
            _run(["systemctl", "stop", svc], use_sudo=True)
            print(f"Stopped MySQL service: {svc}")
            return
    print("No active MySQL service found to stop")


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    scripts_dir = Path(__file__).resolve().parent
    pid_file = scripts_dir / ".manager.pid"
    manager_bin = (root / "build" / "manager" / "manager").resolve()
    # 先按绝对路径匹配；若进程用相对路径启动则用 build/manager/manager 兜底
    patterns = [
        re.escape(str(manager_bin)),
        re.escape("build/manager/manager"),
    ]

    parser = argparse.ArgumentParser(description="Stop manager process")
    parser.add_argument("--stop-db", action="store_true", help="also stop mysql service")
    args = parser.parse_args()

    stop_process(pid_file, "manager", patterns)
    if args.stop_db:
        stop_mysql_server()
    return 0


if __name__ == "__main__":
    sys.exit(main())
