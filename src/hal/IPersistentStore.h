#ifndef BUSTAFERL_IPERSISTENTSTORE_H
#define BUSTAFERL_IPERSISTENTSTORE_H

#include "../data/CycleTrace.h"
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
  // Free-running count of consecutive no-wifi cold cycles. Drives the KEIN-
  // EMPFANG repaint cadence: the screen is repainted only every
  // `no_wifi_repaint_every`th cycle (≈5 min) even though WiFi is retried every
  // cycle (60 s). Reset to 0 the moment a connection succeeds. Distinct from
  // cold_boot_retries, which caps at max_retries and feeds the attempt display.
  uint8_t no_wifi_cycles = 0;
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

  // Epoch of the last refresh actually pushed to the panel (partial,
  // light-full, or deep clean). Drives the UPDATE_STAMP_ENABLED debug stamp
  // bottom-right; 0 = never / stamp absent.
  time_t last_display_update = 0;

  // Wall-clock epoch this cycle asked to wake at (now + planned sleep
  // seconds), written right before sleeping. On the next wake, the drift
  // guard compares now() against this: a healthy RTC lands at ~this value,
  // a corrupt one (the "58B coma") reads far past it and forces a re-sync.
  // 0 = never slept with a known clock (first boot); guard abstains.
  time_t expected_wake_at = 0;
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

  // Diagnostic event memory (data/CycleTrace.h), its own RTC slot with its
  // own magic. Defaulted to a no-op so the host fakes and the native-runtime
  // DiskStore that do not care about the trace need no changes; only
  // Esp32PersistentStore persists it across deep sleep. A never-saved trace
  // reads back as an empty CycleTrace.
  virtual CycleTrace loadTrace() { return CycleTrace{}; }
  virtual void saveTrace(const CycleTrace &) {}
};

} // namespace bustaferl

#endif
