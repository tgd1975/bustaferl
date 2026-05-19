#ifndef BUSTAFERL_TIME_CONSTANTS_H
#define BUSTAFERL_TIME_CONSTANTS_H

namespace bustaferl {

// Time-arithmetic constants used across the parsers, sleep planner, and
// schedule fetcher. Avoid sprinkling literal seconds/minutes through the code.
constexpr long SECONDS_PER_MINUTE = 60;
constexpr long SECONDS_PER_HOUR = 3600;
constexpr long SECONDS_PER_DAY = 86400;

// struct tm year offset: `tm_year` holds years since 1900.
constexpr int TM_YEAR_BASE = 1900;

} // namespace bustaferl

#endif
