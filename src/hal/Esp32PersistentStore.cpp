#include "Esp32PersistentStore.h"

#ifndef NATIVE_BUILD

#include "../config.h"
#include "../render/rle.h"

#include <Arduino.h>
#include <cstring>

namespace bustaferl {

namespace {

// Persistent in RTC slow memory across deep sleep. Cleared on power loss.
constexpr size_t RLE_CAP = RLE_HARDCAP_BYTES;

RTC_DATA_ATTR uint32_t g_magic = 0;
RTC_DATA_ATTR PersistedMeta g_meta{};
RTC_DATA_ATTR uint32_t g_rle_len = 0;
RTC_DATA_ATTR uint8_t g_rle[RLE_CAP] = {0};

// Schedule lives in a separate RTC slot with its own magic so a future
// ScheduleSnapshot schema change does not also wipe the framebuffer.
RTC_DATA_ATTR uint32_t g_sched_magic = 0;
RTC_DATA_ATTR ScheduleSnapshot g_sched{};

constexpr uint32_t MAGIC = 0xB05AFE72; // bustaferl ;) — v2: +Departure.line_label
// Bumped for v2: ScheduleSnapshot shrank from 5 to 4 streams (U1 → S-Bahn),
// so a v1 layout would misalign. First boot after the update re-fetches; the
// user sees one cycle with no hints.
constexpr uint32_t SCHED_MAGIC = 0x5CEDB053; // sched-bustaferl-3

} // namespace

PersistedMeta Esp32PersistentStore::loadMeta() {
  if (g_magic != MAGIC)
    return PersistedMeta{};
  return g_meta;
}

void Esp32PersistentStore::saveMeta(const PersistedMeta &m) {
  g_meta = m;
  g_magic = MAGIC;
}

size_t Esp32PersistentStore::loadFramebuffer(uint8_t *out, size_t cap) {
  if (g_magic != MAGIC || !g_meta.framebuffer_valid)
    return 0;
  if (g_rle_len == 0 || g_rle_len > RLE_CAP)
    return 0;
  return rleDecode(g_rle, g_rle_len, out, cap);
}

bool Esp32PersistentStore::saveFramebuffer(const uint8_t *fb, size_t len) {
  size_t n = rleEncode(fb, len, g_rle, RLE_CAP);
  if (n == 0) {
    g_rle_len = 0;
    g_meta.framebuffer_valid = false;
    g_magic = MAGIC;
    return false;
  }
  g_rle_len = static_cast<uint32_t>(n);
  g_meta.framebuffer_valid = true;
  g_magic = MAGIC;
  return true;
}

ScheduleSnapshot Esp32PersistentStore::loadSchedule() {
  if (g_sched_magic != SCHED_MAGIC)
    return ScheduleSnapshot{};
  return g_sched;
}

void Esp32PersistentStore::saveSchedule(const ScheduleSnapshot &s) {
  g_sched = s;
  g_sched_magic = SCHED_MAGIC;
}

} // namespace bustaferl

#endif
