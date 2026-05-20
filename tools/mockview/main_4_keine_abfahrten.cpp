#ifndef NATIVE_BUILD

#include "config.h"
#include "mockview_runner.h"

using namespace bustaferl;
using namespace bustaferl::mockview;

void setup() {
  RenderInput in;
  in.state = DisplayState::Quiet;
  in.firmware_version = DISPLAY_VERSION_STR;
  // Quiet renderer ignores snapshot — leave default-zeroed.
  runMockView(in);
}

void loop() {}

#endif
