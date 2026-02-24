#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str], check: bool = True, use_sudo: bool = False, **kwargs):
    if use_sudo and os.geteuid() != 0:
        cmd = ["sudo", *cmd]
    r = subprocess.run(
        cmd, check=False, text=True,
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, **kwargs
    )
    if check and r.returncode != 0:
        raise RuntimeError(r.stderr or f"exit code {r.returncode}")
    return r


def module_loaded(name: str) -> bool:
    try:
        data = Path("/proc/modules").read_text(encoding="utf-8")
    except Exception:
        return False
    return any(line.split()[0] == name for line in data.splitlines() if line.strip())


def kernel_build_dir() -> Path:
    return Path("/lib/modules") / os.uname().release / "build"


def ensure_kmod_artifacts(root: Path, kmod_dir: Path) -> bool:
    required = [
        kmod_dir / "softirq_collector.ko",
        kmod_dir / "cpu_stat_collector.ko",
    ]
    if all(p.exists() for p in required):
        return True
    kdir = kernel_build_dir()
    if not kdir.exists():
        return False
    run(["cmake", "--build", str(root / "build"), "--target", "kmod_modules", "-j4"], check=True)
    missing = [str(p) for p in required if not p.exists()]
    if missing:
        raise FileNotFoundError("Kernel modules still missing: " + ", ".join(missing))
    return True


def load_module(ko_path: Path, module_name: str) -> None:
    if module_loaded(module_name):
        return
    if not ko_path.exists():
        raise FileNotFoundError(f"Kernel module not found: {ko_path}")
    run(["insmod", str(ko_path)], use_sudo=True)
    if not module_loaded(module_name):
        raise RuntimeError(f"Kernel module load verification failed: {module_name}")


def verify_required_modules() -> None:
    required = ["cpu_stat_collector", "softirq_collector"]
    missing = [name for name in required if not module_loaded(name)]
    if missing:
        raise RuntimeError("Kernel modules not loaded: " + ", ".join(missing))


def verify_kmod_devices() -> None:
    required = [Path("/dev/cpu_stat_monitor"), Path("/dev/cpu_softirq_monitor")]
    missing = [str(p) for p in required if not p.exists()]
    if missing:
        raise FileNotFoundError("Kernel device nodes missing: " + ", ".join(missing))


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description="Start worker in background")
    parser.add_argument("--worker-bin", default=str(root / "build" / "worker" / "worker"))
    parser.add_argument("--manager", default="127.0.0.1:50051")
    parser.add_argument("--interval", type=int, default=10)
    parser.add_argument("--skip-kmod", action="store_true", help="skip loading kernel modules")
    parser.add_argument("--sudo-worker", dest="sudo_worker", action="store_true", default=True)
    parser.add_argument("--no-sudo-worker", dest="sudo_worker", action="store_false")
    parser.add_argument("--strict-kmod", dest="strict_kmod", action="store_true", default=True)
    parser.add_argument("--no-strict-kmod", dest="strict_kmod", action="store_false")
    args = parser.parse_args()

    worker_bin = Path(args.worker_bin)

    if not worker_bin.exists():
        print(f"[ERROR] worker binary not found: {worker_bin}", file=sys.stderr)
        return 1

    if not args.skip_kmod:
        kmod_dir = root / "worker" / "src" / "kmod"
        try:
            kmod_ready = ensure_kmod_artifacts(root, kmod_dir)
            if kmod_ready:
                load_module(kmod_dir / "cpu_stat_collector.ko", "cpu_stat_collector")
                load_module(kmod_dir / "softirq_collector.ko", "softirq_collector")
                verify_required_modules()
                verify_kmod_devices()
            elif args.strict_kmod:
                print("[ERROR] Kernel modules not available and strict mode enabled", file=sys.stderr)
                return 1
        except Exception as exc:
            print(f"[ERROR] {exc}", file=sys.stderr)
            if args.strict_kmod:
                return 1

    # Create log file for worker output
    log_file = root / "worker" / "worker.log"
    log_fd = open(log_file, "a")

    try:
        cmd = [str(worker_bin), args.manager, str(args.interval)]
        
        # Determine if we need sudo and how to launch
        is_root = (os.geteuid() == 0)
        use_sudo = (args.sudo_worker and not is_root)
        
        if use_sudo:
            # Running as non-root user, need sudo.
            # Cannot use start_new_session=True because sudo needs a terminal for password
            final_cmd = ["sudo", "nohup", *cmd]
            
            # Allow sudo to interfere with terminal for password prompt
            proc = subprocess.Popen(
                final_cmd,
                cwd=str(root),
                stdout=log_fd,
                stderr=subprocess.STDOUT,
                start_new_session=False  # Do not detach session, rely on nohup
            )
        else:
            # Running as root or sudo not needed
            final_cmd = ["nohup", *cmd]
            
            # Detach completely
            proc = subprocess.Popen(
                final_cmd,
                cwd=str(root),
                stdout=log_fd,
                stderr=subprocess.STDOUT,
                start_new_session=True
            )
        
        # 简单等待确保启动
        import time
        time.sleep(2)  # Wait longer to see if it crashes
        
        if proc.poll() is not None:
             print(f"[ERROR] worker failed to start immediately (exit code {proc.returncode})", file=sys.stderr)
             print(f"Check log file for details: {log_file}")
             log_fd.close()
             return 1

        print("启动模块成功 success")
        print(f"worker running in background (PID {proc.pid})", file=sys.stderr)
        log_fd.close() 
        return 0
    except Exception as e:
        print(f"[ERROR] {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
