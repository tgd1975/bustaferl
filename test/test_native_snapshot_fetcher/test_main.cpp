// Host tests for snapshot_fetcher: apiUrlForBatch composition + fetchSnapshot
// batch iteration / merge / failure counting. Uses a FakeNet that routes by
// URL substring (mirrors the pattern from test_native_schedule_fetcher).

#include "data/wienerlinien_parse.h"
#include "hal/INetwork.h"
#include "logic/snapshot_fetcher.h"

#include <cstring>
#include <string>
#include <unity.h>
#include <vector>

using namespace bustaferl;

namespace {

class FakeNet : public INetwork {
public:
  std::vector<std::string> urls_seen;
  // url-substring → body to return. First substring match wins.
  std::vector<std::pair<std::string, std::string>> routes;
  bool fail_all = false;

  bool connect(unsigned) override { return true; }
  bool isConnected() override { return true; }
  HttpResult httpGet(const std::string &url, std::string &out) override {
    urls_seen.push_back(url);
    if (fail_all)
      return {false, 0};
    for (const auto &r : routes) {
      if (url.find(r.first) != std::string::npos) {
        out = r.second;
        return {true, 200};
      }
    }
    return {false, 0};
  }
  HttpResult httpPost(const std::string &, const std::string &,
                      const std::string &, std::string &) override {
    return {false, 0};
  }
};

// Minimal fixture: a monitors body with all five RBLs the production filter
// table expects. Each RBL serves exactly one departure so the merge step has
// something concrete to assert on. Times are deliberately chosen so they only
// differ in the `timePlanned` minute → easy to read in failure messages.
const char *kAllRblsBody = R"JSON({
  "data": {
    "monitors": [
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1001 } } },
        "lines": [{ "name": "58A", "towards": "Atzgersdorf",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:01:00.000+0100" } }
          ] } }]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1002 } } },
        "lines": [{ "name": "58A", "towards": "Hietzing",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:02:00.000+0100" } }
          ] } }]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1003 } } },
        "lines": [{ "name": "58B", "towards": "Atzgersdorf",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:03:00.000+0100" } }
          ] } }]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1004 } } },
        "lines": [{ "name": "U1", "towards": "Leopoldau",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:04:00.000+0100" } }
          ] } }]
      },
      {
        "locationStop": { "properties": { "attributes": { "rbl": 1005 } } },
        "lines": [{ "name": "U1", "towards": "Oberlaa",
          "departures": { "departure": [
            { "departureTime": { "timePlanned": "2024-01-01T12:05:00.000+0100" } }
          ] } }]
      }
    ]
  }
})JSON";

void buildFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {1001, "58A", "Atzgersdorf"};
  f[STREAM_58A_HIETZING] = {1002, "58A", "Hietzing"};
  f[STREAM_58B_ATZ] = {1003, "58B", "Atzgersdorf"};
  f[STREAM_U1_LEOPOLDAU] = {1004, "U1", "Leopoldau"};
  f[STREAM_U1_OBERLAA] = {1005, "U1", "Oberlaa"};
}

} // namespace

void test_apiUrlForBatch_appends_stopIds() {
  int ids[3] = {1001, 1002, 1003};
  std::string url = apiUrlForBatch("http://x?z=1", ids, 3);
  TEST_ASSERT_EQUAL_STRING("http://x?z=1&stopId=1001&stopId=1002&stopId=1003",
                           url.c_str());
}

void test_apiUrlForBatch_empty_count_returns_base() {
  int ids[1] = {0};
  std::string url = apiUrlForBatch("http://x", ids, 0);
  TEST_ASSERT_EQUAL_STRING("http://x", url.c_str());
}

void test_fetchSnapshot_happy_three_batches() {
  // STOPIDS_PER_QUERY=2 → 5 streams split into 3 batches (2+2+1). Every batch
  // returns the same kAllRblsBody — parser then picks out the two stopIds the
  // batch URL contained.
  FakeNet net;
  net.routes.push_back({"stopId=", kAllRblsBody});
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  bool ok = fetchSnapshot(net, "http://x?z=1", f, snap, sum);

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(snap.api_ok);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(0, sum.failed_batches);
  TEST_ASSERT_EQUAL_INT(3, static_cast<int>(net.urls_seen.size()));

  // Every stream must have its first slot populated by the parser.
  for (int i = 0; i < STREAM_COUNT; ++i) {
    TEST_ASSERT_TRUE_MESSAGE(snap.stream[i].slot[0].valid,
                             "stream slot[0] expected valid");
  }
}

void test_fetchSnapshot_url_uses_fetch_order_first_batch() {
  // FETCH_ORDER puts U1-Oberlaa + U1-Leopoldau into the first batch — the
  // first URL must contain those two RBLs, not the 58A pair.
  FakeNet net;
  net.routes.push_back({"stopId=", kAllRblsBody});
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  fetchSnapshot(net, "http://x?z=1", f, snap, sum);

  TEST_ASSERT_TRUE(net.urls_seen.size() >= 1);
  const std::string &first = net.urls_seen[0];
  TEST_ASSERT_NOT_NULL(std::strstr(first.c_str(), "stopId=1005"));
  TEST_ASSERT_NOT_NULL(std::strstr(first.c_str(), "stopId=1004"));
  TEST_ASSERT_NULL(std::strstr(first.c_str(), "stopId=1001"));
}

void test_fetchSnapshot_all_batches_fail_marks_api_not_ok() {
  FakeNet net;
  net.fail_all = true;
  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  bool ok = fetchSnapshot(net, "http://x", f, snap, sum);

  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_FALSE(snap.api_ok);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(3, sum.failed_batches);
}

void test_fetchSnapshot_partial_failure_keeps_api_ok() {
  // First batch URL contains "stopId=1005" (U1-Obe per FETCH_ORDER) — make
  // every other route succeed but route THAT specific URL to a non-matching
  // path so it fails. We can target by an exact-RBL substring.
  FakeNet net;
  // Default route — every URL not matching the failure substring.
  net.routes.push_back({"stopId=", kAllRblsBody});
  // FakeNet returns the first substring match → no way to selectively fail.
  // Switch strategy: parse failure via malformed body for one specific URL.

  // Simpler: drive partial failure by giving back an unparseable body on
  // every URL containing the very first FETCH_ORDER RBL (=1005).
  net.routes.clear();
  net.routes.push_back({"stopId=1005", "{ this is not json"});
  net.routes.push_back({"stopId=", kAllRblsBody});

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  FetchSummary sum;
  bool ok = fetchSnapshot(net, "http://x?z=1", f, snap, sum);

  TEST_ASSERT_TRUE(ok); // 2 of 3 batches succeed
  TEST_ASSERT_TRUE(snap.api_ok);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(1, sum.failed_batches);
  // The two streams in the failed batch must have empty slots; the other
  // three must be populated by their batch's response.
  TEST_ASSERT_FALSE(snap.stream[STREAM_U1_OBERLAA].slot[0].valid);
  TEST_ASSERT_FALSE(snap.stream[STREAM_U1_LEOPOLDAU].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_58B_ATZ].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_58A_HIETZING].slot[0].valid);
  TEST_ASSERT_TRUE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
}

void test_fetchSnapshot_reset_clears_previous_out_state() {
  // Pre-populate `out` with stale data to make sure fetchSnapshot zeros it.
  FakeNet net;
  net.fail_all = true;

  StreamFilter f[STREAM_COUNT];
  buildFilters(f);

  StreamSnapshot snap;
  snap.api_ok = true;
  snap.stream[STREAM_58A_ATZ].slot[0].valid = true;
  snap.stream[STREAM_58A_ATZ].slot[0].when = 999;
  FetchSummary sum;
  sum.total_batches = 99;
  sum.failed_batches = 7;

  fetchSnapshot(net, "http://x", f, snap, sum);

  TEST_ASSERT_FALSE(snap.api_ok);
  TEST_ASSERT_FALSE(snap.stream[STREAM_58A_ATZ].slot[0].valid);
  TEST_ASSERT_EQUAL_INT(3, sum.total_batches);
  TEST_ASSERT_EQUAL_INT(3, sum.failed_batches);
}

void setUp() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_apiUrlForBatch_appends_stopIds);
  RUN_TEST(test_apiUrlForBatch_empty_count_returns_base);
  RUN_TEST(test_fetchSnapshot_happy_three_batches);
  RUN_TEST(test_fetchSnapshot_url_uses_fetch_order_first_batch);
  RUN_TEST(test_fetchSnapshot_all_batches_fail_marks_api_not_ok);
  RUN_TEST(test_fetchSnapshot_partial_failure_keeps_api_ok);
  RUN_TEST(test_fetchSnapshot_reset_clears_previous_out_state);
  return UNITY_END();
}
