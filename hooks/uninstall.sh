#!/usr/bin/env bash
# uninstall.sh — remove agent-pulse hooks from Claude Code settings.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
TARGET_FILE="${HOME}/.claude/settings.json"

if [ "${1:-}" = "--project" ] && [ -n "${2:-}" ]; then
  TARGET_FILE="$2/.claude/settings.json"
fi

if [ ! -f "$TARGET_FILE" ]; then
  echo "no settings file at $TARGET_FILE"
  exit 0
fi

python3 - "$TARGET_FILE" "$HERE" <<'PY'
import json, sys
from pathlib import Path

target_path, hooks_dir = sys.argv[1], Path(sys.argv[2])
with open(target_path) as f:
    cfg = json.load(f)

hooks = cfg.get("hooks", {})
for event, entries in list(hooks.items()):
    cleaned = []
    for entry in entries:
        inner = entry.get("hooks", [])
        if not inner:
            cleaned.append(entry); continue
        cmd = inner[0].get("command", "")
        try:
            p = Path(cmd)
        except Exception:
            cleaned.append(entry); continue
        # Drop only entries pointing inside our hooks dir
        if hooks_dir in p.resolve().parents:
            continue
        cleaned.append(entry)
    if cleaned:
        hooks[event] = cleaned
    else:
        del hooks[event]

cfg["hooks"] = hooks
with open(target_path, "w") as f:
    json.dump(cfg, f, indent=2, ensure_ascii=False)
    f.write("\n")
print(f"cleaned {target_path}")
PY

echo
echo "If anything is still there, restore from the most recent backup:"
echo "  ls -t ~/.claude/backups/agent-pulse-*.json | head -1"
