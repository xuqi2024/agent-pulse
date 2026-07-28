#!/usr/bin/env bash
# monitor.sh — run idf.py monitor and filter our JSON protocol lines
set -euo pipefail
IDF_PATH="${1:-${IDF_PATH:-/Users/xuqi/esp/v5.5.3/esp-idf}}"
FW_DIR="$(cd "$(dirname "$0")"/../firmware && pwd)"

# shellcheck disable=SC1091
source "$IDF_PATH/export.sh"
cd "$FW_DIR"
idf.py monitor | grep --line-buffered -E '^\{|^\[' || true
