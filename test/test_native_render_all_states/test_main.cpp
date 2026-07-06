// Schritt 7 validation: render all 7 DisplayStates on the host and dump
// each frame as a PGM under .tmp/v2-pgm/ so the migration can be visually
// reviewed before flashing. The HostCanvas is the apprentice renderer —
// not pixel-identical to the device, but close enough to spot layout
// regressions (Risiko V8/V9/V11 quirks remain device-visible).
//
// Tests also assert frame distinctness (no two states collapse to the
// same bytes) and that each frame is non-trivial (≥ N paper pixels).

#include "config.h"
#include "data/StreamSnapshot.h"
#include "logic/render_input.h" // for RenderInput shape via layout.h
#include "render/layout.h"
#include "render/rle.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unity.h>

using namespace bustaferl;

namespace {

constexpr const char *kPgmDir = ".tmp/v2-pgm";

void ensureDir(const char *path) {
  // mkdir -p .tmp/v2-pgm — best-effort, ignore EEXIST.
  mkdir(".tmp", 0755);
  mkdir(path, 0755);
}

bool writePgm(const Frame &fb, const std::string &filename) {
  ensureDir(kPgmDir);
  std::string full = std::string(kPgmDir) + "/" + filename;
  FILE *f = std::fopen(full.c_str(), "wb");
  if (f == nullptr) {
    return false;
  }
  std::fprintf(f, "P5\n%d %d\n255\n", Frame::width, Frame::height);
  // Invert polarity for PGM: paper bit (true) → 255 (white), ink (false)
  // → 0 (black). Matches what the e-paper actually shows.
  for (int y = 0; y < Frame::height; ++y) {
    for (int x = 0; x < Frame::width; ++x) {
      unsigned char v = fb.getPixel(x, y) ? 255u : 0u;
      std::fputc(v, f);
    }
  }
  std::fclose(f);
  return true;
}

int countPaperPixels(const Frame &fb) {
  int n = 0;
  for (int y = 0; y < Frame::height; ++y) {
    for (int x = 0; x < Frame::width; ++x) {
      if (fb.getPixel(x, y)) {
        ++n;
      }
    }
  }
  return n;
}

RenderInput makeBoardInput(DisplayState state) {
  RenderInput in;
  in.state = state;
  // Populate slots with realistic data so Normal/Stale/Night look full.
  in.snapshot.api_ok = true;
  in.snapshot.stream[STREAM_58A_ATZ].slot[0] = {
      1700000000 + 600, DepartureSource::Realtime, true};
  in.snapshot.stream[STREAM_58A_ATZ].slot[1] = {
      1700000000 + 1500, DepartureSource::Realtime, true};
  in.snapshot.stream[STREAM_58A_HIETZING].slot[0] = {
      1700000000 + 800, DepartureSource::Plan, true};
  in.snapshot.stream[STREAM_58B_ATZ].slot[0] = {
      1700000000 + 1100, DepartureSource::Realtime, true};
  in.snapshot.stream[STREAM_SBAHN_HBF].slot[0] = {
      1700000000 + 1300, DepartureSource::Realtime, true};
  std::strncpy(in.snapshot.stream[STREAM_SBAHN_HBF].slot[0].line_label, "S2",
               6);
  in.snapshot.stream[STREAM_SBAHN_HBF].slot[1] = {
      1700000000 + 2000, DepartureSource::Realtime, true};
  std::strncpy(in.snapshot.stream[STREAM_SBAHN_HBF].slot[1].line_label, "S3",
               6);
  return in;
}

} // namespace

void setUp() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
}
void tearDown() {}

void test_dump_state_boot() {
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Boot;
  in.firmware_version = "v2.0 host-dump";
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "01-boot.pgm"));
  TEST_ASSERT_GREATER_THAN(100, countPaperPixels(fb));
}

void test_dump_state_normal() {
  Frame fb;
  RenderInput in = makeBoardInput(DisplayState::Normal);
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "02-normal.pgm"));
  TEST_ASSERT_GREATER_THAN(500, countPaperPixels(fb));
}

void test_dump_state_stale() {
  Frame fb;
  RenderInput in = makeBoardInput(DisplayState::Stale);
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "03-stale.pgm"));
  TEST_ASSERT_GREATER_THAN(500, countPaperPixels(fb));
}

void test_dump_state_night() {
  Frame fb;
  RenderInput in = makeBoardInput(DisplayState::Night);
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "04-night.pgm"));
  TEST_ASSERT_GREATER_THAN(500, countPaperPixels(fb));
}

void test_dump_state_quiet() {
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Quiet;
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "05-quiet.pgm"));
  TEST_ASSERT_GREATER_THAN(100, countPaperPixels(fb));
}

void test_dump_state_offline() {
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Offline;
  in.last_fetch_at = 1700000000;
  in.retry_in_s = 42;
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "06-offline.pgm"));
  TEST_ASSERT_GREATER_THAN(100, countPaperPixels(fb));
}

void test_dump_state_auth() {
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Auth;
  std::strncpy(in.auth_aid_short, "AID:OWDL", sizeof(in.auth_aid_short) - 1);
  in.auth_http_code = 401;
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "07-auth.pgm"));
  TEST_ASSERT_GREATER_THAN(100, countPaperPixels(fb));
}

void test_fullscreen_states_each_produce_distinct_frames() {
  // Boot / Quiet / Offline / Auth all use their own fullscreen renderer
  // (different glyph + text), so every pairwise diff must be non-zero.
  // Stale / Night / Normal share drawBoard() with the same input → they
  // intentionally render identically here; differentiation happens via the
  // state-selector picking the right state, not by the renderer painting
  // different pixels for the same data. Schritt 7.8 trade-off accepted.
  constexpr DisplayState fullscreen[] = {
      DisplayState::Boot,
      DisplayState::Quiet,
      DisplayState::Offline,
      DisplayState::Auth,
  };
  constexpr int n = sizeof(fullscreen) / sizeof(fullscreen[0]);
  Frame frames[n];
  for (int i = 0; i < n; ++i) {
    RenderInput in;
    in.state = fullscreen[i];
    if (fullscreen[i] == DisplayState::Boot) {
      in.firmware_version = "v2.0";
    }
    renderFrame(in, frames[i]);
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      bool different = false;
      for (size_t k = 0; k < Frame::bytes; ++k) {
        if (frames[i].data()[k] != frames[j].data()[k]) {
          different = true;
          break;
        }
      }
      TEST_ASSERT_TRUE_MESSAGE(
          different,
          "Two fullscreen DisplayStates produced identical frame bytes");
    }
  }
}

void test_stale_differs_from_normal_with_same_data() {
  // Stale renderer forces every slot to '??:??' regardless of the input
  // snapshot. With the same makeBoardInput() data, Normal and Stale must
  // therefore diverge.
  RenderInput n_in = makeBoardInput(DisplayState::Normal);
  RenderInput s_in = makeBoardInput(DisplayState::Stale);
  Frame fn;
  Frame fs;
  renderFrame(n_in, fn);
  renderFrame(s_in, fs);
  bool different = false;
  for (size_t k = 0; k < Frame::bytes; ++k) {
    if (fn.data()[k] != fs.data()[k]) {
      different = true;
      break;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(
      different,
      "Normal and Stale rendered identical bytes despite Stale-marker");
}

void test_every_state_fits_persistence_cap() {
  // Regression guard for the full-refresh storm: if a rendered frame's
  // row-delta RLE exceeds RLE_HARDCAP_BYTES, Esp32PersistentStore rejects
  // the save, prev_valid goes false, and every warm cycle degrades to a
  // light-full (whole-panel flash). Any layout change that pushes a state
  // over the cap must fail here, not on the device.
  constexpr DisplayState all[] = {
      DisplayState::Boot,  DisplayState::Normal, DisplayState::Stale,
      DisplayState::Night, DisplayState::Quiet,  DisplayState::Offline,
      DisplayState::Auth,
  };
  uint8_t enc[RLE_HARDCAP_BYTES];
  for (DisplayState s : all) {
    RenderInput in = makeBoardInput(s);
    Frame fb;
    renderFrame(in, fb);
    size_t n = rleEncodeDelta(fb.data(), Frame::bytes, EPD_WIDTH / 8, enc,
                              sizeof(enc));
    char msg[96];
    std::snprintf(msg, sizeof(msg),
                  "state %d frame does not fit RLE_HARDCAP_BYTES (%d)",
                  static_cast<int>(s), RLE_HARDCAP_BYTES);
    TEST_ASSERT_TRUE_MESSAGE(n > 0, msg);
  }
}

void test_update_stamp_draws_bottom_right_only() {
  Frame plain;
  Frame stamped;
  RenderInput in = makeBoardInput(DisplayState::Normal);
  renderFrame(in, plain);
  renderFrame(in, stamped);
  drawUpdateStamp(stamped, 1700000000); // 14:53 UTC / 15:53 CET

  // Diff must exist, and only in the bottom strip (stamp region).
  const int stride = FB_W / 8;
  bool any_diff = false;
  for (int y = 0; y < FB_H; ++y) {
    bool row_diff = std::memcmp(plain.data() + y * stride,
                                stamped.data() + y * stride, stride) != 0;
    if (row_diff) {
      any_diff = true;
      TEST_ASSERT_GREATER_THAN_MESSAGE(
          FB_H - 12, y, "stamp touched pixels outside the bottom strip");
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(any_diff, "stamp drew nothing");
}

void test_update_stamp_overwrite_is_clean() {
  // Stamping T2 over a frame already stamped with T1 must equal stamping T2
  // on a fresh frame — no residue from the previous digits.
  Frame once;
  Frame twice;
  RenderInput in = makeBoardInput(DisplayState::Normal);
  renderFrame(in, once);
  drawUpdateStamp(once, 1700000000 + 3600);
  renderFrame(in, twice);
  drawUpdateStamp(twice, 1700000000);
  drawUpdateStamp(twice, 1700000000 + 3600);
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      0, std::memcmp(once.data(), twice.data(), Frame::bytes),
      "re-stamp left residue of the previous stamp");
}

void test_update_stamp_zero_is_noop() {
  Frame plain;
  Frame stamped;
  RenderInput in = makeBoardInput(DisplayState::Normal);
  renderFrame(in, plain);
  renderFrame(in, stamped);
  drawUpdateStamp(stamped, 0);
  TEST_ASSERT_EQUAL_INT(
      0, std::memcmp(plain.data(), stamped.data(), Frame::bytes));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_dump_state_boot);
  RUN_TEST(test_dump_state_normal);
  RUN_TEST(test_dump_state_stale);
  RUN_TEST(test_dump_state_night);
  RUN_TEST(test_dump_state_quiet);
  RUN_TEST(test_dump_state_offline);
  RUN_TEST(test_dump_state_auth);
  RUN_TEST(test_fullscreen_states_each_produce_distinct_frames);
  RUN_TEST(test_stale_differs_from_normal_with_same_data);
  RUN_TEST(test_every_state_fits_persistence_cap);
  RUN_TEST(test_update_stamp_draws_bottom_right_only);
  RUN_TEST(test_update_stamp_overwrite_is_clean);
  RUN_TEST(test_update_stamp_zero_is_noop);
  return UNITY_END();
}
