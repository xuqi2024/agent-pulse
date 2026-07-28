"""Sink: writes RenderState to a destination (serial device)."""
from __future__ import annotations

import threading
import time
from typing import Callable, Optional

from ..protocol import State, Hello
from ..state_machine import RenderState


class SerialSink:
    """Sends State messages to the device. Reconnects on failure."""

    def __init__(self, port: str, baud: int = 115200,
                 on_inbound: Optional[Callable[[dict], None]] = None) -> None:
        self.port = port
        self.baud = baud
        self._on_inbound = on_inbound
        self._ser = None
        self._lock = threading.Lock()
        self._seq = 0
        self._stop = threading.Event()
        self._reader: Optional[threading.Thread] = None
        self._hello_sent = False
        self._backoff = 1.0

    def open(self) -> bool:
        try:
            import serial  # type: ignore
        except ImportError:
            sys.stderr.write("[sink] pyserial not installed\n")
            return False
        try:
            self._ser = serial.Serial(self.port, self.baud, timeout=0.1)
        except Exception as exc:
            sys.stderr.write(f"[sink] open {self.port}: {exc}\n")
            self._ser = None
            return False
        # Start reader
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()
        # Send hello
        self._send_raw(Hello(device_name="agent-pulse-bridge").to_json())
        self._hello_sent = True
        self._backoff = 1.0
        return True

    def close(self) -> None:
        self._stop.set()
        if self._ser:
            try:
                self._ser.close()
            except Exception:
                pass
            self._ser = None

    def write_state(self, st: RenderState) -> None:
        if not self._ser:
            return
        self._seq += 1
        msg = State(
            status=st.status,
            tool=st.tool or None,
            message=st.message or None,
            progress=None if st.progress == 255 else st.progress,
            seq=self._seq,
        )
        self._send_raw(msg.to_json())

    def _send_raw(self, s: str) -> None:
        with self._lock:
            if not self._ser:
                return
            try:
                self._ser.write(s.encode("utf-8"))
            except Exception as exc:
                sys.stderr.write(f"[sink] write: {exc}\n")
                self._reconnect()

    def _read_loop(self) -> None:
        from ..protocol import parse_line  # local import
        buf = b""
        while not self._stop.is_set():
            if not self._ser:
                self._stop.wait(self._backoff)
                continue
            try:
                chunk = self._ser.read(256)
            except Exception as exc:
                sys.stderr.write(f"[sink] read: {exc}\n")
                self._reconnect()
                continue
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, _, buf = buf.partition(b"\n")
                obj = parse_line(line.decode("utf-8", errors="ignore"))
                if obj and self._on_inbound:
                    try:
                        self._on_inbound(obj)
                    except Exception:
                        pass

    def _reconnect(self) -> None:
        if self._ser:
            try:
                self._ser.close()
            except Exception:
                pass
            self._ser = None
        self._backoff = min(self._backoff * 2.0, 10.0)
        time.sleep(self._backoff)
        self.open()


import sys
