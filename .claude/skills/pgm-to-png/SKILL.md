---
name: pgm-to-png
description: Convert PGM dumps (from `make test-native`) to PNG using the existing `scripts/pgm-to-png.py` converter — never reach for `convert`/`magick`/`ffmpeg`/PIL one-liners/`for`-loops. The script already handles batch mode (whole `.tmp/v2-pgm/`), single-file mode, and `--bands` for pixel-band Y-ranges. Use whenever the user asks to convert, visualize, render, or "look at" PGM output, or asks for pixel-band/Y-range info.
---

# pgm-to-png

The repo ships a dedicated converter at `scripts/pgm-to-png.py`. **Always use
it.** Do not reimplement conversion via `convert`, `magick`, `ffmpeg`, ad-hoc
PIL snippets, or `for f in *.pgm; do …; done` loops. The script:

- Reads PGMs from `.tmp/v2-pgm/` by default (project's scratch dir — see
  `[[scratch-files-go-to-tmp]]`).
- Writes PNGs next to their PGMs (`02-normal.pgm` → `02-normal.png`).
- Has a `--bands` mode that prints contiguous Y-ranges of paper pixels —
  the calibration tool for `layout.cpp` Y-constants.

## Invocations

| Goal | Command |
|------|---------|
| Convert every PGM in `.tmp/v2-pgm/` | `scripts/pgm-to-png.py` |
| Convert one file | `scripts/pgm-to-png.py path/to/foo.pgm` |
| Convert + print pixel bands | `scripts/pgm-to-png.py --bands` |
| Convert one file + bands | `scripts/pgm-to-png.py foo.pgm --bands` |

Use the **relative** path `scripts/pgm-to-png.py` (allowlist-friendly, see
`[[bash-no-prompts]]`). The script's shebang is `#!/usr/bin/env python3`, so
no `python` prefix is needed.

## When to invoke

Proactively, whenever:

- The user wants to "see", "look at", "render", or "visualize" PGM output.
- The user wants pixel-band / Y-range info to calibrate `layout.cpp`.
- You just ran `make test-native` and want to inspect the dump.
- The user mentions `.tmp/v2-pgm/`, PGM files, or "convert to PNG".

## What NOT to do

- ❌ `convert .tmp/v2-pgm/*.pgm` (ImageMagick — bypasses `--bands`,
  not what the project uses, may not be installed).
- ❌ `for f in .tmp/v2-pgm/*.pgm; do convert "$f" "${f%.pgm}.png"; done`
  (chained bash + duplicates the script's logic).
- ❌ Inline Python: `python -c "from PIL import Image; …"` (reinvents the
  script, no `--bands`, allowlist miss).
- ❌ Asking the user "should I write a conversion script?" — one already
  exists.

## If the script fails

Read it (`Read scripts/pgm-to-png.py`) and surface the actual error to the
user. Do not work around a broken converter by reimplementing it inline —
fix the script.
