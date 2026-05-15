#ifndef BUSTAFERL_BOOT_SEQUENCER_H
#define BUSTAFERL_BOOT_SEQUENCER_H

#include <cstdint>

namespace bustaferl {

class IClock;
class INetwork;

enum class BootResult {
    Ok,            // wifi + ntp succeeded
    RetryLater,    // failure, schedule short retry-sleep, increment counter
    GiveUp,        // exhausted retries; render "Start fehlgeschlagen"
};

struct BootConfig {
    unsigned wifi_timeout_ms = 10000;
    uint8_t  max_retries     = 5;
};

// Drives the cold-boot sequence steps 1+2 from CONCEPT.md §8 (WiFi → NTP).
// Steps 3+ (API, render) happen after this returns Ok.
BootResult runColdBoot(INetwork& net, IClock& clock, uint8_t retries_so_far,
                      const BootConfig& cfg);

}  // namespace bustaferl

#endif
