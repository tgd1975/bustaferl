// Roundtrip tests for native_runtime::DiskStore — the host mirror of
// Esp32PersistentStore used by test/test_native_runtime/. Runs in env:native.
//
// Cross-includes DiskStore.cpp from the sibling test_native_runtime/ folder.
// PIO 6.x has no shared test_common/; the same pattern is used by
// test_longterm_smoke ↔ test_longterm_soak (see plan §4.1, Schritt 0a.2).

#include "../test_native_runtime/DiskStore.cpp" // NOLINT(bugprone-suspicious-include)

#include <cstdio>
#include <cstring>
#include <string>
#include <unity.h>
#include <vector>

using namespace bustaferl;
using namespace bustaferl::native_runtime;

namespace {

constexpr const char *kTmpPath = ".tmp/native-runtime/test-persist.bin";

void resetFile() { std::remove(kTmpPath); }

} // namespace

void setUp() { resetFile(); }
void tearDown() { resetFile(); }

void test_cold_load_returns_zero_meta() {
  DiskStore s{kTmpPath};
  PersistedMeta m = s.loadMeta();
  TEST_ASSERT_EQUAL_INT64(0, m.last_ntp_sync);
  TEST_ASSERT_FALSE(m.framebuffer_valid);
}

void test_meta_roundtrip() {
  DiskStore s{kTmpPath};
  PersistedMeta in{};
  in.last_ntp_sync = 1700000123;
  in.last_api_success = 1700000456;
  in.partial_count = 42;
  in.filter_miss_streak = 3;
  s.saveMeta(in);

  DiskStore reopen{kTmpPath};
  PersistedMeta out = reopen.loadMeta();
  TEST_ASSERT_EQUAL_INT64(in.last_ntp_sync, out.last_ntp_sync);
  TEST_ASSERT_EQUAL_INT64(in.last_api_success, out.last_api_success);
  TEST_ASSERT_EQUAL_UINT16(in.partial_count, out.partial_count);
  TEST_ASSERT_EQUAL_UINT8(in.filter_miss_streak, out.filter_miss_streak);
}

void test_framebuffer_roundtrip() {
  DiskStore s{kTmpPath};
  std::vector<std::uint8_t> fb(15000, 0xA5);
  for (size_t i = 0; i < 200; ++i)
    fb[i] = static_cast<std::uint8_t>(i);

  TEST_ASSERT_TRUE(s.saveFramebuffer(fb.data(), fb.size()));

  std::vector<std::uint8_t> out(15000, 0);
  size_t n = s.loadFramebuffer(out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT(fb.size(), n);
  TEST_ASSERT_EQUAL_INT(0, std::memcmp(fb.data(), out.data(), fb.size()));
}

void test_framebuffer_load_without_save_returns_zero() {
  DiskStore s{kTmpPath};
  // saveMeta first so the magic is set but framebuffer_valid stays false.
  PersistedMeta m{};
  s.saveMeta(m);
  std::vector<std::uint8_t> out(15000, 0);
  size_t n = s.loadFramebuffer(out.data(), out.size());
  TEST_ASSERT_EQUAL_UINT(0, n);
}

void test_schedule_roundtrip() {
  DiskStore s{kTmpPath};
  ScheduleSnapshot in{};
  in.fetched_at = 1700001000;
  in.hint[0].last_today = 1700009999;
  in.hint[0].next_today[0] = 1700005000;
  in.hint[0].first_tomorrow[0] = 1700050000;
  s.saveSchedule(in);

  DiskStore reopen{kTmpPath};
  ScheduleSnapshot out = reopen.loadSchedule();
  TEST_ASSERT_EQUAL_INT64(in.fetched_at, out.fetched_at);
  TEST_ASSERT_EQUAL_INT64(in.hint[0].last_today, out.hint[0].last_today);
  TEST_ASSERT_EQUAL_INT64(in.hint[0].next_today[0], out.hint[0].next_today[0]);
  TEST_ASSERT_EQUAL_INT64(in.hint[0].first_tomorrow[0],
                          out.hint[0].first_tomorrow[0]);
}

void test_cold_after_file_delete() {
  DiskStore s{kTmpPath};
  PersistedMeta m{};
  m.last_ntp_sync = 1700000000;
  s.saveMeta(m);
  resetFile();

  DiskStore cold{kTmpPath};
  PersistedMeta out = cold.loadMeta();
  TEST_ASSERT_EQUAL_INT64(0, out.last_ntp_sync);
}

int main(int /*argc*/, char ** /*argv*/) {
  // Ensure .tmp/native-runtime/ exists for the file ops to succeed.
  std::system("mkdir -p .tmp/native-runtime");
  UNITY_BEGIN();
  RUN_TEST(test_cold_load_returns_zero_meta);
  RUN_TEST(test_meta_roundtrip);
  RUN_TEST(test_framebuffer_roundtrip);
  RUN_TEST(test_framebuffer_load_without_save_returns_zero);
  RUN_TEST(test_schedule_roundtrip);
  RUN_TEST(test_cold_after_file_delete);
  return UNITY_END();
}
