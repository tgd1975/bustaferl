#include "layout.h"

#ifndef NATIVE_BUILD

#include <Adafruit_GFX.h>
#include <cstdio>
#include <cstring>
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

// --- Layout geometry (device pixels), transcribed from the v2 design handoff
// (docs/design_handoff_v2/HANDOFF.md). Origin top-left; content margin 8..392.
constexpr int MARGIN_L = 8;
constexpr int CONTENT_R = 392; // 400 - 8
constexpr int RULE_W = 384;    // CONTENT_R - MARGIN_L
constexpr uint8_t SZ = 2;      // size for headers / badges / time tokens
constexpr int CELL = 6 * SZ;   // classic-font advance at size 2 = 12 px

// Bus row anatomy.
constexpr int BUS_BADGE_W = 44;
constexpr int BUS_DEST_X = 60;
constexpr int BUS_TIME1_X = 240;
constexpr int BUS_TIME2_X = 330;
// S-Bahn slot anatomy (badge reserves room for the widest label, "REX1").
constexpr int SB_BADGE_W = 56;
constexpr int SB_SLOT1_BADGE_X = 8;
constexpr int SB_SLOT1_TIME_X = 72;
constexpr int SB_SLOT2_BADGE_X = 210;
constexpr int SB_SLOT2_TIME_X = 274;
constexpr int BADGE_H = 20;

// Section y-bands (text-top of the header / first row of each block).
constexpr int TG_HDR_Y = 8;
constexpr int TG_ROW_A_Y = 42;
constexpr int TG_ROW_B_Y = 74;
constexpr int EG_HDR_Y = 108;
constexpr int EG_ROW_Y = 142;
constexpr int SB_HDR_Y = 176;
constexpr int SB_ROW_Y = 210;
constexpr int FOOTER_RULE_Y = 272;
constexpr int FOOTER_TEXT_Y = 279;
constexpr int HDR_RULE_DY = 22; // rule sits 22 px below the header text-top

// Format UTC epoch `t` as local HH:MM. Relies on the TZ environment variable
// being set by Esp32Clock (CET-1CEST,…), so DST is applied automatically.
void formatHHMM(time_t t, char *out, size_t cap) {
  if (cap < 6)
    return;
  struct tm local;
  localtime_r(&t, &local);
  std::snprintf(out, cap, "%02d:%02d", local.tm_hour, local.tm_min);
}

// Global design is white ink on black background, so drawText defaults
// `ink = 1` (white). Badge/banner text overrides with `ink = 0` (black on a
// white box).
void drawText(ExternalCanvas &c, int x, int y, uint8_t size, const char *s,
              uint16_t ink = 1) {
  c.setTextColor(ink);
  c.setTextSize(size);
  c.setCursor(x, y);
  c.print(s);
}

// Section header: bold all-caps line + a full-width separator rule beneath it.
void drawHeader(ExternalCanvas &c, int y, const char *label) {
  drawText(c, MARGIN_L, y, SZ, label);
  c.drawFastHLine(MARGIN_L, y + HDR_RULE_DY, RULE_W, 1);
}

// Right-aligned small note sharing the header's line (e.g. "nach Schleife").
void drawNote(ExternalCanvas &c, int hdr_y, const char *note) {
  const int x = CONTENT_R - static_cast<int>(std::strlen(note)) * 6;
  drawText(c, x, hdr_y + 3, 1, note);
}

// Inverted roll-sign badge: white box with black line code centred-left.
void drawBadge(ExternalCanvas &c, int x, int w, int y, const char *label) {
  c.fillRect(x, y - 2, w, BADGE_H, 1);
  drawText(c, x + 4, y, SZ, label, 0);
}

// In stale mode every slot shows "??:??" regardless of `d.valid` —
// distinguishes "no data right now" (--:--) from "what we had is too old"
// (??:??).
void drawTimeToken(ExternalCanvas &c, int x, int y, const Departure &d,
                   bool stale) {
  char buf[8];
  if (stale) {
    std::snprintf(buf, sizeof(buf), "??:??");
  } else if (d.valid) {
    formatHHMM(d.when, buf, sizeof(buf));
  } else {
    std::snprintf(buf, sizeof(buf), "--:--");
  }
  drawText(c, x, y, SZ, buf);
}

void drawBusRow(ExternalCanvas &c, int y, const char *line, const char *dest,
                const StreamData &s, bool stale) {
  drawBadge(c, MARGIN_L, BUS_BADGE_W, y, line);
  drawText(c, BUS_DEST_X, y, SZ, dest);
  drawTimeToken(c, BUS_TIME1_X, y, s.slot[0], stale);
  drawTimeToken(c, BUS_TIME2_X, y, s.slot[1], stale);
}

// One S-Bahn slot: [badge] time. The line varies per slot, so the badge is
// drawn only when an actual labelled departure is present (never in stale or
// empty slots — there is no line to name).
void drawSbahnSlot(ExternalCanvas &c, int badge_x, int time_x, int y,
                   const Departure &d, bool stale) {
  if (!stale && d.valid && d.line_label[0] != '\0')
    drawBadge(c, badge_x, SB_BADGE_W, y, d.line_label);
  drawTimeToken(c, time_x, y, d, stale);
}

// Inline section banner: white bar replacing a section's data row, black text.
void drawSectionBanner(ExternalCanvas &c, int row_y, const char *msg) {
  c.fillRect(MARGIN_L, row_y - 2, RULE_W, BADGE_H, 1);
  drawText(c, MARGIN_L + 6, row_y, SZ, msg, 0);
}

// Global "data too old" banner across the lower board.
void drawStaleBanner(ExternalCanvas &c) {
  c.fillRect(0, 248, 400, 24, 1);
  // "VERALTET" = 8 chars * 12 px = 96; centred in 400 → x = 152.
  drawText(c, 152, 252, SZ, "VERALTET", 0);
}

void drawFooter(ExternalCanvas &c) {
  c.drawFastHLine(MARGIN_L, FOOTER_RULE_Y, RULE_W, 1);
  drawText(c, MARGIN_L, FOOTER_TEXT_Y, 1, "bustaferl");
  const char *loc = "@ Tullnertalgasse";
  drawText(c, CONTENT_R - static_cast<int>(std::strlen(loc)) * 6, FOOTER_TEXT_Y,
           1, loc);
}

// The three-section board. `stale` blanks every time token to "??:??" and
// suppresses the inline section banners (a global stale state owns the screen).
void drawBoard(ExternalCanvas &c, const RenderInput &in, bool stale) {
  drawHeader(c, TG_HDR_Y, "TULLNERTALGASSE");
  drawBusRow(c, TG_ROW_A_Y, "58A", "-> Atzgers.",
             in.snapshot.stream[STREAM_58A_ATZ], stale);
  drawBusRow(c, TG_ROW_B_Y, "58A", "-> Hietzing",
             in.snapshot.stream[STREAM_58A_HIETZING], stale);

  drawHeader(c, EG_HDR_Y, "ENDEMANNGASSE");
  drawNote(c, EG_HDR_Y, "nach Schleife");
  if (in.filter_dead_58b && !stale) {
    drawSectionBanner(c, EG_ROW_Y, "58B Filter ungueltig");
  } else {
    drawBusRow(c, EG_ROW_Y, "58B", "-> Atzgers.",
               in.snapshot.stream[STREAM_58B_ATZ], stale);
  }

  drawHeader(c, SB_HDR_Y, "ATZGERSDORF S-BAHN");
  drawNote(c, SB_HDR_Y, "-> Hauptbahnhof");
  if (in.oebb_auth_dead && !stale) {
    drawSectionBanner(c, SB_ROW_Y, "OEBB-API: Auth ungueltig");
  } else {
    const StreamData &sb = in.snapshot.stream[STREAM_SBAHN_HBF];
    drawSbahnSlot(c, SB_SLOT1_BADGE_X, SB_SLOT1_TIME_X, SB_ROW_Y, sb.slot[0],
                  stale);
    drawSbahnSlot(c, SB_SLOT2_BADGE_X, SB_SLOT2_TIME_X, SB_ROW_Y, sb.slot[1],
                  stale);
  }
}

// Full-screen cold-boot failure plate.
void drawStartFailed(ExternalCanvas &c) {
  drawText(c, MARGIN_L, TG_HDR_Y, SZ, "bustaferl");
  c.drawFastHLine(MARGIN_L, 30, RULE_W, 1);
  c.fillRect(30, 118, 340, 64, 1);
  drawText(c, 155, 124, 3, "Start", 0);
  drawText(c, 116, 156, SZ, "fehlgeschlagen", 0);
  drawText(c, 98, 200, SZ, "bitte neu starten", 1);
}

// Full-screen power-on splash.
void drawBootSplash(ExternalCanvas &c) {
  drawText(c, 119, 104, 3, "bustaferl");
  c.drawFastHLine(120, 138, 160, 1);
  drawText(c, 92, 162, SZ, "laedt Fahrplan ...");
  drawText(c, 194, 206, 1, "v2");
}

} // namespace

void renderFrame(const RenderInput &in, Frame &fb) {
  fb.clear(false); // black background; content drawn in white
  ExternalCanvas c(FB_W, FB_H, fb.data());

  switch (in.overlay) {
  case OverlayKind::Boot:
    drawBootSplash(c);
    return;
  case OverlayKind::StartFailed:
    drawStartFailed(c);
    return;
  case OverlayKind::Stale:
    drawBoard(c, in, /*stale=*/true);
    drawStaleBanner(c);
    drawFooter(c);
    return;
  case OverlayKind::None:
  default:
    drawBoard(c, in, /*stale=*/false);
    drawFooter(c);
    return;
  }
}

} // namespace bustaferl

#endif // NATIVE_BUILD
