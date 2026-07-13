#include "render/diag_page.h"

#include "data/stream_labels.h"
#include "logic/cycle_trace.h"
#include "render/layout.h" // FB_W / FB_H

#include <cstdio>
#include <cstring>
#include <ctime>

namespace bustaferl {

namespace {

// Body text is FontRole::Fullscreen_Foot (the smallest role) so the dense
// diagnostic lines fit. Left margin, title band, and a fixed line pitch.
constexpr int DIAG_MARGIN_X = 6;
constexpr int DIAG_TITLE_Y = 8;
constexpr int DIAG_TITLE_RULE_DY = 20; // separator rule below the title text
constexpr int DIAG_BODY_Y0 = 34;
constexpr int DIAG_LINE_PITCH = 12;
constexpr int DIAG_FOOTER_Y = FB_H - 10;
constexpr std::size_t DIAG_LINE_BUF = 64;
constexpr std::size_t HHMM_BUF = 8;

int bodyY(int line) { return DIAG_BODY_Y0 + line * DIAG_LINE_PITCH; }

void textAt(render::Canvas &canvas, int x, int y, const char *s) {
  canvas.setRoleFont(FontRole::Fullscreen_Foot);
  canvas.setTextColor(1);
  canvas.setCursor(x, y);
  canvas.print(s);
}

void title(render::Canvas &canvas, const char *text) {
  canvas.setRoleFont(FontRole::Fullscreen_Sub);
  canvas.setTextColor(1);
  canvas.setCursor(DIAG_MARGIN_X, DIAG_TITLE_Y);
  canvas.print(text);
  canvas.drawFastHLine(DIAG_MARGIN_X, DIAG_TITLE_Y + DIAG_TITLE_RULE_DY,
                       FB_W - 2 * DIAG_MARGIN_X, 1);
}

void footer(render::Canvas &canvas, int page) {
  char buf[DIAG_LINE_BUF];
  std::snprintf(buf, sizeof(buf), "Seite %d/%d  kurz: weiter   lang: zurueck",
                page + 1, DIAG_PAGE_COUNT);
  canvas.drawFastHLine(DIAG_MARGIN_X, DIAG_FOOTER_Y - 4,
                       FB_W - 2 * DIAG_MARGIN_X, 1);
  textAt(canvas, DIAG_MARGIN_X, DIAG_FOOTER_Y, buf);
}

void formatHHMM(time_t t, char *out, std::size_t cap) {
  if (t == 0) {
    std::snprintf(out, cap, "--:--");
    return;
  }
  struct tm local {};
  localtime_r(&t, &local);
  std::snprintf(out, cap, "%02d:%02d", local.tm_hour, local.tm_min);
}

char triggerChar(std::uint8_t trigger) {
  switch (static_cast<CycleTrigger>(trigger)) {
  case CycleTrigger::Button:
    return 'B';
  case CycleTrigger::ColdBoot:
    return 'C';
  case CycleTrigger::Timer:
  default:
    return 'T';
  }
}

const char *errorText(std::uint8_t code) {
  switch (static_cast<TraceError>(code)) {
  case TraceError::WifiFail:
    return "WLAN-Verbindung fehlgeschlagen";
  case TraceError::HttpOgd:
    return "WL-Monitor: HTTP-Fehler";
  case TraceError::HttpOebb:
    return "OEBB-Server: keine Antwort";
  case TraceError::ParseFail:
    return "Antwort nicht lesbar (Parse)";
  case TraceError::OebbAuth:
    return "OEBB lehnt Zugang ab (Auth)";
  case TraceError::EfaFail:
    return "Morgen-Fahrplan (EFA) fehlt";
  case TraceError::NtpFail:
    return "Zeitabgleich (NTP) fehlgeschlagen";
  case TraceError::StaleEnter:
    return "Daten veraltet (Beginn)";
  case TraceError::StaleExit:
    return "Daten wieder aktuell";
  case TraceError::Filter58bDead:
    return "58B-Filter liefert nichts";
  case TraceError::RleOverflow:
    return "Bildspeicher-Overflow (RLE)";
  default:
    return "unbekannt";
  }
}

// "s30s" / "s5m" / "s4h" compact sleep formatting.
void formatSleep(std::uint16_t s, char *out, std::size_t cap) {
  constexpr int MIN = 60;
  constexpr int HOUR = 3600;
  if (s >= HOUR) {
    std::snprintf(out, cap, "%dh", s / HOUR);
  } else if (s >= MIN) {
    std::snprintf(out, cap, "%dm", s / MIN);
  } else {
    std::snprintf(out, cap, "%ds", s);
  }
}

const char *sourceLetter(const Departure &d) {
  if (!d.valid) {
    return "-";
  }
  switch (d.source) {
  case DepartureSource::Realtime:
    return "E"; // Echtzeit
  case DepartureSource::Plan:
    return "P";
  case DepartureSource::Hint:
    return "H";
  default:
    return "?";
  }
}

// --- STATUS page ---------------------------------------------------------

int drawStatusBody(render::Canvas &canvas, const DiagView &v, int line) {
  char buf[DIAG_LINE_BUF];
  char hhmm[HHMM_BUF];

  if (v.has_net_info) {
    std::snprintf(buf, sizeof(buf), "WLAN  \"%s\"  %d dBm", v.ssid, v.rssi_dbm);
  } else {
    std::snprintf(buf, sizeof(buf), "WLAN  (keine Info)");
  }
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);
  if (v.has_net_info) {
    std::snprintf(buf, sizeof(buf), "      IP %s", v.ip);
    textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);
  }

  formatHHMM(v.now, hhmm, sizeof(hhmm));
  char last[HHMM_BUF];
  formatHHMM(v.last_ntp_sync, last, sizeof(last));
  std::snprintf(buf, sizeof(buf), "Zeit  %s %s (NTP-Sync %s)", hhmm,
                v.ntp_ok ? "ok" : "UNGESTELLT", last);
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);

  for (int i = 0; i < STREAM_COUNT; ++i) {
    const StreamData &s = v.snap.stream[i];
    char t0[HHMM_BUF];
    char t1[HHMM_BUF];
    formatHHMM(s.slot[0].valid ? s.slot[0].when : 0, t0, sizeof(t0));
    formatHHMM(s.slot[1].valid ? s.slot[1].when : 0, t1, sizeof(t1));
    const char *verdict = !s.endpoint_responded
                              ? "keine Antwort"
                              : (s.slot[0].valid ? "OK" : "--");
    std::snprintf(buf, sizeof(buf), "%-10s %-13s %s %s", streamLabel(i),
                  verdict, t0, t1);
    textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);
  }

  std::snprintf(buf, sizeof(buf), "Streaks  58B-Filter %u   OEBB-Auth %u%s",
                v.filter_miss_streak, v.ogd_auth_streak,
                v.auth_error_seen ? "  AUTH!" : "");
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);

  std::snprintf(buf, sizeof(buf), "Heap %u kB (groesster %u kB)   Uptime %us",
                v.heap_free_kb, v.heap_largest_kb, v.uptime_s);
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);
  return line;
}

void drawStatus(render::Canvas &canvas, const DiagView &v) {
  title(canvas, "STATUS");
  drawStatusBody(canvas, v, 0);
  footer(canvas, v.diag_page);
}

// --- CYCLES page ---------------------------------------------------------

void drawCycles(render::Canvas &canvas, const DiagView &v) {
  title(canvas, "ZYKLEN (neueste zuerst)");
  char buf[DIAG_LINE_BUF];
  int line = 0;
  const int max_lines = (DIAG_FOOTER_Y - DIAG_BODY_Y0) / DIAG_LINE_PITCH - 1;
  for (int i = 0; i < v.trace.cycle_count && line < max_lines; ++i, ++line) {
    const CycleRecord *r = traceCycleAt(v.trace, i);
    char hhmm[HHMM_BUF];
    formatHHMM(static_cast<time_t>(r->at), hhmm, sizeof(hhmm));
    char streams[STREAM_COUNT + 1];
    for (int s = 0; s < STREAM_COUNT; ++s) {
      streams[s] = (r->flags & (1u << s)) ? 'o' : '.';
    }
    streams[STREAM_COUNT] = '\0';
    char sleep[HHMM_BUF];
    formatSleep(r->sleep_s, sleep, sizeof(sleep));
    const char *rescue = (r->flags & CYC_RESCUE_OK)      ? " R+"
                         : (r->flags & CYC_RESCUE_TRIED) ? " R?"
                                                         : "";
    std::snprintf(buf, sizeof(buf), "%s %c %s f%d%s%s %s", hhmm,
                  triggerChar(r->trigger), streams, r->failed_batches, rescue,
                  (r->flags & CYC_STALE) ? " ST" : "", sleep);
    textAt(canvas, DIAG_MARGIN_X, bodyY(line), buf);
  }
  if (v.trace.cycle_count == 0) {
    textAt(canvas, DIAG_MARGIN_X, bodyY(0), "(noch keine Zyklen)");
  }
  footer(canvas, v.diag_page);
}

// --- ERRORS page ---------------------------------------------------------

void drawErrors(render::Canvas &canvas, const DiagView &v) {
  title(canvas, "FEHLER (neueste zuerst)");
  char buf[DIAG_LINE_BUF];
  int line = 0;
  const int max_lines = (DIAG_FOOTER_Y - DIAG_BODY_Y0) / DIAG_LINE_PITCH - 1;
  for (int i = 0; i < v.trace.error_count && line < max_lines; ++i, ++line) {
    const ErrorRecord *e = traceErrorAt(v.trace, i);
    char hhmm[HHMM_BUF];
    formatHHMM(static_cast<time_t>(e->at), hhmm, sizeof(hhmm));
    std::snprintf(buf, sizeof(buf), "%s %s", hhmm, errorText(e->code));
    textAt(canvas, DIAG_MARGIN_X, bodyY(line), buf);
  }
  if (v.trace.error_count == 0) {
    textAt(canvas, DIAG_MARGIN_X, bodyY(0), "(keine Anomalien aufgezeichnet)");
  }
  footer(canvas, v.diag_page);
}

// --- DATA DETAIL page ----------------------------------------------------

void drawDataDetail(render::Canvas &canvas, const DiagView &v) {
  title(canvas, "DATEN-DETAILS");
  char buf[DIAG_LINE_BUF];
  int line = 0;
  for (int i = 0; i < STREAM_COUNT; ++i) {
    const StreamData &s = v.snap.stream[i];
    char t0[HHMM_BUF];
    char t1[HHMM_BUF];
    formatHHMM(s.slot[0].valid ? s.slot[0].when : 0, t0, sizeof(t0));
    formatHHMM(s.slot[1].valid ? s.slot[1].when : 0, t1, sizeof(t1));
    std::snprintf(buf, sizeof(buf), "%-10s %s%s  %s%s", streamLabel(i),
                  sourceLetter(s.slot[0]), t0, sourceLetter(s.slot[1]), t1);
    textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);
  }

  char fetched[HHMM_BUF];
  formatHHMM(v.schedule.fetched_at, fetched, sizeof(fetched));
  std::snprintf(buf, sizeof(buf), "Morgen-Fahrplan geladen %s",
                v.schedule.fetched_at != 0 ? fetched : "nie");
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);

  char lf[HHMM_BUF];
  char dc[HHMM_BUF];
  formatHHMM(v.last_light_full, lf, sizeof(lf));
  formatHHMM(v.last_deep_clean, dc, sizeof(dc));
  std::snprintf(buf, sizeof(buf),
                "Panel  Partials %u   LightFull %s   Clean %s", v.partial_count,
                lf, dc);
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);

  textAt(canvas, DIAG_MARGIN_X, bodyY(line++),
         "Quelle: E=Echtzeit  P=Plan  H=Hint  -=leer");
  footer(canvas, v.diag_page);
}

} // namespace

void drawDiagPage(render::Canvas &canvas, const DiagView &v, DiagPage page) {
  switch (page) {
  case DiagPage::Status:
    drawStatus(canvas, v);
    return;
  case DiagPage::Cycles:
    drawCycles(canvas, v);
    return;
  case DiagPage::Errors:
    drawErrors(canvas, v);
    return;
  case DiagPage::DataDetail:
    drawDataDetail(canvas, v);
    return;
  }
}

void drawBootCheck(render::Canvas &canvas, const DiagView &v) {
  title(canvas, "BOOT-CHECK");
  int line = drawStatusBody(canvas, v, 0);
  ++line; // blank separator

  char buf[DIAG_LINE_BUF];
  if (v.batches_failed > 0) {
    std::snprintf(buf, sizeof(buf), "Abfragen  %d von %d fehlgeschlagen",
                  v.batches_failed, v.batches_total);
  } else if (v.batches_retried > 0) {
    std::snprintf(buf, sizeof(buf), "Abfragen  alle ok, %d mit Wiederholung",
                  v.batches_retried);
  } else {
    std::snprintf(buf, sizeof(buf), "Abfragen  alle beim 1. Versuch ok");
  }
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);

  std::snprintf(buf, sizeof(buf), "RTC  Meta %s | Frame %s | Fahrplan %s",
                v.meta_restored ? "OK" : "neu", v.frame_restored ? "OK" : "neu",
                v.schedule_restored ? "OK" : "neu");
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);

  std::snprintf(buf, sizeof(buf), "WLAN & NTP ok (%d/%d)", v.boot_attempt,
                v.boot_attempts_max);
  textAt(canvas, DIAG_MARGIN_X, bodyY(line++), buf);

  if (v.show_s > 0) {
    std::snprintf(buf, sizeof(buf),
                  "Anzeige startet in %d s - Taste druecken: sofort", v.show_s);
    canvas.drawFastHLine(DIAG_MARGIN_X, DIAG_FOOTER_Y - 4,
                         FB_W - 2 * DIAG_MARGIN_X, 1);
    textAt(canvas, DIAG_MARGIN_X, DIAG_FOOTER_Y, buf);
  }
}

} // namespace bustaferl
