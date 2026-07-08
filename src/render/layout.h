#ifndef BUSTAFERL_LAYOUT_H
#define BUSTAFERL_LAYOUT_H

#include "../data/BootReport.h"
#include "../data/StreamSnapshot.h"
#include "frame_buffer.h"

#include <cstdint>

namespace bustaferl {

constexpr int FB_W = 400;
constexpr int FB_H = 300;
using Frame = FrameBuffer<FB_W, FB_H>;

// Full-frame state: mutually exclusive whole-screen treatments. Per-section
// inline banners (58B filter dead, ÖBB auth dead) ride on the RenderInput
// flags below instead, so they can coexist with live data in other sections.
enum class OverlayKind : std::uint8_t {
  None,
  Stale,       // global: all times "??:??" + VERALTET banner
  StartFailed, // full-screen cold-boot failure plate
  Boot,        // splash — or the boot-check dashboard when a valid
               // RenderInput::boot_report is attached
};

struct RenderInput {
  StreamSnapshot snapshot;
  OverlayKind overlay = OverlayKind::None;
  bool filter_dead_58b = false; // section 2 → "58B Filter ungueltig" banner
  bool oebb_auth_dead = false; // section 3 → "OEBB-API: Auth ungueltig" banner
  // Boot-check dashboard payload; consumed only when overlay == Boot and
  // boot_report.valid — otherwise Boot renders the plain splash. The default
  // member initializer keeps the existing two-element aggregate inits
  // warning-free under -Wmissing-field-initializers.
  BootReport boot_report = {};
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
