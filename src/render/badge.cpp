#include "render/badge.h"

namespace bustaferl {

int drawBadge(render::Canvas &canvas, int x, int y, const char *text,
              BadgeSize sz) {
  const BadgeBounds b = badgeBounds(sz);
  // Paper-coloured rectangle background (inverted polarity — the global
  // clear leaves the canvas ink, paper rectangle reads as a punched-out
  // box).
  canvas.fillRect(x, y, b.w, b.h, 1);

  FontRole role = FontRole::Badge_sm;
  switch (sz) {
  case BadgeSize::Sm:
    role = FontRole::Badge_sm;
    break;
  case BadgeSize::Md:
    role = FontRole::Badge_md;
    break;
  case BadgeSize::Lg:
    role = FontRole::Badge_lg;
    break;
  }
  canvas.setRoleFont(role);
  // Text colour: ink (0) so the line label reads black against the paper
  // rectangle, regardless of the global polarity.
  canvas.setTextColor(0);
  canvas.setCursor(x + b.pad_x, y + b.pad_y);
  canvas.print(text);
  return x + b.w;
}

} // namespace bustaferl
