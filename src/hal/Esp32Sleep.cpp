#include "Esp32Sleep.h"

#ifndef NATIVE_BUILD

#include <esp_sleep.h>

namespace bustaferl {

WakeCause Esp32Sleep::wakeupCause() {
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_UNDEFINED: return WakeCause::ColdBoot;
        case ESP_SLEEP_WAKEUP_TIMER:     return WakeCause::Timer;
        default:                          return WakeCause::Other;
    }
}

void Esp32Sleep::deepSleep(unsigned seconds) {
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
    esp_deep_sleep_start();
    while (true) {}  // unreachable
}

void Esp32Sleep::lightSleep(unsigned seconds) {
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
    esp_light_sleep_start();
}

}  // namespace bustaferl

#endif
