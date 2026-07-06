#include "render/rle.h"

#include <cstring>
#include <unity.h>
#include <vector>

using namespace bustaferl;

void test_roundtrip_random_pattern() {
  std::vector<uint8_t> in(2000);
  for (size_t i = 0; i < in.size(); ++i)
    in[i] = (i * 37) & 0xFF;
  std::vector<uint8_t> enc(in.size() * 2 + 16);
  size_t n = rleEncode(in.data(), in.size(), enc.data(), enc.size());
  TEST_ASSERT_TRUE(n > 0);

  std::vector<uint8_t> dec(in.size());
  size_t m = rleDecode(enc.data(), n, dec.data(), dec.size());
  TEST_ASSERT_EQUAL_UINT(in.size(), m);
  TEST_ASSERT_EQUAL_INT(0, std::memcmp(in.data(), dec.data(), in.size()));
}

void test_all_white_compresses_small() {
  std::vector<uint8_t> in(15000, 0xFF);
  std::vector<uint8_t> enc(2048);
  size_t n = rleEncode(in.data(), in.size(), enc.data(), enc.size());
  TEST_ASSERT_TRUE(n > 0);
  // 15000 / 255 = 59 runs → 118 bytes
  TEST_ASSERT_TRUE(n <= 128);

  std::vector<uint8_t> dec(in.size());
  size_t m = rleDecode(enc.data(), n, dec.data(), dec.size());
  TEST_ASSERT_EQUAL_UINT(in.size(), m);
  for (size_t i = 0; i < in.size(); ++i)
    TEST_ASSERT_EQUAL_HEX8(0xFF, dec[i]);
}

void test_overflow_returns_zero() {
  std::vector<uint8_t> in(1000);
  for (size_t i = 0; i < in.size(); ++i)
    in[i] = i & 1 ? 0xAA : 0x55;
  // alternating bytes → no compression; encoded size = 2 * 1000 = 2000
  std::vector<uint8_t> enc(100);
  size_t n = rleEncode(in.data(), in.size(), enc.data(), enc.size());
  TEST_ASSERT_EQUAL_UINT(0, n);
}

void test_runs_longer_than_255_are_split() {
  std::vector<uint8_t> in(300, 0x42);
  std::vector<uint8_t> enc(16);
  size_t n = rleEncode(in.data(), in.size(), enc.data(), enc.size());
  TEST_ASSERT_EQUAL_UINT(4, n); // (255,0x42)(45,0x42)
  TEST_ASSERT_EQUAL_HEX8(255, enc[0]);
  TEST_ASSERT_EQUAL_HEX8(0x42, enc[1]);
  TEST_ASSERT_EQUAL_HEX8(45, enc[2]);
  TEST_ASSERT_EQUAL_HEX8(0x42, enc[3]);

  std::vector<uint8_t> dec(300);
  size_t m = rleDecode(enc.data(), n, dec.data(), dec.size());
  TEST_ASSERT_EQUAL_UINT(300, m);
}

void test_decode_rejects_odd_length() {
  uint8_t bad[3] = {1, 2, 3};
  uint8_t out[10] = {0};
  TEST_ASSERT_EQUAL_UINT(0, rleDecode(bad, 3, out, sizeof(out)));
}

void test_decode_rejects_zero_run() {
  uint8_t bad[2] = {0, 42};
  uint8_t out[10] = {0};
  TEST_ASSERT_EQUAL_UINT(0, rleDecode(bad, 2, out, sizeof(out)));
}

void test_delta_roundtrip_random_pattern() {
  constexpr size_t kStride = 50;
  std::vector<uint8_t> in(2000);
  for (size_t i = 0; i < in.size(); ++i)
    in[i] = (i * 37) & 0xFF;
  std::vector<uint8_t> enc(in.size() * 2 + 16);
  size_t n =
      rleEncodeDelta(in.data(), in.size(), kStride, enc.data(), enc.size());
  TEST_ASSERT_TRUE(n > 0);

  std::vector<uint8_t> dec(in.size());
  size_t m = rleDecodeDelta(enc.data(), n, kStride, dec.data(), dec.size());
  TEST_ASSERT_EQUAL_UINT(in.size(), m);
  TEST_ASSERT_EQUAL_INT(0, std::memcmp(in.data(), dec.data(), in.size()));
}

void test_delta_roundtrip_length_not_multiple_of_stride() {
  // 2000 % 50 == 0, so also cover a ragged tail row.
  constexpr size_t kStride = 50;
  std::vector<uint8_t> in(1975);
  for (size_t i = 0; i < in.size(); ++i)
    in[i] = (i * 131) & 0xFF;
  std::vector<uint8_t> enc(in.size() * 2 + 16);
  size_t n =
      rleEncodeDelta(in.data(), in.size(), kStride, enc.data(), enc.size());
  TEST_ASSERT_TRUE(n > 0);

  std::vector<uint8_t> dec(in.size());
  size_t m = rleDecodeDelta(enc.data(), n, kStride, dec.data(), dec.size());
  TEST_ASSERT_EQUAL_UINT(in.size(), m);
  TEST_ASSERT_EQUAL_INT(0, std::memcmp(in.data(), dec.data(), in.size()));
}

void test_delta_beats_plain_on_row_repetitive_buffer() {
  // Rows that repeat with mixed content: plain RLE sees short runs inside
  // every row, delta sees all-zero rows after the first — the property the
  // framebuffer persistence relies on to fit RLE_HARDCAP_BYTES.
  constexpr size_t kStride = 50;
  std::vector<uint8_t> in(kStride * 100);
  for (size_t y = 0; y < 100; ++y)
    for (size_t x = 0; x < kStride; ++x)
      in[y * kStride + x] = (x * 73) & 0xFF;
  std::vector<uint8_t> enc(in.size() * 2 + 16);
  size_t plain = rleEncode(in.data(), in.size(), enc.data(), enc.size());
  size_t delta =
      rleEncodeDelta(in.data(), in.size(), kStride, enc.data(), enc.size());
  TEST_ASSERT_TRUE(plain > 0);
  TEST_ASSERT_TRUE(delta > 0);
  TEST_ASSERT_TRUE(delta < plain);
}

void test_delta_rejects_zero_stride() {
  uint8_t in[10] = {0};
  uint8_t out[64] = {0};
  TEST_ASSERT_EQUAL_UINT(0, rleEncodeDelta(in, sizeof(in), 0, out, 64));
  TEST_ASSERT_EQUAL_UINT(0, rleDecodeDelta(out, 2, 0, in, sizeof(in)));
}

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip_random_pattern);
  RUN_TEST(test_all_white_compresses_small);
  RUN_TEST(test_overflow_returns_zero);
  RUN_TEST(test_runs_longer_than_255_are_split);
  RUN_TEST(test_decode_rejects_odd_length);
  RUN_TEST(test_decode_rejects_zero_run);
  RUN_TEST(test_delta_roundtrip_random_pattern);
  RUN_TEST(test_delta_roundtrip_length_not_multiple_of_stride);
  RUN_TEST(test_delta_beats_plain_on_row_repetitive_buffer);
  RUN_TEST(test_delta_rejects_zero_stride);
  return UNITY_END();
}
