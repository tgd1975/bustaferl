#ifndef BUSTAFERL_IPERSISTENTSTORE_H
#define BUSTAFERL_IPERSISTENTSTORE_H

#include "../data/ScheduleHint.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace bustaferl {

// Metadata that needs to survive deep sleep alongside the framebuffer.
struct PersistedMeta {
  time_t last_ntp_sync = 0;
  time_t last_api_success = 0;
  time_t last_light_full = 0;
  time_t last_deep_clean = 0;
  uint16_t partial_count = 0;
  uint8_t filter_miss_streak = 0;
  uint8_t cold_boot_retries = 0;
  bool framebuffer_valid = false;

  // v2 fields (driven by the display state-selector landing in Schritt 7):
  // - has_any_data: false until the first end-to-end fetch succeeded; drives
  //   the Boot screen after power-on / firmware-update (MAGIC bump zeroes it).
  // - last_success_at: epoch of the last fully-successful fetch cycle;
  //   feeds Stale / Offline thresholds.
  // - auth_error_seen: HAFAS err="AID"/"AUTH" sticky flag (cleared on next
  //   OK parse); drives the Auth screen.
  // - ogd_auth_streak: consecutive OGD 401/403 from `fetchWithRetry`; the
  //   caller flips auth_error_seen=true once this reaches 3.
  bool has_any_data = false;
  time_t last_success_at = 0;
  bool auth_error_seen = false;
  uint8_t ogd_auth_streak = 0;
};

class IPersistentStore {
public:
  virtual ~IPersistentStore() = default;
  virtual PersistedMeta loadMeta() = 0;
  virtual void saveMeta(const PersistedMeta &m) = 0;
  // Returns number of bytes written into `out` (decompressed framebuffer).
  // 0 → nothing stored / invalid.
  virtual size_t loadFramebuffer(uint8_t *out, size_t cap) = 0;
  // Compresses and stores. Returns true on success.
  virtual bool saveFramebuffer(const uint8_t *fb, size_t len) = 0;
  // ScheduleSnapshot lives in its own RTC slot with its own magic so a future
  // schema bump on schedule data does not invalidate the (much costlier to
  // rebuild) framebuffer. A never-saved schedule reads back as a
  // zero-initialised ScheduleSnapshot (fetched_at = 0).
  virtual ScheduleSnapshot loadSchedule() = 0;
  virtual void saveSchedule(const ScheduleSnapshot &s) = 0;
};

} // namespace bustaferl

#endif
