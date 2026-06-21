#!/usr/bin/env python3
"""Convert the 1-bpp framebuffers written by render_driver into PNGs.

Reads $BUSTAFERL_FRAMES_DIR/<name>.bin (400x300, row-major, MSB = leftmost,
bit set = white ink) and writes <name>.png into docs/screenshots/, upscaled 2x
(nearest) and 8-bit grayscale to match the existing screenshot format.
"""
import os

from PIL import Image

W, H = 400, 300
STRIDE = W // 8

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
FRAMES = os.environ.get("BUSTAFERL_FRAMES_DIR", "/tmp/frames")
DST = os.environ.get(
    "BUSTAFERL_SCREENSHOT_DIR", os.path.join(ROOT, "docs", "screenshots")
)

NAMES = [
    "01-normal", "02-partial-missing", "03-no-data", "04-stale",
    "05-filter-dead", "06-start-failed", "07-evening-hint",
    "08-oebb-auth", "09-boot",
]

os.makedirs(DST, exist_ok=True)
for name in NAMES:
    with open(os.path.join(FRAMES, name + ".bin"), "rb") as fh:
        data = fh.read()
    img = Image.new("L", (W, H), 0)  # black background
    px = img.load()
    for y in range(H):
        base = y * STRIDE
        for xb in range(STRIDE):
            byte = data[base + xb]
            for b in range(8):
                if (byte >> (7 - b)) & 1:  # set bit = white ink
                    px[xb * 8 + b, y] = 255
    img = img.resize((W * 2, H * 2), Image.NEAREST)
    img.save(os.path.join(DST, name + ".png"))
    print("saved", os.path.join(DST, name + ".png"))
