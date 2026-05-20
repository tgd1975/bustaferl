// Empty stub for the native host build. Adafruit_GFX.h includes this
// unconditionally, but only as a header — no I2CDevice symbol is referenced
// from Adafruit_GFX.cpp itself (only the OLED/TFT .cpps, which we don't
// compile). An empty header is enough.

#ifndef BUSTAFERL_HOST_COMPAT_ADAFRUIT_I2CDEVICE_H
#define BUSTAFERL_HOST_COMPAT_ADAFRUIT_I2CDEVICE_H
#endif
