#include "Esp32Clock.h"

#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <time.h>

namespace bustaferl {

Esp32Clock::Esp32Clock(const char *ntp_server, const char *tz_info)
    : ntp_server_(ntp_server), tz_info_(tz_info) {
  setenv("TZ", tz_info_, 1);
  tzset();
}

time_t Esp32Clock::now() {
  time_t t;
  time(&t);
  return t;
}

std::uint32_t Esp32Clock::ticksMs() { return millis(); }

bool Esp32Clock::ntpSync() {
  // Primary + two well-known fallbacks. The SNTP daemon races them, so a
  // single sluggish pool member can't fail the whole sync.
  configTzTime(tz_info_, ntp_server_, "pool.ntp.org", "time.google.com");
  // Wait up to 10s for the SNTP daemon to set the system clock.
  for (int i = 0; i < 50; ++i) {
    if (isSynced()) {
      last_sync_ = now();
      return true;
    }
    delay(200);
  }
  return false;
}

} // namespace bustaferl

#endif
