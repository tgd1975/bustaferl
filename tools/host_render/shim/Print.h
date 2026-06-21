#ifndef SHIM_PRINT_H
#define SHIM_PRINT_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) {
      if (write(*buffer++))
        n++;
      else
        break;
    }
    return n;
  }
  size_t write(const char *s) {
    return s ? write((const uint8_t *)s, strlen(s)) : 0;
  }
  size_t write(const char *buffer, size_t size) {
    return write((const uint8_t *)buffer, size);
  }
  size_t print(const char *s) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
};
#endif
