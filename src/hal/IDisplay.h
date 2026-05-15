#ifndef BUSTAFERL_IDISPLAY_H
#define BUSTAFERL_IDISPLAY_H

#include <cstdint>

namespace bustaferl {

struct Bbox {
    int x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
};

class IDisplay {
public:
    virtual ~IDisplay() = default;
    // Push a 400x300 / 8 = 15000-byte framebuffer.
    virtual void drawFull(const uint8_t* fb) = 0;
    // Partial refresh only within bbox. Caller guarantees bbox.x is 8-aligned.
    virtual void drawPartial(const uint8_t* fb, const Bbox& bbox) = 0;
    // 1× black/white flash, then redraw fb. Use every ~2h to fight ghosting.
    virtual void lightFull(const uint8_t* fb) = 0;
    // 3× black/white flash, then redraw fb. Once per night.
    virtual void deepClean(const uint8_t* fb) = 0;
};

}  // namespace bustaferl

#endif
