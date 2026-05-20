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
constexpr int LAYOUT_TG_HEADER_Y = 10;
constexpr int LAYOUT_TG_ROW0_Y = 28;
constexpr int LAYOUT_TG_ROW1_Y = 60;
constexpr int LAYOUT_SEP1_Y = 98;
constexpr int LAYOUT_SEP1_H = 2;
constexpr int LAYOUT_EG_HEADER_Y = 108;
constexpr int LAYOUT_EG_ROW_Y = 121;
constexpr int LAYOUT_SEP2_Y = 151;
constexpr int LAYOUT_ATZG_HEADER_Y = 160;
constexpr int LAYOUT_ATZG_ROW_Y = 176;
constexpr int LAYOUT_ATZG_COL2_X = 150;
constexpr int LAYOUT_ATZG_COL3_X = 282;
constexpr int LAYOUT_ATZG_COL3_Y = 176;
constexpr int LAYOUT_NETWORK_Y = 232;

// Per-slot spacing (px to next element + time-column offset).
constexpr int SLOT_GAP_PX = 6;
// Gap between the end of a time string and the plan marker (5×5 hollow
// square). Handoff §"Design Tokens" → plan-marker left margin 3 px.
constexpr int SLOT_PLAN_MARKER_OFFSET_X_GAP = 3;
constexpr int SLOT_PLAN_MARKER_SIZE = 5;

// Time-column offset from badge's right edge. TG/EG fit direction text
// between badge and time; ATZG (Sm badges, no direction) sits the time
// directly next to the badge per the handoff "[badge] 6 px [time]".
constexpr int SLOT_TIME_OFFSET_X_ATZG = 6;
constexpr int SLOT_TIME_OFFSET_X_DEFAULT = 80;

constexpr int slotTimeOffsetX(BadgeSize sz) {
  switch (sz) {
  case BadgeSize::Sm:
    return SLOT_TIME_OFFSET_X_ATZG;
  case BadgeSize::Md:
  case BadgeSize::Lg:
    return SLOT_TIME_OFFSET_X_DEFAULT;
  }
  return SLOT_TIME_OFFSET_X_DEFAULT;
}

// Per-row-font visual height (top-Y semantics: glyph height of the time
// column, which is the tallest element in each row). Badge + direction
// text get vertically centred against this. Values match the design
// handoff's "TG/EG/Atzg departure row" font sizes.
constexpr int ROW_HEIGHT_TG = 28;
constexpr int ROW_HEIGHT_EG = 22;
constexpr int ROW_HEIGHT_ATZG = 18;
constexpr int ROW_HEIGHT_FALLBACK = 16;

constexpr int rowContentHeight(FontRole role) {
  switch (role) {
  case FontRole::TG_Row:
    return ROW_HEIGHT_TG;
  case FontRole::EG_Row:
    return ROW_HEIGHT_EG;
  case FontRole::Atzg_Row:
    return ROW_HEIGHT_ATZG;
  default:
    return ROW_HEIGHT_FALLBACK;
  }
}

// Height of the small direction text rendered next to TG/EG badges
// (helvB10 → ~10 px).
constexpr int DIRECTION_TEXT_HEIGHT = 10;

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

// 7×5 right-arrow sprite (→). helvB10_te has no U+2192, so we paint our own
// to match the design handoff's "ATZGERSDORF → WIEN HBF" header.
constexpr int ARROW_RIGHT_W = 7;
constexpr int ARROW_RIGHT_H = 5;
// clang-format off
constexpr std::uint8_t ARROW_RIGHT_SPRITE[ARROW_RIGHT_H] = {
    0b00001000,
    0b00001100,
    0b11111110,
    0b00001100,
    0b00001000,
};
// clang-format on

void drawArrowRight(render::Canvas &canvas, int x, int y) {
  for (int row = 0; row < ARROW_RIGHT_H; ++row) {
    const std::uint8_t bits = ARROW_RIGHT_SPRITE[row];
    for (int col = 0; col < ARROW_RIGHT_W; ++col) {
      if (bits & (0x80 >> col)) {
        canvas.drawPixel(x + col, y + row, 1);
      }
    }
  }
}

// Section header "ATZGERSDORF → WIEN HBF" — split into two text runs around
// the custom arrow sprite, since the header font has no U+2192 glyph.
void drawAtzgHeader(render::Canvas &canvas, int x, int y) {
  constexpr int ARROW_PADDING_X = 4;
  constexpr int ARROW_TOP_Y_OFFSET = 3; // visually centres against cap-height
  canvas.setRoleFont(FontRole::Section_Header_EG_Atzg);
  canvas.setTextColor(1);
  canvas.setCursor(x, y);
  canvas.print("ATZGERSDORF");
  const int arrow_x = x + canvas.textWidth("ATZGERSDORF") + ARROW_PADDING_X;
  drawArrowRight(canvas, arrow_x, y + ARROW_TOP_Y_OFFSET);
  canvas.setCursor(arrow_x + ARROW_RIGHT_W + ARROW_PADDING_X, y);
  canvas.print("WIEN HBF");
}

// Format the HH:MM (or stale/invalid placeholder) into a small string.
void formatSlotTime(char *out, std::size_t cap, const Departure &d,
                    bool stale) {
  if (stale || !d.valid) {
    // Per docs/design_handoff_display/README.md §"Veraltet": stale and
    // missing times both render as the dash placeholder. logisoso*_tn is
    // numbers-only and lacks "?", so we always use the same dash form.
    std::snprintf(out, cap, "--:--");
  } else {
    formatHHMM(d.when, out, cap);
  }
}

// Per-row inter-time gap (px between time1 end and time2 start). Handoff
// §"TG / EG row anatomy": 20 px for TG, 14 px for EG. ATZG only renders
// one time per slot, so its gap is unused.
constexpr int INTER_TIME_GAP_TG = 20;
constexpr int INTER_TIME_GAP_EG = 14;
constexpr int INTER_TIME_GAP_FALLBACK = 14;

constexpr int interTimeGap(FontRole role) {
  switch (role) {
  case FontRole::TG_Row:
    return INTER_TIME_GAP_TG;
  case FontRole::EG_Row:
    return INTER_TIME_GAP_EG;
  default:
    return INTER_TIME_GAP_FALLBACK;
  }
}

// All the per-slot bits drawSlot needs. Bundled to keep the function below
// the readability-function-size param threshold. `d2` is the optional
// second-departure column (TG/EG rows show two times side-by-side per the
// design handoff); nullptr means single-time layout (ATZG / S-Bahn).
struct SlotSpec {
  int x;
  int y;
  const char *line;
  const char *direction;
  const Departure &d;
  const Departure *d2;
  BadgeSize sz;
  FontRole row_font;
  bool stale;
};

// Render a single departure slot — badge + direction text + HH:MM + plan
// marker. `s.y` is the top of the row's content box (the tallest element,
// the time column). Badge and direction are vertically centred against
// that. Returns the X coordinate where the next slot in the same row
// should start.
int drawSlot(render::Canvas &canvas, const SlotSpec &s) {
  const int row_h = rowContentHeight(s.row_font);
  const int badge_h = badgeBounds(s.sz).h;
  const int badge_y = s.y + (row_h - badge_h) / 2;
  const int badge_right = drawBadge(canvas, s.x, badge_y, s.line, s.sz);

  if (s.direction != nullptr && s.direction[0] != '\0') {
    canvas.setRoleFont(FontRole::Section_Header_EG_Atzg);
    canvas.setTextColor(1);
    const int dir_y = s.y + (row_h - DIRECTION_TEXT_HEIGHT) / 2;
    canvas.setCursor(badge_right + SLOT_GAP_PX, dir_y);
    canvas.print(s.direction);
  }

  char hhmm[8];
  formatSlotTime(hhmm, sizeof(hhmm), s.d, s.stale);
  canvas.setRoleFont(s.row_font);
  canvas.setTextColor(1);
  const int time_offset = slotTimeOffsetX(s.sz);
  const int time_x = badge_right + time_offset;
  canvas.setCursor(time_x, s.y);
  canvas.print(hhmm);
  const int time1_w = canvas.textWidth(hhmm);
  const int plan_mark_y = s.y + (row_h - SLOT_PLAN_MARKER_SIZE) / 2;

  // Plan marker only when the time actually displays a real "HH:MM" — in
  // stale mode the time is "--:--" and the handoff explicitly says "no
  // plan marker on the dashes".
  if (!s.stale && s.d.valid && s.d.source != DepartureSource::Realtime) {
    drawPlanMark(canvas, time_x + time1_w + SLOT_PLAN_MARKER_OFFSET_X_GAP,
                 plan_mark_y);
  }

  int right_x = time_x + time1_w;

  if (s.d2 != nullptr) {
    char hhmm2[8];
    formatSlotTime(hhmm2, sizeof(hhmm2), *s.d2, s.stale);
    const int gap = interTimeGap(s.row_font);
    const int time2_x = time_x + time1_w + gap;
    canvas.setCursor(time2_x, s.y);
    canvas.print(hhmm2);
    const int time2_w = canvas.textWidth(hhmm2);
    if (!s.stale && s.d2->valid && s.d2->source != DepartureSource::Realtime) {
      drawPlanMark(canvas, time2_x + time2_w + SLOT_PLAN_MARKER_OFFSET_X_GAP,
                   plan_mark_y);
    }
    right_x = time2_x + time2_w;
  }

  return right_x;
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
  const StreamData &s58a_atz = in.snapshot.stream[STREAM_58A_ATZ];
  drawSlot(canvas,
           SlotSpec{LAYOUT_PAD_X, LAYOUT_TG_ROW0_Y, "58A",
                    display_dir(STREAM_58A_ATZ), s58a_atz.slot[0],
                    &s58a_atz.slot[1], BadgeSize::Lg, FontRole::TG_Row, stale});
  const StreamData &s58a_hietzing = in.snapshot.stream[STREAM_58A_HIETZING];
  drawSlot(canvas, SlotSpec{LAYOUT_PAD_X, LAYOUT_TG_ROW1_Y, "58A",
                            display_dir(STREAM_58A_HIETZING),
                            s58a_hietzing.slot[0], &s58a_hietzing.slot[1],
                            BadgeSize::Lg, FontRole::TG_Row, stale});

  canvas.fillRect(0, LAYOUT_SEP1_Y, FB_W, LAYOUT_SEP1_H, 1);

  drawSectionHeader(canvas, FontRole::Section_Header_EG_Atzg, LAYOUT_PAD_X,
                    LAYOUT_EG_HEADER_Y, "ENDEMANNGASSE - NACH SCHLEIFE");
  const StreamData &s58b_atz = in.snapshot.stream[STREAM_58B_ATZ];
  drawSlot(canvas,
           SlotSpec{LAYOUT_PAD_X, LAYOUT_EG_ROW_Y, "58B",
                    display_dir(STREAM_58B_ATZ), s58b_atz.slot[0],
                    &s58b_atz.slot[1], BadgeSize::Md, FontRole::EG_Row, stale});

  canvas.fillRect(0, LAYOUT_SEP2_Y, FB_W, 1, 1);

  drawAtzgHeader(canvas, LAYOUT_PAD_X, LAYOUT_ATZG_HEADER_Y);
  const StreamData &sb = in.snapshot.stream[STREAM_SBAHN_HBF];
  drawSlot(canvas, SlotSpec{LAYOUT_PAD_X, LAYOUT_ATZG_ROW_Y,
                            sbahnLineLabel(sb.slot[0]), "", sb.slot[0], nullptr,
                            BadgeSize::Sm, FontRole::Atzg_Row, stale});
  drawSlot(canvas, SlotSpec{LAYOUT_ATZG_COL2_X, LAYOUT_ATZG_ROW_Y,
                            sbahnLineLabel(sb.slot[1]), "", sb.slot[1], nullptr,
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
