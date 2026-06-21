// On-device test for the render layer. layout.cpp depends on Adafruit_GFX,
// so verification must happen on the ESP32. We assert framebuffer-level
// invariants (non-blank chrome, overlay confined to its y-band, slot time
// changes after filling a departure) instead of pixel-perfect snapshots —
// font metrics can shift across Adafruit_GFX versions, but the gross
// invariants stay stable.

#include "config.h"
#include "data/StreamSnapshot.h"
#include "render/layout.h"

#include <Arduino.h>
#include <cstring>
#include <memory>
#include <unity.h>

using namespace bustaferl;

namespace {

// Frame is 15 KB. The Arduino-ESP32 loop task has an 8 KB stack, so even one
// `Frame fb;` local would guru-meditate the chip *before* Serial.print and
// silently kill the whole test run. Heap-allocate via unique_ptr — the
// buffer goes to PSRAM/DRAM, ownership stays scoped to the test.
using FramePtr = std::unique_ptr<Frame>;
FramePtr makeFrame() { return std::unique_ptr<Frame>(new Frame()); }

// Layout draws white ink on a black background, so an untouched byte after
// fb.clear(false) is 0x00. "Drawn" bytes are any non-zero byte — the more
// of them, the more content was rendered on top of the black background.
int countDrawnBytes(const uint8_t *fb) {
  int n = 0;
  for (size_t i = 0; i < Frame::bytes; ++i)
    if (fb[i] != 0x00)
      ++n;
  return n;
}

} // namespace

void test_render_empty_snapshot_draws_chrome() {
  FramePtr fb = makeFrame();
  StreamSnapshot snap{};
  RenderInput in{snap, OverlayKind::None};
  renderFrame(in, *fb);
  int drawn = countDrawnBytes(fb->data());
  Serial.printf("[engine] render empty snapshot: drawn_bytes=%d\n", drawn);
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      50, drawn,
      "renderFrame produced a blank framebuffer — chrome not drawn?");
}

void test_section_banner_changes_framebuffer_in_band() {
  // The 58B section banner replaces only the Endemanngasse data row; the diff
  // vs a normal render must stay inside that row's y-band [136, 162).
  FramePtr fb_a = makeFrame();
  FramePtr fb_b = makeFrame();
  StreamSnapshot snap{};
  RenderInput normal{snap, OverlayKind::None};
  RenderInput banner{snap, OverlayKind::None};
  banner.filter_dead_58b = true;
  renderFrame(normal, *fb_a);
  renderFrame(banner, *fb_b);
  TEST_ASSERT_TRUE_MESSAGE(
      std::memcmp(fb_a->data(), fb_b->data(), Frame::bytes) != 0,
      "58B section banner did not change framebuffer");

  const int stride = FB_W / 8;
  int diff_inside = 0;
  int diff_outside = 0;
  for (int y = 0; y < FB_H; ++y) {
    for (int xb = 0; xb < stride; ++xb) {
      bool diff =
          fb_a->data()[y * stride + xb] != fb_b->data()[y * stride + xb];
      if (!diff)
        continue;
      if (y >= 136 && y < 162)
        ++diff_inside;
      else
        ++diff_outside;
    }
  }
  Serial.printf("[engine] banner diff: inside=%d outside=%d\n", diff_inside,
                diff_outside);
  TEST_ASSERT_GREATER_THAN(0, diff_inside);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, diff_outside,
                                "section banner leaked outside its row band");
}

void test_filling_slot_changes_framebuffer() {
  FramePtr fb_empty = makeFrame();
  FramePtr fb_filled = makeFrame();
  StreamSnapshot snap{};
  renderFrame({snap, OverlayKind::None}, *fb_empty);
  // 12:31 CET = 1704108660 UTC. Fills slot[0] for 58A→Atzgersdorf.
  snap.stream[STREAM_58A_ATZ].slot[0] = {1704108660, DepartureSource::Realtime,
                                         true};
  renderFrame({snap, OverlayKind::None}, *fb_filled);
  int diff = 0;
  for (size_t i = 0; i < Frame::bytes; ++i)
    if (fb_empty->data()[i] != fb_filled->data()[i])
      ++diff;
  Serial.printf("[engine] valid-slot render: changed_bytes=%d\n", diff);
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      0, diff,
      "Filling a slot did not change framebuffer — formatHHMM broken?");
}

void test_each_state_changes_framebuffer() {
  // Every distinct state (4 full-frame OverlayKinds + the 2 per-section banner
  // flags) must produce a distinguishable framebuffer — guards against a
  // fall-through bug in renderFrame's dispatch.
  FramePtr base = makeFrame();
  FramePtr stale = makeFrame();
  FramePtr failed = makeFrame();
  FramePtr boot = makeFrame();
  FramePtr dead58b = makeFrame();
  FramePtr oebb = makeFrame();
  StreamSnapshot snap{};
  renderFrame({snap, OverlayKind::None}, *base);
  renderFrame({snap, OverlayKind::Stale}, *stale);
  renderFrame({snap, OverlayKind::StartFailed}, *failed);
  renderFrame({snap, OverlayKind::Boot}, *boot);
  RenderInput d{snap, OverlayKind::None};
  d.filter_dead_58b = true;
  renderFrame(d, *dead58b);
  RenderInput o{snap, OverlayKind::None};
  o.oebb_auth_dead = true;
  renderFrame(o, *oebb);

  TEST_ASSERT_NOT_EQUAL(0,
                        std::memcmp(base->data(), stale->data(), Frame::bytes));
  TEST_ASSERT_NOT_EQUAL(
      0, std::memcmp(base->data(), failed->data(), Frame::bytes));
  TEST_ASSERT_NOT_EQUAL(0,
                        std::memcmp(base->data(), boot->data(), Frame::bytes));
  TEST_ASSERT_NOT_EQUAL(
      0, std::memcmp(base->data(), dead58b->data(), Frame::bytes));
  TEST_ASSERT_NOT_EQUAL(0,
                        std::memcmp(base->data(), oebb->data(), Frame::bytes));
  TEST_ASSERT_NOT_EQUAL(
      0, std::memcmp(stale->data(), failed->data(), Frame::bytes));
  TEST_ASSERT_NOT_EQUAL(0,
                        std::memcmp(failed->data(), boot->data(), Frame::bytes));
  TEST_ASSERT_NOT_EQUAL(
      0, std::memcmp(dead58b->data(), oebb->data(), Frame::bytes));
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_render_empty_snapshot_draws_chrome);
  RUN_TEST(test_section_banner_changes_framebuffer_in_band);
  RUN_TEST(test_filling_slot_changes_framebuffer);
  RUN_TEST(test_each_state_changes_framebuffer);
  UNITY_END();
}

void loop() {}
