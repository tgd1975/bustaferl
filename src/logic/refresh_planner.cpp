#include "refresh_planner.h"

#include <cstring>

namespace bustaferl {

static Bbox diffBbox(const uint8_t *a, const uint8_t *b, int w, int h) {
  const int stride = w / 8;
  int min_x = w, min_y = h, max_x = -1, max_y = -1;
  for (int y = 0; y < h; ++y) {
    for (int xb = 0; xb < stride; ++xb) {
      uint8_t diff = a[y * stride + xb] ^ b[y * stride + xb];
      if (!diff)
        continue;
      int x0 = xb * 8;
      int x1 = x0 + 7;
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
  bb.x = min_x & ~7;
  bb.y = min_y;
  bb.w = ((max_x + 1) - bb.x + 7) & ~7;
  bb.h = max_y - min_y + 1;
  return bb;
}

RefreshDecision planRefresh(const uint8_t *prev, const uint8_t *curr,
                            bool prev_valid, time_t now, time_t last_light_full,
                            uint16_t partial_count, const RefreshConfig &cfg) {
  RefreshDecision d;

  if (!prev_valid) {
    d.kind = RefreshKind::LightFull;
    return d;
  }
  if (std::memcmp(prev, curr, cfg.width * cfg.height / 8) == 0) {
    d.kind = RefreshKind::None;
    return d;
  }

  bool time_trigger = (last_light_full == 0) ||
                      (now - last_light_full >= cfg.light_full_every_s);
  bool cap_trigger = partial_count >= cfg.partial_hardcap;
  if (time_trigger || cap_trigger) {
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
