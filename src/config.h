#ifndef BUSTAFERL_CONFIG_H
#define BUSTAFERL_CONFIG_H

// Wiener Linien RBLs — fill these in before flashing.
#define RBL_TULL_ATZGERSDORF  0
#define RBL_TULL_HIETZING     0
#define RBL_ENDEMANN          0

// 58B at Endemanngasse runs a loop; we only want the post-loop direction.
// This must match the `towards` string the OGD API returns (case-sensitive,
// prefix match). Verify against a real API response before flashing.
#define FILTER_TOWARDS_58B    "Atzgersdorf"

// Line names as the API reports them.
#define LINE_58A              "58A"
#define LINE_58B              "58B"

// Direction labels for the 58A streams. Used only for filtering the response
// — verify exact strings from a real API call.
#define TOWARDS_58A_ATZ       "Atzgersdorf"
#define TOWARDS_58A_HIETZING  "Hietzing"

// e-Paper GPIO
#define EPD_CS    5
#define EPD_DC    17
#define EPD_RST   16
#define EPD_BUSY  4

// Behaviour thresholds (seconds unless noted).
#define STALE_THRESHOLD_S        180     // 3 min until stale state
#define WAKE_BEFORE_BUS_S        900     // 15 min safety lead
#define BOOT_MARGIN_S             30     // boot + wifi + api + render reserve
#define POLL_INTERVAL_S           30     // API poll cadence while awake
#define ACTIVE_THRESHOLD_S       120     // below this delta, stay awake
#define NO_DATA_SLEEP_S         1800     // sleep when API has no departures
#define PARTIAL_HARDCAP           80     // safety cap before forced light full
#define LIGHT_FULL_INTERVAL_S   7200     // 2 h between scheduled light fulls
#define NTP_INTERVAL_S         86400     // 24 h between NTP syncs
#define COLD_BOOT_RETRY_S         60
#define COLD_BOOT_MAX_RETRIES      5

#define FILTER_HEALTH_DEAD_AFTER   3     // consecutive misses → dead

// Display geometry — used everywhere; do not change without re-laying out.
#define EPD_WIDTH    400
#define EPD_HEIGHT   300
#define FB_BYTES    (EPD_WIDTH * EPD_HEIGHT / 8)  // 15000

// RLE persistence: ~3kB budget for the compressed framebuffer in RTC slow
// memory. Above this we force a light full refresh next cycle.
#define RLE_BUDGET_BYTES        3072
#define RLE_HARDCAP_BYTES       7168

// Time zone for display formatting (Vienna with DST).
#define NTP_SERVER   "at.pool.ntp.org"
#define TZ_INFO      "CET-1CEST,M3.5.0,M10.5.0/3"

// HTTP endpoint for the OGD realtime monitor. The caller appends `&rbl=…`.
#define WL_API_BASE  "https://www.wienerlinien.at/ogd_realtime/monitor?activateTrafficInfo=stoerunglang"

#endif
