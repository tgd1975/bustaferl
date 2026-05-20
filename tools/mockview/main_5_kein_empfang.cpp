#ifndef NATIVE_BUILD

#include "config.h"
#include "mock_data.h"
#include "mockview_runner.h"

using namespace bustaferl;
using namespace bustaferl::mockview;

void setup() {
  RenderInput in;
  in.state = DisplayState::Offline;
  in.firmware_version = DISPLAY_VERSION_STR;
  in.last_fetch_at = kMockNow - 437; // ~7 min ago
  in.retry_in_s = 23;
  runMockView(in);
}

void loop() {}

#endif
