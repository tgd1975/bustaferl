#include "RecordingRenderer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace bustaferl::native_runtime {

namespace {

// Cheap content hash over the slot timestamps + overlay kind. Stable across
// runs, so identical inputs produce identical frames (precondition for the
// dedup test).
std::uint32_t hashInput(const RenderInput &in) {
  std::uint32_t h = 2166136261u;
  auto mix = [&](std::uint32_t x) {
    h ^= x;
    h *= 16777619u;
  };
  for (int s = 0; s < STREAM_COUNT; ++s) {
    const auto &sd = in.snapshot.stream[s];
    for (int k = 0; k < SLOTS_PER_STREAM; ++k) {
      mix(static_cast<std::uint32_t>(sd.slot[k].when & 0xFFFFFFFFu));
      mix(static_cast<std::uint32_t>(
          static_cast<std::uint8_t>(sd.slot[k].source)));
      mix(sd.slot[k].valid ? 1u : 0u);
    }
    mix(sd.endpoint_responded ? 1u : 0u);
    mix(sd.filter_matched ? 1u : 0u);
  }
  mix(static_cast<std::uint32_t>(in.state));
  mix(in.snapshot.api_ok ? 1u : 0u);
  return h;
}

std::string zeropad(unsigned n, unsigned width) {
  std::ostringstream os;
  os << std::setw(static_cast<int>(width)) << std::setfill('0') << n;
  return os.str();
}

} // namespace

RecordingRenderer::RecordingRenderer(std::string dump_dir)
    : dump_dir_(std::move(dump_dir)) {}

void RecordingRenderer::renderDeterministic(const RenderInput &in, Frame &fb) {
  // Pseudo-raster: tile the framebuffer into 20×20 px cells; cell (cx, cy) is
  // black iff the corresponding bit of the input hash is set. Trivially
  // deterministic, gives visually-distinguishable PGMs in the .tmp dump.
  //
  // Deliberately NOT the real renderFrame: linking layout.cpp into the
  // hand-rolled native-runtime g++ recipe would drag in the whole
  // ArduinoFake + Adafruit_GFX + U8g2 dependency graph that only PlatformIO
  // manages. The soak's forensic channel for "what would the panel show" is
  // the LoggingRenderer slot trace in run.log, not these PGMs.
  fb.clear(true);
  constexpr int CELL = 20;
  constexpr int COLS = Frame::width / CELL;
  constexpr int ROWS = Frame::height / CELL;
  const std::uint32_t h = hashInput(in);
  // Seed a xorshift32 from the hash so we get 600 bits, not just 32.
  std::uint32_t state = h == 0 ? 1u : h;
  for (int cy = 0; cy < ROWS; ++cy) {
    for (int cx = 0; cx < COLS; ++cx) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      const bool black = (state & 1u) == 0u;
      if (black) {
        fb.fillRect(cx * CELL, cy * CELL, CELL, CELL, false);
      }
    }
  }
}

bool RecordingRenderer::framesEqual(const Frame &a, const Frame &b) const {
  return std::memcmp(a.data(), b.data(), Frame::bytes) == 0;
}

void RecordingRenderer::writePgm(const Frame &fb,
                                 const std::string &path) const {
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (f == nullptr)
    return;
  std::fprintf(f, "P5\n%d %d\n255\n", Frame::width, Frame::height);
  // Convert 1bpp → 8bpp grayscale row-by-row.
  for (int y = 0; y < Frame::height; ++y) {
    for (int x = 0; x < Frame::width; ++x) {
      const std::uint8_t v = fb.getPixel(x, y) ? 0xFF : 0x00;
      std::fputc(v, f);
    }
  }
  std::fclose(f);
}

void RecordingRenderer::render(const RenderInput &in, Frame &fb) {
  renderDeterministic(in, fb);
  ++render_count_;
  if (have_prev_ && framesEqual(prev_, fb))
    return;
  const std::string path =
      dump_dir_ + "/frame-" + zeropad(dump_count_, 6) + ".pgm";
  writePgm(fb, path);
  ++dump_count_;
  std::memcpy(prev_.data(), fb.data(), Frame::bytes);
  have_prev_ = true;
}

} // namespace bustaferl::native_runtime
