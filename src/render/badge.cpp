#include "render/badge.h"

#include "render/bitmap_fonts.h"

namespace bustaferl {

int drawBadgeGeometry(Frame &fb, int x, int y, BadgeSize sz) {
  const BadgeBounds b = badgeBounds(sz);
  // Background is paper (white-on-black inverted polarity → `paper` = ink-bit
  // = true here, since the global clear()s the buffer to ink/false-bits).
  fb.fillRect(x, y, b.w, b.h, /*white=*/true);
  return x + b.w;
}

#ifndef NATIVE_BUILD

} // namespace bustaferl

#include <Adafruit_GFX.h>

namespace bustaferl {

int drawBadge(Adafruit_GFX &canvas, Frame &fb, int x, int y, const char *text,
              BadgeSize sz) {
  const BadgeBounds b = badgeBounds(sz);
  drawBadgeGeometry(fb, x, y, sz);

  // Text uses U8g2 for the actual rasterisation. setRoleFont() picks the
  // right Logisoso/Helv variant per BadgeSize.
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
  render::setRoleFont(canvas, role);
  // U8g2's setCursor uses the font baseline; place near badge centre with a
  // padding offset. Y = baseline (top-edge + h - pad).
  canvas.setCursor(x + b.pad_x, y + b.h - b.pad_x);
  // Text colour: ink (paper bit = 0 → ink shows through the paper rectangle).
  canvas.setTextColor(0);
  canvas.print(text);
  return x + b.w;
}

#endif // NATIVE_BUILD

} // namespace bustaferl
