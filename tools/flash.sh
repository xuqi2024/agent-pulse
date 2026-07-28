#!/usr/bin/env bash
# flash.sh — build + flash the agent-pulse firmware
# Usage: tools/flash.sh [esp-idf-path]
set -euo pipefail

IDF_PATH="${1:-${IDF_PATH:-/Users/xuqi/esp/v5.5.3/esp-idf}}"
FW_DIR="$(cd "$(dirname "$0")/../firmware" && pwd)"

if [ ! -f "$IDF_PATH/export.sh" ]; then
  echo "error: $IDF_PATH does not look like an ESP-IDF install" >&2
  echo "usage: $0 /path/to/esp-idf" >&2
  exit 1
fi

# shellcheck disable=SC1091
source "$IDF_PATH/export.sh"

cd "$FW_DIR"
idf.py set-target esp32s3
idf.py build
PORT="$(python3 - <<'PY'
import sys
sys.path.insert(0, str(__import__('pathlib').Path('/Users/xuqi/workspace/agent-pulse/bridge')))
from agent_pulse.serial_link import pick_port
c = pick_port()
print(c.device if c else '/dev/cu.usbmodem0')
PY
)"
echo "==> flashing to $PORT"
idf.py -p "$PORT" flash
