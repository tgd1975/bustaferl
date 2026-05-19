// On-device test for Esp32PersistentStore. RTC slow memory and the RLE
// pipeline are ESP32-only, so verification happens here rather than in the
// host suite. Tests run in upload order: validate the cold-RTC default first,
// then save/load round-trips, then the overflow path that clears
// framebuffer_valid (after which the store state is poisoned for the rest of
// the suite — keep it last).

#include "config.h"
#include "hal/Esp32PersistentStore.h"

#include <Arduino.h>
#include <cstring>
#include <memory>
#include <unity.h>

using namespace bustaferl;

namespace {
Esp32PersistentStore g_store;
} // namespace

void test_cold_rtc_load_returns_defaults() {
  // On a fresh upload the RTC magic word is unset, so loadMeta must return a
  // zero-initialised PersistedMeta. Must run before any saveMeta.
  PersistedMeta m = g_store.loadMeta();
  Serial.printf("[engine] cold loadMeta: last_api=%lld partial=%u "
                "fb_valid=%d\n",
                static_cast<long long>(m.last_api_success),
                static_cast<unsigned>(m.partial_count), m.framebuffer_valid);
  TEST_ASSERT_EQUAL_INT64_MESSAGE(0, m.last_ntp_sync,
                                  "cold RTC must default last_ntp_sync to 0");
  TEST_ASSERT_EQUAL_INT64(0, m.last_api_success);
  TEST_ASSERT_EQUAL_INT64(0, m.last_light_full);
  TEST_ASSERT_EQUAL_INT64(0, m.last_deep_clean);
  TEST_ASSERT_EQUAL_UINT(0, m.partial_count);
  TEST_ASSERT_EQUAL_UINT8(0, m.filter_miss_streak);
  TEST_ASSERT_EQUAL_UINT8(0, m.cold_boot_retries);
  TEST_ASSERT_FALSE(m.framebuffer_valid);
}

void test_cold_rtc_load_schedule_returns_zero() {
  // Same magic-word check as loadMeta, separate slot — must come back blank
  // before any saveSchedule.
  ScheduleSnapshot s = g_store.loadSchedule();
  TEST_ASSERT_EQUAL_INT64(0, s.fetched_at);
  for (int i = 0; i < STREAM_COUNT; ++i) {
    TEST_ASSERT_EQUAL_INT64(0, s.hint[i].last_today);
    TEST_ASSERT_EQUAL_INT64(0, s.hint[i].first_tomorrow[0]);
    TEST_ASSERT_EQUAL_INT64(0, s.hint[i].first_tomorrow[1]);
  }
}

void test_cold_rtc_load_framebuffer_returns_zero() {
  // FB_BYTES is 15 KB — too large for the Arduino loop() stack (~8 KB).
  // Heap-allocate every framebuffer in this suite (memory: frame-must-be-heap).
  auto out = std::make_unique<uint8_t[]>(FB_BYTES);
  std::memset(out.get(), 0, FB_BYTES);
  TEST_ASSERT_EQUAL_UINT(0, g_store.loadFramebuffer(out.get(), FB_BYTES));
}

void test_save_meta_round_trip() {
  PersistedMeta m{};
  m.last_ntp_sync = 1700000001;
  m.last_api_success = 1700000002;
  m.last_light_full = 1700000003;
  m.last_deep_clean = 1700000004;
  m.partial_count = 17;
  m.filter_miss_streak = 2;
  m.cold_boot_retries = 1;
  m.framebuffer_valid = false;
  g_store.saveMeta(m);
  PersistedMeta loaded = g_store.loadMeta();
  TEST_ASSERT_EQUAL_INT64(1700000001, loaded.last_ntp_sync);
  TEST_ASSERT_EQUAL_INT64(1700000002, loaded.last_api_success);
  TEST_ASSERT_EQUAL_INT64(1700000003, loaded.last_light_full);
  TEST_ASSERT_EQUAL_INT64(1700000004, loaded.last_deep_clean);
  TEST_ASSERT_EQUAL_UINT(17, loaded.partial_count);
  TEST_ASSERT_EQUAL_UINT8(2, loaded.filter_miss_streak);
  TEST_ASSERT_EQUAL_UINT8(1, loaded.cold_boot_retries);
  TEST_ASSERT_FALSE(loaded.framebuffer_valid);
}

void test_save_framebuffer_round_trip_white() {
  auto fb = std::make_unique<uint8_t[]>(FB_BYTES);
  std::memset(fb.get(), 0xFF, FB_BYTES);
  TEST_ASSERT_TRUE_MESSAGE(g_store.saveFramebuffer(fb.get(), FB_BYTES),
                           "saveFramebuffer(all-white) returned false");
  PersistedMeta m = g_store.loadMeta();
  TEST_ASSERT_TRUE_MESSAGE(m.framebuffer_valid,
                           "framebuffer_valid not set after successful save");

  auto out = std::make_unique<uint8_t[]>(FB_BYTES);
  std::memset(out.get(), 0, FB_BYTES);
  size_t n = g_store.loadFramebuffer(out.get(), FB_BYTES);
  Serial.printf("[engine] save/load white roundtrip: %u bytes returned\n",
                static_cast<unsigned>(n));
  TEST_ASSERT_EQUAL_UINT(FB_BYTES, n);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, std::memcmp(fb.get(), out.get(), FB_BYTES),
                                "decoded framebuffer differs from input");
}

void test_save_framebuffer_round_trip_random_pattern() {
  auto fb = std::make_unique<uint8_t[]>(FB_BYTES);
  for (size_t i = 0; i < FB_BYTES; ++i) {
    // run-friendly pattern: 8-byte plateaus, change every 8 bytes
    fb[i] = static_cast<uint8_t>((i / 8) & 0xFF);
  }
  TEST_ASSERT_TRUE(g_store.saveFramebuffer(fb.get(), FB_BYTES));
  auto out = std::make_unique<uint8_t[]>(FB_BYTES);
  std::memset(out.get(), 0, FB_BYTES);
  size_t n = g_store.loadFramebuffer(out.get(), FB_BYTES);
  Serial.printf("[engine] save/load patterned roundtrip: %u bytes\n",
                static_cast<unsigned>(n));
  TEST_ASSERT_EQUAL_UINT(FB_BYTES, n);
  TEST_ASSERT_EQUAL_INT(0, std::memcmp(fb.get(), out.get(), FB_BYTES));
}

void test_save_schedule_round_trip() {
  ScheduleSnapshot s{};
  s.fetched_at = 1700001234;
  s.hint[STREAM_58A_ATZ].last_today = 1700009999;
  s.hint[STREAM_58A_ATZ].first_tomorrow[0] = 1700020000;
  s.hint[STREAM_58A_ATZ].first_tomorrow[1] = 1700020600;
  s.hint[STREAM_U1_OBERLAA].last_today = 1700008888;
  g_store.saveSchedule(s);
  ScheduleSnapshot loaded = g_store.loadSchedule();
  TEST_ASSERT_EQUAL_INT64(1700001234, loaded.fetched_at);
  TEST_ASSERT_EQUAL_INT64(1700009999, loaded.hint[STREAM_58A_ATZ].last_today);
  TEST_ASSERT_EQUAL_INT64(1700020000,
                          loaded.hint[STREAM_58A_ATZ].first_tomorrow[0]);
  TEST_ASSERT_EQUAL_INT64(1700020600,
                          loaded.hint[STREAM_58A_ATZ].first_tomorrow[1]);
  TEST_ASSERT_EQUAL_INT64(1700008888,
                          loaded.hint[STREAM_U1_OBERLAA].last_today);
  // Streams we did not touch must still come back zeroed.
  TEST_ASSERT_EQUAL_INT64(0, loaded.hint[STREAM_58B_ATZ].first_tomorrow[0]);
}

void test_save_framebuffer_overflow_clears_valid() {
  // Build a worst-case input that won't fit into RLE_HARDCAP_BYTES: alternate
  // 0xAA/0x55 every byte → zero compression → encoded size = 2 * FB_BYTES.
  auto fb = std::make_unique<uint8_t[]>(FB_BYTES);
  for (size_t i = 0; i < FB_BYTES; ++i)
    fb[i] = (i & 1) ? 0xAA : 0x55;
  bool ok = g_store.saveFramebuffer(fb.get(), FB_BYTES);
  PersistedMeta m = g_store.loadMeta();
  Serial.printf("[engine] overflow save returned=%d fb_valid_after=%d\n", ok,
                m.framebuffer_valid);
  TEST_ASSERT_FALSE_MESSAGE(ok, "worst-case input must not fit in RLE budget");
  TEST_ASSERT_FALSE_MESSAGE(m.framebuffer_valid,
                            "framebuffer_valid not cleared on overflow");
  auto out = std::make_unique<uint8_t[]>(FB_BYTES);
  std::memset(out.get(), 0, FB_BYTES);
  TEST_ASSERT_EQUAL_UINT_MESSAGE(0,
                                 g_store.loadFramebuffer(out.get(), FB_BYTES),
                                 "load must refuse to decode after overflow");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_cold_rtc_load_returns_defaults);
  RUN_TEST(test_cold_rtc_load_framebuffer_returns_zero);
  RUN_TEST(test_cold_rtc_load_schedule_returns_zero);
  RUN_TEST(test_save_meta_round_trip);
  RUN_TEST(test_save_schedule_round_trip);
  RUN_TEST(test_save_framebuffer_round_trip_white);
  RUN_TEST(test_save_framebuffer_round_trip_random_pattern);
  // Overflow test poisons the store state, keep last.
  RUN_TEST(test_save_framebuffer_overflow_clears_valid);
  UNITY_END();
}

void loop() {}
