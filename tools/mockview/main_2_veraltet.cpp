#ifndef NATIVE_BUILD

#include "config.h"
#include "mock_data.h"
#include "mockview_runner.h"

using namespace bustaferl;
using namespace bustaferl::mockview;

void setup() {
  RenderInput in;
  in.state = DisplayState::Stale;
  in.firmware_version = DISPLAY_VERSION_STR;
  // Renderer overrides slot times with "??:??" when state == Stale, but the
  // snapshot's structural fields still drive layout (labels, presence).
  in.snapshot = buildNormalSnapshot();
  runMockView(in);
}

void loop() {}

#endif
