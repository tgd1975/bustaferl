#ifndef BUSTAFERL_ICLOCK_H
#define BUSTAFERL_ICLOCK_H

#include <ctime>

namespace bustaferl {

class IClock {
public:
    virtual ~IClock() = default;
    virtual time_t now() = 0;
    // Returns true on a successful sync.
    virtual bool ntpSync() = 0;
    // Last successful NTP sync, or 0 if never.
    virtual time_t lastSync() const = 0;
};

}  // namespace bustaferl

#endif
