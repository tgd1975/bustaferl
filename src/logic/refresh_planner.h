#ifndef BUSTAFERL_REFRESH_PLANNER_H
#define BUSTAFERL_REFRESH_PLANNER_H

#include <cstddef>
#include <cstdint>
#include <ctime>

#include "../hal/IDisplay.h"

namespace bustaferl {

enum class RefreshKind {
  None,
  Partial,
  LightFull,
  DeepClean,
};

struct RefreshDecision {
  RefreshKind kind = RefreshKind::None;
  Bbox bbox; // valid only for Partial
};

struct RefreshConfig {
  int width = 400;
  int height = 300;
  int light_full_every_s = 7200;
  uint16_t partial_hardcap = 80;
};

// Compares two framebuffers (row-major, 1 bit per pixel, 8 px per byte, MSB
// = leftmost). Returns the decision and (for partial) the bbox aligned to
// 8-px boundaries on the x axis. `prev_valid` lets the caller signal that
// the previous framebuffer is unusable (cold boot, RLE overflow).
RefreshDecision planRefresh(const uint8_t *prev, const uint8_t *curr,
                            bool prev_valid, time_t now, time_t last_light_full,
                            uint16_t partial_count, const RefreshConfig &cfg);

} // namespace bustaferl

#endif
