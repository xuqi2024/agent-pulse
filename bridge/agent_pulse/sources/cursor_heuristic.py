"""Cursor heuristic: poll the front window title via AppleScript (macOS only).

We match keywords like "Generating", "Applying", "Indexing" → processing.
Anything else → idle.

This is best-effort. If Cursor is not running, the call returns an error
and we just sit idle.
"""
from __future__ import annotations

import platform
import subprocess
import time
import sys
import os
from typing import Optional

from . import Source
from ..state_machine import SourceEvent


KEYWORDS = ("generating", "applying", "indexing", "thinking", "running")


def _get_cursor_window() -> Optional[str]:
    if platform.system() != "Darwin":
        return None
    try:
        out = subprocess.check_output(
            ["osascript", "-e",
             'tell application "System Events" to tell process "Cursor" '
             'to get name of front window'],
            stderr=subprocess.DEVNULL, timeout=2.0,
        )
        return out.decode("utf-8", errors="ignore").strip()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, FileNotFoundError):
        return None


class CursorHeuristic(Source):
    """macOS only. On other OSes, this source just sits idle."""

    def __init__(self, on_event, interval_s: float = 2.0) -> None:
        super().__init__("cursor", on_event)
        self.interval_s = interval_s

    def _run(self) -> None:
        if platform.system() != "Darwin":
            # no-op on non-macOS
            while not self._stop.is_set():
                self._stop.wait(60.0)
            return
        last_state = None
        while not self._stop.is_set():
            title = _get_cursor_window()
            if title is None:
                # Cursor not running — emit idle once, then sleep
                if last_state != "idle":
                    self.emit(SourceEvent(source="cursor", status="idle"))
                    last_state = "idle"
            else:
                t = title.lower()
                if any(k in t for k in KEYWORDS):
                    if last_state != "processing":
                        self.emit(SourceEvent(
                            source="cursor", status="processing",
                            message=title[:60],
                        ))
                        last_state = "processing"
                else:
                    if last_state != "idle":
                        self.emit(SourceEvent(source="cursor", status="idle"))
                        last_state = "idle"
            self._stop.wait(self.interval_s)
