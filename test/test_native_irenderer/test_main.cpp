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
  DisplayState last_state = DisplayState::Normal;
  void render(const RenderInput &in, Frame &fb) override {
    ++calls;
    last_state = in.state;
    fb.clear(true);
  }
};

} // namespace

void setUp() {}
void tearDown() {}

void test_fake_renderer_records_call() {
  FakeRenderer r;
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Stale;

  r.render(in, fb);

  TEST_ASSERT_EQUAL(1, r.calls);
  TEST_ASSERT_EQUAL(DisplayState::Stale, r.last_state);
}

void test_renderer_handle_via_base_pointer() {
  FakeRenderer impl;
  IRenderer &r = impl;
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Normal;

  r.render(in, fb);

  TEST_ASSERT_EQUAL(1, impl.calls);
  TEST_ASSERT_EQUAL(DisplayState::Normal, impl.last_state);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_fake_renderer_records_call);
  RUN_TEST(test_renderer_handle_via_base_pointer);
  return UNITY_END();
}
