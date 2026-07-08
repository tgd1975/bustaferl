#ifndef BUSTAFERL_ESP32CLOCK_H
#define BUSTAFERL_ESP32CLOCK_H

#ifndef NATIVE_BUILD

#include "IClock.h"

namespace bustaferl {

class Esp32Clock : public IClock {
public:
  Esp32Clock(const char *ntp_server, const char *tz_info);
  time_t now() override;
  bool ntpSync() override;
  std::uint32_t ticksMs() override;
  time_t lastSync() const override { return last_sync_; }
  void setLastSync(time_t t) { last_sync_ = t; }

private:
  const char *ntp_server_;
  const char *tz_info_;
  time_t last_sync_ = 0;
};

} // namespace bustaferl

#endif
#endif
