#!/usr/bin/env bash
# on_notification.sh — surface permission requests on the device.
# Claude Code emits a Notification event when it wants the user to approve
# something. We only care about permission prompts.

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$HERE/lib.sh"

AP_CLI_BIN="$(ap_resolve_ap_cli)"
[ -z "$AP_CLI_BIN" ] && exit 0

JSON="$(ap_read_stdin)"
NTYPE="$(printf '%s' "$JSON" | ap_json_get notification_type)"
case "$NTYPE" in
  permission_prompt|tool_permission)
    MSG="$(printf '%s' "$JSON" | ap_json_get message | ap_truncate 50)"
    "$AP_CLI_BIN" set error permission "$MSG" 2>/dev/null || true
    ;;
  *)
    :  # ignore other notification types
    ;;
esac
exit 0
