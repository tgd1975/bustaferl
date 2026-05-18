#ifndef BUSTAFERL_STRING_UTIL_H
#define BUSTAFERL_STRING_UTIL_H

#include <cstring>
#include <string>

namespace bustaferl {

// True iff `s` begins with `prefix`. An empty prefix matches everything; a
// null `s` never matches a non-empty prefix.
inline bool startsWith(const char *s, const std::string &prefix) {
  if (prefix.empty())
    return true;
  if (!s)
    return false;
  return std::strncmp(s, prefix.c_str(), prefix.size()) == 0;
}

} // namespace bustaferl

#endif
