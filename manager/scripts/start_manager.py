#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


def command_exists(name: str) -> bool:
    return subprocess.run(
        ["bash", "-lc", f"command -v {name} >/dev/null 2>&1"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    ).returncode == 0


def ensure_mysql_server_started() -> None:
    candidates = ["mysqld", "mysql", "mariadb"]
    if command_exists("systemctl"):
        for svc in candidates:
            if subprocess.run(["systemctl", "is-active", "--quiet", svc], capture_output=True).returncode == 0:
                return
        for svc in candidates:
            if subprocess.run(["systemctl", "start", svc], capture_output=True).returncode == 0:
                return
    if command_exists("service"):
        for svc in candidates:
            if subprocess.run(["service", svc, "start"], capture_output=True).returncode == 0:
                return


def wait_mysql_ready(timeout_sec: int = 30) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        r = subprocess.run(["mysqladmin", "-u", "root", "ping"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if r.returncode == 0:
            return
        time.sleep(1)
    raise RuntimeError("MySQL server is not ready (mysqladmin ping timeout)")


def init_database(sql_file: Path) -> None:
    if not sql_file.exists():
        raise FileNotFoundError(f"SQL file not found: {sql_file}")
    bootstrap_sql = """
CREATE DATABASE IF NOT EXISTS monitor_db DEFAULT CHARACTER SET utf8mb4;
CREATE USER IF NOT EXISTS 'monitor'@'localhost' IDENTIFIED BY 'monitor123';
CREATE USER IF NOT EXISTS 'monitor'@'%' IDENTIFIED BY 'monitor123';
GRANT ALL PRIVILEGES ON monitor_db.* TO 'monitor'@'localhost';
GRANT ALL PRIVILEGES ON monitor_db.* TO 'monitor'@'%';
FLUSH PRIVILEGES;
""".strip()
    r = subprocess.run(["mysql", "-u", "root", "-e", bootstrap_sql], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr or "mysql bootstrap failed")
    with sql_file.open("r", encoding="utf-8") as f:
        r = subprocess.run(["mysql", "-u", "root"], stdin=f, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr or "mysql init sql failed")


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description="Start manager in background")
    parser.add_argument("--manager-bin", default=str(root / "build" / "manager" / "manager"))
    parser.add_argument("--listen", default="0.0.0.0:50051")
    parser.add_argument("--sql", default=str(root / "manager" / "sql" / "server.sql"))
    parser.add_argument("--no-db-init", action="store_true", help="skip mysql start and sql init")
    args = parser.parse_args()

    manager_bin = Path(args.manager_bin)
    sql_file = Path(args.sql)

    if not manager_bin.exists():
        print(f"[ERROR] manager binary not found: {manager_bin}", file=sys.stderr)
        return 1

    try:
        if not args.no_db_init:
            ensure_mysql_server_started()
            wait_mysql_ready()
            init_database(sql_file)

        proc = subprocess.Popen(
            [str(manager_bin), args.listen],
            cwd=str(root),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        print("success")
        print(f"manager running in background (PID {proc.pid})", file=sys.stderr)
        return 0
    except Exception as e:
        print(f"[ERROR] {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
