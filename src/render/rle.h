#ifndef BUSTAFERL_RLE_H
#define BUSTAFERL_RLE_H

#include <cstddef>
#include <cstdint>

namespace bustaferl {

// Byte-level run-length encoding. Output format: pairs of (count, value).
// `count` is 1..255. Runs longer than 255 are split.
//
// Returns bytes written into `out`, or 0 on overflow (output would exceed
// `out_cap`).
size_t rleEncode(const uint8_t* in, size_t in_len, uint8_t* out,
                 size_t out_cap);

// Decodes the buffer produced by rleEncode. Returns bytes written, or 0 on
// malformed input or insufficient `out_cap`.
size_t rleDecode(const uint8_t* in, size_t in_len, uint8_t* out,
                 size_t out_cap);

}  // namespace bustaferl

#endif
