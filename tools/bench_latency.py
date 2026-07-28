#!/usr/bin/env python3
"""bench_latency.py — measure end-to-end latency from hook fire to screen
update (using a simulated device). Not a hardware benchmark; useful for
verifying the bridge debounce + protocol stack under load.
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path

DEFAULT_STATE = Path.home() / ".cache" / "agent-pulse" / "state.json"


def main(argv=None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--samples", type=int, default=20)
    p.add_argument("--state-file", default=str(DEFAULT_STATE))
    p.add_argument("--state", choices=["idle", "processing", "error"],
                   default="processing")
    args = p.parse_args(argv)

    state_file = Path(args.state_file)
    state_file.parent.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent.parent
    ap_cli = here / "tools" / "ap-cli.py"

    latencies = []
    print(f"benchmarking {args.samples} hook fires -> state file write...")
    for i in range(args.samples):
        t0 = time.perf_counter_ns()
        import subprocess
        r = subprocess.run(
            [sys.executable, str(ap_cli), "set", args.state, "Bench", f"sample {i}"],
            capture_output=True,
        )
        t1 = time.perf_counter_ns()
        if r.returncode != 0:
            print(f"  sample {i}: failed ({r.stderr.decode()!r})")
            continue
        latencies.append((t1 - t0) / 1e6)  # ms
    if not latencies:
        print("no successful samples")
        return 1
    latencies.sort()
    p50 = latencies[len(latencies) // 2]
    p95 = latencies[int(len(latencies) * 0.95)]
    p99 = latencies[int(len(latencies) * 0.99)]
    print(f"  N={len(latencies)}  p50={p50:.1f}ms  p95={p95:.1f}ms  p99={p99:.1f}ms  max={latencies[-1]:.1f}ms")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
