// Tier 1 — diagnostic page + boot-check text rendering (render/diag_page).
// Uses a FakeCanvas that captures every print() so the formatting logic is
// verified without the Adafruit/host font stack. The pixel-accurate device
// rendering is covered by the on-device render tests.

#include "data/DiagView.h"
#include "logic/cycle_trace.h"
#include "render/canvas.h"
#include "render/diag_page.h"

#include <string>
#include <unity.h>

using namespace bustaferl;

namespace {

// Records printed text (newline-joined) and swallows all geometry.
class FakeCanvas : public render::Canvas {
public:
  std::string out;
  void drawPixel(int, int, std::uint16_t) override {}
  int width() const override { return FB_W; }
  int height() const override { return FB_H; }
  void setCursor(int, int) override {}
  void setTextColor(std::uint16_t) override {}
  void setRoleFont(FontRole) override {}
  void print(const char *text) override {
    out += text;
    out += '\n';
  }
  int textWidth(const char *) override { return 0; }
};

bool contains(const std::string &hay, const char *needle) {
  return hay.find(needle) != std::string::npos;
}

DiagView sampleView() {
  DiagView v;
  v.has_net_info = true;
  std::snprintf(v.ssid, sizeof(v.ssid), "MeinNetz");
  std::snprintf(v.ip, sizeof(v.ip), "192.168.1.42");
  v.rssi_dbm = -58;
  v.now = 1736510400; // some 2025 epoch
  v.ntp_ok = true;
  v.last_ntp_sync = v.now - 3600;
  v.snap.stream[STREAM_58A_ATZ].endpoint_responded = true;
  v.snap.stream[STREAM_58A_ATZ].slot[0].valid = true;
  v.snap.stream[STREAM_58A_ATZ].slot[0].when = v.now + 300;
  v.snap.stream[STREAM_58A_ATZ].slot[0].source = DepartureSource::Realtime;
  v.heap_free_kb = 187;
  v.heap_largest_kb = 113;
  v.uptime_s = 42;
  return v;
}

} // namespace

void setUp() {}
void tearDown() {}

void test_status_page_lists_key_fields() {
  FakeCanvas c;
  drawDiagPage(c, sampleView(), DiagPage::Status);
  TEST_ASSERT_TRUE(contains(c.out, "STATUS"));
  TEST_ASSERT_TRUE(contains(c.out, "MeinNetz"));
  TEST_ASSERT_TRUE(contains(c.out, "192.168.1.42"));
  TEST_ASSERT_TRUE(contains(c.out, "Zeit"));
  TEST_ASSERT_TRUE(contains(c.out, "Heap"));
  TEST_ASSERT_TRUE(contains(c.out, "Seite 1/4"));
  TEST_ASSERT_TRUE(contains(c.out, "Letzter Reset"));
  TEST_ASSERT_TRUE(contains(c.out, "normal"));
}

void test_status_page_shows_brownout_reset_reason() {
  DiagView v = sampleView();
  v.last_reset_reason = ResetReason::Brownout;
  FakeCanvas c;
  drawDiagPage(c, v, DiagPage::Status);
  TEST_ASSERT_TRUE(contains(c.out, "Letzter Reset"));
  TEST_ASSERT_TRUE(contains(c.out, "Brownout"));
}

void test_cycles_page_shows_history_newest_first() {
  DiagView v = sampleView();
  CycleRecord a;
  a.at = static_cast<std::uint32_t>(v.now - 60);
  a.trigger = static_cast<std::uint8_t>(CycleTrigger::Timer);
  a.sleep_s = 30;
  tracePushCycle(v.trace, a);
  CycleRecord b;
  b.at = static_cast<std::uint32_t>(v.now);
  b.trigger = static_cast<std::uint8_t>(CycleTrigger::Button);
  b.flags = CYC_RESCUE_TRIED | CYC_RESCUE_OK;
  b.sleep_s = 30;
  tracePushCycle(v.trace, b);

  FakeCanvas c;
  drawDiagPage(c, v, DiagPage::Cycles);
  TEST_ASSERT_TRUE(contains(c.out, "ZYKLEN"));
  // Rescue marker from the newest (button) record + the trigger char.
  TEST_ASSERT_TRUE(contains(c.out, "R+"));
  TEST_ASSERT_TRUE(contains(c.out, "B "));
}

void test_cycles_page_empty_note() {
  FakeCanvas c;
  drawDiagPage(c, sampleView(), DiagPage::Cycles);
  TEST_ASSERT_TRUE(contains(c.out, "noch keine Zyklen"));
}

void test_errors_page_translates_codes() {
  DiagView v = sampleView();
  ErrorRecord e;
  e.at = static_cast<std::uint32_t>(v.now);
  e.code = static_cast<std::uint8_t>(TraceError::OebbAuth);
  tracePushError(v.trace, e);

  FakeCanvas c;
  drawDiagPage(c, v, DiagPage::Errors);
  TEST_ASSERT_TRUE(contains(c.out, "FEHLER"));
  TEST_ASSERT_TRUE(contains(c.out, "OEBB lehnt Zugang ab"));
}

void test_data_detail_shows_sources_and_legend() {
  FakeCanvas c;
  drawDiagPage(c, sampleView(), DiagPage::DataDetail);
  TEST_ASSERT_TRUE(contains(c.out, "DATEN-DETAILS"));
  TEST_ASSERT_TRUE(contains(c.out, "Panel"));
  TEST_ASSERT_TRUE(contains(c.out, "E=Echtzeit"));
}

void test_boot_check_shows_rtc_and_attempt() {
  DiagView v = sampleView();
  v.boot_attempt = 1;
  v.boot_attempts_max = 5;
  v.show_s = 15;
  FakeCanvas c;
  drawBootCheck(c, v);
  TEST_ASSERT_TRUE(contains(c.out, "BOOT-CHECK"));
  TEST_ASSERT_TRUE(contains(c.out, "RTC"));
  TEST_ASSERT_TRUE(contains(c.out, "WLAN & NTP ok (1/5)"));
  TEST_ASSERT_TRUE(contains(c.out, "Anzeige startet in 15 s"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_status_page_lists_key_fields);
  RUN_TEST(test_status_page_shows_brownout_reset_reason);
  RUN_TEST(test_cycles_page_shows_history_newest_first);
  RUN_TEST(test_cycles_page_empty_note);
  RUN_TEST(test_errors_page_translates_codes);
  RUN_TEST(test_data_detail_shows_sources_and_legend);
  RUN_TEST(test_boot_check_shows_rtc_and_attempt);
  return UNITY_END();
}
