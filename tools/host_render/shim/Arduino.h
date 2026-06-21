#ifndef SHIM_ARDUINO_H
#define SHIM_ARDUINO_H
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "Print.h"
#include "pgmspace.h"

typedef bool boolean;
typedef uint8_t byte;

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(PSTR(s)))

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// Minimal Arduino String — only what Adafruit_GFX::getTextBounds(String) needs.
class String {
public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  const char *c_str() const { return s_.c_str(); }
  unsigned length() const { return (unsigned)s_.size(); }
  char operator[](int i) const { return s_[(size_t)i]; }

private:
  std::string s_;
};
#endif
