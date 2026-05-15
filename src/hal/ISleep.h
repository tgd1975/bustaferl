#ifndef BUSTAFERL_ISLEEP_H
#define BUSTAFERL_ISLEEP_H

namespace bustaferl {

enum class WakeCause {
  ColdBoot,
  Timer,
  Other,
};

class ISleep {
public:
  virtual ~ISleep() = default;
  virtual WakeCause wakeupCause() = 0;
  // Enter deep sleep for `seconds`. Does not return.
  [[noreturn]] virtual void deepSleep(unsigned seconds) = 0;
  // Light sleep that does return; used between polls inside the active phase.
  virtual void lightSleep(unsigned seconds) = 0;
};

} // namespace bustaferl

#endif
