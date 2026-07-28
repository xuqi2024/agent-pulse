"""State machine: merge multiple sources with priority and debounce.

Sources (in priority order, highest wins):
  - file   (claude_code hooks)         priority 10
  - copilot (vscode extension)         priority  5
  - cursor  (AppleScript heuristic)    priority  3
  - manual  (ap-cli / file flag)       priority  1

Debounce:
  - processing: must be seen for at least DEBOUNCE_PROC_MS before rendering
  - idle:       must be stable for    DEBOUNCE_IDLE_MS before rendering

Return-to-idle:
  - processing -> idle: only when an explicit "end" arrives (no expiry)
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Optional, Dict


PRIORITY = {
    "file":   10,
    "copilot": 5,
    "cursor":  3,
    "manual":  1,
}

DEBOUNCE_PROC_MS = 300
DEBOUNCE_IDLE_MS = 800


@dataclass
class SourceEvent:
    source: str               # one of PRIORITY keys
    status: str               # "processing" | "idle"
    tool: Optional[str] = None
    message: Optional[str] = None
    progress: Optional[int] = None
    ts_ms: int = field(default_factory=lambda: int(time.time() * 1000))


@dataclass
class RenderState:
    status: str = "idle"
    tool: str = ""
    message: str = ""
    progress: int = 255     # 255 = N/A
    source: str = "boot"


class StateMachine:
    """Hold the latest event from each source; produce a debounced view."""

    def __init__(self) -> None:
        self._latest: Dict[str, SourceEvent] = {}
        self._last_emitted: RenderState = RenderState()
        self._pending: Optional[SourceEvent] = None
        self._pending_since_ms: int = 0

    # ---- input ------------------------------------------------------------

    def submit(self, ev: SourceEvent) -> None:
        if ev.source not in PRIORITY:
            return
        prev = self._latest.get(ev.source)
        if prev and self._event_equal(prev, ev):
            return  # dedupe identical events
        self._latest[ev.source] = ev

    def force_idle(self, source: str = "manual") -> None:
        self.submit(SourceEvent(source=source, status="idle"))

    # ---- output -----------------------------------------------------------

    def current(self) -> RenderState:
        """Best (highest-priority) event from the most recent tier."""
        best: Optional[SourceEvent] = None
        for src in sorted(self._latest.keys(), key=lambda s: -PRIORITY[s]):
            ev = self._latest[src]
            if ev.status == "processing":
                return self._render(ev)
            if best is None or PRIORITY[ev.source] > PRIORITY[best.source]:
                best = ev
        if best is not None:
            return self._render(best)
        return self._last_emitted

    def tick(self, now_ms: Optional[int] = None) -> RenderState:
        """Step the debounce. Returns the current state to render.

        The debounce rules:
        - The chosen event must hold for DEBOUNCE_PROC_MS (processing) or
          DEBOUNCE_IDLE_MS (idle) before being emitted.
        - If the event changes during the debounce window, restart the timer.
        """
        if now_ms is None:
            now_ms = int(time.time() * 1000)
        target = self.current()

        # Stable? emit and clear pending
        if self._states_equal(self._last_emitted, target):
            self._pending = None
            return self._last_emitted

        # (Re)start debounce if pending is None or target changed
        if self._pending is None or not self._states_equal(self._pending, target):
            self._pending = target
            self._pending_since_ms = now_ms
            return self._last_emitted

        # Pending matches target — check elapsed
        elapsed = now_ms - self._pending_since_ms
        need = DEBOUNCE_PROC_MS if target.status == "processing" else DEBOUNCE_IDLE_MS
        if elapsed >= need:
            self._last_emitted = target
            self._pending = None
            return self._last_emitted
        return self._last_emitted

    # ---- introspection ---------------------------------------------------

    def snapshot(self) -> dict:
        return {
            src: {
                "status": ev.status,
                "tool": ev.tool,
                "message": ev.message,
                "progress": ev.progress,
                "ts_ms": ev.ts_ms,
            }
            for src, ev in self._latest.items()
        }

    # ---- helpers ---------------------------------------------------------

    @staticmethod
    def _event_equal(a: SourceEvent, b: SourceEvent) -> bool:
        return (a.status == b.status and a.tool == b.tool
                and a.message == b.message and a.progress == b.progress)

    @staticmethod
    def _states_equal(a, b) -> bool:
        """Field-by-field comparison, normalized (None <-> '' <-> 255)."""
        def _n(x):
            tool = getattr(x, "tool", None) or ""
            msg = getattr(x, "message", None) or ""
            prog = getattr(x, "progress", None)
            if prog is None: prog = 255
            return (x.status, tool, msg, prog)
        return _n(a) == _n(b)

    def _to_event(self, st: RenderState, now_ms: int) -> SourceEvent:
        return SourceEvent(
            source=st.source,
            status=st.status,
            tool=st.tool or None,
            message=st.message or None,
            progress=None if st.progress == 255 else st.progress,
            ts_ms=now_ms,
        )

    def _render(self, ev: SourceEvent) -> RenderState:
        return RenderState(
            status=ev.status,
            tool=ev.tool or "",
            message=ev.message or "",
            progress=ev.progress if ev.progress is not None else 255,
            source=ev.source,
        )
