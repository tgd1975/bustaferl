#ifndef BUSTAFERL_RENDER_BADGE_H
#define BUSTAFERL_RENDER_BADGE_H

#include "frame_buffer.h"
#include "layout.h" // for Frame alias

namespace bustaferl {

// Line-badge primitive. Visually: a `paper`-coloured rounded rectangle with
// `ink` text inside, drawn over the global `ink` background. Three sizes
// per the design handoff (display.jsx line-height / padding numbers):
//
//   sm: 22 × 14 — Atzg S-Bahn row
//   md: 28 × 18 — EG row
//   lg: 36 × 22 — TG row
//
// Geometry is split from the text rendering so the geometry can be host-
// asserted on the framebuffer; text goes through U8g2 and is ESP32-only.
enum class BadgeSize : std::uint8_t { Sm, Md, Lg };

struct BadgeBounds {
  int w;
  int h;
  int pad_x; // horizontal padding between bg edge and text
};

// Badge dimensions from docs/design_handoff_display/display.jsx Z. 158-160.
constexpr int BADGE_SM_W = 22;
constexpr int BADGE_SM_H = 14;
constexpr int BADGE_SM_PAD = 3;
constexpr int BADGE_MD_W = 28;
constexpr int BADGE_MD_H = 18;
constexpr int BADGE_MD_PAD = 4;
constexpr int BADGE_LG_W = 36;
constexpr int BADGE_LG_H = 22;
constexpr int BADGE_LG_PAD = 5;

constexpr BadgeBounds badgeBounds(BadgeSize sz) {
  switch (sz) {
  case BadgeSize::Sm:
    return BadgeBounds{BADGE_SM_W, BADGE_SM_H, BADGE_SM_PAD};
  case BadgeSize::Md:
    return BadgeBounds{BADGE_MD_W, BADGE_MD_H, BADGE_MD_PAD};
  case BadgeSize::Lg:
    return BadgeBounds{BADGE_LG_W, BADGE_LG_H, BADGE_LG_PAD};
  }
  return BadgeBounds{BADGE_SM_W, BADGE_SM_H, BADGE_SM_PAD};
}

// Paint the `paper`-rectangle behind the text. Pure geometry — host-testable.
// Returns the right-edge X coordinate of the badge (callers chain horizontal
// layouts off this).
int drawBadgeGeometry(Frame &fb, int x, int y, BadgeSize sz);

} // namespace bustaferl

#ifndef NATIVE_BUILD
class Adafruit_GFX;
namespace bustaferl {
// Full badge render: geometry + line-label text through U8g2. Caller-side
// font is set via setRoleFont(canvas, Badge_*). Returns same right-edge.
int drawBadge(Adafruit_GFX &canvas, Frame &fb, int x, int y, const char *text,
              BadgeSize sz);
} // namespace bustaferl
#endif

#endif
