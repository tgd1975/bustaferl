#ifndef BUSTAFERL_RENDER_DIAG_PAGE_H
#define BUSTAFERL_RENDER_DIAG_PAGE_H

#include "../data/DiagView.h"
#include "../logic/diag_mode.h" // DiagPage
#include "canvas.h"
#include "layout.h" // Frame

namespace bustaferl {

// Plain-text diagnostic pages (double-click mode) + the boot-check screen.
// Deliberately no badges/glyphs — dense monospace-ish text is the right form
// for a diagnostic dump the user reads to understand an anomaly. All draw
// into a render::Canvas so both the ESP32 and the host renderer exercise the
// same code.

// One diagnostic page, dispatched by `page`.
void drawDiagPage(render::Canvas &canvas, const DiagView &v, DiagPage page);

// Boot-check screen shown for a few seconds after a cold boot (reuses the
// STATUS layout plus the boot-specific self-test + countdown lines).
void drawBootCheck(render::Canvas &canvas, const DiagView &v);

// Frame-level entry points: construct the target-appropriate Canvas (Adafruit
// on ESP32, Host on native), clear the frame, and draw. Called by cycle_runner.
void renderDiagPage(const DiagView &v, DiagPage page, Frame &fb);
void renderBootCheck(const DiagView &v, Frame &fb);

} // namespace bustaferl

#endif
