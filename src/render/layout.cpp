#include "layout.h"

#ifndef NATIVE_BUILD

#include "../data/stream_labels.h"
#include "badge.h"
#include "bitmap_fonts.h"
#include "display_state.h"
#include "network_plan.h"
#include "plan_marker.h"

#include <Adafruit_GFX.h>
#include <cstdio>
#include <ctime>

namespace bustaferl {

namespace {

// Adafruit_GFX subclass that paints into the externally-owned 1-bpp
// framebuffer. Same layout as GFXcanvas1: row-major, MSB = leftmost,
// 1 = white, 0 = black.
class ExternalCanvas : public Adafruit_GFX {
public:
  ExternalCanvas(int16_t w, int16_t h, uint8_t *buf)
      : Adafruit_GFX(w, h), buf_(buf) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || x >= _width || y < 0 || y >= _height) {
      return;
    }
    const size_t idx = static_cast<size_t>(y) * (_width / 8) + (x / 8);
    const uint8_t mask = 0x80 >> (x & 7);
    if (color) {
      buf_[idx] |= mask;
    } else {
      buf_[idx] &= ~mask;
    }
  }

private:
  uint8_t *buf_;
};

void formatHHMM(time_t t, char *out, size_t cap) {
  if (cap < 6) {
    return;
  }
  struct tm local{};
  localtime_r(&t, &local);
  std::snprintf(out, cap, "%02d:%02d", local.tm_hour, local.tm_min);
}

void drawSectionHeader(Adafruit_GFX &canvas, FontRole role, int x, int y,
                       const char *text) {
  render::setRoleFont(canvas, role);
  canvas.setTextColor(1);
  canvas.setCursor(x, y);
  canvas.print(text);
}

// Render a single departure slot — badge + direction text + HH:MM + plan
// marker. Returns the X coordinate where the next slot in the same row
// should start.
int drawSlot(Adafruit_GFX &canvas, Frame &fb, int x, int y, const char *line,
             const char *direction, const Departure &d, BadgeSize sz,
             FontRole row_font, bool stale) {
  const int badge_right = drawBadge(canvas, fb, x, y, line, sz);

  // Direction text immediately after the badge.
  if (direction != nullptr && direction[0] != '\0') {
    render::setRoleFont(canvas, FontRole::Section_Header_EG_Atzg);
    canvas.setTextColor(1);
    canvas.setCursor(badge_right + 6, y + badgeBounds(sz).h - 4);
    canvas.print(direction);
  }

  // Time text.
  char hhmm[8];
  if (stale) {
    std::snprintf(hhmm, sizeof(hhmm), "??:??");
  } else if (d.valid) {
    formatHHMM(d.when, hhmm, sizeof(hhmm));
  } else {
    std::snprintf(hhmm, sizeof(hhmm), "--:--");
  }
  render::setRoleFont(canvas, row_font);
  canvas.setTextColor(1);
  const int time_x = badge_right + 80;
  const int time_y = y + badgeBounds(sz).h - 2;
  canvas.setCursor(time_x, time_y);
  canvas.print(hhmm);

  // Plan marker next to non-realtime slots.
  if (d.valid && d.source != DepartureSource::Realtime) {
    drawPlanMark(fb, time_x + 60, time_y - 8);
  }

  return time_x + 80;
}

void drawBoard(Adafruit_GFX &canvas, Frame &fb, const RenderInput &in) {
  const bool stale = (in.state == DisplayState::Stale);

  // TG section.
  drawSectionHeader(canvas, FontRole::Section_Header_TG, 18, 14,
                    "TULLNERTALGASSE");
  drawSlot(canvas, fb, 18, 22, "58A", display_dir(STREAM_58A_ATZ),
           in.snapshot.stream[STREAM_58A_ATZ].slot[0], BadgeSize::Lg,
           FontRole::TG_Row, stale);
  drawSlot(canvas, fb, 18, 58, "58A", display_dir(STREAM_58A_HIETZING),
           in.snapshot.stream[STREAM_58A_HIETZING].slot[0], BadgeSize::Lg,
           FontRole::TG_Row, stale);

  // 2 px separator between TG and EG.
  fb.fillRect(0, 98, FB_W, 2, true);

  // EG section.
  drawSectionHeader(canvas, FontRole::Section_Header_EG_Atzg, 18, 108,
                    "ENDEMANNGASSE - NACH SCHLEIFE");
  drawSlot(canvas, fb, 18, 116, "58B", display_dir(STREAM_58B_ATZ),
           in.snapshot.stream[STREAM_58B_ATZ].slot[0], BadgeSize::Md,
           FontRole::EG_Row, stale);

  // 1 px separator between EG and Atzg.
  fb.fillRect(0, 150, FB_W, 1, true);

  // Atzg section. Three columns reserved; only column 0 (and optionally 1)
  // get data — the design accommodates 3 S-Bahn slots, v2 only fills 2 of
  // the 3 (Variante A). The third column stays empty with --:-- placeholder.
  drawSectionHeader(canvas, FontRole::Section_Header_EG_Atzg, 18, 160,
                    "ATZGERSDORF -> WIEN HBF");
  const StreamData &sb = in.snapshot.stream[STREAM_SBAHN_HBF];
  // S-Bahn slots use the parsed line label from the realtime feed.
  const char *line_s0 = (sb.slot[0].valid && sb.slot[0].line_label[0] != '\0')
                            ? sb.slot[0].line_label
                            : "S?";
  const char *line_s1 = (sb.slot[1].valid && sb.slot[1].line_label[0] != '\0')
                            ? sb.slot[1].line_label
                            : "S?";
  drawSlot(canvas, fb, 18, 168, line_s0, "", sb.slot[0], BadgeSize::Sm,
           FontRole::Atzg_Row, stale);
  drawSlot(canvas, fb, 150, 168, line_s1, "", sb.slot[1], BadgeSize::Sm,
           FontRole::Atzg_Row, stale);
  // Third column placeholder (Variante A — left visibly empty).
  // Skip data badge; just a dash row for the user to read "no third slot".
  render::setRoleFont(canvas, FontRole::Atzg_Row);
  canvas.setTextColor(1);
  canvas.setCursor(282, 180);
  canvas.print("--:--");

  // Network plan at the bottom.
  drawNetworkPlan(canvas, fb, 18, 232, FB_W - 36);
}

} // namespace

void renderFrame(const RenderInput &in, Frame &fb) {
  // Schritt 7.8 final dispatcher. Each frame begins ink/black; per-state
  // renderers paint paper/ink on top.
  fb.clear(false);
  ExternalCanvas canvas(FB_W, FB_H, fb.data());

  switch (in.state) {
  case DisplayState::Boot:
    drawBoot(canvas, in.firmware_version);
    return;
  case DisplayState::Offline:
    drawOffline(canvas, in.last_fetch_at, in.retry_in_s);
    return;
  case DisplayState::Auth:
    drawAuth(canvas, in.auth_aid_short, in.auth_http_code);
    return;
  case DisplayState::Quiet:
    drawQuiet(canvas);
    return;
  case DisplayState::Stale:
  case DisplayState::Night:
  case DisplayState::Normal:
    drawBoard(canvas, fb, in);
    return;
  }
}

} // namespace bustaferl

#endif // NATIVE_BUILD
