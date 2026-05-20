#!/usr/bin/env python3
"""
Convert PGM dumps from `make test-native` to PNG for visual inspection.

PNGs land next to their PGMs (.tmp/v2-pgm/02-normal.pgm → .tmp/v2-pgm/02-normal.png).

Usage:
    scripts/pgm-to-png.py                 # convert all .pgm in .tmp/v2-pgm/
    scripts/pgm-to-png.py --bands         # also print pixel-band Y-ranges per state
    scripts/pgm-to-png.py path/to/foo.pgm # convert a single file

Pixel-band mode lists every contiguous Y-range that contains paper pixels
(foreground on the inverted e-paper). Use it to verify section anchors when
calibrating layout.cpp Y-constants without re-eyeballing the PNG every time.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image

DEFAULT_DIR = Path(".tmp/v2-pgm")
BAND_MIN_PIXELS = 3  # ignore rows with <= this many paper pixels (noise floor)


def convert(pgm: Path) -> Path:
    png = pgm.with_suffix(".png")
    Image.open(pgm).save(png)
    return png


def bands(pgm: Path) -> list[tuple[int, int, int]]:
    """Return [(y_start, y_end, peak_paper_px), ...] for contiguous paint bands."""
    img = Image.open(pgm)
    w, h = img.size
    px = img.load()
    counts = [sum(1 for x in range(w) if px[x, y] > 128) for y in range(h)]
    result: list[tuple[int, int, int]] = []
    in_band = False
    start = 0
    for y in range(h):
        if counts[y] > BAND_MIN_PIXELS and not in_band:
            in_band = True
            start = y
        elif counts[y] <= BAND_MIN_PIXELS and in_band:
            in_band = False
            result.append((start, y - 1, max(counts[start:y])))
    if in_band:
        result.append((start, h - 1, max(counts[start:h])))
    return result


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="*", type=Path, help="PGM files (default: all in .tmp/v2-pgm/)")
    ap.add_argument("--bands", action="store_true", help="print pixel-band Y-ranges per file")
    args = ap.parse_args()

    targets = args.paths or sorted(DEFAULT_DIR.glob("*.pgm"))
    if not targets:
        print(f"no PGM files found in {DEFAULT_DIR}", file=sys.stderr)
        return 1

    for pgm in targets:
        png = convert(pgm)
        print(f"{pgm.name} -> {png.name} ({png.stat().st_size} B)")
        if args.bands:
            for y0, y1, peak in bands(pgm):
                print(f"  y={y0:3d}..{y1:3d} ({y1 - y0 + 1:3d} px), peak={peak}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
