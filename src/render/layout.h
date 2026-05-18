#ifndef BUSTAFERL_LAYOUT_H
#define BUSTAFERL_LAYOUT_H

#include "../data/StreamSnapshot.h"
#include "frame_buffer.h"

#include <cstdint>

namespace bustaferl {

constexpr int FB_W = 400;
constexpr int FB_H = 300;
using Frame = FrameBuffer<FB_W, FB_H>;

enum class OverlayKind : std::uint8_t {
  None,
  Stale,       // "veraltet"
  FilterDead,  // "58B Filter ungültig"
  StartFailed, // "Start fehlgeschlagen"
};

struct RenderInput {
  StreamSnapshot snapshot;
  OverlayKind overlay = OverlayKind::None;
};

// Renders the layout described in CONCEPT.md §3 into the framebuffer.
// Only used on the ESP32 build (depends on Adafruit GFX). The header is safe
// to include from the host build, but the symbol resolves only when GFX is
// available.
#ifndef NATIVE_BUILD
void renderFrame(const RenderInput &in, Frame &fb);
#endif

} // namespace bustaferl

#endif
