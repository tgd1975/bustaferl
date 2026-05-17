// Long-term Deep-Sleep + Persistent Store test (~30 min). Drives
// real deep-sleep wake cycles instead of light-sleep substitutes —
// every wake is a full chip reset, the RTC slow memory survives, the
// production magic-word + RLE round-trip path is exercised end to end.
// This is the only test that proves the production deep-sleep path
// behaves like the unit-test light-sleep substitute does.
//
// Cycle structure:
//   cold boot -> assert wake=ColdBoot, magic absent, save magic +
//                meta + RLE'd dummy framebuffer -> deepSleep(WAKE_S)
//   wake 1..N -> assert wake=Timer, magic+meta round-trip, RLE
//                round-trip, mutate meta, save, deepSleep(WAKE_S)
//   wake N    -> assert final invariants, do NOT sleep again so the
//                test framework can read the END marker.
//
// The cycle counter is itself stored in RTC slow memory so each wake
// knows where it is in the script. PIO sees one upload, N cold reads
// of the serial stream, one final Unity report. See
// [[push-past-not-testable]] and [[verify-via-on-device-tests]].
//
// Run via `make test-longterm-wake` (env:longterm-wake).

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#include <unity.h>

#include "hal/Esp32PersistentStore.h"
#include "hal/Esp32Sleep.h"

using namespace bustaferl;

namespace {

constexpr unsigned WAKE_S = 5;  // per-cycle deep-sleep duration
constexpr unsigned CYCLES = 30; // ~30 * (boot + WAKE_S) ≈ 5–10 min wallclock
constexpr uint32_t MAGIC = 0xBEEFC0DE;
constexpr int HEAP_LEAK_BUDGET_BYTES = 16 * 1024;

RTC_DATA_ATTR uint32_t g_rtc_magic = 0;
RTC_DATA_ATTR uint16_t g_rtc_cycle = 0;
RTC_DATA_ATTR uint32_t g_rtc_initial_heap = 0;

Esp32PersistentStore g_store;
Esp32Sleep g_sleep;

uint8_t g_fb[400 * 300 / 8]; // 15 KB scratch — file-scope, not on stack

void fillPattern(uint8_t *buf, size_t len, uint8_t seed) {
  for (size_t i = 0; i < len; ++i)
    buf[i] = static_cast<uint8_t>(seed ^ (i & 0xff));
}

bool verifyPattern(const uint8_t *buf, size_t len, uint8_t seed) {
  for (size_t i = 0; i < len; ++i)
    if (buf[i] != static_cast<uint8_t>(seed ^ (i & 0xff)))
      return false;
  return true;
}

} // namespace

void test_cold_boot_initialises_rtc(void) {
  WakeCause c = g_sleep.wakeupCause();
  Serial.printf(
      "[wake_cycle] cold_boot? wakeupCause=%d magic=0x%08x cycle=%u\n",
      static_cast<int>(c), g_rtc_magic, g_rtc_cycle);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(WakeCause::ColdBoot),
                        static_cast<int>(c));
  TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(
      MAGIC, g_rtc_magic,
      "cold boot but RTC magic already set — previous run not powered off?");

  g_rtc_magic = MAGIC;
  g_rtc_cycle = 0;
  g_rtc_initial_heap = ESP.getFreeHeap();

  PersistedMeta m;
  m.last_ntp_sync = 1700000000;
  m.partial_count = 1;
  m.filter_miss_streak = 0;
  m.framebuffer_valid = true;
  g_store.saveMeta(m);

  fillPattern(g_fb, sizeof(g_fb), 0);
  TEST_ASSERT_TRUE_MESSAGE(g_store.saveFramebuffer(g_fb, sizeof(g_fb)),
                           "cold: saveFramebuffer failed");
}

void test_post_wake_persists_meta_and_framebuffer(void) {
  WakeCause c = g_sleep.wakeupCause();
  Serial.printf("[wake_cycle] cycle=%u wake=%d magic=0x%08x heap=%u\n",
                g_rtc_cycle, static_cast<int>(c), g_rtc_magic,
                ESP.getFreeHeap());

  TEST_ASSERT_EQUAL_INT(static_cast<int>(WakeCause::Timer),
                        static_cast<int>(c));
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(MAGIC, g_rtc_magic,
                                   "RTC magic lost across deep sleep");

  PersistedMeta m = g_store.loadMeta();
  Serial.printf("[wake_cycle] loaded: ntp=%lld partial=%u fb_valid=%d\n",
                static_cast<long long>(m.last_ntp_sync), m.partial_count,
                m.framebuffer_valid);
  TEST_ASSERT_EQUAL_INT_MESSAGE(1700000000, static_cast<int>(m.last_ntp_sync),
                                "meta.last_ntp_sync drifted across sleep");
  TEST_ASSERT_TRUE_MESSAGE(m.framebuffer_valid,
                           "meta.framebuffer_valid lost across sleep");

  uint8_t loaded[sizeof(g_fb)];
  size_t got = g_store.loadFramebuffer(loaded, sizeof(loaded));
  Serial.printf("[wake_cycle] loadFramebuffer bytes=%u\n",
                static_cast<unsigned>(got));
  TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(g_fb), got,
                                   "framebuffer size mismatch after wake");
  TEST_ASSERT_TRUE_MESSAGE(
      verifyPattern(loaded, got, static_cast<uint8_t>(g_rtc_cycle - 1)),
      "framebuffer payload corrupted across deep sleep");

  // Roll the pattern + meta so the next cycle is a distinct round-trip
  // and a leak across saveFramebuffer surfaces.
  fillPattern(g_fb, sizeof(g_fb), static_cast<uint8_t>(g_rtc_cycle));
  TEST_ASSERT_TRUE_MESSAGE(g_store.saveFramebuffer(g_fb, sizeof(g_fb)),
                           "saveFramebuffer failed mid-run");
  m.partial_count = static_cast<uint16_t>(m.partial_count + 1);
  g_store.saveMeta(m);

  uint32_t now = ESP.getFreeHeap();
  int drift = static_cast<int>(g_rtc_initial_heap) - static_cast<int>(now);
  Serial.printf("[wake_cycle] heap initial=%u now=%u drift=%d budget=%d\n",
                g_rtc_initial_heap, now, drift, HEAP_LEAK_BUDGET_BYTES);
  TEST_ASSERT_LESS_THAN_INT_MESSAGE(
      HEAP_LEAK_BUDGET_BYTES, drift,
      "free-heap drift exceeds budget across wake cycles");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (g_rtc_magic != MAGIC) {
    // Cold boot or RTC blank: initialise + start the cycle script.
    UNITY_BEGIN();
    RUN_TEST(test_cold_boot_initialises_rtc);
    UNITY_END();
    Serial.printf("[wake_cycle] cold cycle done; sleeping %u s, %u cycles to "
                  "go\n",
                  WAKE_S, CYCLES);
    Serial.flush();
    g_rtc_cycle = 1;
    g_sleep.deepSleep(WAKE_S);
    // unreachable
  }

  if (g_rtc_cycle < CYCLES) {
    UNITY_BEGIN();
    RUN_TEST(test_post_wake_persists_meta_and_framebuffer);
    UNITY_END();
    Serial.printf("[wake_cycle] cycle %u/%u done; sleeping %u s\n", g_rtc_cycle,
                  CYCLES, WAKE_S);
    Serial.flush();
    g_rtc_cycle = static_cast<uint16_t>(g_rtc_cycle + 1);
    g_sleep.deepSleep(WAKE_S);
    // unreachable
  }

  // Final cycle: run the post-wake assertions and stop (no further sleep)
  // so the host can collect the Unity END marker.
  UNITY_BEGIN();
  RUN_TEST(test_post_wake_persists_meta_and_framebuffer);
  UNITY_END();
  Serial.printf("[wake_cycle] DONE: %u cycles completed, exiting\n", CYCLES);
}

void loop() {}
