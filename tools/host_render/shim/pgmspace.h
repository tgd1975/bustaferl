#ifndef SHIM_PGMSPACE_H
#define SHIM_PGMSPACE_H
#include <stdint.h>
#include <string.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif
// pgm_read_pointer is intentionally left to Adafruit_GFX.cpp (it picks the
// definition by pointer width); defining it here too triggers a redefinition
// warning. It is only used for custom GFXfonts, which this layout doesn't use.
#ifndef memcpy_P
#define memcpy_P memcpy
#endif
#endif
