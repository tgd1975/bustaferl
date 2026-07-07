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

// Direction labels for the 58A streams. The 58A→Atzgersdorf branch reports
// "Bhf. Atzgersdorf S (üb. Atzgersdorfer Str.)"; the Hietzing branch reports
// "Hietzing U".
#define TOWARDS_58A_ATZ "Bhf. Atzgersdorf"
#define TOWARDS_58A_HIETZING "Hietzing"

// DIVA stop IDs for the EFA schedule API (XSLT_DM_REQUEST). One per
// physical Haltestelle (not per RBL — DIVA aggregates the directions).
// Looked up via XSLT_STOPFINDER_REQUEST?name_sf=…; see CONCEPT.md §12.
#define DIVA_TULLNERTALGASSE 60201395
#define DIVA_ENDEMANNGASSE 60200278

// Direction strings as the EFA API reports them in
// `departureList[].servingLine.direction`. These DIFFER from the OGD
// `towards` strings — verify against a live response on first flash.
#define EFA_TOWARDS_58A_ATZ "Wien Atzgersdorf"
#define EFA_TOWARDS_58A_HIETZING "Wien Hietzing"
#define EFA_TOWARDS_58B_ATZ "Wien Atzgersdorf" // TODO verify post-loop variant

// ÖBB Wien Atzgersdorf → Wien Hauptbahnhof (v2 S-Bahn stream).
// Data layer: HAFAS-internal location IDs per locality (NOT the DB EVAs;
// 81xxxxxx resolves to the station name but HAFAS attaches no jnyL legs
// to those — see docs/v2-sbahn-migration-plan.md §4.1 Pre-Phase).
#define OEBB_EXTID_ATZG "1292301"    // Wien Atzgersdorf Bahnhst
#define OEBB_EXTID_WIENHBF "1290401" // Wien Hbf (U)

// Role layer: which extId fills which request slot.
#define OEBB_STBLOC_EXTID OEBB_EXTID_ATZG
#define OEBB_DIRLOC_EXTID OEBB_EXTID_WIENHBF

#define OEBB_MGATE_URL "https://fahrplan.oebb.at/bin/mgate.exe"
// HAFAS auth tuple — empirically confirmed via PoC 2026-05-19 (four calls
// over 30 min, HTTP 200 + err=OK + 6 jnyL entries each). Re-verify when the
// auth_error_seen tripwire fires.
#define OEBB_HAFAS_AID "OWDL4fE4ixNiPBBm"
#define OEBB_HAFAS_CLIENT_JSON                                                 \
  "{\"id\":\"OEBB\",\"type\":\"WEB\",\"name\":\"webapp\",\"l\":\"vs_webapp\"}"
#define OEBB_HAFAS_VER "1.67"
// jnyFltrL bitmask: "63" (bits 0-5) keeps S-Bahn + Regio + REX (cls=16,32),
// drops buses (cls=64). Confirmed PoC 2026-05-19.
#define OEBB_JNYFLTR_PRODUCTS "63"
#define OEBB_MAX_JNY 6

// Display state-selector thresholds (logic/render_input, comes in Schritt 7).
// Seeded here in Schritt 2.5 so config.h is single-source-of-truth for the
// whole v2 surface; consumers land in Session D.
#define OFFLINE_THRESHOLD_S 300  //  5 min ohne erfolgreichen Fetch + WiFi unten
#define STALE_THRESHOLD_V2_S 600 // 10 min ohne erfolgreichen Fetch
#define QUIET_HORIZON_S 1200     // 20 min — keine Abfahrten → Quiet
#define NIGHT_FIRST_DEP_MIN_AHEAD_S                                            \
  1800 // 30 min — nächste Plan-Abfahrt weiter → Night

// Service window: Spätbetrieb 58A/58B/S-Bahn endet ~0:30, Frühbetrieb ab ~5:00.
// END_HOUR < START_HOUR ⇒ Fenster wrappt um Mitternacht. outsideServiceWindow()
// gibt true zurück für [SERVICE_WINDOW_END_HOUR, SERVICE_WINDOW_START_HOUR),
// also 01:00–04:59.
#define SERVICE_WINDOW_START_HOUR 5
#define SERVICE_WINDOW_END_HOUR 1

// Display version string (foot line on the Boot screen).
#define DISPLAY_VERSION_STR "v2.0 · UC8176 · 400×300"

// Debug aid: small "upd HH:MM" stamp bottom-right showing when the panel
// content last actually changed. 0 = off. The stamp only updates together
// with a real refresh — an unchanged frame stays untouched (no extra panel
// updates just to tick the stamp). Overridable so the native-runtime soak
// binary (whose renderer is a pseudo-raster without the render/ layer) can
// compile it out via -DUPDATE_STAMP_ENABLED=0.
#ifndef UPDATE_STAMP_ENABLED
#define UPDATE_STAMP_ENABLED 1
#endif

// EFA schedule endpoint. Caller appends &name_dm=<DIVA>&itdDate*=...
#define WL_EFA_DM_BASE                                                         \
  "https://www.wienerlinien.at/ogd_routing/"                                   \
  "XSLT_DM_REQUEST?outputFormat=JSON&language=de&stateless=1"                  \
  "&mode=direct&type_dm=stop&useRealtime=0"

// e-Paper GPIO
#define EPD_CS 5
#define EPD_DC 17
#define EPD_RST 16
#define EPD_BUSY 4

// Onboard "BOOT" button — GPIO 0, active-low with internal pullup. Short
// press triggers an update cycle (wakes from deep sleep if applicable);
// hold past BTN_LONG_PRESS_MS triggers a B/W panel reset + redraw.
#define BTN_BOOT_PIN 0
#define BTN_LONG_PRESS_MS 2000

// Behaviour thresholds (seconds unless noted).
#define STALE_THRESHOLD_S 180  // 3 min until stale state
#define WAKE_BEFORE_BUS_S 900  // 15 min safety lead
#define BOOT_MARGIN_S 30       // boot + wifi + api + render reserve
#define POLL_INTERVAL_S 30     // API poll cadence while awake
#define ACTIVE_THRESHOLD_S 120 // below this delta, stay awake
#define NO_DATA_SLEEP_S 1800   // API ok, no departures (overnight)
#define API_FAILURE_RETRY_S 60 // API/network failed → short retry instead
#define PARTIAL_HARDCAP 15     // partials before a ghost-clearing light full
#define LIGHT_FULL_INTERVAL_S 3600 // 1 h between scheduled light fulls
#define NTP_INTERVAL_S 86400       // 24 h between NTP syncs
#define COLD_BOOT_RETRY_S 60
#define COLD_BOOT_MAX_RETRIES 5

#define FILTER_HEALTH_DEAD_AFTER 3 // consecutive misses → dead

// Rescue fetch: when a cycle rendered with an incomplete snapshot (some API
// batch failed), keep re-fetching after the display update and push one extra
// refresh as soon as the data is complete — but only inside this window after
// the update, so panel refreshes never land back-to-back.
#define RESCUE_WINDOW_START_S 20
#define RESCUE_WINDOW_END_S 40
#define RESCUE_RETRY_PAUSE_S 5
#define RESCUE_MAX_ATTEMPTS 3

// Nightly deep-clean trigger thresholds. When the next planned sleep is at
// least LONG_SLEEP_FOR_NIGHTLY_CLEAN_S and the last deep clean is older than
// NIGHTLY_DEEP_CLEAN_INTERVAL_S, the cycle promotes the upcoming partial to
// a full deep clean instead.
#define LONG_SLEEP_FOR_NIGHTLY_CLEAN_S (4 * 3600)
#define NIGHTLY_DEEP_CLEAN_INTERVAL_S (20 * 3600)

// Display geometry — used everywhere; do not change without re-laying out.
#define EPD_WIDTH 400
#define EPD_HEIGHT 300
#define FB_BYTES (EPD_WIDTH * EPD_HEIGHT / 8) // 15000

// RLE persistence hard cap. The compressed framebuffer must fit into the
// RTC-slow-memory slot reserved by Esp32PersistentStore; encodes that exceed
// this size are rejected (the next cycle falls back to a light full refresh).
#define RLE_HARDCAP_BYTES 7168

// Time zone for display formatting (Vienna with DST).
#define NTP_SERVER "at.pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// HTTP endpoint for the OGD realtime monitor. The caller appends `&rbl=…`.
#define WL_API_BASE                                                            \
  "https://www.wienerlinien.at/ogd_realtime/"                                  \
  "monitor?activateTrafficInfo=stoerunglang"

#endif
