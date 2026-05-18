#ifndef BUSTAFERL_REFRESH_PLANNER_H
#define BUSTAFERL_REFRESH_PLANNER_H

#include "../hal/IDisplay.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace bustaferl {

enum class RefreshKind : std::uint8_t {
  None,
  Partial,
  LightFull,
  DeepClean,
};

struct RefreshDecision {
  RefreshKind kind = RefreshKind::None;
  Bbox bbox; // valid only for Partial
};

// Production panel geometry (GxEPD2 4.2" 400×300).
constexpr int DEFAULT_FB_WIDTH = 400;
constexpr int DEFAULT_FB_HEIGHT = 300;
constexpr int DEFAULT_LIGHT_FULL_EVERY_S = 7200; // 2 h ghosting refresh
constexpr uint16_t DEFAULT_PARTIAL_HARDCAP = 80;

struct RefreshConfig {
  int width = DEFAULT_FB_WIDTH;
  int height = DEFAULT_FB_HEIGHT;
  int light_full_every_s = DEFAULT_LIGHT_FULL_EVERY_S;
  uint16_t partial_hardcap = DEFAULT_PARTIAL_HARDCAP;
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
