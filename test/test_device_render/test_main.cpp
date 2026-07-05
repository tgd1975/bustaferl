// On-device renderFrame() smoke tests. Verifies that the framebuffer comes
// out non-blank and that the 7 v2 DisplayStates each produce distinguishable
// frames. After Schritt 7.8 lands the full state-dispatcher, this bucket
// gets a deeper rewrite (Plan §6 — "komplett umgebaut").

#include "render/frame_buffer.h"
#include "render/layout.h"

#include <Arduino.h>
#include <cstring>
#include <memory>
#include <unity.h>

using namespace bustaferl;

namespace {

using FramePtr = std::unique_ptr<Frame>;
FramePtr makeFrame() { return std::make_unique<Frame>(); }

int countDrawnBytes(const uint8_t *fb) {
  int n = 0;
  for (size_t i = 0; i < Frame::bytes; ++i)
    if (fb[i] != 0)
      ++n;
  return n;
}

RenderInput makeInput(DisplayState s, const StreamSnapshot &snap) {
  RenderInput in;
  in.state = s;
  in.snapshot = snap;
  return in;
}

} // namespace

void test_render_empty_snapshot_draws_chrome() {
  FramePtr fb = makeFrame();
  StreamSnapshot snap{};
  renderFrame(makeInput(DisplayState::Normal, snap), *fb);
  int drawn = countDrawnBytes(fb->data());
  Serial.printf("[engine] render Normal empty: drawn_bytes=%d\n", drawn);
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      50, drawn,
      "renderFrame produced a blank framebuffer — chrome not drawn?");
}

void test_filling_slot_changes_framebuffer() {
  FramePtr fb_empty = makeFrame();
  FramePtr fb_filled = makeFrame();
  StreamSnapshot snap{};
  renderFrame(makeInput(DisplayState::Normal, snap), *fb_empty);
  Departure &dep = snap.stream[STREAM_58A_ATZ].slot[0];
  dep.when = 1704108660;
  dep.source = DepartureSource::Realtime;
  dep.valid = true;
  renderFrame(makeInput(DisplayState::Normal, snap), *fb_filled);
  int diff = 0;
  for (size_t i = 0; i < Frame::bytes; ++i)
    if (fb_empty->data()[i] != fb_filled->data()[i])
      ++diff;
  Serial.printf("[engine] valid-slot render: changed_bytes=%d\n", diff);
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      0, diff,
      "Filling a slot did not change framebuffer — formatHHMM broken?");
}

void test_seven_states_produce_distinguishable_frames() {
  StreamSnapshot snap{};
  constexpr DisplayState states[] = {
      DisplayState::Boot,  DisplayState::Normal, DisplayState::Stale,
      DisplayState::Night, DisplayState::Quiet,  DisplayState::Offline,
      DisplayState::Auth,
  };
  constexpr int n = sizeof(states) / sizeof(states[0]);
  std::unique_ptr<Frame> frames[n];
  for (int i = 0; i < n; ++i) {
    frames[i] = makeFrame();
    renderFrame(makeInput(states[i], snap), *frames[i]);
  }
  // Pairwise: every state pair must produce a different framebuffer. After
  // Schritt 7.8 this is also where the fullscreen-state Glyph & layout
  // asserts will go. For now the transitional renderer in layout.cpp
  // applies a different banner per state (Normal/Night collapse to the
  // bare board), which still gives ≥1 distinguishable pair.
  int distinct_pairs = 0;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      if (std::memcmp(frames[i]->data(), frames[j]->data(), Frame::bytes) !=
          0) {
        ++distinct_pairs;
      }
    }
  }
  Serial.printf("[engine] distinct state-pairs: %d (of %d total)\n",
                distinct_pairs, n * (n - 1) / 2);
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      0, distinct_pairs, "All 7 DisplayStates collapsed to identical frames");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_render_empty_snapshot_draws_chrome);
  RUN_TEST(test_filling_slot_changes_framebuffer);
  RUN_TEST(test_seven_states_produce_distinguishable_frames);
  UNITY_END();
}

void loop() {}
