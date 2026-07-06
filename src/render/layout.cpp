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

// Layout constants — matches uploads/mockview-1-d40ff3ee.png. Content
// blocks (TG/EG/SB rows) use an 8 px side padding; the EG and SB section
// headers are full-width inverted bars that span edge-to-edge with no
// container padding. No separator lines — the bars are the section
// dividers.
constexpr int LAYOUT_PAD_X = 8;
constexpr int LAYOUT_TG_BAR_Y = 0;
constexpr int LAYOUT_TG_ROW0_Y = 22;
constexpr int LAYOUT_TG_ROW1_Y = 54;

// Inverted full-width header bar (EG, SB, optional notice banner).
constexpr int HEADER_BAR_H = 16;
constexpr int HEADER_BAR_TEXT_PAD_X = 8;
constexpr int HEADER_BAR_TEXT_PAD_Y = 3;

constexpr int LAYOUT_EG_BAR_Y = 90;
constexpr int LAYOUT_EG_ROW_Y = 112;
constexpr int LAYOUT_SB_BAR_Y = 140;
constexpr int LAYOUT_SB_ROW_Y = 162;

constexpr int LAYOUT_NETWORK_Y = 196;

// Common column grid for TG/EG time columns. Both time digits across all
// three rows (2× TG + 1× EG) are right-aligned to these X positions so the
// times form a clean vertical table. Each column reserves PLAN_MARK total
// width to the right so a plan-marker on one row doesn't shift the digits
// relative to another row.
constexpr int PLAN_MARK_TOTAL_W = 3 + PLAN_MARK_SIZE; // gap + marker
constexpr int COL_TIME2_DIGIT_RIGHT =
    FB_W - LAYOUT_PAD_X - PLAN_MARK_TOTAL_W; // 400-8-8 = 384
// Pitch sized to fit TG_Row's "HH:MM" (logisoso28_tn ≈ 85 px wide) plus a
// readable inter-column gap — 100 px is what makes the times stop touching.
constexpr int COL_TIME_GRID_PITCH = 100;
constexpr int COL_TIME1_DIGIT_RIGHT =
    COL_TIME2_DIGIT_RIGHT - COL_TIME_GRID_PITCH; // 304

// S-Bahn: three equal-width slots filling the section between LAYOUT_PAD_X
// and FB_W - LAYOUT_PAD_X.
constexpr int SB_SLOT_WIDTH = (FB_W - 2 * LAYOUT_PAD_X) / 3; // 384/3 = 128

// Per-slot spacing (px to next element + time-column offset).
constexpr int SLOT_GAP_PX = 6;
// Gap between the end of a time string and the superscript ring marker.
constexpr int SLOT_PLAN_MARKER_OFFSET_X_GAP = 3;

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

// Inverted full-width header bar (paper fill, ink text). Used by all three
// section headers (TG since the symmetry pass — it was plain caps on ink
// before) and the optional notice banner. Spans edge-to-edge with no
// container padding.
void drawHeaderBar(render::Canvas &canvas, int y, const char *text) {
  canvas.fillRect(0, y, FB_W, HEADER_BAR_H, 1);
  canvas.setRoleFont(FontRole::Section_Header_EG_Atzg);
  canvas.setTextColor(0);
  canvas.setCursor(HEADER_BAR_TEXT_PAD_X, y + HEADER_BAR_TEXT_PAD_Y);
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

void drawArrowRight(render::Canvas &canvas, int x, int y, std::uint16_t color) {
  for (int row = 0; row < ARROW_RIGHT_H; ++row) {
    const std::uint8_t bits = ARROW_RIGHT_SPRITE[row];
    for (int col = 0; col < ARROW_RIGHT_W; ++col) {
      if (bits & (0x80 >> col)) {
        canvas.drawPixel(x + col, y + row, color);
      }
    }
  }
}

// S-Bahn header bar: inverted full-width bar containing
// "ATZGERSDORF → WIEN HBF". Two text runs around the custom arrow sprite,
// since the header font has no U+2192 glyph.
void drawSbahnHeaderBar(render::Canvas &canvas, int y) {
  constexpr int ARROW_PADDING_X = 4;
  constexpr int ARROW_TOP_Y_OFFSET = 3; // visually centres against cap-height
  canvas.fillRect(0, y, FB_W, HEADER_BAR_H, 1);
  canvas.setRoleFont(FontRole::Section_Header_EG_Atzg);
  canvas.setTextColor(0);
  const int text_top_y = y + HEADER_BAR_TEXT_PAD_Y;
  canvas.setCursor(HEADER_BAR_TEXT_PAD_X, text_top_y);
  canvas.print("ATZGERSDORF");
  const int arrow_x =
      HEADER_BAR_TEXT_PAD_X + canvas.textWidth("ATZGERSDORF") + ARROW_PADDING_X;
  drawArrowRight(canvas, arrow_x, text_top_y + ARROW_TOP_Y_OFFSET, 0);
  canvas.setCursor(arrow_x + ARROW_RIGHT_W + ARROW_PADDING_X, text_top_y);
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

bool slotIsPlan(const Departure &d, bool stale) {
  return !stale && d.valid && d.source != DepartureSource::Realtime;
}

// Render a single departure slot — badge + direction text + HH:MM + plan
// marker. `s.y` is the top of the row's content box (the tallest element,
// the time column). Badge and direction are vertically centred against
// that. Returns the X coordinate where the next slot in the same row
// should start.
//
// Time-column layout:
//   * Single-time slot (s.d2 == nullptr, ATZG row): time sits at
//     `badge_right + slotTimeOffsetX(s.sz)` (left-anchored next to the
//     badge).
//   * Double-time slot (s.d2 != nullptr, TG/EG row): times are
//     right-aligned to the shared column grid (COL_TIME1_DIGIT_RIGHT and
//     COL_TIME2_DIGIT_RIGHT) so all three TG/EG rows form a clean
//     vertical table independent of font width.
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
  const int time1_w = canvas.textWidth(hhmm);
  // Superscript-style "°" marker — sits at the top of the cap-line of the
  // time digits, not vertically centred against the whole row.
  const int plan_mark_y = s.y;
  const bool d_plan = slotIsPlan(s.d, s.stale);

  if (s.d2 == nullptr) {
    // Single-time slot (ATZG): left-anchored next to the badge.
    const int time_x = badge_right + slotTimeOffsetX(s.sz);
    canvas.setCursor(time_x, s.y);
    canvas.print(hhmm);
    if (d_plan) {
      drawPlanMark(canvas, time_x + time1_w + SLOT_PLAN_MARKER_OFFSET_X_GAP,
                   plan_mark_y);
    }
    return time_x + time1_w + (d_plan ? PLAN_MARK_TOTAL_W : 0);
  }

  // Double-time slot (TG/EG): digits right-aligned to the shared column
  // grid. Marker space is pre-reserved in COL_TIME2_DIGIT_RIGHT so the
  // digit right edges are identical whether or not a row has a plan
  // marker.
  char hhmm2[8];
  formatSlotTime(hhmm2, sizeof(hhmm2), *s.d2, s.stale);
  const int time2_w = canvas.textWidth(hhmm2);
  const bool d2_plan = slotIsPlan(*s.d2, s.stale);
  const int time1_x = COL_TIME1_DIGIT_RIGHT - time1_w;
  const int time2_x = COL_TIME2_DIGIT_RIGHT - time2_w;

  canvas.setCursor(time1_x, s.y);
  canvas.print(hhmm);
  if (d_plan) {
    drawPlanMark(canvas, COL_TIME1_DIGIT_RIGHT + SLOT_PLAN_MARKER_OFFSET_X_GAP,
                 plan_mark_y);
  }
  canvas.setCursor(time2_x, s.y);
  canvas.print(hhmm2);
  if (d2_plan) {
    drawPlanMark(canvas, COL_TIME2_DIGIT_RIGHT + SLOT_PLAN_MARKER_OFFSET_X_GAP,
                 plan_mark_y);
  }
  return COL_TIME2_DIGIT_RIGHT + PLAN_MARK_TOTAL_W;
}

// Pick the line label for an S-Bahn slot, with fallback when the parser
// hasn't filled `line_label` (e.g. plan-only slot).
const char *sbahnLineLabel(const Departure &slot) {
  if (slot.valid && slot.line_label[0] != '\0') {
    return slot.line_label;
  }
  return "S?";
}

// One S-Bahn slot — badge + HH:MM when valid, dashed placeholder without
// badge when invalid/stale. Slots are equally wide; content is left-anchored
// inside the slot box.
void drawSbahnSlot(render::Canvas &canvas, int x, int y, const Departure &d,
                   bool stale) {
  if (stale || !d.valid) {
    canvas.setRoleFont(FontRole::Atzg_Row);
    canvas.setTextColor(1);
    canvas.setCursor(x, y);
    canvas.print("--:--");
    return;
  }
  drawSlot(canvas, SlotSpec{x, y, sbahnLineLabel(d), "", d, nullptr,
                            BadgeSize::Sm, FontRole::Atzg_Row, stale});
}

void drawBoard(render::Canvas &canvas, const RenderInput &in) {
  const bool stale = (in.state == DisplayState::Stale);

  drawHeaderBar(canvas, LAYOUT_TG_BAR_Y, "TULLNERTALGASSE");
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

  drawHeaderBar(canvas, LAYOUT_EG_BAR_Y, "ENDEMANNGASSE · NACH SCHLEIFE");
  const StreamData &s58b_atz = in.snapshot.stream[STREAM_58B_ATZ];
  drawSlot(canvas,
           SlotSpec{LAYOUT_PAD_X, LAYOUT_EG_ROW_Y, "58B",
                    display_dir(STREAM_58B_ATZ), s58b_atz.slot[0],
                    &s58b_atz.slot[1], BadgeSize::Md, FontRole::EG_Row, stale});

  drawSbahnHeaderBar(canvas, LAYOUT_SB_BAR_Y);
  const StreamData &sb = in.snapshot.stream[STREAM_SBAHN_HBF];
  drawSbahnSlot(canvas, LAYOUT_PAD_X, LAYOUT_SB_ROW_Y, sb.slot[0], stale);
  drawSbahnSlot(canvas, LAYOUT_PAD_X + SB_SLOT_WIDTH, LAYOUT_SB_ROW_Y,
                sb.slot[1], stale);
  // Third column shows the next S-Bahn departure after the first two. Renders
  // "--:--" on its own when the slot is empty (drawSbahnSlot handles invalid).
  drawSbahnSlot(canvas, LAYOUT_PAD_X + 2 * SB_SLOT_WIDTH, LAYOUT_SB_ROW_Y,
                sb.slot[2], stale);

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

void drawUpdateStamp(Frame &fb, std::time_t t) {
  if (t == 0)
    return;

#ifndef NATIVE_BUILD
  render::AdafruitGfxCanvas canvas(fb.data(), FB_W, FB_H);
#else
  render::HostCanvas canvas(fb);
#endif

  struct tm local;
  localtime_r(&t, &local);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "upd %02d:%02d", local.tm_hour, local.tm_min);

  canvas.setRoleFont(FontRole::Network_Label);
  const int w = canvas.textWidth(buf);
  constexpr int STAMP_H = 8; // 5x7 glyphs + 1px breathing room
  constexpr int STAMP_PAD = 2;
  const int x = FB_W - w - STAMP_PAD;
  const int y = FB_H - STAMP_H;
  // Clear the region to ink first so overwriting an older stamp leaves no
  // residue pixels behind narrower digits.
  canvas.fillRect(x - STAMP_PAD, y - 1, w + 2 * STAMP_PAD, STAMP_H + 1, 0);
  canvas.setTextColor(1);
  canvas.setCursor(x, y);
  canvas.print(buf);
}

} // namespace bustaferl
