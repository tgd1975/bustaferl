#include "boot_sequencer.h"

#include "../hal/IClock.h"
#include "../hal/INetwork.h"

namespace bustaferl {

BootResult runColdBoot(INetwork& net, IClock& clock, uint8_t retries_so_far,
                      const BootConfig& cfg) {
    if (!net.connect(cfg.wifi_timeout_ms)) {
        return retries_so_far + 1 >= cfg.max_retries ? BootResult::GiveUp
                                                     : BootResult::RetryLater;
    }
    if (!clock.ntpSync()) {
        return retries_so_far + 1 >= cfg.max_retries ? BootResult::GiveUp
                                                     : BootResult::RetryLater;
    }
    return BootResult::Ok;
}

}  // namespace bustaferl
