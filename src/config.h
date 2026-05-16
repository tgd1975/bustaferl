#ifndef BUSTAFERL_CONFIG_H
#define BUSTAFERL_CONFIG_H

// Wiener Linien RBLs.
//
// Confirmed via the iTip endpoint
//   https://www.wienerlinien.at/itip/bf/opendata.php?station=…&line=…&stopId=N
// where N is the RBL.
#define RBL_TULL_ATZGERSDORF 8131 // Tullnertalgasse, 58A → Atzgersdorf
#define RBL_TULL_HIETZING 3757    // Tullnertalgasse, 58A → Hietzing
#define RBL_ENDEMANN                                                           \
  8132 // Endemanngasse,    58B → Bhf. Atzgersdorf S (post-loop)
#define RBL_SUEDTIROLER_LEOPOLDAU 4105 // Südtiroler Platz, U1 → Leopoldau
#define RBL_SUEDTIROLER_OBERLAA 4124   // Südtiroler Platz, U1 → Oberlaa

// All towards-prefixes are case-sensitive prefix matches against the OGD
// API's `towards` field. The strings below are taken from a live response
// (2026-05); the parser's regression test locks them against the captured
// fixture in test/test_wienerlinien_parse/fixtures/wl_live.h.
//
// 58B at Endemanngasse runs a loop; the API distinguishes the two passes by
// the "(üb. Rosenhügelstr.)" suffix versus "(üb. Atzgersdorfer Str.)". The
// 58B filter combined with line=58B picks only the post-loop direction.
#define FILTER_TOWARDS_58B "Bhf. Atzgersdorf"

// Line names as the API reports them.
#define LINE_58A "58A"
#define LINE_58B "58B"
#define LINE_U1 "U1"

// Direction labels for the 58A streams. The 58A→Atzgersdorf branch reports
// "Bhf. Atzgersdorf S (üb. Atzgersdorfer Str.)"; the Hietzing branch reports
// "Hietzing U".
#define TOWARDS_58A_ATZ "Bhf. Atzgersdorf"
#define TOWARDS_58A_HIETZING "Hietzing"

// U1 endpoints; each RBL is one-directional, so the towards filter is mostly
// a belt-and-braces guard against a future schedule change.
#define TOWARDS_U1_LEOPOLDAU "Leopoldau"
#define TOWARDS_U1_OBERLAA "Oberlaa"

// e-Paper GPIO
#define EPD_CS 5
#define EPD_DC 17
#define EPD_RST 16
#define EPD_BUSY 4

// Behaviour thresholds (seconds unless noted).
#define STALE_THRESHOLD_S 180      // 3 min until stale state
#define WAKE_BEFORE_BUS_S 900      // 15 min safety lead
#define BOOT_MARGIN_S 30           // boot + wifi + api + render reserve
#define POLL_INTERVAL_S 30         // API poll cadence while awake
#define ACTIVE_THRESHOLD_S 120     // below this delta, stay awake
#define NO_DATA_SLEEP_S 1800       // API ok, no departures (overnight)
#define API_FAILURE_RETRY_S 60     // API/network failed → short retry instead
#define PARTIAL_HARDCAP 80         // safety cap before forced light full
#define LIGHT_FULL_INTERVAL_S 7200 // 2 h between scheduled light fulls
#define NTP_INTERVAL_S 86400       // 24 h between NTP syncs
#define COLD_BOOT_RETRY_S 60
#define COLD_BOOT_MAX_RETRIES 5

#define FILTER_HEALTH_DEAD_AFTER 3 // consecutive misses → dead

// Display geometry — used everywhere; do not change without re-laying out.
#define EPD_WIDTH 400
#define EPD_HEIGHT 300
#define FB_BYTES (EPD_WIDTH * EPD_HEIGHT / 8) // 15000

// RLE persistence: ~3kB budget for the compressed framebuffer in RTC slow
// memory. Above this we force a light full refresh next cycle.
#define RLE_BUDGET_BYTES 3072
#define RLE_HARDCAP_BYTES 7168

// Time zone for display formatting (Vienna with DST).
#define NTP_SERVER "at.pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// HTTP endpoint for the OGD realtime monitor. The caller appends `&rbl=…`.
#define WL_API_BASE                                                            \
  "https://www.wienerlinien.at/ogd_realtime/"                                  \
  "monitor?activateTrafficInfo=stoerunglang"

#endif
