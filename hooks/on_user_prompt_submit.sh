#!/usr/bin/env bash
# on_user_prompt_submit.sh — capture the user's prompt and mark processing.
# The bridge's debounce will hold the screen on idle for a few hundred ms
# before flipping to processing; that's intentional (avoid flicker on short
# prompts that Claude handles in one breath).

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$HERE/lib.sh"

AP_CLI_BIN="$(ap_resolve_ap_cli)"
[ -z "$AP_CLI_BIN" ] && exit 0

# Read the JSON prompt from stdin
MSG="$(ap_read_stdin | ap_json_get prompt | head -c 200 | tr -d '\n')"
TRUNC="$(ap_truncate "$MSG" 50)"

# We don't set "processing" here directly — we let PreToolUse do it. This
# hook just records what the user asked, so the screen can show it during
# the initial "thinking" phase. If the agent goes straight to text without
# any tool call, the screen will see "processing Prompt ..." briefly.
[ -n "$TRUNC" ] && "$AP_CLI_BIN" set processing Prompt "$TRUNC" 2>/dev/null || true
exit 0
