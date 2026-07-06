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

// v2 bump: STREAM_COUNT shrank from 5 to 4, Departure gained line_label,
// PersistedMeta picked up four new fields. The old layout would deserialize
// as garbage; force a fresh persist on the first boot after the update. The
// user sees one cycle with no hints + a light-full instead of a partial.
// .73: framebuffer switched from plain RLE to row-delta RLE — decoding an
// old plain-RLE slot with the delta decoder would produce a scrambled frame.
constexpr uint32_t MAGIC = 0xB05AFE73; // bustaferl v2, delta-RLE fb

// Framebuffer row width in bytes; encode and decode must agree on it.
constexpr size_t FB_STRIDE = EPD_WIDTH / 8;
// Bumped because STREAM_COUNT and the stream-index meaning changed: the old
// index 3/4 held U1 schedule hints; after v2 index 3 is the S-Bahn (no hint
// path). Reusing the old slot would inject U1 hints into the S-Bahn stream.
constexpr uint32_t SCHED_MAGIC = 0x5CEDB053; // sched-bustaferl-3 (v2)

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
  return rleDecodeDelta(g_rle, g_rle_len, FB_STRIDE, out, cap);
}

bool Esp32PersistentStore::saveFramebuffer(const uint8_t *fb, size_t len) {
  size_t n = rleEncodeDelta(fb, len, FB_STRIDE, g_rle, RLE_CAP);
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
