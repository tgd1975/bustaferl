#include "refresh_planner.h"

#include <cstring>

namespace bustaferl {

// One framebuffer byte holds 8 horizontally adjacent pixels (MSB = leftmost).
// All x-bbox arithmetic snaps to this 8-px boundary so the partial refresh
// envelope aligns with bytes — the panel can't update sub-byte regions.
constexpr int PIXELS_PER_BYTE = 8;
constexpr int BYTE_PIXEL_MASK = PIXELS_PER_BYTE - 1; // i.e. 7

static Bbox diffBbox(const uint8_t *a, const uint8_t *b, int w, int h) {
  const int stride = w / PIXELS_PER_BYTE;
  int min_x = w, min_y = h, max_x = -1, max_y = -1;
  for (int y = 0; y < h; ++y) {
    for (int xb = 0; xb < stride; ++xb) {
      uint8_t diff = a[y * stride + xb] ^ b[y * stride + xb];
      if (!diff)
        continue;
      int x0 = xb * PIXELS_PER_BYTE;
      int x1 = x0 + BYTE_PIXEL_MASK;
      if (x0 < min_x)
        min_x = x0;
      if (x1 > max_x)
        max_x = x1;
      if (y < min_y)
        min_y = y;
      if (y > max_y)
        max_y = y;
    }
  }
  if (max_x < 0)
    return Bbox{};
  Bbox bb;
  bb.x = min_x & ~BYTE_PIXEL_MASK;
  bb.y = min_y;
  bb.w = ((max_x + 1) - bb.x + BYTE_PIXEL_MASK) & ~BYTE_PIXEL_MASK;
  bb.h = max_y - min_y + 1;
  return bb;
}

// 7 parameters split between inputs (prev/curr/prev_valid), wall-clock state
// (now/last_light_full/partial_count), and tunables (cfg). Wrapping the state
// trio into a struct would buy nothing — its values come from three
// independent persistent counters and have no shared invariant.
// NOLINTNEXTLINE(readability-function-size)
RefreshDecision planRefresh(const uint8_t *prev, const uint8_t *curr,
                            bool prev_valid, time_t now, time_t last_light_full,
                            uint16_t partial_count, const RefreshConfig &cfg,
                            bool panel_ram_untrusted) {
  RefreshDecision d;

  if (!prev_valid) {
    d.kind = RefreshKind::LightFull;
    return d;
  }
  if (std::memcmp(prev, curr, cfg.width * cfg.height / 8) == 0) {
    d.kind = RefreshKind::None;
    return d;
  }

  // The panel content differs and something will reach the glass. If the
  // on-glass RAM is untrusted (deep-sleep wake), a partial would leave stale
  // garbage outside the bbox on a fast-partial-update panel — force a full.
  bool time_trigger = (last_light_full == 0) ||
                      (now - last_light_full >= cfg.light_full_every_s);
  bool cap_trigger = partial_count >= cfg.partial_hardcap;
  if (time_trigger || cap_trigger || panel_ram_untrusted) {
    d.kind = RefreshKind::LightFull;
    return d;
  }

  d.kind = RefreshKind::Partial;
  d.bbox = diffBbox(prev, curr, cfg.width, cfg.height);
  if (d.bbox.empty()) {
    d.kind = RefreshKind::None;
  }
  return d;
}

} // namespace bustaferl
