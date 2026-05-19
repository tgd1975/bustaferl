// Dedup + determinism tests for native_runtime::RecordingRenderer.
// Runs in env:native.

#include "../test_native_runtime/RecordingRenderer.cpp" // NOLINT(bugprone-suspicious-include)

#include <cstdio>
#include <cstring>
#include <string>
#include <unity.h>

using namespace bustaferl;
using namespace bustaferl::native_runtime;

namespace {

constexpr const char *kDir = ".tmp/native-runtime/test-renderer";

void cleanDir() {
  // Best-effort: rm a handful of canonical filenames; the renderer never
  // writes more than ~10 frames in any of these tests.
  for (unsigned i = 0; i < 16; ++i) {
    char p[256];
    std::snprintf(p, sizeof(p), "%s/frame-%06u.pgm", kDir, i);
    std::remove(p);
  }
}

void ensureDir() {
  std::string cmd = "mkdir -p ";
  cmd += kDir;
  std::system(cmd.c_str());
}

RenderInput sampleInput(int seed) {
  RenderInput in{};
  in.snapshot.api_ok = true;
  in.snapshot.stream[0].endpoint_responded = true;
  in.snapshot.stream[0].filter_matched = true;
  in.snapshot.stream[0].slot[0].when = 1700000000 + seed * 60;
  in.snapshot.stream[0].slot[0].source = DepartureSource::Realtime;
  in.snapshot.stream[0].slot[0].valid = true;
  return in;
}

} // namespace

void setUp() {
  ensureDir();
  cleanDir();
}
void tearDown() { cleanDir(); }

void test_first_render_writes_one_pgm() {
  RecordingRenderer r{kDir};
  Frame fb;
  r.render(sampleInput(1), fb);
  TEST_ASSERT_EQUAL_UINT(1, r.render_count());
  TEST_ASSERT_EQUAL_UINT(1, r.dump_count());
}

void test_identical_input_produces_one_pgm() {
  RecordingRenderer r{kDir};
  Frame fb;
  RenderInput in = sampleInput(7);
  r.render(in, fb);
  r.render(in, fb);
  r.render(in, fb);
  TEST_ASSERT_EQUAL_UINT(3, r.render_count());
  TEST_ASSERT_EQUAL_UINT(1, r.dump_count());
}

void test_different_input_produces_more_pgms() {
  RecordingRenderer r{kDir};
  Frame fb;
  r.render(sampleInput(1), fb);
  r.render(sampleInput(2), fb);
  r.render(sampleInput(3), fb);
  TEST_ASSERT_EQUAL_UINT(3, r.render_count());
  TEST_ASSERT_EQUAL_UINT(3, r.dump_count());
}

void test_determinism_same_input_yields_same_bytes() {
  RecordingRenderer r1{kDir};
  RecordingRenderer r2{kDir};
  Frame fb1;
  Frame fb2;
  RenderInput in = sampleInput(42);
  r1.render(in, fb1);
  r2.render(in, fb2);
  TEST_ASSERT_EQUAL_INT(0, std::memcmp(fb1.data(), fb2.data(), Frame::bytes));
}

void test_pgm_file_actually_exists() {
  RecordingRenderer r{kDir};
  Frame fb;
  r.render(sampleInput(99), fb);
  std::string p = std::string(kDir) + "/frame-000000.pgm";
  std::FILE *f = std::fopen(p.c_str(), "rb");
  TEST_ASSERT_NOT_NULL(f);
  // Read header magic to confirm it's a P5 PGM.
  char hdr[3] = {0, 0, 0};
  std::fread(hdr, 1, 2, f);
  std::fclose(f);
  TEST_ASSERT_EQUAL_CHAR('P', hdr[0]);
  TEST_ASSERT_EQUAL_CHAR('5', hdr[1]);
}

int main(int /*argc*/, char ** /*argv*/) {
  UNITY_BEGIN();
  RUN_TEST(test_first_render_writes_one_pgm);
  RUN_TEST(test_identical_input_produces_one_pgm);
  RUN_TEST(test_different_input_produces_more_pgms);
  RUN_TEST(test_determinism_same_input_yields_same_bytes);
  RUN_TEST(test_pgm_file_actually_exists);
  return UNITY_END();
}
