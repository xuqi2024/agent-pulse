#!/usr/bin/env bash
# on_post_tool_use.sh — after a tool runs. We don't immediately flip to
# idle (Claude will likely call another tool right away); instead, we mark
# the end-of-tool for the bridge's "must see end to go idle" rule. The
# actual idle transition happens in on_stop.sh.

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$HERE/lib.sh"

AP_CLI_BIN="$(ap_resolve_ap_cli)"
[ -z "$AP_CLI_BIN" ] && exit 0

# Write a sentinel state file that the bridge can read to know the most
# recent tool finished. We append a tiny marker to the message.
JSON="$(ap_read_stdin)"
TOOL="$(printf '%s' "$JSON" | ap_json_get tool_name)"
[ -z "$TOOL" ] && TOOL="tool"

# We don't go to idle here — we just record the tool name so the bridge
# knows the agent is between tools. The idle switch is done in on_stop.
# To avoid bouncing the screen during multi-tool sequences, we keep status
# as "processing" but refresh the tool name.
"$AP_CLI_BIN" set processing "$TOOL" 2>/dev/null || true
exit 0
