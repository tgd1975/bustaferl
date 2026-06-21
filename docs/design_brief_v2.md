# Design brief: bustaferl e-paper departure board (v2)

A prompt for claude.ai/design. The look is yours to craft; the constraints in
section 2 are hard physics. Anything that violates them cannot be built on the
device, and the design fails. A previous attempt failed exactly because it
ignored these (web fonts, opacity, umlauts, overflowing the canvas so the two
times collided into `07:3207:47`). Treat section 2 as the contract and design
freely inside it.

## 0. How to use this brief

You are designing a static information layout for a tiny 1-bit e-paper screen
driven by a microcontroller. Produce a confident, legible little station board.
Then hand back not just pictures but a pixel-exact spec (section 6) so the
firmware author can reproduce your design 1:1 with a very limited graphics
library.

## 1. The device and its purpose

- **Panel:** Waveshare 4.2" e-paper, **400 x 300 pixels, black and white only**.
- **Where:** mounted in a hallway. A person glances at it on the way out to
  decide "leave now or wait?". It shows **raw next-departure times only** — no
  countdowns, no current clock, no "leave now" advice, no recommendations.
- **How it is drawn:** an ESP32 using the **Adafruit GFX** library. Think of it
  as an HTML5 Canvas 2D context with **no anti-aliasing and no color** — only
  the primitives listed in section 2.4. The board is drawn **white on black**,
  like an old split-flap / LED station sign.

## 2. Hard rendering contract (non-negotiable)

### 2.1 Canvas

- Exactly **400 x 300 px**. Origin `(0,0)` is top-left; x grows right, y grows
  down.
- **Nothing** may be placed outside `0..399` horizontally or `0..299`
  vertically.
- No two elements may overlap unless intentionally composited (e.g. ink text on
  a paper-filled badge).

### 2.2 Color

- **1 bit per pixel. Two colors only: INK and PAPER.** On the device the
  background is INK (black) and the foreground is PAPER (white).
- **Forbidden:** anti-aliasing, grayscale, opacity / alpha, gradients, drop
  shadows, blur, dithered "gray". There is no half-tone. Every pixel is fully
  ink or fully paper.
- To de-emphasize something, make it **smaller** or **remove** it — never shade
  it.

### 2.3 Text and fonts

Only these fonts exist on the device. Pick per text element from this menu:

1. **Classic 6x8 monospace, integer-scaled** (the recommended default —
   predictable and tabular, every glyph the same width):
   - size 1 = `6 x 8` px per character cell (a 5x7 glyph plus 1 px spacing)
   - size 2 = `12 x 16`, size 3 = `18 x 24`, size 4 = `24 x 32`
   - Only **integer** sizes. No 1.5x.
2. **Bundled bitmap "FreeFonts"** (1-bit, no anti-aliasing):
   - `FreeMono` / `FreeMonoBold` / `FreeMonoOblique` (monospace)
   - `FreeSans` / `FreeSansBold` / `FreeSansOblique` (proportional)
   - `FreeSerif` / `FreeSerifBold` / `FreeSerifOblique` (proportional)
   - each available at **9, 12, 18, or 24 pt**
   - tiny fonts: `TomThumb` (~3x5), `Picopixel`, `Org_01`

Rules:

- **Any column of times that must line up MUST use a monospace font** (classic,
  or `FreeMono*`). Proportional digits will misalign the columns.
- **ASCII only** (`0x20`–`0x7E`). **Forbidden characters:** umlauts and eszett
  (`a-umlaut o-umlaut u-umlaut`, `ss`), typographic arrows and shapes
  (`-> arrow`, `down-triangle`, `diamond`, `filled-square`, `bullet`,
  `dotted-circle`, em/en dash), smart quotes, middot, ellipsis. These do not
  exist in the device fonts and will render as blanks or garbage.
  - German workarounds (already the project convention): a-umlaut -> `ae`,
    o-umlaut -> `oe`, u-umlaut -> `ue`, eszett -> `ss`, or use uppercase
    (`SUED...`). Use ASCII `->` for an arrow and `--` for a dash.
  - If you want a real arrow, an S-Bahn pictogram, or a marker glyph, you
    **must** supply it as a 1-bit bitmap icon (section 6E). It will be blitted,
    not typed.
- Cursor convention to assume in your spec: for the **classic** font the
  `(x,y)` you give is the **top-left** of the first character cell; for the
  **FreeFonts** it is the **baseline** (left edge, baseline). State which you
  used per text element.

### 2.4 Shapes (the entire toolbox)

This is the complete set of drawing operations the device has. If you cannot
express an element with these, redesign it.

- `pixel`, `line`, horizontal line, vertical line
- `rect` (outline), `fillrect`
- `roundrect` (outline or filled)
- `circle` (outline), `fillcircle`
- `triangle` (outline), `filltriangle`
- `bitmap` — blit a 1-bit image you provide
- `text` — a string in one of the section 2.3 fonts

No other curves (only circles), no polygons (only triangles), no fonts beyond
the menu.

### 2.5 Layout discipline

- **Positions are absolute pixel coordinates.** Do not rely on flexbox/grid
  auto-sizing, text wrapping, line-height, or font fallback. The device places
  every glyph at a coordinate you computed.
- **Truncate, never wrap.** If a label is too long for its box, cut it and end
  with `.` (the project already does this: `Atzgers.` for Atzgersdorf). Never
  wrap a data row to a second line.
- **Two adjacent time columns must have an explicit gap.** Show the arithmetic
  that the widest case fits (this is the exact failure of the last attempt).

### 2.6 Refresh-friendliness (nice to have, not required)

The screen updates by **partial refresh**. Regions that change often (the
times; any status banner) ideally start at an x-coordinate that is a **multiple
of 8** and do not share a horizontal band with static text, so an update does
not smear its neighbors. Treat this as a tie-breaker, not a hard rule.

## 3. What the screen shows

Three stacked sections, top to bottom. The device is mounted at the
Tullnertalgasse bus stop.

1. **`TULLNERTALGASSE`** (this stop)
   - `58A -> Atzgers.` : next **two** departures, e.g. `07:32  07:47`
   - `58A -> Hietzing` : next **two** departures, e.g. `07:35  07:50`
2. **`ENDEMANNGASSE`** (the next stop; carries the note `nach Schleife`)
   - `58B -> Atzgers.` : next **two** departures, e.g. `07:41  07:56`
3. **`ATZGERSDORF S-BAHN`**, direction **`-> Hauptbahnhof`**
   - The next **two trains**, each labelled with its **own** line, which varies
     per train: `S2`, `S3`, `S4`, or `REX1`. The line label sits **before** its
     time, e.g. `S2 07:33   S3 07:39`.

Per-time tokens you must design for (all are exactly 5 characters):

- `07:32` — a real departure time (`HH:MM`, 24-hour, zero-padded).
- `--:--` — no departure available for that slot.
- `??:??` — **stale**: shown for **all** times when the data is too old to
  trust.

Labels:

- Bus rows have a **fixed** line (`58A`, `58B`). S-Bahn slots **vary** (`S2`..
  `S4`, `REX1`). **Reserve the line column for the widest realistic label,
  `REX1` (4 chars).** (Rare longer labels are abbreviated to `xx` by the
  firmware — design for 4.)
- Direction text is short and already truncated: `Atzgers.`, `Hietzing`.

Optional, your call: each time is internally either **live** (real-time) or
**plan** (from the timetable). The project has historically shown **no**
difference between them. If you want to distinguish plan times, use a
**1-bit-safe** marker (e.g. a small hollow square drawn next to the time with
`rect`) — **never** gray or opacity. State your choice and spec the marker.

## 4. The states to design

The firmware selects the state; your layout renders each one. **Keep the
sections and labels recognizable across all states** so the user always knows
which stop they are looking at. Provide one mockup per state.

1. **Normal** — current data. A mix of real times and the odd `--:--` is normal.
2. **Veraltet (stale)** — every time becomes `??:??`. Show a clear `VERALTET`
   signal. Structure and labels stay; only the times blank out.
3. **No departures / night** — many or all slots are `--:--`, with **no** error
   signal (this is normal outside service hours).
4. **Partial** — some slots `--:--`, the rest normal (e.g. `58B` has only one
   upcoming bus).
5. **58B filter broken** — show `58B Filter ungueltig` (the direction filter
   stopped matching). Other rows still fine.
6. **OEBB API auth invalid** — show `OEBB-API: Auth ungueltig`, affecting the
   S-Bahn section (the rail backend rejected the request). Bus rows still fine.
7. **Start failed (cold boot)** — the device could not get online at power-up;
   show `Start fehlgeschlagen`.

Optional 8th: a boot / loading splash (`bustaferl`, `laedt Fahrplan ...`) shown
before the first data arrives.

All status strings are ASCII exactly as written above: `VERALTET`,
`58B Filter ungueltig`, `OEBB-API: Auth ungueltig`, `Start fehlgeschlagen`.

## 5. Your creative latitude

Inside section 2 you choose everything about the look:

- typography pairing from the font menu (e.g. a heavier header font over
  monospace data)
- section headers: rules / underlines, inversion, spacing rhythm
- how line labels read: plain text, or an inverted "roll-sign" **badge** (a
  `fillrect` in paper with `ink` text on top)
- how stale / error states are signalled: a bottom banner, a full-screen takeover,
  or inline — your choice
- optional iconography, declared as bitmaps (section 6E)
- whether to distinguish live vs plan times

Make it crisp and intentional, and **use the full 400 x 300** — the last attempt
left dead space at the bottom and then overflowed the right edge. Fill the
canvas deliberately.

## 6. Required deliverables

Produce **all** of the following. This is what makes the design buildable.

**A. Pixel-exact mockups, one per state**, each **exactly 400 x 300**, pure
black/white, **no anti-aliasing or smoothing**. Render them by drawing onto an
HTML `<canvas>` (or SVG with absolute coordinates) using **only** the section
2.4 primitives — `fillRect`/`strokeRect`, `lineTo`, `arc` for circles,
triangles, `putImageData`/`drawImage` for bitmaps, and monospace `fillText`
with `imageSmoothingEnabled = false` and font smoothing off. Drawing this way
guarantees the picture equals the spec. **Do not** build the layout with
flexbox/grid/auto text — absolute coordinates only.

**B. A draw-list per state** — an ordered table or JSON array of every drawing
operation, in device pixels, in draw order. Each entry has: `op` (one of
`text | hline | vline | line | rect | fillrect | roundrect | circle |
fillcircle | triangle | bitmap`), its coordinates (`x,y` plus `w,h` or
`x2,y2` or `r`), `color` (`ink | paper`), and for `text`: the exact `string`,
the `font` (from the section 2.3 menu), and the `size`/`pt`. This list should
read almost like the firmware's draw calls.

**C. Font declaration** — which fonts and sizes you used, and each one's
character cell width x height in pixels. For the classic font: `6*size x 8*size`.

**D. Fit proof** — for every row with two time columns, and for each S-Bahn
`label + time` slot, show the arithmetic that the widest content fits the
available width with the stated gap:
`chars * cell_width + gaps <= box_width <= 400 - margins`. Explicitly confirm no
element exceeds `0..399 / 0..299`.

**E. Custom glyphs (if any)** — every non-ASCII symbol you use (arrow, S-Bahn
logo, plan marker, status icon) as a **1-bit bitmap**: give its width x height
and the bit pattern, either as an XBM byte array or a `0/1` pixel grid. Keep
them small and legible at 1-bit.

**F. Region map** — a short table of the y-bands for the three sections and the
x of each column, so the structure is explicit and stable across states.

## 7. Reference: metrics and primitives

Use these exact numbers in your fit proof.

- Canvas `400 x 300`, origin top-left.
- **Classic font cell:** `width = 6 * size`, `height = 8 * size` (5x7 glyph
  inside). A 5-character string like `07:32` therefore measures
  `5 * 6 * size` wide:
  - size 2 -> `60` px wide, `16` tall
  - size 3 -> `90` px wide, `24` tall
- **FreeFont note:** treat each as a bitmap font at the stated pt. If you use
  one, keep a >=10% width safety margin in the fit proof because proportional
  advances vary. For time columns prefer the classic font or `FreeMono*`.
- **Primitive set** = `{ pixel, line, hline, vline, rect, fillrect, roundrect,
  circle, fillcircle, triangle, filltriangle, bitmap, text }`.
- **Color tokens:** `ink` (black, the background) and `paper` (white, the
  text). The canvas starts fully `ink`; every element you add paints `paper`
  (or paints `ink` back, e.g. text inside a paper badge).
- A **badge** = `fillrect(paper)` then `text(ink)` on top.

## 8. Worked example (match this format)

One Tullnertalgasse row, classic font size 2 (cell `12 x 16`), left margin 8,
row top at `y = 38`. Background is already `ink`.

```text
op    string            x    y    font/size      color   span (x0..x1)
text  "58A -> Atzgers."   8   38   classic/2      paper   8..188   (15 chars * 12 = 180)
text  "07:32"           220   38   classic/2      paper   220..280 (5 chars * 12 = 60)
text  "07:47"           320   38   classic/2      paper   320..380 (5 chars * 12 = 60)
```

Fit proof for this row:

- gap between the two times = `320 - 280 = 40` px (> 0, no collision)
- right edge `380 <= 392` (= `400 - 8` margin)
- nothing below `y = 38 + 16 = 54`, well inside 300

S-Bahn slot pattern (label reserved for `REX1` = 4 chars = 48 px at size 2):

```text
op    string   x    y    font/size   color   span
text  "S2"      8   246  classic/2   paper   8..32   (reserve 8..56 for up to REX1)
text  "07:33"  64   246  classic/2   paper   64..124
text  "S3"    210   246  classic/2   paper   210..234 (reserve 210..258)
text  "07:39" 266   246  classic/2   paper   266..326
```

Produce the whole board for every state in exactly this style. **If any element
would violate section 2, change the design until it does not** — do not hand
over anything that cannot be drawn with the listed primitives.
