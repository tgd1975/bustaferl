// Tier 1 — the 58A live-vs-Fahrplan deviation gauge (render/deviation_gauge).
// A recording Canvas captures the primitive draw ops so the gauge's shape
// logic is verified without the font/Adafruit stack. Assertions target stable
// invariants (bar direction, square-vs-bar, overflow tab) rather than exact
// pixels, so the geometry can still be tuned against a real render.

#include "render/canvas.h"
#include "render/deviation_gauge.h"

#include <unity.h>
#include <vector>

using namespace bustaferl;

namespace {

struct Rect {
  int x, y, w, h;
  bool filled;
};
struct Line {
  int x, y, len;
  bool vertical;
};

class RecordingCanvas : public render::Canvas {
public:
  std::vector<Rect> rects;
  std::vector<Line> lines;

  void drawPixel(int, int, std::uint16_t) override {}
  int width() const override { return 400; }
  int height() const override { return 300; }
  void fillRect(int x, int y, int w, int h, std::uint16_t) override {
    rects.push_back({x, y, w, h, true});
  }
  void drawRect(int x, int y, int w, int h, std::uint16_t) override {
    rects.push_back({x, y, w, h, false});
  }
  void drawFastHLine(int x, int y, int w, std::uint16_t) override {
    lines.push_back({x, y, w, false});
  }
  void drawFastVLine(int x, int y, int h, std::uint16_t) override {
    lines.push_back({x, y, h, true});
  }
  void setCursor(int, int) override {}
  void setTextColor(std::uint16_t) override {}
  void setRoleFont(FontRole) override {}
  void print(const char *) override {}
  int textWidth(const char *) override { return 0; }

  int fillCount() const {
    int n = 0;
    for (const auto &r : rects) {
      if (r.filled) {
        ++n;
      }
    }
    return n;
  }
  int drawRectCount() const {
    int n = 0;
    for (const auto &r : rects) {
      if (!r.filled) {
        ++n;
      }
    }
    return n;
  }
};

constexpr int TOP = 100;
constexpr int CX = 200;
constexpr int ZERO_Y = TOP + GAUGE_ZERO_DY;

} // namespace

void setUp() {}
void tearDown() {}

void test_track_and_baseline_always_drawn() {
  RecordingCanvas c;
  drawDeviationGauge(c, CX, TOP, /*has_dev=*/true, 0);
  // A full-height vertical track at cx.
  bool track = false;
  for (const auto &l : c.lines) {
    if (l.vertical && l.x == CX && l.len == GAUGE_H) {
      track = true;
    }
  }
  TEST_ASSERT_TRUE(track);
  // A zero baseline: the widest horizontal tick, centred on cx, at zeroY.
  int widest = 0;
  for (const auto &l : c.lines) {
    if (!l.vertical && l.y == ZERO_Y && l.len > widest) {
      widest = l.len;
    }
  }
  TEST_ASSERT_GREATER_OR_EQUAL(7, widest);
}

void test_no_live_draws_square_not_bar() {
  RecordingCanvas c;
  drawDeviationGauge(c, CX, TOP, /*has_dev=*/false, 0);
  TEST_ASSERT_EQUAL(1, c.drawRectCount()); // hollow square
  TEST_ASSERT_EQUAL(0, c.fillCount());     // no bar
}

void test_on_time_shows_nub_not_square() {
  RecordingCanvas c;
  drawDeviationGauge(c, CX, TOP, /*has_dev=*/true, 0);
  TEST_ASSERT_EQUAL(0, c.drawRectCount()); // no square
  TEST_ASSERT_GREATER_OR_EQUAL(1, c.fillCount());
  // The nub straddles the zero line and is at least 2 px tall.
  bool nub = false;
  for (const auto &r : c.rects) {
    if (r.filled && r.y <= ZERO_Y && r.y + r.h >= ZERO_Y && r.h >= 2) {
      nub = true;
    }
  }
  TEST_ASSERT_TRUE(nub);
}

void test_late_bar_extends_up() {
  RecordingCanvas c;
  drawDeviationGauge(c, CX, TOP, /*has_dev=*/true, 2);
  // A bar whose top is above the zero line (smaller y) — "late" grows up.
  bool up = false;
  for (const auto &r : c.rects) {
    if (r.filled && r.y < ZERO_Y && r.y + r.h >= ZERO_Y - 1) {
      up = true;
    }
  }
  TEST_ASSERT_TRUE(up);
}

void test_early_bar_extends_down() {
  RecordingCanvas c;
  drawDeviationGauge(c, CX, TOP, /*has_dev=*/true, -2);
  bool down = false;
  for (const auto &r : c.rects) {
    if (r.filled && r.y + r.h > ZERO_Y && r.y <= ZERO_Y + 1) {
      down = true;
    }
  }
  TEST_ASSERT_TRUE(down);
}

void test_overflow_up_adds_tab_above_track() {
  RecordingCanvas c;
  drawDeviationGauge(c, CX, TOP, /*has_dev=*/true, 99);
  bool tab = false;
  for (const auto &r : c.rects) {
    if (r.filled && r.y < TOP) { // above the track top
      tab = true;
    }
  }
  TEST_ASSERT_TRUE(tab);
}

void test_overflow_down_adds_tab_below_track() {
  RecordingCanvas c;
  drawDeviationGauge(c, CX, TOP, /*has_dev=*/true, -99);
  bool tab = false;
  for (const auto &r : c.rects) {
    if (r.filled && r.y >= TOP + GAUGE_H) { // below the track bottom
      tab = true;
    }
  }
  TEST_ASSERT_TRUE(tab);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_track_and_baseline_always_drawn);
  RUN_TEST(test_no_live_draws_square_not_bar);
  RUN_TEST(test_on_time_shows_nub_not_square);
  RUN_TEST(test_late_bar_extends_up);
  RUN_TEST(test_early_bar_extends_down);
  RUN_TEST(test_overflow_up_adds_tab_above_track);
  RUN_TEST(test_overflow_down_adds_tab_below_track);
  return UNITY_END();
}
