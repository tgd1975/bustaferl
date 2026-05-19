#include "Esp32Sleep.h"

#ifndef NATIVE_BUILD

#include "../config.h"

#include <driver/gpio.h>
#include <esp_sleep.h>

namespace bustaferl {

WakeCause Esp32Sleep::wakeupCause() {
  switch (esp_sleep_get_wakeup_cause()) {
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    return WakeCause::ColdBoot;
  case ESP_SLEEP_WAKEUP_TIMER:
    return WakeCause::Timer;
  // EXT0 fires when GPIO 0 goes LOW during deep sleep; GPIO fires for the
  // same edge during light sleep. Both are the boot-button being pressed.
  case ESP_SLEEP_WAKEUP_EXT0:
  case ESP_SLEEP_WAKEUP_GPIO:
    return WakeCause::Button;
  default:
    return WakeCause::Other;
  }
}

void Esp32Sleep::deepSleep(unsigned seconds) {
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  // Also wake on the boot button (active-low). EXT0 is the wake-source that
  // supports deep sleep on a single RTC GPIO at a fixed level.
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(BTN_BOOT_PIN), 0);
  esp_deep_sleep_start();
  while (true) {
  } // unreachable
}

void Esp32Sleep::lightSleep(unsigned seconds) {
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  // Light sleep uses the GPIO wake source (EXT0 is deep-sleep only). Wake
  // when GPIO 0 is held LOW so a press interrupts the active-phase poll
  // rather than waiting for the next 30 s timer.
  gpio_wakeup_enable(static_cast<gpio_num_t>(BTN_BOOT_PIN),
                     GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
}

} // namespace bustaferl

#endif
