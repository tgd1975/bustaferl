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
  // Status returned on the success attempt; non-2xx forces fetchWithRetry to
  // keep retrying. Failure attempts always return {false, 0}.
  int success_status = 200;
  // Status returned when call_count < succeed_on_attempt; default 0 (transport
  // fail). Set to e.g. 503 to simulate server errors that should still retry.
  int fail_status = 0;

  bool connect(unsigned) override { return true; }
  bool isConnected() override { return true; }
  HttpResult httpGet(const std::string &, std::string &out) override {
    ++call_count;
    if (call_count >= succeed_on_attempt) {
      out = body_to_return;
      return {true, success_status};
    }
    out.clear();
    return {fail_status != 0, fail_status};
  }
  HttpResult httpPost(const std::string &, const std::string &body,
                      const std::string &, std::string &out) override {
    last_post_body = body;
    ++call_count;
    if (call_count >= succeed_on_attempt) {
      out = body_to_return;
      return {true, success_status};
    }
    out.clear();
    return {fail_status != 0, fail_status};
  }

  std::string last_post_body;
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

// HttpResult propagation: a 200 reaches the caller as ok=true + status=200.
void test_success_carries_http_status_200() {
  FakeNet net;
  net.succeed_on_attempt = 1;
  net.success_status = 200;
  std::string body;
  FetchConfig cfg;
  cfg.max_attempts = 1;
  cfg.backoff_ms_base = 0;
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT(200, r.http_status);
}

// 401 stays visible to the caller (auth-tripwire prep — Schritt 5 will use
// this to drive PersistedMeta::ogd_auth_streak). Session B verifies only the
// status-propagation; retry-policy refinements come with Schritt 5.6.
void test_auth_401_status_visible_to_caller() {
  FakeNet net;
  net.succeed_on_attempt = 99;   // never succeeds via 2xx
  net.fail_status = 401;         // every call returns 401
  std::string body;
  FetchConfig cfg;
  cfg.max_attempts = 2;
  cfg.backoff_ms_base = 0;
  FetchOutcome r = fetchWithRetry(net, "http://x", body, cfg);
  TEST_ASSERT_FALSE(r.ok);       // 401 is not a 2xx success
  TEST_ASSERT_EQUAL_INT(401, r.http_status);
}

// POST direct-call: FakeNet records the body so the caller can verify the
// payload made it through. Preparation for Schritt 5's fetchOebbStream.
void test_post_direct_call_sees_body_and_status() {
  FakeNet net;
  net.success_status = 200;
  net.body_to_return = "{\"ok\":true}";
  std::string out;
  HttpResult r = net.httpPost("http://x", "{\"req\":1}", "application/json",
                              out);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_INT(200, r.http_status);
  TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", out.c_str());
  TEST_ASSERT_EQUAL_STRING("{\"req\":1}", net.last_post_body.c_str());
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
  RUN_TEST(test_success_carries_http_status_200);
  RUN_TEST(test_auth_401_status_visible_to_caller);
  RUN_TEST(test_post_direct_call_sees_body_and_status);
  return UNITY_END();
}
