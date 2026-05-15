#ifndef BUSTAFERL_ESP32DISPLAY_H
#define BUSTAFERL_ESP32DISPLAY_H

#ifndef NATIVE_BUILD

#include "IDisplay.h"

namespace bustaferl {

// Wraps GxEPD2_BW<GxEPD2_420, …> (UC8176, 400×300, 4.2"). The header keeps
// the GxEPD2 includes in the .cpp to limit compile pressure.
class Esp32Display : public IDisplay {
public:
  Esp32Display();
  ~Esp32Display() override;

  Esp32Display(const Esp32Display &) = delete;
  Esp32Display &operator=(const Esp32Display &) = delete;

  void init();

  void drawFull(const uint8_t *fb) override;
  void drawPartial(const uint8_t *fb, const Bbox &bbox) override;
  void lightFull(const uint8_t *fb) override;
  void deepClean(const uint8_t *fb) override;

private:
  struct Impl;
  Impl *impl_;
};

} // namespace bustaferl

#endif
#endif
