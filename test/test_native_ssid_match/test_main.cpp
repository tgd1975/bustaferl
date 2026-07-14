// Pure-logic tests for logic/ssid_match: the case-insensitive SSID compare and
// the "configured matches a visible SSID only ignoring case" detector that
// drives the KEIN-EMPFANG "did you mean?" hint (field-observed "A-NET2" config
// vs "a-net2" broadcast).

#include "logic/ssid_match.h"

#include <cstdio>
#include <unity.h>

using namespace bustaferl;

namespace {

ConfiguredSsids wanted(const char *a, const char *b = nullptr) {
  ConfiguredSsids c;
  std::snprintf(c.ssid[0], sizeof(c.ssid[0]), "%s", a);
  c.count = 1;
  if (b != nullptr) {
    std::snprintf(c.ssid[1], sizeof(c.ssid[1]), "%s", b);
    c.count = 2;
  }
  return c;
}

ScanResult visible(const char *a, const char *b = nullptr) {
  ScanResult r;
  std::snprintf(r.aps[0].ssid, sizeof(r.aps[0].ssid), "%s", a);
  r.count = 1;
  if (b != nullptr) {
    std::snprintf(r.aps[1].ssid, sizeof(r.aps[1].ssid), "%s", b);
    r.count = 2;
  }
  return r;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_ignore_case_equal_lengths() {
  TEST_ASSERT_TRUE(ssidEqualsIgnoreCase("A-NET2", "a-net2"));
  TEST_ASSERT_TRUE(ssidEqualsIgnoreCase("a-net2", "a-net2"));
  TEST_ASSERT_TRUE(ssidEqualsIgnoreCase("MiXeD", "mixed"));
}

void test_ignore_case_different_strings() {
  TEST_ASSERT_FALSE(ssidEqualsIgnoreCase("a-net2", "a-net2-o"));
  TEST_ASSERT_FALSE(ssidEqualsIgnoreCase("a-net2", "b-net2"));
  // Prefix must not count as equal (length differs).
  TEST_ASSERT_FALSE(ssidEqualsIgnoreCase("net", "network"));
}

void test_ignore_case_null_safe() {
  TEST_ASSERT_FALSE(ssidEqualsIgnoreCase(nullptr, "x"));
  TEST_ASSERT_FALSE(ssidEqualsIgnoreCase("x", nullptr));
}

void test_ignore_case_leaves_non_ascii_exact() {
  // Only A-Z/a-z fold; bytes outside that range compare exactly. A high byte
  // (0xC3) is not case-folded, so it must differ from an ASCII letter in the
  // same slot. Split escape into its own array to avoid hex-run ambiguity.
  const char with_high[] = {'n', 'e', 't', '\xc3', '\0'};
  const char plain[] = {'n', 'e', 't', 'a', '\0'};
  TEST_ASSERT_FALSE(ssidEqualsIgnoreCase(with_high, plain));
  // Two identical high bytes still compare equal.
  TEST_ASSERT_TRUE(ssidEqualsIgnoreCase(with_high, with_high));
}

void test_mismatch_detected_case_only() {
  SsidCaseMismatch m = findCaseMismatch(wanted("A-NET2"), visible("a-net2"));
  TEST_ASSERT_TRUE(m.found);
  TEST_ASSERT_EQUAL_STRING("A-NET2", m.configured);
  TEST_ASSERT_EQUAL_STRING("a-net2", m.visible);
}

void test_exact_match_is_not_a_mismatch() {
  // An exact match would have connected — not reported as a "did you mean?".
  SsidCaseMismatch m = findCaseMismatch(wanted("a-net2"), visible("a-net2"));
  TEST_ASSERT_FALSE(m.found);
}

void test_no_match_at_all() {
  SsidCaseMismatch m =
      findCaseMismatch(wanted("A-NET2"), visible("ZTE_A36C25"));
  TEST_ASSERT_FALSE(m.found);
}

void test_mismatch_scans_all_visible() {
  // The lowercase twin is the 2nd visible AP; still found.
  SsidCaseMismatch m =
      findCaseMismatch(wanted("A-NET2"), visible("ZTE_ED635E", "a-net2"));
  TEST_ASSERT_TRUE(m.found);
  TEST_ASSERT_EQUAL_STRING("a-net2", m.visible);
}

void test_mismatch_checks_secondary_configured() {
  // Primary absent, secondary present only by case.
  SsidCaseMismatch m = findCaseMismatch(wanted("Zuhause", "Fallback-Net"),
                                        visible("fallback-net"));
  TEST_ASSERT_TRUE(m.found);
  TEST_ASSERT_EQUAL_STRING("Fallback-Net", m.configured);
  TEST_ASSERT_EQUAL_STRING("fallback-net", m.visible);
}

void test_empty_inputs_no_mismatch() {
  TEST_ASSERT_FALSE(
      findCaseMismatch(ConfiguredSsids{}, visible("a-net2")).found);
  TEST_ASSERT_FALSE(findCaseMismatch(wanted("a-net2"), ScanResult{}).found);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_ignore_case_equal_lengths);
  RUN_TEST(test_ignore_case_different_strings);
  RUN_TEST(test_ignore_case_null_safe);
  RUN_TEST(test_ignore_case_leaves_non_ascii_exact);
  RUN_TEST(test_mismatch_detected_case_only);
  RUN_TEST(test_exact_match_is_not_a_mismatch);
  RUN_TEST(test_no_match_at_all);
  RUN_TEST(test_mismatch_scans_all_visible);
  RUN_TEST(test_mismatch_checks_secondary_configured);
  RUN_TEST(test_empty_inputs_no_mismatch);
  return UNITY_END();
}
