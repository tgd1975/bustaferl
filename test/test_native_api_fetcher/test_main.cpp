#include "hal/INetwork.h"
#include "logic/api_fetcher.h"

#include <string>
#include <unity.h>

using namespace bustaferl;

namespace {

class FakeNet : public INetwork {
public:
  int call_count = 0;
  int succeed_on_attempt = 1; // 1-based: succeed at this call and onwards
  std::string body_to_return = "ok-body";

  bool connect(unsigned) override { return true; }
  bool isConnected() override { return true; }
  bool httpGet(const std::string &, std::string &out) override {
    ++call_count;
    if (call_count >= succeed_on_attempt) {
      out = body_to_return;
      return true;
    }
    out.clear();
    return false;
  }
};

} // namespace

void test_first_try_success() {
  FakeNet net;
  net.succeed_on_attempt = 1;
  std::string body;
  FetchConfig cfg;
  cfg.max_attempts = 3;
  cfg.backoff_ms_base = 0; // tests must stay instant
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT(1, r.attempts_taken);
  TEST_ASSERT_EQUAL_INT(1, net.call_count);
  TEST_ASSERT_EQUAL_STRING("ok-body", body.c_str());
}

void test_succeeds_on_second_attempt() {
  FakeNet net;
  net.succeed_on_attempt = 2;
  std::string body;
  FetchConfig cfg;
  cfg.max_attempts = 3;
  cfg.backoff_ms_base = 0;
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT(2, r.attempts_taken);
  TEST_ASSERT_EQUAL_INT(2, net.call_count);
}

void test_succeeds_on_last_attempt() {
  FakeNet net;
  net.succeed_on_attempt = 3;
  std::string body;
  FetchConfig cfg;
  cfg.max_attempts = 3;
  cfg.backoff_ms_base = 0;
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT(3, r.attempts_taken);
  TEST_ASSERT_EQUAL_INT(3, net.call_count);
}

void test_all_attempts_fail() {
  FakeNet net;
  net.succeed_on_attempt = 99; // never succeeds
  std::string body = "dirty-prev-content";
  FetchConfig cfg;
  cfg.max_attempts = 3;
  cfg.backoff_ms_base = 0;
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_INT(3, r.attempts_taken);
  TEST_ASSERT_EQUAL_INT(3, net.call_count);
  TEST_ASSERT_TRUE_MESSAGE(body.empty(), "body must be cleared on failure");
}

void test_single_attempt_no_retry() {
  FakeNet net;
  net.succeed_on_attempt = 2; // would succeed on attempt 2 but...
  std::string body;
  FetchConfig cfg;
  cfg.max_attempts = 1; // ...we only allow 1 try
  cfg.backoff_ms_base = 0;
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_INT(1, r.attempts_taken);
  TEST_ASSERT_EQUAL_INT(1, net.call_count);
}

void test_body_overwritten_on_success() {
  FakeNet net;
  net.succeed_on_attempt = 2;
  net.body_to_return = "fresh";
  std::string body = "stale";
  FetchConfig cfg;
  cfg.max_attempts = 3;
  cfg.backoff_ms_base = 0;
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("fresh", body.c_str());
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_first_try_success);
  RUN_TEST(test_succeeds_on_second_attempt);
  RUN_TEST(test_succeeds_on_last_attempt);
  RUN_TEST(test_all_attempts_fail);
  RUN_TEST(test_single_attempt_no_retry);
  RUN_TEST(test_body_overwritten_on_success);
  return UNITY_END();
}
