#ifndef BUSTAFERL_NATIVE_RUNTIME_NOOPDISPLAY_H
#define BUSTAFERL_NATIVE_RUNTIME_NOOPDISPLAY_H

#include "../../src/hal/IDisplay.h"

namespace bustaferl::native_runtime {

// Counts panel-driver calls but does not touch any pixels — the
// RecordingRenderer (see 9.3) is the channel for inspecting framebuffers.
// Keeping these as plain counters lets the smoke target assert e.g.
// "deepClean() fired exactly once per night".
class NoOpDisplay : public IDisplay {
public:
  void drawFull(const uint8_t * /*fb*/) override { ++draw_full; }
  void drawPartial(const uint8_t * /*fb*/, const Bbox & /*bbox*/) override {
    ++draw_partial;
  }
  void lightFull(const uint8_t * /*fb*/) override { ++light_full; }
  void deepClean(const uint8_t * /*fb*/) override { ++deep_clean; }

  unsigned draw_full = 0;
  unsigned draw_partial = 0;
  unsigned light_full = 0;
  unsigned deep_clean = 0;
};

} // namespace bustaferl::native_runtime

#endif
