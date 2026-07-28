"""Source base class."""
from __future__ import annotations

import threading
from typing import Callable, Optional

from ..state_machine import SourceEvent


class Source:
    """A source of SourceEvents. Subclass and implement _run().

    Lifecycle:
      - start() spawns a background thread that calls _run()
      - stop() signals shutdown; the thread should exit on self._stop.is_set()
    """

    def __init__(self, name: str, on_event: Callable[[SourceEvent], None]) -> None:
        self.name = name
        self._on_event = on_event
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._stop.clear()
        self._thread = threading.Thread(target=self._safe_run, name=f"ap-{self.name}", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=2.0)

    def _safe_run(self) -> None:
        try:
            self._run()
        except Exception as exc:  # don't crash the daemon
            sys.stderr.write(f"[{self.name}] {type(exc).__name__}: {exc}\n")

    def _run(self) -> None:  # pragma: no cover
        raise NotImplementedError

    def emit(self, ev: SourceEvent) -> None:
        self._on_event(ev)


import sys
