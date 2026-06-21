#!/usr/bin/env bash
# Regenerate docs/screenshots/*.png from the REAL renderer
# (src/render/layout.cpp) on the host — no ESP32 required.
#
# It fetches Adafruit GFX (the version platformio.ini pins) into a scratch dir,
# compiles layout.cpp against it plus the minimal Arduino shims in shim/, runs
# the driver to dump 1-bpp framebuffers, and converts them to PNGs.
#
# Requires: g++, python3 + Pillow, and network access to
# raw.githubusercontent.com (GFX is not vendored). ArduinoJson is NOT needed —
# layout.cpp does not parse JSON.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
GFXVER="1.11.9" # keep in sync with platformio.ini
WORK="${TMPDIR:-/tmp}/bustaferl-host-render"
mkdir -p "$WORK/gfx" "$WORK/frames"

base="https://raw.githubusercontent.com/adafruit/Adafruit-GFX-Library/$GFXVER"
for f in Adafruit_GFX.h Adafruit_GFX.cpp gfxfont.h glcdfont.c; do
  [ -f "$WORK/gfx/$f" ] || curl -fsSL -o "$WORK/gfx/$f" "$base/$f"
done

g++ -std=gnu++17 -DARDUINO=10819 \
  -I "$ROOT/src" -I "$HERE/shim" -I "$WORK/gfx" \
  "$HERE/render_driver.cpp" "$ROOT/src/render/layout.cpp" \
  "$WORK/gfx/Adafruit_GFX.cpp" -o "$WORK/render"

BUSTAFERL_FRAMES_DIR="$WORK/frames" "$WORK/render"
BUSTAFERL_FRAMES_DIR="$WORK/frames" python3 "$HERE/to_png.py"

echo "done -> $ROOT/docs/screenshots/"
