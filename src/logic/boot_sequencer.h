#ifndef BUSTAFERL_BOOT_SEQUENCER_H
#define BUSTAFERL_BOOT_SEQUENCER_H

#include <cstdint>

namespace bustaferl {

class IClock;
class INetwork;

enum class BootResult : std::uint8_t {
  Ok,         // wifi + ntp succeeded
  RetryLater, // failure, schedule short retry-sleep, increment counter
  GiveUp,     // exhausted retries; render "Start fehlgeschlagen"
};

constexpr unsigned DEFAULT_WIFI_TIMEOUT_MS = 10000;
constexpr uint8_t DEFAULT_COLD_BOOT_MAX_RETRIES = 5;

struct BootConfig {
  unsigned wifi_timeout_ms = DEFAULT_WIFI_TIMEOUT_MS;
  uint8_t max_retries = DEFAULT_COLD_BOOT_MAX_RETRIES;
};

// Drives the cold-boot sequence steps 1+2 from CONCEPT.md §8 (WiFi → NTP).
// Steps 3+ (API, render) happen after this returns Ok.
BootResult runColdBoot(INetwork &net, IClock &clock, uint8_t retries_so_far,
                       const BootConfig &cfg);

} // namespace bustaferl

#endif
