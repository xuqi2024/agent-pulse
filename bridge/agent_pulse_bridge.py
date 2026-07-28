"""agent_pulse_bridge: CLI entry point.

Subcommands:
  daemon        Run the bridge: collect events from all sources, merge with
                debounce, forward to the serial device.
  doctor        Sanity check: list candidate serial ports, simulate, etc.
  simulate      Render a snapshot of a given state to a PNG.
  hello         Send a single hello message to the device and exit.
"""
from __future__ import annotations

import argparse
import sys
import time
import signal
from pathlib import Path
from typing import Optional

from agent_pulse.protocol import Hello
from agent_pulse.serial_link import list_ports, pick_port
from agent_pulse.state_machine import (
    StateMachine, RenderState, SourceEvent,
)
from agent_pulse.simulator import render as render_image
from agent_pulse.sources.file_source import FileSource
from agent_pulse.sources.cursor_heuristic import CursorHeuristic
from agent_pulse.sources.copilot_heuristic import CopilotHeuristic
from agent_pulse.sinks import SerialSink


def _cmd_doctor(args: argparse.Namespace) -> int:
    print("agent-pulse doctor")
    print("=" * 50)
    cands = list_ports()
    if not cands:
        print("  no candidate serial ports found")
    else:
        for c in cands:
            print(f"  {c.device}   ({c.label})")
    print()
    print("python deps:")
    for mod in ("serial", "watchdog", "PIL"):
        try:
            __import__(mod if mod != "PIL" else "PIL")
            print(f"  {mod:10s} OK")
        except ImportError:
            print(f"  {mod:10s} MISSING")
    print()
    print("state file:")
    from agent_pulse.sources.file_source import DEFAULT_PATH
    print(f"  {DEFAULT_PATH}")
    return 0


def _cmd_simulate(args: argparse.Namespace) -> int:
    from agent_pulse.simulator import main as sim_main
    argv = [
        "--state", args.state,
        "--tool", args.tool,
        "--message", args.message,
        "--progress", str(args.progress),
        "--out", args.out,
    ]
    return sim_main(argv)


def _cmd_hello(args: argparse.Namespace) -> int:
    import serial  # type: ignore
    try:
        s = serial.Serial(args.port, 115200, timeout=1.0)
        s.write(Hello(device_name="agent-pulse-bridge").to_json().encode("utf-8"))
        s.close()
        print(f"sent hello to {args.port}")
        return 0
    except Exception as exc:
        print(f"failed: {exc}", file=sys.stderr)
        return 1


def _cmd_daemon(args: argparse.Namespace) -> int:
    # 1) pick port
    port = args.port
    if not port or args.auto_port:
        cand = pick_port(args.port_hint)
        if not cand:
            print("no serial port found. Pass --port /dev/cu.wchusbserial* or similar.",
                  file=sys.stderr)
            return 2
        port = cand.device
        print(f"[bridge] using {port} ({cand.label})")

    # 2) set up sink
    sink = SerialSink(port, baud=115200)
    if not sink.open():
        print("failed to open serial port", file=sys.stderr)
        return 3

    # 3) state machine + sources
    sm = StateMachine()
    file_src = FileSource(on_event=sm.submit)
    cursor_src = CursorHeuristic(on_event=sm.submit)
    copilot_src = CopilotHeuristic(on_event=sm.submit)

    file_src.start()
    cursor_src.start()
    copilot_src.start()

    print("[bridge] running (Ctrl-C to stop)")

    stop = {"v": False}
    def on_sig(*_):
        stop["v"] = True
    signal.signal(signal.SIGINT, on_sig)
    signal.signal(signal.SIGTERM, on_sig)

    last_render_seq = -1
    while not stop["v"]:
        st = sm.tick()
        # Send every state change; bridge doesn't have a seq for now, so
        # SerialSink will assign one.
        sink.write_state(st)
        # 20 Hz render loop is overkill; 5 Hz is plenty for a state display.
        time.sleep(0.2)
    print("[bridge] stopping...")
    file_src.stop()
    cursor_src.stop()
    copilot_src.stop()
    sink.close()
    return 0


def main(argv: Optional[list] = None) -> int:
    p = argparse.ArgumentParser(prog="agent-pulse")
    p.add_argument("--version", action="store_true")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("doctor", help="check toolchain and ports")

    sim = sub.add_parser("simulate", help="render a state to PNG")
    sim.add_argument("--state", choices=["idle", "processing", "error"],
                     default="idle")
    sim.add_argument("--tool", default="Bash")
    sim.add_argument("--message", default="")
    sim.add_argument("--progress", type=int, default=255)
    sim.add_argument("--out", default="examples/sim_%s.png")

    dae = sub.add_parser("daemon", help="run the bridge")
    dae.add_argument("--port", help="serial port (e.g. /dev/cu.wchusbserial*)")
    dae.add_argument("--auto-port", action="store_true",
                     help="auto-pick the first known port")
    dae.add_argument("--port-hint", help="substring to match if auto-porting")

    hell = sub.add_parser("hello", help="send one hello message and exit")
    hell.add_argument("--port", required=True)

    args = p.parse_args(argv)
    if args.version:
        from agent_pulse import __version__
        print(f"agent-pulse {__version__}")
        return 0
    if args.cmd == "doctor":    return _cmd_doctor(args)
    if args.cmd == "simulate":  return _cmd_simulate(args)
    if args.cmd == "daemon":    return _cmd_daemon(args)
    if args.cmd == "hello":     return _cmd_hello(args)
    p.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
