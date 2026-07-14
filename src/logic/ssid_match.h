#ifndef BUSTAFERL_LOGIC_SSID_MATCH_H
#define BUSTAFERL_LOGIC_SSID_MATCH_H

#include "hal/ScanResult.h" // ConfiguredSsids, ScanResult, SsidCaseMismatch

namespace bustaferl {

// ASCII case-insensitive equality of two NUL-terminated SSIDs. 802.11 SSIDs are
// byte strings; we only fold A-Z/a-z (the realistic typo class), leaving any
// non-ASCII bytes compared exactly.
bool ssidEqualsIgnoreCase(const char *a, const char *b);

// Scan `visible` for the first AP that equals any `wanted` SSID case-
// insensitively but NOT exactly (an exact match would have connected). Returns
// the first such pair, or {found=false} if none — meaning the failure is a
// genuinely absent/renamed AP, not a casing typo.
SsidCaseMismatch findCaseMismatch(const ConfiguredSsids &wanted,
                                  const ScanResult &visible);

} // namespace bustaferl

#endif
