#ifndef BUSTAFERL_ESP32PERSISTENTSTORE_H
#define BUSTAFERL_ESP32PERSISTENTSTORE_H

#ifndef NATIVE_BUILD

#include "IPersistentStore.h"

namespace bustaferl {

// Stores meta + RLE-compressed framebuffer in the ESP32's RTC slow memory
// (RTC_DATA_ATTR). Survives deep sleep, lost on power cycle. Used buffer is
// declared in the .cpp file.
class Esp32PersistentStore : public IPersistentStore {
public:
  PersistedMeta loadMeta() override;
  void saveMeta(const PersistedMeta &m) override;
  size_t loadFramebuffer(uint8_t *out, size_t cap) override;
  bool saveFramebuffer(const uint8_t *fb, size_t len) override;
  ScheduleSnapshot loadSchedule() override;
  void saveSchedule(const ScheduleSnapshot &s) override;
};

} // namespace bustaferl

#endif
#endif
