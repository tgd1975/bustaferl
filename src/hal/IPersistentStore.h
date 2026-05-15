#ifndef BUSTAFERL_IPERSISTENTSTORE_H
#define BUSTAFERL_IPERSISTENTSTORE_H

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace bustaferl {

// Metadata that needs to survive deep sleep alongside the framebuffer.
struct PersistedMeta {
    time_t   last_ntp_sync       = 0;
    time_t   last_api_success    = 0;
    time_t   last_light_full     = 0;
    time_t   last_deep_clean     = 0;
    uint16_t partial_count       = 0;
    uint8_t  filter_miss_streak  = 0;
    uint8_t  cold_boot_retries   = 0;
    bool     framebuffer_valid   = false;
};

class IPersistentStore {
public:
    virtual ~IPersistentStore() = default;
    virtual PersistedMeta loadMeta() = 0;
    virtual void          saveMeta(const PersistedMeta& m) = 0;
    // Returns number of bytes written into `out` (decompressed framebuffer).
    // 0 → nothing stored / invalid.
    virtual size_t loadFramebuffer(uint8_t* out, size_t cap) = 0;
    // Compresses and stores. Returns true on success.
    virtual bool   saveFramebuffer(const uint8_t* fb, size_t len) = 0;
};

}  // namespace bustaferl

#endif
