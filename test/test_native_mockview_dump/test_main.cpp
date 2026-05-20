// Host-side reproduction of the on-device mockview-N firmwares. Renders the
// same RenderInput each main_<n>_*.cpp wires up and dumps the framebuffer as
// PGM under .tmp/v2-pgm/mockview-<n>.pgm. scripts/pgm-to-png.py converts to
// PNG for docs/screenshots/host/. Stays in lockstep with tools/mockview by
// reusing the same buildNormalSnapshot / buildNightSnapshot builders.

#include "config.h"
#include "data/StreamSnapshot.h"
#include "render/layout.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unity.h>

// Source-include the mockview builders. Their NATIVE_BUILD guard was removed
// so the same TU can be compiled into both env:mockview-* (device) and this
// host test. Single source of truth for kMockNow + slot contents.
#include "../../tools/mockview/mock_data.cpp"

using namespace bustaferl;
using namespace bustaferl::mockview;

namespace {

constexpr const char *kPgmDir = ".tmp/v2-pgm";

void ensureDir() {
  mkdir(".tmp", 0755);
  mkdir(kPgmDir, 0755);
}

bool writePgm(const Frame &fb, const std::string &filename) {
  ensureDir();
  const std::string full = std::string(kPgmDir) + "/" + filename;
  FILE *f = std::fopen(full.c_str(), "wb");
  if (f == nullptr) {
    return false;
  }
  std::fprintf(f, "P5\n%d %d\n255\n", Frame::width, Frame::height);
  for (int y = 0; y < Frame::height; ++y) {
    for (int x = 0; x < Frame::width; ++x) {
      const unsigned char v = fb.getPixel(x, y) ? 255u : 0u;
      std::fputc(v, f);
    }
  }
  std::fclose(f);
  return true;
}

void renderAndDump(const RenderInput &in, const char *filename) {
  Frame fb;
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, filename));
}

} // namespace

void setUp() {
  // Force UTC — mockview firmware never calls setEnvTz, so localtime_r on the
  // ESP32 effectively renders UTC. The host inherits the system TZ from the
  // environment (usually CEST in dev), which would shift every HH:MM by +1/+2
  // and silently desync the host PNG from the device flash. Pin to UTC.
  setenv("TZ", "UTC0", 1);
  tzset();
}
void tearDown() {}

void test_mockview_1_normal() {
  RenderInput in;
  in.state = DisplayState::Normal;
  in.firmware_version = DISPLAY_VERSION_STR;
  in.snapshot = buildNormalSnapshot();
  renderAndDump(in, "mockview-1.pgm");
}

void test_mockview_2_veraltet() {
  RenderInput in;
  in.state = DisplayState::Stale;
  in.firmware_version = DISPLAY_VERSION_STR;
  in.snapshot = buildNormalSnapshot();
  renderAndDump(in, "mockview-2.pgm");
}

void test_mockview_3_nachtbetrieb() {
  RenderInput in;
  in.state = DisplayState::Night;
  in.firmware_version = DISPLAY_VERSION_STR;
  in.snapshot = buildNightSnapshot();
  renderAndDump(in, "mockview-3.pgm");
}

void test_mockview_4_keine_abfahrten() {
  RenderInput in;
  in.state = DisplayState::Quiet;
  in.firmware_version = DISPLAY_VERSION_STR;
  renderAndDump(in, "mockview-4.pgm");
}

void test_mockview_5_kein_empfang() {
  RenderInput in;
  in.state = DisplayState::Offline;
  in.firmware_version = DISPLAY_VERSION_STR;
  in.last_fetch_at = kMockNow - 437; // ~7 min ago
  in.retry_in_s = 23;
  renderAndDump(in, "mockview-5.pgm");
}

void test_mockview_6_auth_fehler() {
  RenderInput in;
  in.state = DisplayState::Auth;
  in.firmware_version = DISPLAY_VERSION_STR;
  std::strncpy(in.auth_aid_short, "OWDL4fE4", AUTH_AID_SHORT_CAP - 1);
  in.auth_http_code = 200;
  renderAndDump(in, "mockview-6.pgm");
}

void test_mockview_7_boot() {
  RenderInput in;
  in.state = DisplayState::Boot;
  in.firmware_version = DISPLAY_VERSION_STR;
  renderAndDump(in, "mockview-7.pgm");
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_mockview_1_normal);
  RUN_TEST(test_mockview_2_veraltet);
  RUN_TEST(test_mockview_3_nachtbetrieb);
  RUN_TEST(test_mockview_4_keine_abfahrten);
  RUN_TEST(test_mockview_5_kein_empfang);
  RUN_TEST(test_mockview_6_auth_fehler);
  RUN_TEST(test_mockview_7_boot);
  return UNITY_END();
}
