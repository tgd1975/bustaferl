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

void Esp32Sleep::lightSleep(unsigned seconds) {
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  // Light sleep uses the GPIO wake source (EXT0 is deep-sleep only). Wake
  // when GPIO 0 is held LOW so a press interrupts the active-phase poll
  // rather than waiting for the next 30 s timer.
  gpio_wakeup_enable(static_cast<gpio_num_t>(BTN_BOOT_PIN),
                     GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  // Roughly 1 in 40 light sleeps never returns: the chip comes back on
  // RTCWDT_RTC_RESET instead, with the serial line cut mid-print at exactly
  // this call (observed 2026-08-16, twice). Without this line the next
  // occurrence leaves no evidence either — so record what the call actually
  // did. esp_light_sleep_start() can reject the request outright
  // (ESP_ERR_SLEEP_REJECT, returns immediately) or refuse a too-short
  // duration, and both are invisible today because the result is discarded.
  // A wake far short of `seconds` points at the wake source; no line at all
  // after "[sleep] staying active" means the chip never came back.
  const int64_t entered_us = esp_timer_get_time();
  const esp_err_t err = esp_light_sleep_start();
  const int64_t slept_ms = (esp_timer_get_time() - entered_us) / 1000;
  Serial.printf(
      "[sleep] light sleep returned err=%d after %lld ms (asked %u s)\n",
      static_cast<int>(err), static_cast<long long>(slept_ms), seconds);
}

} // namespace bustaferl

#endif
