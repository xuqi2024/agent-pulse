"""Serial port discovery: locate the CH340K or USB-OTG CDC port."""
from __future__ import annotations

import glob
import os
import sys
from dataclasses import dataclass
from typing import List, Optional


# Known USB-serial bridge VID:PID pairs, in order of preference.
KNOWN_DEVICES = [
    ("1A86:7523", "CH340K"),       # CH340K on SZPI-ESP32S3 board
    ("1A86:55D4", "CH9102"),       # CH9102 family
    ("10C4:EA60", "CP2102"),
    ("0403:6001", "FTDI"),
    ("303A:4002", "ESP32-S3 USB-OTG CDC"),
    ("303A:1002", "ESP32-S3 USB-Serial/JTAG"),
]


@dataclass
class Candidate:
    device: str
    label: str
    vid_pid: str = ""


def _all_candidates() -> List[Candidate]:
    out: List[Candidate] = []
    if sys.platform == "darwin":
        # macOS — use cu.* (call-out unit), not tty.* (gets blocked by DCD)
        for pattern in ("/dev/cu.wchusbserial*",
                        "/dev/cu.usbserial*",
                        "/dev/cu.SLAB_USBtoUART",
                        "/dev/cu.usbmodem*"):
            for p in sorted(glob.glob(pattern)):
                out.append(Candidate(device=p, label=os.path.basename(p)))
    elif sys.platform.startswith("linux"):
        # Linux: prefer stable by-id symlinks
        for p in sorted(glob.glob("/dev/serial/by-id/*")):
            out.append(Candidate(device=p, label=os.path.basename(p)))
        for p in sorted(glob.glob("/dev/ttyACM*")) + sorted(glob.glob("/dev/ttyUSB*")):
            out.append(Candidate(device=p, label=os.path.basename(p)))
    else:
        # Windows
        try:
            import serial.tools.list_ports as lp
            for c in lp.comports():
                out.append(Candidate(
                    device=c.device,
                    label=f"{c.manufacturer or '?'} {c.product or c.device}",
                    vid_pid=f"{c.vid:04X}:{c.pid:04X}" if c.vid is not None else "",
                ))
        except Exception:
            pass
    return out


def list_ports() -> List[Candidate]:
    return _all_candidates()


def pick_port(preferred: Optional[str] = None) -> Optional[Candidate]:
    """Pick the best port. If `preferred` matches a known device, use it."""
    cands = _all_candidates()
    if not cands:
        return None
    if preferred:
        for c in cands:
            if preferred in c.device:
                return c
    # Prefer CH340K, then anything with a known VID:PID
    for c in cands:
        bn = os.path.basename(c.device).lower()
        if "wchusbserial" in bn or "ch340" in bn:
            return c
    return cands[0] if cands else None
