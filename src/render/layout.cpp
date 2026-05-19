#include "layout.h"

#include "../data/stream_labels.h"
#include "badge.h"
#include "bitmap_fonts.h"
#include "canvas.h"
#include "display_state.h"
#include "network_plan.h"
#include "plan_marker.h"

#ifndef NATIVE_BUILD
#include "canvas_adafruit.h"
#else
#include "canvas_host.h"
#endif

#include <cstdio>
#include <ctime>

namespace bustaferl {

namespace {

// Layout constants from the design handoff (docs/design_handoff_display/
// README.md "Layout"). Side padding 18 px, section anchors at specific Y
// rows that match the screen-1-normal.png mockup.
constexpr int LAYOUT_PAD_X = 18;
constexpr int LAYOUT_TG_HEADER_Y = 14;
constexpr int LAYOUT_TG_ROW0_Y = 22;
constexpr int LAYOUT_TG_ROW1_Y = 58;
constexpr int LAYOUT_SEP1_Y = 98;
constexpr int LAYOUT_SEP1_H = 2;
constexpr int LAYOUT_EG_HEADER_Y = 108;
constexpr int LAYOUT_EG_ROW_Y = 116;
constexpr int LAYOUT_SEP2_Y = 150;
constexpr int LAYOUT_ATZG_HEADER_Y = 160;
constexpr int LAYOUT_ATZG_ROW_Y = 168;
constexpr int LAYOUT_ATZG_COL2_X = 150;
constexpr int LAYOUT_ATZG_COL3_X = 282;
constexpr int LAYOUT_ATZG_COL3_Y = 180;
constexpr int LAYOUT_NETWORK_Y = 232;

// Per-slot spacing (px to next element + time-column offset).
constexpr int SLOT_GAP_PX = 6;
constexpr int SLOT_TIME_OFFSET_X = 80;
constexpr int SLOT_PLAN_MARKER_OFFSET_X = 60;
constexpr int SLOT_PLAN_MARKER_OFFSET_Y = 8;

// Minimum buffer size to format "HH:MM\0".
constexpr std::size_t HHMM_BUF_MIN = 6;

void formatHHMM(std::time_t t, char *out, std::size_t cap) {
  if (cap < HHMM_BUF_MIN) {
    return;
  }
  struct tm local{};
  localtime_r(&t, &local);
  std::snprintf(out, cap, "%02d:%02d", local.tm_hour, local.tm_min);
}

void drawSectionHeader(render::Canvas &canvas, FontRole role, int x, int y,
                       const char *text) {
  canvas.setRoleFont(role);
  canvas.setTextColor(1);
  canvas.setCursor(x, y);
  canvas.print(text);
}

// Format the HH:MM (or stale/invalid placeholder) into a small string.
void formatSlotTime(char *out, std::size_t cap, const Departure &d,
                    bool stale) {
  if (stale) {
    std::snprintf(out, cap, "??:??");
  } else if (d.valid) {
    formatHHMM(d.when, out, cap);
  } else {
    std::snprintf(out, cap, "--:--");
  }
}

// All the per-slot bits drawSlot needs. Bundled to keep the function below
// the readability-function-size param threshold.
struct SlotSpec {
  int x;
  int y;
  const char *line;
  const char *direction;
  const Departure &d;
  BadgeSize sz;
  FontRole row_font;
  bool stale;
};

// Render a single departure slot — badge + direction text + HH:MM + plan
// marker. Returns the X coordinate where the next slot in the same row
// should start.
int drawSlot(render::Canvas &canvas, const SlotSpec &s) {
  const int badge_right = drawBadge(canvas, s.x, s.y, s.line, s.sz);
  const int badge_h = badgeBounds(s.sz).h;

  if (s.direction != nullptr && s.direction[0] != '\0') {
    canvas.setRoleFont(FontRole::Section_Header_EG_Atzg);
    canvas.setTextColor(1);
    canvas.setCursor(badge_right + SLOT_GAP_PX, s.y + badge_h - 4);
    canvas.print(s.direction);
  }

  char hhmm[8];
  formatSlotTime(hhmm, sizeof(hhmm), s.d, s.stale);
  canvas.setRoleFont(s.row_font);
  canvas.setTextColor(1);
  const int time_x = badge_right + SLOT_TIME_OFFSET_X;
  const int time_y = s.y + badge_h - 2;
  canvas.setCursor(time_x, time_y);
  canvas.print(hhmm);

  if (s.d.valid && s.d.source != DepartureSource::Realtime) {
    drawPlanMark(canvas, time_x + SLOT_PLAN_MARKER_OFFSET_X,
                 time_y - SLOT_PLAN_MARKER_OFFSET_Y);
  }

  return time_x + SLOT_TIME_OFFSET_X;
}

// Pick the line label for an S-Bahn slot, with fallback when the parser
// hasn't filled `line_label` (e.g. plan-only slot).
const char *sbahnLineLabel(const Departure &slot) {
  if (slot.valid && slot.line_label[0] != '\0') {
    return slot.line_label;
  }
  return "S?";
}

void drawBoard(render::Canvas &canvas, const RenderInput &in) {
  const bool stale = (in.state == DisplayState::Stale);

  drawSectionHeader(canvas, FontRole::Section_Header_TG, LAYOUT_PAD_X,
                    LAYOUT_TG_HEADER_Y, "TULLNERTALGASSE");
  drawSlot(canvas, SlotSpec{LAYOUT_PAD_X, LAYOUT_TG_ROW0_Y, "58A",
                            display_dir(STREAM_58A_ATZ),
                            in.snapshot.stream[STREAM_58A_ATZ].slot[0],
                            BadgeSize::Lg, FontRole::TG_Row, stale});
  drawSlot(canvas, SlotSpec{LAYOUT_PAD_X, LAYOUT_TG_ROW1_Y, "58A",
                            display_dir(STREAM_58A_HIETZING),
                            in.snapshot.stream[STREAM_58A_HIETZING].slot[0],
                            BadgeSize::Lg, FontRole::TG_Row, stale});

  canvas.fillRect(0, LAYOUT_SEP1_Y, FB_W, LAYOUT_SEP1_H, 1);

  drawSectionHeader(canvas, FontRole::Section_Header_EG_Atzg, LAYOUT_PAD_X,
                    LAYOUT_EG_HEADER_Y, "ENDEMANNGASSE - NACH SCHLEIFE");
  drawSlot(canvas, SlotSpec{LAYOUT_PAD_X, LAYOUT_EG_ROW_Y, "58B",
                            display_dir(STREAM_58B_ATZ),
                            in.snapshot.stream[STREAM_58B_ATZ].slot[0],
                            BadgeSize::Md, FontRole::EG_Row, stale});

  canvas.fillRect(0, LAYOUT_SEP2_Y, FB_W, 1, 1);

  drawSectionHeader(canvas, FontRole::Section_Header_EG_Atzg, LAYOUT_PAD_X,
                    LAYOUT_ATZG_HEADER_Y, "ATZGERSDORF -> WIEN HBF");
  const StreamData &sb = in.snapshot.stream[STREAM_SBAHN_HBF];
  drawSlot(canvas,
           SlotSpec{LAYOUT_PAD_X, LAYOUT_ATZG_ROW_Y, sbahnLineLabel(sb.slot[0]),
                    "", sb.slot[0], BadgeSize::Sm, FontRole::Atzg_Row, stale});
  drawSlot(canvas, SlotSpec{LAYOUT_ATZG_COL2_X, LAYOUT_ATZG_ROW_Y,
                            sbahnLineLabel(sb.slot[1]), "", sb.slot[1],
                            BadgeSize::Sm, FontRole::Atzg_Row, stale});
  // Variante A — third slot intentionally empty.
  canvas.setRoleFont(FontRole::Atzg_Row);
  canvas.setTextColor(1);
  canvas.setCursor(LAYOUT_ATZG_COL3_X, LAYOUT_ATZG_COL3_Y);
  canvas.print("--:--");

  drawNetworkPlan(canvas, LAYOUT_PAD_X, LAYOUT_NETWORK_Y,
                  FB_W - 2 * LAYOUT_PAD_X);
}

} // namespace

void renderFrame(const RenderInput &in, Frame &fb) {
  fb.clear(false); // ink background

#ifndef NATIVE_BUILD
  render::AdafruitGfxCanvas canvas(fb.data(), FB_W, FB_H);
#else
  render::HostCanvas canvas(fb);
#endif

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
    drawBoard(canvas, in);
    return;
  }
}

} // namespace bustaferl
