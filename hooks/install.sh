#!/usr/bin/env bash
# install.sh — register agent-pulse hooks with Claude Code.
# Idempotent: re-running updates the existing entries.
#
# Writes into the USER-LEVEL settings file (~/.claude/settings.json) so
# agent-pulse works for every project without per-project setup. A
# project-level install is also supported via --project <path>.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
TARGET_FILE="${HOME}/.claude/settings.json"
BACKUP_DIR="${HOME}/.claude/backups"
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"

if [ "${1:-}" = "--project" ] && [ -n "${2:-}" ]; then
  PROJECT_DIR="$2"
  TARGET_FILE="${PROJECT_DIR}/.claude/settings.json"
  mkdir -p "$(dirname "$TARGET_FILE")"
fi

# Sanity check: is jq available? It's optional — we fall back to python3.
HAVE_JQ=0
if command -v jq >/dev/null 2>&1; then
  HAVE_JQ=1
fi

# Dry-run by default is opt-out; just print a summary.
echo "==> agent-pulse hook install"
echo "    target: $TARGET_FILE"
[ "$HAVE_JQ" -eq 0 ] && echo "    (jq not found; using python3)"
echo

# Backup existing file
if [ -f "$TARGET_FILE" ]; then
  mkdir -p "$BACKUP_DIR"
  cp "$TARGET_FILE" "$BACKUP_DIR/agent-pulse-${TIMESTAMP}.json"
  echo "==> backed up to $BACKUP_DIR/agent-pulse-${TIMESTAMP}.json"
fi

# Ensure target file exists and is valid JSON
if [ ! -f "$TARGET_FILE" ]; then
  echo '{}' > "$TARGET_FILE"
fi

# Build the hooks block. We use absolute paths so Claude Code resolves them
# regardless of cwd.
AP_CLI_RESOLVED="$HERE/../tools/ap-cli.py"
AP_CLI_ABS="$(cd "$(dirname "$AP_CLI_RESOLVED")" && pwd)/$(basename "$AP_CLI_RESOLVED")"

# Use python3 to merge — handles both jq-absent and jq-present cases safely.
python3 - "$TARGET_FILE" "$HERE" "$AP_CLI_ABS" <<'PY'
import json
import sys
from pathlib import Path

target_path, hooks_dir, ap_cli_abs = sys.argv[1], sys.argv[2], sys.argv[3]
hooks_dir = Path(hooks_dir)

with open(target_path) as f:
    cfg = json.load(f)

cfg.setdefault("hooks", {})

def add(matcher, event, script):
    script_abs = str((hooks_dir / script).resolve())
    cfg["hooks"].setdefault(event, [])
    # remove old agent-pulse entries first
    cfg["hooks"][event] = [
        h for h in cfg["hooks"][event]
        if not any(
            hh.get("hooks", [{}])[0].get("command", "").endswith(Path(script).name)
            for hh in [h]
            if "hooks" in h
        )
    ]
    cfg["hooks"][event].append({
        "matcher": matcher,
        "hooks": [
            {"type": "command", "command": f"{script_abs}", "env": {"AP_CLI": ap_cli_abs}}
        ],
    })

# Wire every event we care about
add("*", "SessionStart",      "on_session_start.sh")
add("*", "UserPromptSubmit",  "on_user_prompt_submit.sh")
add("*", "PreToolUse",        "on_pre_tool_use.sh")
add("*", "PostToolUse",       "on_post_tool_use.sh")
add("*", "Stop",              "on_stop.sh")
add("*", "Notification",      "on_notification.sh")
add("*", "SessionEnd",        "on_session_end.sh")

with open(target_path, "w") as f:
    json.dump(cfg, f, indent=2, ensure_ascii=False)
    f.write("\n")
print(f"==> wrote {target_path}")
PY

echo
echo "Done. Verify with:"
echo "  cat $TARGET_FILE | python3 -m json.tool | grep -A 2 '\"hooks\"' | head -50"
echo
echo "To uninstall:"
echo "  $HERE/uninstall.sh"
[ -n "${PROJECT_DIR:-}" ] && echo "  $HERE/uninstall.sh --project $PROJECT_DIR"
