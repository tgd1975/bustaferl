#include "Esp32Clock.h"

#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <time.h>

namespace bustaferl {

Esp32Clock::Esp32Clock(const char* ntp_server, const char* tz_info)
    : ntp_server_(ntp_server), tz_info_(tz_info) {
    setenv("TZ", tz_info_, 1);
    tzset();
}

time_t Esp32Clock::now() {
    time_t t;
    time(&t);
    return t;
}

bool Esp32Clock::ntpSync() {
    configTzTime(tz_info_, ntp_server_);
    // Wait up to 10s for the SNTP daemon to set the system clock.
    for (int i = 0; i < 50; ++i) {
        time_t t = now();
        if (t > 1700000000) {  // anything past 2023 is plausibly real
            last_sync_ = t;
            return true;
        }
        delay(200);
    }
    return false;
}

}  // namespace bustaferl

#endif
