#!/usr/bin/env bash
# 统一启动：默认启动 manager 和 worker；--manager 或 --worker 只启动一个。
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

run_manager=0
run_worker=0

if [[ $# -eq 0 ]]; then
  run_manager=1
  run_worker=1
else
  for arg in "$@"; do
    case "$arg" in
      --manager) run_manager=1 ;;
      --worker)  run_worker=1 ;;
      *)
        echo "[ERROR] Unknown option: $arg" >&2
        echo "Usage: $0 [--manager] [--worker]" >&2
        echo "  No args: start both manager and worker." >&2
        echo "  --manager: only start manager." >&2
        echo "  --worker:  only start worker." >&2
        exit 1
        ;;
    esac
  done
  # 若只传了其中一个，就只跑一个；若两个都传则两个都跑
  if [[ $run_manager -eq 0 && $run_worker -eq 0 ]]; then
    run_manager=1
    run_worker=1
  fi
fi

failed=0
if [[ $run_manager -eq 1 ]]; then
  if python3 "$ROOT/manager/scripts/start_manager.py"; then
    : ok
  else
    ((failed++)) || true
  fi
fi
if [[ $run_worker -eq 1 ]]; then
  if python3 "$ROOT/worker/scripts/start_worker.py"; then
    : ok
  else
    ((failed++)) || true
  fi
fi

[[ $failed -eq 0 ]]
