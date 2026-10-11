"""Draws the FOHScope app icon (Resources/AppIcon.png, 1024x1024).

Same family as LiveRack / VenueTuner: flat dark macOS tile, faint grid, no gradients, no glow.
1/3-octave RTA bars in the VenueTuner teal, one narrow peak in red with a feedback marker
above it (what is ringing), and a spectrograph strip along the bottom.
Writes AppIcon.png (1024), AppIcon.ico (Windows) and fohscope.png (256, Linux).
Run: python3 Resources/make_icon.py
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw

S = 4
W = 1024 * S

TILE = (22, 23, 24)
TILE_EDGE = (52, 56, 61)
GRID = (36, 39, 42)
TEAL = (79, 209, 197)
TEAL_DIM = (39, 104, 98)
RED = (229, 72, 77)
AMBER = (224, 179, 65)
HEAT = [(0, 0, 0), (27, 12, 65), (106, 23, 110), (187, 55, 84), (241, 113, 31), (251, 199, 42), (252, 255, 164)]


def px(v):
    return int(round(v * S))


def rect(d, box, **kw):
    d.rectangle([px(c) for c in box], **kw)


def heat(t):
    t = max(0.0, min(1.0, t)) * (len(HEAT) - 1)
    i = min(int(t), len(HEAT) - 2)
    f = t - i
    return tuple(int(a + (b - a) * f) for a, b in zip(HEAT[i], HEAT[i + 1]))


def main():
    icon = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(icon)
    d.rounded_rectangle([px(100), px(100), px(924), px(924)], radius=px(185), fill=TILE, outline=TILE_EDGE, width=px(4))

    left, right, top, base = 190, 834, 230, 690
    for y in (top + 115, top + 230, top + 345):
        d.line([px(left), px(y), px(right), px(y)], fill=GRID, width=px(4))
    for x in (left + 161, left + 322, left + 483):
        d.line([px(x), px(top), px(x), px(base)], fill=GRID, width=px(4))

    # RTA: 15 bars, a pink-ish house curve with one ringing band.
    n, gap = 15, 10
    w = (right - left - gap * (n - 1)) / n
    ring = 10
    for i in range(n):
        h = 0.42 + 0.16 * math.sin(i * 0.55 + 0.4) - 0.012 * i
        if i == ring:
            h = 0.93
        x0 = left + i * (w + gap)
        y0 = base - h * (base - top)
        rect(d, (x0, y0, x0 + w, base), fill=RED if i == ring else TEAL)
        rect(d, (x0, y0, x0 + w, y0 + 12), fill=RED if i == ring else (150, 235, 226))
        if i != ring:
            rect(d, (x0, y0 + 12, x0 + w, base), fill=TEAL_DIM)
            rect(d, (x0, y0, x0 + w, y0 + 12), fill=TEAL)

    # Feedback marker over the ringing band.
    cx = left + ring * (w + gap) + w / 2
    ty = top - 4
    d.polygon([(px(cx - 26), px(ty - 34)), (px(cx + 26), px(ty - 34)), (px(cx), px(ty))], fill=AMBER)

    # Spectrograph strip: rows of heat, the ring as a bright vertical line.
    sy0, sy1 = 726, 834
    cols, rows = 48, 6
    cw, rh = (right - left) / cols, (sy1 - sy0) / rows
    rx = (cx - left) / (right - left)
    for r in range(rows):
        for c in range(cols):
            f = c / cols
            t = 0.42 + 0.18 * math.sin(f * 6 + r * 0.9) - 0.15 * f
            if abs(f - rx) < 1.2 / cols:
                t = 1.0 - r * 0.04
            x0 = left + c * cw
            y0 = sy0 + r * rh
            rect(d, (x0, y0, x0 + cw + 0.5, y0 + rh + 0.5), fill=heat(t))
    rect(d, (left, sy0, right, sy1), outline=TILE_EDGE, width=px(3))

    out = Path(__file__).with_name("AppIcon.png")
    full = icon.resize((1024, 1024), Image.LANCZOS)
    full.save(out)
    # Windows installer icon and the Linux AppImage / desktop icon.
    full.save(out.with_name("AppIcon.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    full.resize((256, 256), Image.LANCZOS).save(out.with_name("fohscope.png"))
    print(out)


if __name__ == "__main__":
    main()
