"""File source: watch a state.json file written by Claude Code hooks.

This is the most reliable path. The hooks write
~/.cache/agent-pulse/state.json with an atomic tmp+mv; we observe the change
via watchdog (inotify/FSEvents/ReadDirectoryChangesW).
"""
from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Optional

try:
    from watchdog.observers import Observer
    from watchdog.events import FileSystemEventHandler
    _HAS_WATCHDOG = True
except ImportError:  # pragma: no cover
    _HAS_WATCHDOG = False

from . import Source
from ..state_machine import SourceEvent


DEFAULT_PATH = Path.home() / ".cache" / "agent-pulse" / "state.json"


class FileSource(Source):
    def __init__(self, on_event, path: Optional[Path] = None) -> None:
        super().__init__("file", on_event)
        self.path = Path(path) if path else DEFAULT_PATH

    def _run(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        if not self.path.exists():
            self.path.write_text('{"status":"idle"}\n')

        if _HAS_WATCHDOG:
            self._watchdog_loop()
        else:
            self._polling_loop()

    # ---- fallback: 250ms polling -----------------------------------------

    def _polling_loop(self) -> None:
        last_mtime = 0.0
        last_payload = None
        while not self._stop.is_set():
            try:
                if self.path.exists():
                    m = self.path.stat().st_mtime
                    if m != last_mtime:
                        last_mtime = m
                        payload = self.path.read_text()
                        if payload != last_payload:
                            last_payload = payload
                            self._dispatch(payload)
            except Exception as exc:
                sys.stderr.write(f"[file] {exc}\n")
            self._stop.wait(0.25)

    # ---- watchdog --------------------------------------------------------

    def _watchdog_loop(self) -> None:
        outer = self

        class Handler(FileSystemEventHandler):
            def on_modified(self, event):
                if event.is_directory:
                    return
                if Path(event.src_path) == outer.path:
                    try:
                        outer._dispatch(Path(event.src_path).read_text())
                    except Exception as exc:
                        sys.stderr.write(f"[file] {exc}\n")

            def on_created(self, event):
                self.on_modified(event)

            def on_moved(self, event):
                if Path(event.dest_path) == outer.path:
                    try:
                        outer._dispatch(Path(event.dest_path).read_text())
                    except Exception as exc:
                        sys.stderr.write(f"[file] {exc}\n")

        obs = Observer()
        obs.schedule(Handler(), str(self.path.parent), recursive=False)
        obs.start()
        try:
            # also do an initial read
            self._dispatch(self.path.read_text())
            while not self._stop.is_set():
                self._stop.wait(0.5)
        finally:
            obs.stop()
            obs.join(timeout=1.0)

    def _dispatch(self, payload: str) -> None:
        try:
            data = json.loads(payload)
        except json.JSONDecodeError:
            return
        status = data.get("status", "idle")
        if status not in ("idle", "processing", "error", "permission"):
            status = "idle"
        ev = SourceEvent(
            source="file",
            status=status,
            tool=data.get("tool") or None,
            message=data.get("message") or None,
            progress=data.get("progress"),
        )
        self.emit(ev)


import sys
