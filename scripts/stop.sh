#!/usr/bin/env bash
# 统一停止：默认停止 manager 和 worker；--manager 或 --worker 只停止一个。
set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

run_manager=0
run_worker=0
stop_db=0

for arg in "$@"; do
  case "$arg" in
    --manager) run_manager=1 ;;
    --worker)  run_worker=1 ;;
    --stop-db) stop_db=1 ;;
    *)
      echo "[ERROR] Unknown option: $arg" >&2
      echo "Usage: $0 [--manager] [--worker] [--stop-db]" >&2
      echo "  No args: stop both manager and worker." >&2
      echo "  --manager: only stop manager." >&2
      echo "  --worker:  only stop worker." >&2
      echo "  --stop-db: when stopping manager, also stop MySQL." >&2
      exit 1
      ;;
  esac
done

if [[ $run_manager -eq 0 && $run_worker -eq 0 ]]; then
  run_manager=1
  run_worker=1
fi

failed=0
if [[ $run_manager -eq 1 ]]; then
  if [[ $stop_db -eq 1 ]]; then
    python3 "$ROOT/manager/scripts/stop_manager.py" --stop-db || ((failed++)) || true
  else
    python3 "$ROOT/manager/scripts/stop_manager.py" || ((failed++)) || true
  fi
fi
if [[ $run_worker -eq 1 ]]; then
  python3 "$ROOT/worker/scripts/stop_worker.py" || ((failed++)) || true
fi

[[ $failed -eq 0 ]]
