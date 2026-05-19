#include "layout.h"

#ifndef NATIVE_BUILD

#include <Adafruit_GFX.h>
#include <cstdio>
#include <ctime>

namespace bustaferl {

namespace {

// Adafruit_GFX subclass that draws into an externally-owned 1-bpp framebuffer.
// Same layout as GFXcanvas1: row-major, MSB = leftmost, 1 = white, 0 = black.
class ExternalCanvas : public Adafruit_GFX {
public:
  ExternalCanvas(int16_t w, int16_t h, uint8_t *buf)
      : Adafruit_GFX(w, h), buf_(buf) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || x >= _width || y < 0 || y >= _height)
      return;
    const size_t idx = static_cast<size_t>(y) * (_width / 8) + (x / 8);
    const uint8_t mask = 0x80 >> (x & 7);
    if (color)
      buf_[idx] |= mask;
    else
      buf_[idx] &= ~mask;
  }

private:
  uint8_t *buf_;
};

// Format UTC epoch `t` as local HH:MM. Relies on the TZ environment variable
// being set by Esp32Clock (CET-1CEST,…), so DST is applied automatically —
// hard-coding 3600 s would be 1 hour off through the summer.
void formatHHMM(time_t t, char *out, size_t cap) {
  if (cap < 6)
    return;
  struct tm local;
  localtime_r(&t, &local);
  std::snprintf(out, cap, "%02d:%02d", local.tm_hour, local.tm_min);
}

// Global design is white ink on black background, so drawText defaults
// `ink = 1` (white). drawOverlay overrides with `ink = 0` because the
// alert banner is a white box where the text must read black.
void drawText(ExternalCanvas &c, int x, int y, uint8_t size, const char *s,
              uint16_t ink = 1) {
  c.setTextColor(ink);
  c.setTextSize(size);
  c.setCursor(x, y);
  c.print(s);
}

void drawHeader(ExternalCanvas &c, int y, const char *label) {
  drawText(c, 8, y, 2, label);
  c.drawFastHLine(8, y + 18, 384, 1); // white separator line
}

// In stale mode, every slot shows "??:??" regardless of the (now untrusted)
// `d.valid` — distinguishes "we have no data right now" (--:--) from
// "what we *had* is too old to display" (??:??).
void drawSlot(ExternalCanvas &c, int x, int y, const Departure &d, bool stale) {
  char buf[8];
  if (stale) {
    std::snprintf(buf, sizeof(buf), "??:??");
  } else if (d.valid) {
    formatHHMM(d.when, buf, sizeof(buf));
  } else {
    std::snprintf(buf, sizeof(buf), "--:--");
  }
  drawText(c, x, y, 2, buf);
}

void drawStreamLine(ExternalCanvas &c, int y, const char *prefix,
                    const StreamData &s, bool stale) {
  drawText(c, 8, y, 2, prefix);
  drawSlot(c, 220, y, s.slot[0], stale);
  drawSlot(c, 320, y, s.slot[1], stale);
}

void drawOverlay(ExternalCanvas &c, OverlayKind kind) {
  const char *msg = nullptr;
  switch (kind) {
  case OverlayKind::Stale:
    msg = "VERALTET";
    break;
  case OverlayKind::FilterDead:
    msg = "58B Filter ungueltig";
    break;
  case OverlayKind::StartFailed:
    msg = "Start fehlgeschlagen";
    break;
  default:
    return;
  }
  // Banner is a white box with a black border + black text, regardless of
  // the global ink scheme, so the alert stays high-contrast against the
  // black background.
  int y = 270;
  c.fillRect(0, y - 4, 400, 28, 1);
  c.drawRect(4, y - 4, 392, 26, 0);
  drawText(c, 12, y, 2, msg, 0);
}

} // namespace

void renderFrame(const RenderInput &in, Frame &fb) {
  const bool stale = (in.overlay == OverlayKind::Stale);

  fb.clear(false); // black background; content drawn in white
  ExternalCanvas c(FB_W, FB_H, fb.data());

  drawHeader(c, 4, "TULLNERTALGASSE");
  drawStreamLine(c, 38, "58A -> Atzgers.", in.snapshot.stream[STREAM_58A_ATZ],
                 stale);
  drawStreamLine(c, 66, "58A -> Hietzing",
                 in.snapshot.stream[STREAM_58A_HIETZING], stale);

  drawHeader(c, 110, "ENDEMANNGASSE");
  drawStreamLine(c, 144, "58B -> Atzgers.", in.snapshot.stream[STREAM_58B_ATZ],
                 stale);
  drawText(c, 8, 174, 1, "(nach Schleife)");

  // v2-Übergang (Session B): U1-Streams entfallen, der S-Bahn-Stream
  // (STREAM_SBAHN_HBF) wird erst in Session D vom neuen Renderer gezeichnet.
  // Vorläufig zeigt der dritte Block den S-Bahn-Slot mit dem alten
  // drawStreamLine-Helper, damit das Display nicht leer bleibt bevor das
  // Layout-Rewrite landet.
  drawHeader(c, 196, "ATZGERSDORF S");
  drawStreamLine(c, 222, "S-Bahn -> Hbf", in.snapshot.stream[STREAM_SBAHN_HBF],
                 stale);

  if (in.overlay != OverlayKind::None)
    drawOverlay(c, in.overlay);
}

} // namespace bustaferl

#endif // NATIVE_BUILD
