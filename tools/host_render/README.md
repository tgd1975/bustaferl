# Host render harness

Regenerate `docs/screenshots/*.png` from the **real** renderer
(`src/render/layout.cpp`) on a PC — no ESP32 required.

`render/layout.cpp` depends on Adafruit GFX, which hard-includes `Arduino.h` /
`Print.h` / `pgmspace.h`. This harness supplies minimal host shims for those
(in [`shim/`](shim/)) so the exact firmware drawing code compiles and runs on
the host; the output is therefore pixel-identical to what the device draws
(same glyphs, same coordinates), just thresholded to 1-bit and saved as PNG.

## Run

```bash
tools/host_render/render.sh
```

Requirements:

- `g++`
- `python3` + Pillow (`pip install Pillow`)
- network access to `raw.githubusercontent.com` (Adafruit GFX is fetched, not
  vendored; the version is pinned to match `platformio.ini`)

ArduinoJson is **not** needed — `layout.cpp` does not parse JSON.

## What it does

1. fetches Adafruit GFX into `$TMPDIR/bustaferl-host-render/gfx`
2. compiles `render_driver.cpp` + `src/render/layout.cpp` + `Adafruit_GFX.cpp`
   against the shims
3. runs the driver, which renders the nine display states to 1-bpp `.bin`
   framebuffers
4. `to_png.py` converts them to 2× grayscale PNGs in `docs/screenshots/`

## Files

| File | Purpose |
|---|---|
| `render_driver.cpp` | builds a `RenderInput` per state, calls `renderFrame`, dumps the framebuffer |
| `shim/` | minimal host stand-ins for `Arduino.h` / `Print.h` / `pgmspace.h` and the two BusIO headers GFX 1.11.9 includes |
| `to_png.py` | 1-bpp `.bin` → 2× grayscale PNG |
| `render.sh` | fetch + compile + run + convert |

## Scope

Covers `render/layout.cpp` only. The on-device path (GxEPD2 panel I/O in
`hal/Esp32Display.cpp`) still needs the device; this harness verifies the
**layout**, not the panel driver.
