# Handoff: bustaferl Display Layout (V2)

> **Status: umgesetzt in v2.0.** Dieses Bundle ist die autoritative
> Referenz für das e-Paper-Layout. Die Pixel-Parität zwischen
> Host-Renderer und Gerät ist über `test_native_render_all_states`
> abgesichert.

## Overview

The new display layout for the bustaferl e-paper module. Shows the next bus and S‑Bahn departures relevant to the stop where the device is mounted (Tullnertalgasse), plus the next departures from Endemanngasse (the following stop) and from the S‑Bahn station Atzgersdorf toward Wien Hbf. A small network plan at the bottom anchors the user spatially.

Replaces the V1 5‑stream layout. The whole display runs **white‑on‑black** (inverted) like a station LED board.

## About the Design Files

The files in this bundle are **design references created in HTML/React**. They are not production code to copy. The task is to **recreate the visual design 1:1 on the real e‑paper device** using whatever rendering stack the bustaferl firmware already uses (e.g. GxEPD2, U8g2, LVGL, custom blitter on top of the Waveshare HAL).

To inspect the design, open `index.html` in a browser. To see the visual reference for each state, see `screen-*.png`.

## Fidelity

**High-fidelity.** Pixel‑perfect mockups at the real device resolution (400 × 300). Every position, font size, separator weight, and indicator was iterated against the actual display dimensions. Reproduce as closely as the chosen text rasterizer allows. The two pixel fonts (VT323 and Silkscreen) should be substituted with bitmap equivalents on device — see *Typography* below.

## Display Specs

- **Resolution:** 400 × 300 pixels
- **Module:** Waveshare 4.2" UC8176, B/W
- **Color mode:** 1‑bit. **No grayscale, no antialiasing, no opacity.** Every pixel is either ink (drawn) or paper (background). Anything that looks "dimmed" in the mocks should be either removed or replaced with a distinct shape — never half‑shaded.
- **Polarity:** Inverted. Background = ink (black), foreground = paper (white). On the canvas mocks paper is rendered as warm `#f4f1e8` for realism; on the device it is the e‑paper white.
- **Origin:** Top‑left = (0,0). All measurements below are in real device pixels.

## Typography

Two bitmap‑style fonts in the mocks. Substitute with on‑device bitmap fonts of similar metrics:

| Mock font | Used for | Substitute target |
|---|---|---|
| **VT323** (Google Fonts) | All data: times, line+direction text, plan indicator | A clean monospace ~12 px x‑height bitmap font with tall ascenders. `u8g2_font_logisoso16_tr` and the **Profont** family are good starting points. |
| **Silkscreen** (Google Fonts) | Section headers, FullscreenError text | Bold 6–8 px small caps bitmap font. `u8g2_font_5x8_tr`, `u8g2_font_helvR08_tr`, or a Silkscreen TTF rasterized to a bitmap. |

Sizes used in the design (logical px == device px since no scaling):

- Section header (TG): **12 px** Silkscreen 700, uppercase, letter‑spacing 1
- Section headers (EG, Atzg): **10 px** Silkscreen 700, uppercase, letter‑spacing 1
- TG departure rows: **28 px / 32 px line‑height** VT323
- EG departure row: **22 px / 24 px line‑height** VT323
- Atzg S‑Bahn row: **20 px / 24 px line‑height** VT323
- Network plan station labels: **7 px** Silkscreen, letter‑spacing 0.5
- Network plan "you are here" arrow ▼: **10 px** VT323
- FullscreenError glyph: **90 px** VT323
- FullscreenError title: **18 px** Silkscreen 700, letter‑spacing 2
- FullscreenError sub: **16 px** VT323
- FullscreenError foot: **8 px** Silkscreen, letter‑spacing 1
- Status banner (used in Veraltet only — but the banner itself was removed; keep capability for future use): **10 px** Silkscreen, letter‑spacing 1

Numbers must be **tabular** — every digit the same width, so columns of times line up.

## Layout (Normal screen, 400 × 300)

```text
┌──────────────────────────────────────────────────┐
│ (10 px top padding · 18 px side padding)         │
│ TULLNERTALGASSE                                  │  ← header, 12 px Silkscreen
│ ┌─┐                                              │
│ │58A│  Atzgers.       18:32   18:48              │  ← 28 px VT323
│ └─┘                                              │
│ ┌─┐                                              │
│ │58A│  Hietzing       18:35   18:50 □            │  ← □ = plan marker
│ └─┘                                              │
│ ─────────────────────────────────────────  2 px  │  ← thick separator
│ ENDEMANNGASSE · NACH SCHLEIFE                    │  ← header, 10 px Silkscreen
│ ┌─┐                                              │
│ │58B│  Atzgers.       18:41   19:01              │  ← 22 px VT323
│ └─┘                                              │
│ ─────────────────────────────────────────  1 px  │  ← thin separator
│ ATZGERSDORF → WIEN HBF                           │  ← header, 10 px Silkscreen
│ ┌─┐         ┌─┐         ┌─┐                      │
│ │S2│ 18:37  │S3│ 18:51  │S4│ 19:05 □             │  ← 20 px VT323, 3 cols
│ └─┘         └─┘         └─┘                      │
│                                                  │
│              · ── ◆                              │
│                   │                              │
│                   │     ▼                        │
│              ◆ ── · ── ■ ── ·                    │  ← network plan
│             Hbf Atzg Ende Tull Hietz             │  ← labels, 7 px Silkscreen
└──────────────────────────────────────────────────┘
```

### Region boundaries (Y coordinates, device pixels)

| Y range | Region | Notes |
|---|---|---|
| 0 – ~98 | **Tullnertalgasse block** | 10 px top padding, 12 px header, 6 px gap, two 32‑px rows, 8 px bottom padding |
| ~98 – ~100 | 2 px solid separator | full width minus 0 px (edge to edge) |
| ~100 – ~150 | **Endemanngasse block** | 8 px top padding, 10 px header, 3 px gap, one 24‑px row, 8 px bottom padding |
| ~150 – ~151 | 1 px solid separator | full width |
| ~151 – ~300 | **Atzgersdorf block + Network plan** | 8 px top padding, 10 px header, 6 px gap, S‑Bahn row, then `margin-top: auto` pushes the network plan to the bottom |

Side padding: **18 px** left and right for all blocks.

### TG / EG row anatomy

Each row is a 3‑column grid: `auto | 1fr | auto`

- **Col 1 — Line badge**
  - `58A`, `58B`, etc. — Silkscreen 700, white text on paper rectangle
  - Sizes: `lg` (TG) = 14 px font, 2 × 5 padding, min‑width 28; `md` (EG) = 11 px font, 1 × 4 padding, min‑width 22; `sm` (S‑Bahn) = 9 px font, 0 × 3 padding, min‑width 18
  - **Invert host:** because the display is inverted, the badge is paper‑on‑ink relative to the inverted display, which means **white background, black text** when rendered on device.
- **Col 2 — Direction text** (e.g. "Atzgers.", "Hietzing"). VT323 at the row's font size. Max ~8 characters — truncate, don't wrap.
- **Col 3 — Two times** with **20 px gap** between them (14 px for EG). Each time is `HH:MM` (5 chars). If a time is "plan" (not real‑time), append a **5 × 5 px hollow square** (□) with 3 px left margin, vertically centered.

`columnGap` between the three columns: 12 px.

### Atzgersdorf S‑Bahn row anatomy

A 3‑column equal grid (`repeat(3, 1fr)`), each cell is:

`[badge] 6 px [time]`

Always 3 trains (S2 / S3 / S4 or whatever the next three are). Plan marker rule identical to TG/EG.

### Network plan

Bottom anchor — gets `margin-top: auto` so it sits at the bottom of the Atzg block. Padding: 4 px top, 6 px sides.

5 equal columns: `Hbf | Atzg | Ende | Tull | Hietz`.

**Top marker row** — height 10 px, contains:

- A 1‑px horizontal line from center of col 0 to center of col 1
- Col 0 (Hbf): 4 × 4 px filled square (dot)
- Col 1 (Atzg): 7 × 7 px **diamond** (square rotated 45°). Marks the transfer node.
- Cols 2–4: empty

**Middle row** — small spacer, contains the ▼ arrow over col 3 (Tull) AND a vertical 1‑px line from the top of this row through to the bottom, positioned at the center of col 1 (Atzg). The vertical line connects the two Atzg markers, forming an L‑shaped junction.

**Bottom marker row** — height 10 px, contains:

- A 1‑px horizontal line from center of col 1 to center of col 4
- Col 0: empty
- Col 1 (Atzg): 7 × 7 px diamond
- Col 2 (Ende): 4 × 4 px dot
- Col 3 (Tull): **8 × 8 px** filled square — the "you are here" marker
- Col 4 (Hietz): 4 × 4 px dot

**Labels row** — 7 px Silkscreen, centered in each column, 4 px top margin. Tull and Atzg are bold (font‑weight 700); the rest are regular (400).

### Plan / live indicator

The single visual encoding the whole display relies on:

- **Live (real‑time data, fresh)** → time appears alone. `18:32`
- **Plan (from static timetable, no real‑time data)** → time is followed by a **5 × 5 px hollow square** (1 px stroke) with 3 px left margin. `18:48 □`

If a time is empty (`--:--` or unset), **no marker is rendered** — there is no time to qualify.

There are no other indicators in the layout. No delay annotations, no cancellation strikethroughs. If a service is delayed, the displayed time is the new (live) time. If a service is cancelled, it simply does not appear in the list — show the next scheduled one instead.

## Screens / States

The screen the firmware renders is a function of the data + connection state. There are **7 states**:

### 1. Normal — `screen-1-normal.png`

Default rendering with current data. Mix of live and plan times is expected and normal.

### 2. Veraltet (Stale) — `screen-2-veraltet.png`

API has been silent for more than 3 minutes (per §4 of the protocol spec). All times render as `--:--`. **No plan marker** on the dashes. No banner. The empty rows themselves are the signal.

The bus line, direction, and section structure remain visible — so the user still knows what stop they are at and what lines normally go where. Only the times are blanked.

### 3. Nachtbetrieb (Night mode) — `screen-3-nachtbetrieb.png`

Outside service hours. Shows the **first departures of the morning** for each section — not night buses. Every time is a plan time (real‑time data isn't meaningful yet), so every time is suffixed with □. Identical layout to Normal, just with very early morning times.

Trigger condition (firmware): no live data **AND** current time is outside service hours. Look up the next first departures from the static timetable.

### 4. Keine Abfahrten (No departures) — `screen-4-keine-abfahrten.png`

No bus or train departs in the next 20 minutes. Replace the entire body with a centered placeholder:

- 72 px VT323 em‑dash `—` glyph
- 14 px Silkscreen "Keine Abfahrten", letter‑spacing 2, bold
- 18 px VT323 "in den nächsten 20 min", 6 px below

No network plan in this state.

### 5. Kein Empfang (Offline) — `screen-5-kein-empfang.png`

Network unreachable (WLAN/MQTT failure). Full‑screen error:

- Glyph: `!` (90 px VT323)
- Title: "Kein Empfang" (18 px Silkscreen)
- Sub: "Letzte Aktualisierung HH:MM" — the real wall‑clock time of the last successful fetch
- Foot (8 px Silkscreen, near bottom): "WLAN · Retry in 30s" or similar diagnostic

### 6. Auth‑Fehler — `screen-6-auth-fehler.png`

Backend refused the request (per §9 of the protocol spec — AID / Client ID outdated). Full‑screen error:

- Glyph: `§9`
- Title: "Auth‑Fehler"
- Sub: "Client‑ID veraltet · bitte neu registrieren"
- Foot: "AID 0x8F · ERR 401" — substitute the real AID and the actual HTTP status returned

This is intentionally diagnostic — the user is expected to be the operator pulling the device off the post to re‑provision it.

### 7. Boot — `screen-7-boot.png`

Initial render after power‑on or reset, before the first data has arrived.

- Glyph: `◌` (dotted circle — substitute `o` or a Unicode dotted‑circle fallback if not available in the bitmap font)
- Title: "bustaferl"
- Sub: "lädt Fahrplan…"
- Foot: "v2.0 · UC8176 · 400×300" — substitute real firmware version

## Data Shape

Each render takes a `data` object with this shape (TypeScript‑ish notation):

```ts
type DepartureRow = {
  line: string;            // "58A", "58B", "S2", …
  dir?: string;            // "Atzgers.", "Hietzing", …  (TG/EG only)
  times: [string, string]; // ["HH:MM", "HH:MM"]  (TG/EG only)
  liveTimes?: [boolean, boolean]; // defaults to [true, true]
  // S-Bahn variant:
  t?: string;              // "HH:MM"
  live?: boolean;          // defaults to true
};

type BoardData = {
  tg: DepartureRow[];   // exactly 2 rows (the 2 directions of 58A)
  eg: DepartureRow;     // 1 row (58B toward Atzgersdorf)
  sb?: DepartureRow[];  // exactly 3 rows
  sbNotice?: string;    // alternative to sb — a single message line
};
```

The firmware should normalize incoming MQTT / HTTP payloads into this shape and call a single render function with it. The state (Normal / Stale / Night / etc.) is selected by the firmware before the render call, not inside it.

## State Selection Logic (firmware side)

Suggested decision order on each refresh tick:

1. **Boot** — `firstRenderEver && !haveAnyData` → Boot
2. **Auth-Fehler** — last HTTP response was 401/403 → Auth‑Fehler
3. **Kein Empfang** — no successful fetch for >5 min AND WLAN is down → Offline
4. **Veraltet** — no successful fetch for >3 min (per §4) → Stale
5. **Keine Abfahrten** — all next departures are >20 min away → Quiet
6. **Nachtbetrieb** — current time outside service window AND firstDeparture is >30 min away → Night, with first‑departure lookup
7. **Normal** — otherwise

These thresholds are starting points — tune to taste against real device behavior.

## Design Tokens

```text
Colors
  ink           #0d0d0d   (drawn pixels)
  paper         #f4f1e8   (background — actual e-paper white on device)

Spacing (device pixels)
  side padding              18
  separator (TG/EG)         2 px
  separator (EG/Atzg)       1 px
  vertical gap, header→row  3–6 (size-dependent)
  column gap (data rows)    12
  inter-time gap (TG)       20
  inter-time gap (EG)       14
  inter-time gap (Atzg)     6 (badge→time)
  plan-marker left margin   3

Markers (network plan)
  dot         4 × 4 filled
  diamond     7 × 7 rotated 45°
  big         8 × 8 filled
  line        1 px stroke (horizontal & vertical)

Plan indicator
  hollow square  5 × 5, 1 px stroke
```

## Files in this bundle

| File | Purpose |
|---|---|
| `index.html` | Mount point. Loads the React design canvas with all 7 states arranged side by side. Open in a browser to see the live design. |
| `display.jsx` | The `<Display>` shell (bezel, scale wrapper, inversion) plus the visual primitives `<Hdr>`, `<Row>`, `<Sub>`, `<Time>`, `<Badge>`, `<Arrow>`, `<Banner>`. The badge sizing table at the bottom of this file is the canonical sizing reference. |
| `board.jsx` | The actual board layout and all 7 state variants (`BoardNormal`, `BoardStale`, `BoardNight`, `BoardQuiet`, `BoardOffline`, `BoardAuth`, `BoardBoot`). The `NetworkPlan` component and the plan indicator `<PlanMark>` / `<T>` are defined here. |
| `app.jsx` | Wires the 7 states into a `<DesignCanvas>` for browsing. Not relevant for device implementation. |
| `design-canvas.jsx` | The canvas component itself. Not relevant for device implementation. |
| `screen-1-normal.png` … `screen-7-boot.png` | Visual reference for each state, rendered at 2×. The live HTML is authoritative for pixel positions — use these screenshots to confirm you've covered each state. |

## Implementation Tips

- **Render the inverted screen as the default.** Don't draw paper pixels onto an unset framebuffer — clear the framebuffer to ink first, then draw paper pixels for text, badges, lines, markers. This is opposite to most display libraries' defaults.
- **Badges are an inverted rectangle.** Draw a paper‑filled rectangle, then draw ink text on top of it. Padding around the text inside the badge is exactly as specified in the badge sizing table — don't auto‑compute from text bounds, use the fixed `padding × min‑width` values so badges are uniform across lines.
- **The plan marker is 1 px stroke, 3 px inside.** Draw a 5 × 5 rectangle outline; don't fill it. The "hollowness" is what distinguishes plan from live. A filled marker would read as a different state entirely.
- **Truncate, don't wrap.** Direction text is sometimes longer than the column allows ("Schwedenplatz" doesn't fit). Truncate to fit with a trailing `.` (already done in the design: `Atzgers.`, `Schwedenpl.`). Never wrap a row.
- **Separators are solid lines, not dotted/dashed.** The 2 px separator under TG is a single thick line; the 1 px separator under EG is a single thin line. Both span edge‑to‑edge.
- **The network plan is geometrically simple but precise.** Lines must hit the centers of their markers. The vertical line connecting the two Atzg diamonds is what reads the relationship — without it the lower row floats unattached.
- **No partial refresh during transitions between states.** Full refresh — e‑paper artifacts will look worse than the 1‑second flash.
