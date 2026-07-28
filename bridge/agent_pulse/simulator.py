"""Headless simulator: render the three screens to PNG files using Pillow.

This is for development without a board. Run:
    agent-pulse simulate --state idle
    agent-pulse simulate --state processing
    agent-pulse simulate --state error

The output paths are controllable via --out.
"""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Optional

from PIL import Image, ImageDraw, ImageFont

from .state_machine import RenderState


W, H = 320, 240

# Color palette (RGB 888)
COLORS = {
    "bg_idle":   (14, 27, 44),
    "bg_proc":   (27, 31, 42),
    "bg_err":    (42, 14, 18),
    "fg_green":  (110, 227, 161),
    "fg_yellow": (255, 209, 102),
    "fg_red":    (255, 107, 107),
    "fg_white":  (255, 255, 255),
    "fg_gray":   (120, 130, 145),
    "fg_cyan":   (80, 220, 220),
}


def _try_font(size: int) -> ImageFont.ImageFont:
    candidates = [
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "C:/Windows/Fonts/consola.ttf",
    ]
    for c in candidates:
        if Path(c).exists():
            try:
                return ImageFont.truetype(c, size)
            except Exception:
                pass
    return ImageFont.load_default()


def render(state: RenderState, out_path: Path) -> Path:
    if state.status == "processing":
        bg = COLORS["bg_proc"]; accent = COLORS["fg_yellow"]
    elif state.status == "error":
        bg = COLORS["bg_err"]; accent = COLORS["fg_red"]
    else:
        bg = COLORS["bg_idle"]; accent = COLORS["fg_green"]

    img = Image.new("RGB", (W, H), bg)
    d = ImageDraw.Draw(img)

    # top bar
    d.rectangle([0, 0, W, 22], fill=bg)
    d.text((6, 4), "*  agent-pulse  *", fill=accent, font=_try_font(11))

    if state.status == "processing":
        d.text((6, 4), ">>>  AGENT RUNNING", fill=accent, font=_try_font(11))
        f_big = _try_font(20)
        f_body = _try_font(13)
        d.text((8, 38), f"tool:  {state.tool or 'agent'}", fill=accent, font=f_big)
        # message
        msg = (state.message or "")[:50]
        d.text((8, 78), msg, fill=COLORS["fg_white"], font=f_body)
        # progress bar
        if state.progress != 255:
            bar_x, bar_y, bar_w, bar_h = 8, 200, W - 16, 14
            d.rectangle([bar_x, bar_y, bar_x + bar_w, bar_y + bar_h],
                        fill=COLORS["bg_idle"])
            fill = int(bar_w * state.progress / 100)
            d.rectangle([bar_x, bar_y, bar_x + fill, bar_y + bar_h], fill=accent)
            d.text((bar_x + bar_w - 28, bar_y - 12), f"{state.progress}%",
                   fill=accent, font=_try_font(10))
    elif state.status == "error":
        f_huge = _try_font(22)
        d.text((W // 2 - 100, 50), "PERMISSION", fill=accent, font=f_huge)
        d.text((W // 2 - 70, 80), "REQUIRED", fill=accent, font=f_huge)
        d.text((20, 150), (state.message or "")[:30], fill=COLORS["fg_white"],
               font=_try_font(13))
        d.text((30, 210), "[press BOOT to clear]", fill=COLORS["fg_gray"],
               font=_try_font(11))
    else:
        # idle
        # breathing dot (static, full radius for a snapshot)
        cx, cy, r = W // 2, 80, 8
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=accent)
        f_huge = _try_font(28)
        f_sub = _try_font(14)
        bbox = d.textbbox((0, 0), "OK", font=f_huge)
        d.text(((W - (bbox[2] - bbox[0])) // 2, 120), "OK",
               fill=accent, font=f_huge)
        bbox2 = d.textbbox((0, 0), "standby", font=f_sub)
        d.text(((W - (bbox2[2] - bbox2[0])) // 2, 156), "standby",
               fill=COLORS["fg_gray"], font=f_sub)
        d.text((W // 2 - 60, 210), "waiting for agent ...",
               fill=COLORS["fg_gray"], font=_try_font(11))

    img.save(out_path)
    return out_path


def main(argv: Optional[list] = None) -> int:
    p = argparse.ArgumentParser(prog="agent-pulse-sim")
    p.add_argument("--state", choices=["idle", "processing", "error"],
                   default="idle")
    p.add_argument("--tool", default="Bash")
    p.add_argument("--message", default="")
    p.add_argument("--progress", type=int, default=255)
    p.add_argument("--out", default="examples/sim_{state}.png")
    args = p.parse_args(argv)
    out_str = args.out.format(state=args.state) if "{state}" in args.out else args.out
    out = Path(out_str)
    out.parent.mkdir(parents=True, exist_ok=True)
    state = RenderState(
        status=args.state, tool=args.tool, message=args.message,
        progress=args.progress, source="simulator",
    )
    saved = render(state, out)
    print(f"wrote {saved} ({W}x{H})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
