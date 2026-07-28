"""Wire protocol: JSON line per message, terminated with '\\n'.

The grammar is intentionally small. We don't pull in a JSON library on the
host side either — these encoders/decoders handle just the message types
we care about.
"""
from __future__ import annotations

import json
import time
from dataclasses import dataclass, field, asdict
from typing import Optional


# --- message classes --------------------------------------------------------

@dataclass
class Hello:
    t: str = "hello"
    protocol_version: int = 1
    device_name: str = ""
    width: int = 320
    height: int = 240

    def to_json(self) -> str:
        d = asdict(self)
        return json.dumps(d, separators=(",", ":")) + "\n"


@dataclass
class State:
    t: str = "state"
    status: str = "idle"           # idle | processing | error | permission
    tool: Optional[str] = None
    message: Optional[str] = None
    progress: Optional[int] = None
    seq: int = 0
    ts: int = field(default_factory=lambda: int(time.time() * 1000))

    def to_json(self) -> str:
        d = asdict(self)
        d = {k: v for k, v in d.items() if v is not None}
        return json.dumps(d, separators=(",", ":")) + "\n"


@dataclass
class Config:
    t: str = "config"
    brightness: Optional[int] = None
    theme: Optional[str] = None
    screen_rotation: Optional[int] = None
    idle_animation: Optional[bool] = None
    fps_cap: Optional[int] = None

    def to_json(self) -> str:
        d = asdict(self)
        d = {k: v for k, v in d.items() if v is not None}
        return json.dumps(d, separators=(",", ":")) + "\n"


# --- inbound (from firmware) ------------------------------------------------

INBOUND_TYPES = {"hello_ack", "pong", "state_ack", "btn", "error"}


def parse_line(line: str) -> Optional[dict]:
    """Parse a single JSON line from the firmware. Returns None on error."""
    line = line.strip()
    if not line or line[0] != "{":
        return None
    try:
        obj = json.loads(line)
    except json.JSONDecodeError:
        return None
    if not isinstance(obj, dict):
        return None
    t = obj.get("t")
    if t not in INBOUND_TYPES:
        return None
    return obj
