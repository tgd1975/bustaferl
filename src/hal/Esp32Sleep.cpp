#include "Esp32Sleep.h"

#ifndef NATIVE_BUILD

#include "../config.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>

namespace bustaferl {

// How often pause() samples the boot button while waiting.
constexpr unsigned BUTTON_POLL_MS = 20;

WakeCause Esp32Sleep::wakeupCause() {
  switch (esp_sleep_get_wakeup_cause()) {
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    return WakeCause::ColdBoot;
  case ESP_SLEEP_WAKEUP_TIMER:
    return WakeCause::Timer;
  // EXT0 fires when GPIO 0 goes LOW during deep sleep — the live path. GPIO
  // is the same edge out of light sleep, which only test_device_sleep still
  // enters; kept so the mapping stays complete for that proxy.
  case ESP_SLEEP_WAKEUP_EXT0:
  case ESP_SLEEP_WAKEUP_GPIO:
    return WakeCause::Button;
  default:
    return WakeCause::Other;
  }
}

ResetReason Esp32Sleep::lastResetReason() {
  switch (esp_reset_reason()) {
  case ESP_RST_POWERON:
  case ESP_RST_DEEPSLEEP:
  case ESP_RST_SW:
    return ResetReason::Normal;
  case ESP_RST_BROWNOUT:
    return ResetReason::Brownout;
  case ESP_RST_TASK_WDT:
  case ESP_RST_INT_WDT:
  case ESP_RST_WDT:
  case ESP_RST_PANIC:
    return ResetReason::WatchdogOrPanic;
  default:
    return ResetReason::Other;
  }
}

void Esp32Sleep::deepSleep(unsigned seconds) {
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  // Also wake on the boot button (active-low). EXT0 is the wake-source that
  // supports deep sleep on a single RTC GPIO at a fixed level.
  const gpio_num_t btn = static_cast<gpio_num_t>(BTN_BOOT_PIN);
  esp_sleep_enable_ext0_wakeup(btn, 0);
  // Hold GPIO0's pull-up through deep sleep. INPUT_PULLUP is a digital-domain
  // config that dies when the IO domain powers down, leaving the pin to float —
  // the field symptom was erratic/dropped button wakes ("sometimes works,
  // sometimes not"). Configuring the RTC-domain pull (enable pull-up, disable
  // pull-down) keeps the line firmly HIGH at rest so the button reliably pulls
  // it LOW and EXT0 latches every press. rtc_gpio_hold_en latches the pad's
  // config so it survives the power-down.
  rtc_gpio_pullup_en(btn);
  rtc_gpio_pulldown_dis(btn);
  rtc_gpio_hold_en(btn);
  esp_deep_sleep_start();
  while (true) {
  } // unreachable
}

// Test-only; see the declaration in Esp32Sleep.h. Not reachable from the
// cycle, which waits via pause().
void Esp32Sleep::lightSleep(unsigned seconds) {
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  // GPIO wake, because EXT0 is deep-sleep only: a press on GPIO 0 ends the
  // sleep early rather than waiting out the timer.
  gpio_wakeup_enable(static_cast<gpio_num_t>(BTN_BOOT_PIN),
                     GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
}

void Esp32Sleep::pause(unsigned seconds) {
  // Plain wait, no power-down — see ISleep::pause() for why.
  //
  // Polls the boot button so a press still cuts the wait short, which is what
  // the GPIO wake did back when this was a light sleep (GPIO 0 is active-low).
  // The 20 ms cadence keeps the response no worse than before; classifyPress()
  // downstream does the short/long/double discrimination.
  const int64_t deadline_us =
      esp_timer_get_time() + static_cast<int64_t>(seconds) * 1000000;
  while (esp_timer_get_time() < deadline_us) {
    if (digitalRead(BTN_BOOT_PIN) == LOW) {
      return;
    }
    delay(BUTTON_POLL_MS);
  }
}

} // namespace bustaferl

#endif
