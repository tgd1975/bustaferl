#ifndef NATIVE_BUILD

#include "mockview_runner.h"
#include "hal/Esp32Display.h"

#include <Arduino.h>
#include <esp_sleep.h>

namespace bustaferl::mockview {

void runMockView(const RenderInput &in) {
  Serial.begin(115200);
  delay(100);
  Serial.printf("[mockview] state=%d\n", static_cast<int>(in.state));

  Esp32Display display;
  display.init();

  Frame *frame = new Frame();
  renderFrame(in, *frame);
  // deepClean (3× B/W flash + drawFull) erases ghost charge from whatever
  // the panel showed before (v1 layout, partial v2, blank). ~6 s per flash —
  // acceptable trade-off for the visual-inspection use-case.
  display.deepClean(frame->data());

  Serial.println("[mockview] rendered — entering deep sleep forever");
  Serial.flush();
  esp_deep_sleep_start();
}

} // namespace bustaferl::mockview

#endif
