#!/usr/bin/env python3
"""ap-cli: write a state line to ~/.cache/agent-pulse/state.json.

This is the program invoked by Claude Code hooks. It must be fast (sub-50ms
startup) because hooks run synchronously and we don't want to slow the agent.

Usage:
  ap-cli set idle
  ap-cli set processing Bash "Running pytest"
  ap-cli set error permission
  ap-cli clear
  ap-cli get
"""
from __future__ import annotations

import json
import os
import sys
import time
from pathlib import Path

DEFAULT = Path.home() / ".cache" / "agent-pulse" / "state.json"


def _atomic_write(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(payload, separators=(",", ":")) + "\n")
    os.replace(tmp, path)


def main(argv=None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    cmd = argv[0]
    if cmd == "set":
        if len(argv) < 2:
            print("usage: ap-cli set <status> [tool] [message]", file=sys.stderr)
            return 1
        status = argv[1]
        if status not in ("idle", "processing", "error", "permission"):
            print(f"unknown status: {status}", file=sys.stderr)
            return 1
        tool = argv[2] if len(argv) >= 3 else None
        message = " ".join(argv[3:]) if len(argv) >= 4 else None
        payload = {
            "version": 1,
            "ts": int(time.time() * 1000),
            "status": status,
        }
        if tool: payload["tool"] = tool
        if message: payload["message"] = message
        _atomic_write(DEFAULT, payload)
        return 0
    if cmd == "clear":
        _atomic_write(DEFAULT, {"version": 1, "ts": int(time.time() * 1000), "status": "idle"})
        return 0
    if cmd == "get":
        if DEFAULT.exists():
            sys.stdout.write(DEFAULT.read_text())
        else:
            sys.stdout.write('{"status":"idle"}\n')
        return 0
    print(f"unknown subcommand: {cmd}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
