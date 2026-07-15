// Render every DisplayState + the four board DATA cases (live-only,
// schedule-only, no-data, mixed) on the host and dump each frame as a PGM under
// .tmp/v2-pgm/ for visual review before flashing. The HostCanvas is the
// apprentice renderer — not pixel-identical to the device, but close enough to
// spot layout regressions.
//
// Tests also assert frame distinctness and that each frame is non-trivial
// (≥ N paper pixels).

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
  // A sparse but realistic board (some slots left empty) used by the
  // persistence-cap, update-stamp, and alignment tests.
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

// Data-case demo builders. Every board is DisplayState::Normal now; what
// differs is the DATA (live / scheduled / none / mixed). These stand in for the
// removed Stale/Night/Quiet screens — the same board, driven by its data.

// Fill EVERY rendered slot (both TG/EG columns + all three S-Bahn slots) with
// the given source, so a "pure" demo shows a fully-populated board rather than
// makeBoardInput's sparse mix (which left several slots as "--:--"). For live
// (Realtime) slots a scheduled reference is set too, so the 58A gauge draws a
// real deviation BAR — a Realtime slot with planned==0 would still render the
// hollow "no comparison" square, which is what made live-only look scheduled.
RenderInput makeFullBoard(DepartureSource src) {
  RenderInput in;
  in.state = DisplayState::Normal;
  in.snapshot.api_ok = true;
  const time_t base = 1700000000;
  const bool live = (src == DepartureSource::Realtime);
  // Per-slot deviation (minutes late) so the live gauge bars vary visibly.
  auto fill = [&](int stream, int slot, int off_min, int dev_min,
                  const char *label) {
    Departure &d = in.snapshot.stream[stream].slot[slot];
    d = {base + off_min * 60, src, true};
    if (live) {
      // `when` is the live time; planned = when - deviation.
      d.planned = d.when - static_cast<time_t>(dev_min) * 60;
    }
    if (label[0] != '\0') {
      std::strncpy(d.line_label, label, 6);
    }
  };
  fill(STREAM_58A_ATZ, 0, 2, 1, "");       // +1 min
  fill(STREAM_58A_ATZ, 1, 18, -1, "");     // early
  fill(STREAM_58A_HIETZING, 0, 5, 3, "");  // +3 min
  fill(STREAM_58A_HIETZING, 1, 20, 0, ""); // on time
  fill(STREAM_58B_ATZ, 0, 11, 2, "");      // +2 min
  fill(STREAM_58B_ATZ, 1, 31, 0, "");      // on time
  fill(STREAM_SBAHN_HBF, 0, 7, 1, "S2");   // (no gauge on S-Bahn rows)
  fill(STREAM_SBAHN_HBF, 1, 21, 0, "S3");
  fill(STREAM_SBAHN_HBF, 2, 29, 0, "S2");
  return in;
}

// All departures live realtime (gauge draws deviation bars).
RenderInput makeLiveOnlyInput() {
  return makeFullBoard(DepartureSource::Realtime);
}

// All departures scheduled (Plan) — the overnight / no-live-data case
// (gauge draws the hollow "no live comparison" square).
RenderInput makeScheduleOnlyInput() {
  return makeFullBoard(DepartureSource::Plan);
}

// The everyday board: a full board with a realistic live + scheduled mix.
// Nearer departures are live (realtime), later ones fall back to the plan —
// so both the deviation gauges and the ° plan-markers appear together.
RenderInput makeMixedInput() {
  RenderInput in = makeFullBoard(DepartureSource::Realtime);
  // Push the later column of each row to Plan so live/scheduled coexist.
  in.snapshot.stream[STREAM_58A_ATZ].slot[1].source = DepartureSource::Plan;
  in.snapshot.stream[STREAM_58A_HIETZING].slot[1].source =
      DepartureSource::Plan;
  in.snapshot.stream[STREAM_58B_ATZ].slot[1].source = DepartureSource::Plan;
  in.snapshot.stream[STREAM_SBAHN_HBF].slot[2].source = DepartureSource::Plan;
  return in;
}

// No data at all — every slot invalid, so the board renders "--:--"
// placeholders. The old Stale / Quiet case, now just an empty Normal board.
RenderInput makeNoDataInput() {
  RenderInput in;
  in.state = DisplayState::Normal;
  in.snapshot.api_ok = true; // reachable API, just nothing to show
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

// The board, mixed live + scheduled data (the everyday case).
void test_dump_board_mixed() {
  Frame fb;
  RenderInput in = makeMixedInput();
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "02-board-mixed.pgm"));
  TEST_ASSERT_GREATER_THAN(500, countPaperPixels(fb));
}

// Data case: only live realtime departures.
void test_dump_board_live_only() {
  Frame fb;
  RenderInput in = makeLiveOnlyInput();
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "03-board-live-only.pgm"));
  TEST_ASSERT_GREATER_THAN(500, countPaperPixels(fb));
}

// Data case: only scheduled (Plan) departures — overnight / no live data.
void test_dump_board_schedule_only() {
  Frame fb;
  RenderInput in = makeScheduleOnlyInput();
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "04-board-schedule-only.pgm"));
  TEST_ASSERT_GREATER_THAN(500, countPaperPixels(fb));
}

// Data case: no data at all — every slot renders "--:--". Was Stale/Quiet.
void test_dump_board_no_data() {
  Frame fb;
  RenderInput in = makeNoDataInput();
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "05-board-no-data.pgm"));
  // Still non-trivial: headers, badges, network plan, and the "--:--" dashes.
  TEST_ASSERT_GREATER_THAN(500, countPaperPixels(fb));
}

void test_dump_state_offline() {
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Offline;
  in.last_fetch_at = 1700000000;
  in.retry_in_s = 42;
  // Visible APs the scan saw — rendered as the "Gefundene Netze" list.
  in.visible_aps.count = 2;
  std::snprintf(in.visible_aps.aps[0].ssid, sizeof(in.visible_aps.aps[0].ssid),
                "%s", "A-NET2");
  in.visible_aps.aps[0].rssi_dbm = -67;
  in.visible_aps.aps[0].channel = 6;
  std::snprintf(in.visible_aps.aps[1].ssid, sizeof(in.visible_aps.aps[1].ssid),
                "%s", "Nachbar-WLAN");
  in.visible_aps.aps[1].rssi_dbm = -82;
  in.visible_aps.aps[1].channel = 11;
  // SSIDs the device is looking for — shown on the "gesucht:" line.
  in.wanted_ssids.count = 1;
  std::snprintf(in.wanted_ssids.ssid[0], sizeof(in.wanted_ssids.ssid[0]), "%s",
                "Zuhause-WLAN");
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "06-offline.pgm"));
  TEST_ASSERT_GREATER_THAN(100, countPaperPixels(fb));
}

// Case-mismatch variant of the KEIN-EMPFANG screen: the "did you mean?" hint
// replaces the plain "gesucht:" line. Dumped so the layout can be reviewed.
void test_dump_state_offline_case_mismatch() {
  Frame fb;
  RenderInput in;
  in.state = DisplayState::Offline;
  in.last_fetch_at = 1700000000;
  in.retry_in_s = 42;
  in.visible_aps.count = 1;
  std::snprintf(in.visible_aps.aps[0].ssid, sizeof(in.visible_aps.aps[0].ssid),
                "%s", "a-net2");
  in.visible_aps.aps[0].rssi_dbm = -63;
  in.wanted_ssids.count = 1;
  std::snprintf(in.wanted_ssids.ssid[0], sizeof(in.wanted_ssids.ssid[0]), "%s",
                "A-NET2");
  in.case_mismatch.found = true;
  std::snprintf(in.case_mismatch.configured,
                sizeof(in.case_mismatch.configured), "%s", "A-NET2");
  std::snprintf(in.case_mismatch.visible, sizeof(in.case_mismatch.visible),
                "%s", "a-net2");
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "06b-offline-case.pgm"));
  TEST_ASSERT_GREATER_THAN(100, countPaperPixels(fb));
}

// The visible-AP list must actually reach the glass: renderFrame paints text
// in paper (1) on an ink (0) background, so more SSID rows ⇒ more paper pixels
// than fewer rows under the same "Gefundene Netze" header. (Comparing against
// an empty scan is confounded — the no-networks fallback line is itself long
// text.)
void test_offline_ssid_list_adds_ink() {
  RenderInput one;
  one.state = DisplayState::Offline;
  one.visible_aps.count = 1;
  std::snprintf(one.visible_aps.aps[0].ssid,
                sizeof(one.visible_aps.aps[0].ssid), "%s", "A-NET2");
  one.visible_aps.aps[0].rssi_dbm = -67;
  Frame fb_one;
  renderFrame(one, fb_one);

  RenderInput three = one;
  three.visible_aps.count = 3;
  std::snprintf(three.visible_aps.aps[1].ssid,
                sizeof(three.visible_aps.aps[1].ssid), "%s", "Nachbar-WLAN");
  three.visible_aps.aps[1].rssi_dbm = -82;
  std::snprintf(three.visible_aps.aps[2].ssid,
                sizeof(three.visible_aps.aps[2].ssid), "%s", "Gast-Netz");
  three.visible_aps.aps[2].rssi_dbm = -90;
  Frame fb_three;
  renderFrame(three, fb_three);

  // Two extra SSID rows ⇒ more paper text ⇒ strictly more paper pixels.
  const int paper_one = countPaperPixels(fb_one);
  const int paper_three = countPaperPixels(fb_three);
  TEST_ASSERT_TRUE(paper_three > paper_one);
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

void test_dump_state_wifi_auth() {
  Frame fb;
  RenderInput in;
  in.state = DisplayState::WifiAuth;
  in.wanted_ssids.count = 1;
  std::snprintf(in.wanted_ssids.ssid[0], sizeof(in.wanted_ssids.ssid[0]), "%s",
                "a-net2");
  renderFrame(in, fb);
  TEST_ASSERT_TRUE(writePgm(fb, "08-wifi-auth.pgm"));
  TEST_ASSERT_GREATER_THAN(100, countPaperPixels(fb));
}

void test_fullscreen_states_each_produce_distinct_frames() {
  // Boot / Offline / Auth / WifiAuth each use their own fullscreen renderer
  // (different glyph + text), so every pairwise diff must be non-zero. Normal
  // is the only board state; the others are all distinct fullscreen error /
  // placeholder screens.
  constexpr DisplayState fullscreen[] = {
      DisplayState::Boot,
      DisplayState::Offline,
      DisplayState::Auth,
      DisplayState::WifiAuth,
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

void test_no_data_board_differs_from_full_board() {
  // The board's content is driven by its data: a no-data board (all "--:--")
  // must render differently from the mixed board with real times.
  RenderInput full = makeBoardInput(DisplayState::Normal);
  RenderInput empty = makeNoDataInput();
  Frame ff;
  Frame fe;
  renderFrame(full, ff);
  renderFrame(empty, fe);
  bool different = false;
  for (size_t k = 0; k < Frame::bytes; ++k) {
    if (ff.data()[k] != fe.data()[k]) {
      different = true;
      break;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(different,
                           "Full and no-data boards rendered identical bytes");
}

void test_every_state_fits_persistence_cap() {
  // Regression guard for the full-refresh storm: if a rendered frame's
  // row-delta RLE exceeds RLE_HARDCAP_BYTES, Esp32PersistentStore rejects
  // the save, prev_valid goes false, and every warm cycle degrades to a
  // light-full (whole-panel flash). Any layout change that pushes a state
  // over the cap must fail here, not on the device.
  constexpr DisplayState all[] = {
      DisplayState::Boot,
      DisplayState::Normal,
      DisplayState::Offline,
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

// Leftmost ink column of the 58A-Hietzing first-time cell (row 1). The cell's
// right edge is COL_TIME1_DIGIT_RIGHT (284) and TG_Row "HH:MM" is ≈85 px wide,
// so [150, 290) × the row-1 Y band [54, 90) safely brackets time1 without the
// badge (left of ~140) or the gauge/time2 (right of 290). -1 if no ink.
int leftmostInkOfHietzingTime1(const Frame &fb) {
  constexpr int kX0 = 150, kX1 = 290, kY0 = 54, kY1 = 90;
  for (int x = kX0; x < kX1; ++x) {
    for (int y = kY0; y < kY1; ++y) {
      if (!fb.getPixel(x, y)) { // ink = false
        return x;
      }
    }
  }
  return -1;
}

// Regression for the field-observed "58A Hietzing first time shifted ~2 px
// left". The times are right-aligned; the old anchor used the string's INK
// width (u8g2 getUTF8Width), which shrinks when the trailing digit has less
// right-side ink (…:_1 vs …:_9). That floated the left edge by value. Anchoring
// to a fixed template width must keep the left edge identical for any minute.
void test_hietzing_time1_left_edge_is_value_stable() {
  // Two epochs whose local HH:MM end in different-ink digits. Base is
  // 1700000000 = 2023-11-14 22:53:20 CET; +N*60 walks the minute.
  // …:54 (trailing 4) vs …:59 (trailing 9) vs …:51 (trailing 1).
  const time_t base = 1700000000;
  int edges[3];
  const int minute_offsets[3] = {1, 6, -2}; // → :54, :59, :51
  for (int i = 0; i < 3; ++i) {
    RenderInput in = makeBoardInput(DisplayState::Normal);
    in.snapshot.stream[STREAM_58A_HIETZING].slot[0] = {
        base + minute_offsets[i] * 60, DepartureSource::Realtime, true};
    Frame fb;
    renderFrame(in, fb);
    edges[i] = leftmostInkOfHietzingTime1(fb);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, edges[i],
                                     "no ink found in the time1 cell");
  }
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      edges[0], edges[1],
      "time1 left edge moved between minutes ending :4 and :9");
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      edges[0], edges[2],
      "time1 left edge moved between minutes ending :4 and :1");
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_dump_state_boot);
  RUN_TEST(test_dump_board_mixed);
  RUN_TEST(test_dump_board_live_only);
  RUN_TEST(test_dump_board_schedule_only);
  RUN_TEST(test_dump_board_no_data);
  RUN_TEST(test_dump_state_offline);
  RUN_TEST(test_dump_state_offline_case_mismatch);
  RUN_TEST(test_offline_ssid_list_adds_ink);
  RUN_TEST(test_dump_state_auth);
  RUN_TEST(test_dump_state_wifi_auth);
  RUN_TEST(test_fullscreen_states_each_produce_distinct_frames);
  RUN_TEST(test_no_data_board_differs_from_full_board);
  RUN_TEST(test_every_state_fits_persistence_cap);
  RUN_TEST(test_update_stamp_draws_bottom_right_only);
  RUN_TEST(test_update_stamp_overwrite_is_clean);
  RUN_TEST(test_update_stamp_zero_is_noop);
  RUN_TEST(test_hietzing_time1_left_edge_is_value_stable);
  return UNITY_END();
}
