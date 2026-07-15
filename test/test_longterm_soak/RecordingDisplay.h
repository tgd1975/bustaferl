#ifndef BUSTAFERL_TEST_RECORDING_DISPLAY_H
#define BUSTAFERL_TEST_RECORDING_DISPLAY_H

// IDisplay stub for soak / smoke long-term tests. Counts calls and runs
// the RLE save/load roundtrip on every drawPartial/lightFull/deepClean —
// matching the prod heap path — but never pushes pixels to the e-paper
// panel. Saves panel lifetime in 1 h soaks running every minute against
// synthetic data, while keeping the heap-leak detector behaviour equivalent.

#include "hal/IDisplay.h"
#include "render/rle.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace bustaferl {
namespace test {

class RecordingDisplay : public IDisplay {
public:
  // Frame is 400×300/8 = 15000 bytes. RLE hardcap from config.h is 7168;
  // anything that does not compress under that overflows. We mirror the
  // prod cap so the stub catches the same overflow class as RTC RAM.
  static constexpr size_t kFrameBytes = (400 * 300) / 8;
  static constexpr size_t kRleHardcap = 7168;

  int full_calls = 0;
  int partial_calls = 0;
  int light_full_calls = 0;
  int deep_clean_calls = 0;
  int rle_overflow_observed = 0;

  void drawFull(const uint8_t *fb) override {
    ++full_calls;
    roundtrip(fb);
  }
  void drawPartial(const uint8_t *fb, const Bbox & /*b*/) override {
    ++partial_calls;
    roundtrip(fb);
  }
  void lightFull(const uint8_t *fb) override {
    ++light_full_calls;
    roundtrip(fb);
  }
  void deepClean(const uint8_t *fb) override {
    ++deep_clean_calls;
    roundtrip(fb);
  }

private:
  // RLE-encode the framebuffer into a heap-allocated buffer of prod-cap
  // size, then decode it back into a second heap-allocated buffer. This
  // walks the same allocation pattern as Esp32PersistentStore + the prod
  // GxEPD2 push path without touching the panel.
  void roundtrip(const uint8_t *fb) {
    std::vector<uint8_t> encoded(kRleHardcap, 0);
    size_t enc_len = rleEncode(fb, kFrameBytes, encoded.data(), encoded.size());
    if (enc_len == 0) {
      ++rle_overflow_observed;
      return;
    }
    std::vector<uint8_t> decoded(kFrameBytes, 0);
    rleDecode(encoded.data(), enc_len, decoded.data(), decoded.size());
  }
};

} // namespace test
} // namespace bustaferl

#endif
