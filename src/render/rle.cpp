#include "rle.h"

#include <limits>

namespace bustaferl {

// Maximum run length per (count, value) pair — count fits one byte.
constexpr size_t RLE_MAX_RUN = std::numeric_limits<uint8_t>::max();

size_t rleEncode(const uint8_t *in, size_t in_len, uint8_t *out,
                 size_t out_cap) {
  size_t op = 0;
  size_t ip = 0;
  while (ip < in_len) {
    uint8_t v = in[ip];
    size_t run = 1;
    while (ip + run < in_len && in[ip + run] == v && run < RLE_MAX_RUN)
      ++run;
    if (op + 2 > out_cap)
      return 0;
    out[op++] = static_cast<uint8_t>(run);
    out[op++] = v;
    ip += run;
  }
  return op;
}

size_t rleDecode(const uint8_t *in, size_t in_len, uint8_t *out,
                 size_t out_cap) {
  if (in_len % 2 != 0)
    return 0;
  size_t op = 0;
  for (size_t ip = 0; ip < in_len; ip += 2) {
    uint8_t run = in[ip];
    uint8_t v = in[ip + 1];
    if (run == 0)
      return 0;
    if (op + run > out_cap)
      return 0;
    for (uint8_t i = 0; i < run; ++i)
      out[op++] = v;
  }
  return op;
}

} // namespace bustaferl
