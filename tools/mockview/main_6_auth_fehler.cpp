#ifndef NATIVE_BUILD

#include "config.h"
#include "mockview_runner.h"

#include <cstring>

using namespace bustaferl;
using namespace bustaferl::mockview;

void setup() {
  RenderInput in;
  in.state = DisplayState::Auth;
  in.firmware_version = DISPLAY_VERSION_STR;
  std::strncpy(in.auth_aid_short, "OWDL4fE4", AUTH_AID_SHORT_CAP - 1);
  // HAFAS err=AID returns HTTP 200 — see migration plan §11.7
  in.auth_http_code = 200;
  runMockView(in);
}

void loop() {}

#endif
