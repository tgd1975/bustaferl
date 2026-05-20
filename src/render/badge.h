#ifndef BUSTAFERL_RENDER_BADGE_H
#define BUSTAFERL_RENDER_BADGE_H

#include "render/canvas.h"

#include <cstdint>

namespace bustaferl {

// Line-badge primitive. Visually: a `paper`-coloured rounded rectangle with
// `ink` text inside, drawn over the global `ink` background. Three sizes
// per the design handoff (display.jsx line-height / padding numbers):
//
//   sm: 22 × 14 — Atzg S-Bahn row
//   md: 28 × 18 — EG row
//   lg: 36 × 22 — TG row
enum class BadgeSize : std::uint8_t { Sm, Md, Lg };

struct BadgeBounds {
  int w;
  int h;
  int pad_x; // horizontal padding between bg edge and text
  int pad_y; // vertical padding between top of bg and text top
};

// Badge dimensions from docs/design_handoff_display/display.jsx Z. 158-160.
// pad_y is sized so the badge font (helvB14/B10/B08) sits vertically centred.
constexpr int BADGE_SM_W = 22;
constexpr int BADGE_SM_H = 14;
constexpr int BADGE_SM_PAD = 3;
constexpr int BADGE_SM_PAD_Y = 3;
constexpr int BADGE_MD_W = 28;
constexpr int BADGE_MD_H = 18;
constexpr int BADGE_MD_PAD = 4;
constexpr int BADGE_MD_PAD_Y = 4;
constexpr int BADGE_LG_W = 36;
constexpr int BADGE_LG_H = 22;
constexpr int BADGE_LG_PAD = 5;
constexpr int BADGE_LG_PAD_Y = 4;

constexpr BadgeBounds badgeBounds(BadgeSize sz) {
  switch (sz) {
  case BadgeSize::Sm:
    return BadgeBounds{BADGE_SM_W, BADGE_SM_H, BADGE_SM_PAD, BADGE_SM_PAD_Y};
  case BadgeSize::Md:
    return BadgeBounds{BADGE_MD_W, BADGE_MD_H, BADGE_MD_PAD, BADGE_MD_PAD_Y};
  case BadgeSize::Lg:
    return BadgeBounds{BADGE_LG_W, BADGE_LG_H, BADGE_LG_PAD, BADGE_LG_PAD_Y};
  }
  return BadgeBounds{BADGE_SM_W, BADGE_SM_H, BADGE_SM_PAD, BADGE_SM_PAD_Y};
}

// Full badge render: paper rectangle + ink text via the Canvas's text API.
// Returns the right-edge X of the badge (callers chain horizontal layouts
// off this).
int drawBadge(render::Canvas &canvas, int x, int y, const char *text,
              BadgeSize sz);

} // namespace bustaferl

#endif
