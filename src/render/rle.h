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
size_t rleEncode(const uint8_t *in, size_t in_len, uint8_t *out,
                 size_t out_cap);

// Decodes the buffer produced by rleEncode. Returns bytes written, or 0 on
// malformed input or insufficient `out_cap`.
size_t rleDecode(const uint8_t *in, size_t in_len, uint8_t *out,
                 size_t out_cap);

// Row-delta variants: XOR each row (`stride` bytes wide) with the previous
// row, then apply the same byte RLE. Rendered frames have strong row-to-row
// coherence (glyph bands repeat vertically), so the delta stream is mostly
// zeros and encodes ~30% tighter — the difference between a densely-inked
// departure board fitting the RTC slot (RLE_HARDCAP_BYTES) or not. `stride` is
// the framebuffer row width in bytes (EPD_WIDTH / 8) and must be identical
// between encode and decode. Returns like the plain variants; 0 on overflow,
// malformed input, or stride == 0.
size_t rleEncodeDelta(const uint8_t *in, size_t in_len, size_t stride,
                      uint8_t *out, size_t out_cap);

size_t rleDecodeDelta(const uint8_t *in, size_t in_len, size_t stride,
                      uint8_t *out, size_t out_cap);

} // namespace bustaferl

#endif
