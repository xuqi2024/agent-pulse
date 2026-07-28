#!/usr/bin/env bash
# lib.sh — common functions for agent-pulse Claude Code hooks
# Source this from each on_*.sh script.
#
# Contract: every hook MUST exit 0 (Claude Code halts on non-zero). Anything
# we do here is best-effort: if it fails, swallow the error and exit 0.

# Find ap-cli. Prefer the one next to this script's parent, then $PATH, then
# the pipx / venv install.
ap_resolve_ap_cli() {
  if [ -n "${AP_CLI:-}" ] && [ -x "$AP_CLI" ]; then
    echo "$AP_CLI"; return
  fi
  local here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
  if [ -x "$here/tools/ap-cli.py" ]; then
    echo "$here/tools/ap-cli.py"; return
  fi
  if command -v ap-cli >/dev/null 2>&1; then
    command -v ap-cli; return
  fi
  echo ""
}

# Read all of stdin (Claude Code passes a single JSON object).
ap_read_stdin() {
  cat
}

# Truncate a string to N characters without breaking words.
ap_truncate() {
  local s="${1:-}" n="${2:-40}"
  if [ "${#s}" -le "$n" ]; then
    printf '%s' "$s"
  else
    printf '%s…' "${s:0:$((n-1))}"
  fi
}

# Extract a top-level string field from JSON on stdin. Uses python3 (the host
# almost certainly has it; jq is sometimes missing).
ap_json_get() {
  local field="$1"
  python3 -c "
import json, sys
try:
  obj = json.load(sys.stdin)
  v = obj.get('$field', '')
  if v is None: v = ''
  if isinstance(v, (dict, list)):
    print(json.dumps(v, separators=(',', ':')))
  else:
    print(v)
except Exception:
  pass
"
}

# Extract a nested string from a sub-object. Usage: ap_json_get_nested tool_input file_path
ap_json_get_nested() {
  local parent="$1" field="$2"
  python3 -c "
import json, sys
try:
  obj = json.load(sys.stdin)
  v = obj.get('$parent', {}).get('$field', '')
  if v is None: v = ''
  print(v)
except Exception:
  pass
"
}
