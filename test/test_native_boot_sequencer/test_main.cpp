#include "hal/IClock.h"
#include "hal/INetwork.h"
#include "logic/boot_sequencer.h"

#include <unity.h>

using namespace bustaferl;

namespace {

class FakeNet : public INetwork {
public:
  bool will_connect = true;
  bool connect(unsigned) override { return will_connect; }
  bool isConnected() override { return will_connect; }
  bool httpGet(const std::string &, std::string &) override { return false; }
};

class FakeClock : public IClock {
public:
  bool will_sync = true;
  time_t fake_now = 1700000000;
  time_t now() override { return fake_now; }
  bool ntpSync() override { return will_sync; }
  time_t lastSync() const override { return fake_now; }
};

} // namespace

void test_happy_path_returns_ok() {
  FakeNet n;
  FakeClock c;
  BootConfig cfg;
  auto r = runColdBoot(n, c, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(BootResult::Ok), static_cast<int>(r));
}

void test_wifi_fail_first_attempt_retry_later() {
  FakeNet n;
  n.will_connect = false;
  FakeClock c;
  BootConfig cfg;
  cfg.max_retries = 5;
  auto r = runColdBoot(n, c, 0, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(BootResult::RetryLater),
                    static_cast<int>(r));
}

void test_wifi_fail_last_attempt_give_up() {
  FakeNet n;
  n.will_connect = false;
  FakeClock c;
  BootConfig cfg;
  cfg.max_retries = 5;
  auto r = runColdBoot(n, c, 4, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(BootResult::GiveUp), static_cast<int>(r));
}

void test_ntp_fail_treated_like_wifi_fail() {
  FakeNet n;
  FakeClock c;
  c.will_sync = false;
  BootConfig cfg;
  cfg.max_retries = 5;
  auto r = runColdBoot(n, c, 4, cfg);
  TEST_ASSERT_EQUAL(static_cast<int>(BootResult::GiveUp), static_cast<int>(r));
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_happy_path_returns_ok);
  RUN_TEST(test_wifi_fail_first_attempt_retry_later);
  RUN_TEST(test_wifi_fail_last_attempt_give_up);
  RUN_TEST(test_ntp_fail_treated_like_wifi_fail);
  return UNITY_END();
}
