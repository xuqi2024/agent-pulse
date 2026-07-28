"""Copilot heuristic: a tiny HTTP server that accepts status reports from
the bundled VSCode extension (vscode-extension/).

The extension POSTs to http://127.0.0.1:7711/status with a JSON body. We
forward the events to the state machine.
"""
from __future__ import annotations

import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Optional

from . import Source
from ..state_machine import SourceEvent


DEFAULT_PORT = 7711


class CopilotHeuristic(Source):
    def __init__(self, on_event, port: int = DEFAULT_PORT) -> None:
        super().__init__("copilot", on_event)
        self.port = port
        self._server: Optional[ThreadingHTTPServer] = None
        self._thread: Optional[threading.Thread] = None

    def _run(self) -> None:
        outer = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *_args, **_kw):  # silence default logging
                return

            def do_POST(self):  # noqa: N802
                if self.path != "/status":
                    self.send_response(404); self.end_headers(); return
                length = int(self.headers.get("Content-Length", "0"))
                body = self.rfile.read(length).decode("utf-8", errors="ignore")
                try:
                    obj = json.loads(body)
                except json.JSONDecodeError:
                    self.send_response(400); self.end_headers(); return
                status = obj.get("status", "idle")
                if status not in ("idle", "processing", "error", "permission"):
                    status = "idle"
                outer.emit(SourceEvent(
                    source="copilot",
                    status=status,
                    tool=obj.get("tool"),
                    message=obj.get("message"),
                ))
                self.send_response(204); self.end_headers()

            def do_GET(self):  # noqa: N802
                if self.path == "/health":
                    self.send_response(200)
                    self.send_header("Content-Type", "text/plain")
                    self.end_headers()
                    self.wfile.write(b"ok")
                else:
                    self.send_response(404); self.end_headers()

        try:
            self._server = ThreadingHTTPServer(("127.0.0.1", self.port), H)
        except OSError as exc:
            sys.stderr.write(f"[copilot] cannot bind 127.0.0.1:{self.port}: {exc}\n")
            return
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()
        # emit initial idle
        self.emit(SourceEvent(source="copilot", status="idle"))
        try:
            while not self._stop.is_set():
                self._stop.wait(0.5)
        finally:
            self._server.shutdown()
            self._server.server_close()


import sys
