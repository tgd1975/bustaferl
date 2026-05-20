#ifndef NATIVE_BUILD

#include "config.h"
#include "mock_data.h"
#include "mockview_runner.h"

using namespace bustaferl;
using namespace bustaferl::mockview;

void setup() {
  RenderInput in;
  in.state = DisplayState::Normal;
  in.firmware_version = DISPLAY_VERSION_STR;
  in.snapshot = buildNormalSnapshot();
  runMockView(in);
}

void loop() {}

#endif
