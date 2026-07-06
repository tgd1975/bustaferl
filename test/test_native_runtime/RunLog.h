#ifndef BUSTAFERL_NATIVE_RUNTIME_RUNLOG_H
#define BUSTAFERL_NATIVE_RUNTIME_RUNLOG_H

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>

namespace bustaferl::native_runtime {

// Append-only, wall-clock-timestamped run log for long soaks. Every line is
// flushed immediately so a hung process still leaves a complete trace up to
// the moment it stopped — the timestamp gap IS the freeze diagnosis. Lines
// are mirrored to stderr so an attached terminal sees them live.
class RunLog {
public:
  explicit RunLog(const std::string &path) : f_(std::fopen(path.c_str(), "a")) {
    line("=== run started ===");
  }
  ~RunLog() {
    line("=== run ended ===");
    if (f_ != nullptr)
      std::fclose(f_);
  }
  RunLog(const RunLog &) = delete;
  RunLog &operator=(const RunLog &) = delete;

  // printf-style, one line per call; the wall-clock prefix is added here.
  void line(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    char stamp[32];
    const std::time_t now = std::time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);

    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (f_ != nullptr) {
      std::fprintf(f_, "[%s] %s\n", stamp, msg);
      std::fflush(f_);
    }
    std::fprintf(stderr, "[%s] %s\n", stamp, msg);
  }

private:
  std::FILE *f_;
};

} // namespace bustaferl::native_runtime

#endif
