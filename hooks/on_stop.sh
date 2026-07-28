#!/usr/bin/env bash
# on_stop.sh — Claude finished its turn. Switch to idle.

set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$HERE/lib.sh"

AP_CLI_BIN="$(ap_resolve_ap_cli)"
[ -z "$AP_CLI_BIN" ] && exit 0

"$AP_CLI_BIN" set idle 2>/dev/null || true
exit 0
