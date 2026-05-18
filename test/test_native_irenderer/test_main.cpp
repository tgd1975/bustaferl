// IRenderer is a HAL abstraction the cycle_runner (Schritt 7.2) consumes
// without pulling in Adafruit_GFX / Arduino.h. This host test pins that
// property: the header must compile native, and a hand-rolled fake must
// satisfy the interface.

#include "hal/IRenderer.h"

#include <unity.h>

using namespace bustaferl;

namespace {

class FakeRenderer : public IRenderer {
public:
  int calls = 0;
  OverlayKind last_overlay = OverlayKind::None;
  void render(const RenderInput &in, Frame &fb) override {
    ++calls;
    last_overlay = in.overlay;
    fb.clear(true);
  }
};

} // namespace

void setUp() {}
void tearDown() {}

void test_fake_renderer_records_call() {
  FakeRenderer r;
  Frame fb;
  StreamSnapshot snap;
  RenderInput in{snap, OverlayKind::Stale};

  r.render(in, fb);

  TEST_ASSERT_EQUAL(1, r.calls);
  TEST_ASSERT_EQUAL(OverlayKind::Stale, r.last_overlay);
}

void test_renderer_handle_via_base_pointer() {
  FakeRenderer impl;
  IRenderer &r = impl;
  Frame fb;
  StreamSnapshot snap;
  RenderInput in{snap, OverlayKind::None};

  r.render(in, fb);

  TEST_ASSERT_EQUAL(1, impl.calls);
  TEST_ASSERT_EQUAL(OverlayKind::None, impl.last_overlay);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_fake_renderer_records_call);
  RUN_TEST(test_renderer_handle_via_base_pointer);
  return UNITY_END();
}
