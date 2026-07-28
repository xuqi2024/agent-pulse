#!/usr/bin/env bash
# on_pre_tool_use.sh — switch to processing and tag the tool.
# Triggered before Claude Code runs any tool.

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$HERE/lib.sh"

AP_CLI_BIN="$(ap_resolve_ap_cli)"
[ -z "$AP_CLI_BIN" ] && exit 0

JSON="$(ap_read_stdin)"
TOOL="$(printf '%s' "$JSON" | ap_json_get tool_name)"
[ -z "$TOOL" ] && TOOL="tool"

# Best-effort "what is this tool doing" target
TARGET=""
case "$TOOL" in
  Read|read|read_file)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input file_path)"
    ;;
  Edit|edit|MultiEdit|write_file|Write)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input file_path)"
    ;;
  Bash|bash|run_command)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input command)"
    TARGET="$(ap_truncate "$TARGET" 50)"
    ;;
  Glob|glob|list_files)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input pattern)"
    ;;
  Grep|grep|search)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input pattern)"
    ;;
  WebFetch|web_fetch)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input url)"
    TARGET="$(ap_truncate "$TARGET" 50)"
    ;;
  WebSearch|web_search)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input query)"
    ;;
  Agent|agent|task)
    TARGET="$(printf '%s' "$JSON" | ap_json_get_nested tool_input prompt)"
    TARGET="$(ap_truncate "$TARGET" 50)"
    ;;
esac

if [ -n "$TARGET" ]; then
  "$AP_CLI_BIN" set processing "$TOOL" "$TARGET" 2>/dev/null || true
else
  "$AP_CLI_BIN" set processing "$TOOL" 2>/dev/null || true
fi
exit 0
