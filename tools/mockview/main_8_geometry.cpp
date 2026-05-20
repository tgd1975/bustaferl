// Geometry probe: bypasses renderFrame entirely. Draws fixed test patterns
// directly into the frame to isolate whether visual drift sits in the
// render layer or in the HAL/display path.
//
// Patterns drawn (all in top-Y coordinates, 400×300 framebuffer):
//   - 1-px outer border (0,0)→(399,299) — proves stride/dimensions.
//   - Diagonal (0,0)→(399,299) — proves x/y mapping.
//   - 50×50 filled square at (100,100) — proves fillRect.
//   - 4 corner pixels — proves bounds.
//   - Single horizontal tick row every 50 px at y=150.
//   - Single vertical tick column every 50 px at x=200.
//   - One U8g2 logisoso28 "5" glyph at (10,200) — proves font scale.
//   - One U8g2 helvB18 "Hi" at (200,250) — second font sample.

#ifndef NATIVE_BUILD

#include "hal/Esp32Display.h"
#include "render/canvas_adafruit.h"
#include "render/layout.h" // for Frame, FB_W, FB_H

#include <Arduino.h>
#include <esp_sleep.h>

using namespace bustaferl;

namespace {

constexpr int W = FB_W;
constexpr int H = FB_H;

void drawProbePatterns(render::Canvas &c) {
  // 1) Outer border (1 px white on black background).
  c.drawFastHLine(0, 0, W, 1);
  c.drawFastHLine(0, H - 1, W, 1);
  c.drawFastVLine(0, 0, H, 1);
  c.drawFastVLine(W - 1, 0, H, 1);

  // 2) Diagonals.
  c.drawLine(0, 0, W - 1, H - 1, 1);
  c.drawLine(W - 1, 0, 0, H - 1, 1);

  // 3) 50×50 filled square at (100, 100).
  c.fillRect(100, 100, 50, 50, 1);

  // 4) Four corner pixels (already on the border, but explicit).
  c.drawPixel(0, 0, 1);
  c.drawPixel(W - 1, 0, 1);
  c.drawPixel(0, H - 1, 1);
  c.drawPixel(W - 1, H - 1, 1);

  // 5) Horizontal tick row at y=150 — short verticals every 50 px.
  for (int x = 0; x < W; x += 50) {
    c.drawFastVLine(x, 145, 10, 1);
  }
  // 6) Vertical tick col at x=200 — short horizontals every 50 px.
  for (int y = 0; y < H; y += 50) {
    c.drawFastHLine(195, y, 10, 1);
  }

  // 7) U8g2 font samples for scale verification.
  // logisoso28 — same font the TG-Row uses. Expected glyph height: ~28 px.
  c.setRoleFont(FontRole::TG_Row);
  c.setTextColor(1);
  c.setCursor(10, 200);
  c.print("5");

  // helvB18 — same font the EG-section header uses. Expected height: ~18 px.
  c.setRoleFont(FontRole::Section_Header_EG_Atzg);
  c.setTextColor(1);
  c.setCursor(220, 260);
  c.print("Hi");
}

Esp32Display g_display;
Frame *g_frame = nullptr;

} // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("[geometry] starting probe");

  g_display.init();

  g_frame = new Frame();
  g_frame->clear(false); // black background (ink), white shapes on top

  render::AdafruitGfxCanvas canvas(g_frame->data(), W, H);
  drawProbePatterns(canvas);

  g_display.deepClean(g_frame->data());

  Serial.println("[geometry] rendered — deep sleep forever");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {}

#endif
