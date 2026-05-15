#ifndef BUSTAFERL_FRAME_BUFFER_H
#define BUSTAFERL_FRAME_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace bustaferl {

// 1 bit per pixel, MSB = leftmost, row-major. Black = 0, white = 1.
template <int W, int H>
class FrameBuffer {
public:
    static constexpr int    width  = W;
    static constexpr int    height = H;
    static constexpr size_t bytes  = (W * H) / 8;

    FrameBuffer() { clear(); }

    void clear(bool white = true) {
        std::memset(buf_, white ? 0xFF : 0x00, bytes);
    }

    uint8_t*       data()       { return buf_; }
    const uint8_t* data() const { return buf_; }

    void setPixel(int x, int y, bool white) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        const size_t idx = y * (W / 8) + (x / 8);
        const uint8_t mask = 0x80 >> (x & 7);
        if (white) buf_[idx] |= mask;
        else       buf_[idx] &= ~mask;
    }

    bool getPixel(int x, int y) const {
        if (x < 0 || x >= W || y < 0 || y >= H) return true;
        const size_t idx = y * (W / 8) + (x / 8);
        const uint8_t mask = 0x80 >> (x & 7);
        return (buf_[idx] & mask) != 0;
    }

    void fillRect(int x, int y, int w, int h, bool white) {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx) setPixel(xx, yy, white);
    }

private:
    uint8_t buf_[bytes];
};

}  // namespace bustaferl

#endif
