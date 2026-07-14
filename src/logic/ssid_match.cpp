#include "logic/ssid_match.h"

#include <cstdio>
#include <cstring>

namespace bustaferl {

namespace {

char toLowerAscii(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

} // namespace

bool ssidEqualsIgnoreCase(const char *a, const char *b) {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  while (*a != '\0' && *b != '\0') {
    if (toLowerAscii(*a) != toLowerAscii(*b)) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == *b; // both hit NUL at the same length
}

SsidCaseMismatch findCaseMismatch(const ConfiguredSsids &wanted,
                                  const ScanResult &visible) {
  SsidCaseMismatch out;
  for (int w = 0; w < wanted.count; ++w) {
    const char *want = wanted.ssid[w];
    for (int v = 0; v < visible.count; ++v) {
      const char *seen = visible.aps[v].ssid;
      // Exact match would have connected — skip it; only the case-only
      // difference is interesting here.
      if (std::strcmp(want, seen) == 0) {
        continue;
      }
      if (ssidEqualsIgnoreCase(want, seen)) {
        out.found = true;
        std::snprintf(out.configured, sizeof(out.configured), "%s", want);
        std::snprintf(out.visible, sizeof(out.visible), "%s", seen);
        return out;
      }
    }
  }
  return out;
}

} // namespace bustaferl
