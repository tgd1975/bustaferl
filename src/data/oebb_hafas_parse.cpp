#include "oebb_hafas_parse.h"

#include "time_constants.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

namespace bustaferl {

namespace {

// HAFAS verschachtelt `prodL` / `opL` / `himL` tief — der Default-Nest-Limit
// (10) wird in den großen Antworten gerissen.
constexpr int OEBB_JSON_NESTING_LIMIT = 20;

// `dDateS` = "YYYYMMDD", `dTimeS` = "HHMMSS". HAFAS liefert lokale Vienna-Zeit
// ohne TZ-Suffix; mktime mit `tm_isdst = -1` lässt libc anhand $TZ entscheiden.
constexpr int OEBB_DATE_LEN = 8;
constexpr int OEBB_TIME_LEN = 6;
// Byte offsets inside the YYYYMMDD date string.
constexpr int OEBB_DATE_MONTH_OFFSET = 4;
constexpr int OEBB_DATE_DAY_OFFSET = 6;

// `DEC` ist auf Arduino-ESP32 als Print-Format-Macro reserviert (Print.h);
// lokaler Name vermeidet die Kollision.
constexpr int DECIMAL_BASE = 10;

int parseFixed2(const char *p) {
  return (p[0] - '0') * DECIMAL_BASE + (p[1] - '0');
}

int parseFixed4(const char *p) {
  return (p[0] - '0') * 1000 + (p[1] - '0') * 100 +
         (p[2] - '0') * DECIMAL_BASE + (p[3] - '0');
}

// HAFAS-Datum/Zeit → epoch (Europe/Vienna lokal, via mktime + $TZ). 0 bei
// Parse-Fehler oder fehlendem Datum.
time_t parseHafasDateTime(const char *date, const char *time) {
  if (!date || !time)
    return 0;
  if (std::strlen(date) < OEBB_DATE_LEN || std::strlen(time) < OEBB_TIME_LEN)
    return 0;
  struct tm tm{};
  tm.tm_year = parseFixed4(date) - TM_YEAR_BASE;
  tm.tm_mon = parseFixed2(date + OEBB_DATE_MONTH_OFFSET) - 1;
  tm.tm_mday = parseFixed2(date + OEBB_DATE_DAY_OFFSET);
  tm.tm_hour = parseFixed2(time);
  tm.tm_min = parseFixed2(time + 2);
  tm.tm_sec = parseFixed2(time + 4);
  tm.tm_isdst = -1;
  time_t t = mktime(&tm);
  return (t == (time_t)-1) ? 0 : t;
}

// `nameS = "S 1"` → `line_label = "S1"`; `"REX 1"` → `"REX1"`. Wenn der
// nach dem Strippen verbleibende Name nicht mehr in LINE_LABEL_CAP-1 (5)
// Zeichen passt, kürzen wir nach §2.2 zu "xx".
void copyLineLabelStripped(const char *src, char *dst, std::size_t cap) {
  dst[0] = '\0';
  if (!src || cap == 0)
    return;
  // Erst strippen, dann gegen die Kapazität prüfen.
  std::size_t n = 0;
  for (const char *p = src; *p; ++p) {
    if (*p == ' ' || *p == '\t')
      continue;
    ++n;
  }
  if (n > cap - 1) {
    std::strncpy(dst, "xx", cap - 1);
    dst[cap - 1] = '\0';
    return;
  }
  std::size_t w = 0;
  for (const char *p = src; *p && w < cap - 1; ++p) {
    if (*p == ' ' || *p == '\t')
      continue;
    dst[w++] = *p;
  }
  dst[w] = '\0';
}

} // namespace

std::string buildOebbRequest(const OebbStreamFilter &f) {
  // mgate.exe StationBoard request — Schema in docs/v2-sbahn-migration-plan.md
  // Anhang A. AID / client / ver kommen aus config.h.
  JsonDocument doc;
  doc["id"] = "bustaferl";
  doc["ver"] = OEBB_HAFAS_VER;
  doc["lang"] = "deu";
  JsonObject auth = doc["auth"].to<JsonObject>();
  auth["type"] = "AID";
  auth["aid"] = OEBB_HAFAS_AID;

  // Client-JSON liegt als String-Literal in config.h vor; einmal parsen, dann
  // als Sub-Doc einsetzen, damit der Output-Body kein doppelt-escapter String
  // wird.
  JsonDocument client_doc;
  deserializeJson(client_doc, OEBB_HAFAS_CLIENT_JSON);
  doc["client"] = client_doc.as<JsonObject>();

  doc["formatted"] = false;

  JsonArray svcReqL = doc["svcReqL"].to<JsonArray>();
  JsonObject req_wrap = svcReqL.add<JsonObject>();
  req_wrap["meth"] = "StationBoard";
  JsonObject req = req_wrap["req"].to<JsonObject>();
  req["type"] = "DEP";
  JsonObject stbLoc = req["stbLoc"].to<JsonObject>();
  stbLoc["type"] = "S";
  stbLoc["extId"] = f.stbloc_extid;
  JsonObject dirLoc = req["dirLoc"].to<JsonObject>();
  dirLoc["type"] = "S";
  dirLoc["extId"] = f.dirloc_extid;
  req["maxJny"] = f.max_jny;
  JsonArray jnyFltrL = req["jnyFltrL"].to<JsonArray>();
  JsonObject fltr = jnyFltrL.add<JsonObject>();
  fltr["type"] = "PROD";
  fltr["mode"] = "INC";
  fltr["value"] = f.products;

  std::string out;
  serializeJson(doc, out);
  return out;
}

// HAFAS-StationBoard-Parser. Depth tracks the response shape (svcResL →
// res → jnyL → stbStop → fields); refactoring would hide the per-jny match
// logic behind helpers that all need the same common[prodL] reference.
// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
bool parseOebbStationBoard(const std::string &json, StreamData &out_stream,
                           OebbParseResult &result) {
  out_stream = StreamData{};
  result = OebbParseResult{};

  JsonDocument doc;
  auto derr = deserializeJson(
      doc, json, DeserializationOption::NestingLimit(OEBB_JSON_NESTING_LIMIT));
  if (derr)
    return false;

  // cppcheck-suppress badBitmaskCheck // ArduinoJson operator| (default value)
  const char *err = doc["err"] | "";
  if (std::strcmp(err, "AID") == 0 || std::strcmp(err, "AUTH") == 0) {
    result.auth_error_seen = true;
    // Auth-Fehler → kein endpoint_responded, kein filter_matched.
    return true;
  }
  if (std::strcmp(err, "OK") != 0) {
    // FAIL, PROBLEMS, PARSE, … → Stale/Offline-Pfad, KEIN Auth.
    return true;
  }

  JsonObjectConst res = doc["svcResL"][0]["res"].as<JsonObjectConst>();
  if (res.isNull()) {
    return true;
  }
  result.endpoint_responded = true;

  JsonArrayConst jnyL = res["jnyL"].as<JsonArrayConst>();
  JsonArrayConst common_prodL = res["common"]["prodL"].as<JsonArrayConst>();
  if (jnyL.isNull()) {
    return true;
  }

  int matched = 0;
  for (JsonObjectConst jny : jnyL) {
    if (matched >= SLOTS_PER_STREAM)
      break;
    JsonObjectConst stbStop = jny["stbStop"].as<JsonObjectConst>();
    if (stbStop.isNull())
      continue;
    // cppcheck-suppress badBitmaskCheck
    bool cancelled = stbStop["dCncl"] | false;
    if (cancelled)
      continue;

    // Reisedatum lebt auf jny-Level (`date: "YYYYMMDD"`). HAFAS lässt
    // `stbStop.dDateS/dDateR` für Same-Day-Abfahrten weg — Pre-Phase
    // 2026-05-19 verifiziert. Fallback auf stbStop-Datum, falls künftig
    // doch Day-Rollover-Fälle damit kommen.
    // cppcheck-suppress badBitmaskCheck
    const char *jny_date = jny["date"] | (const char *)nullptr;
    // cppcheck-suppress badBitmaskCheck
    const char *dDateS_field = stbStop["dDateS"] | (const char *)nullptr;
    // cppcheck-suppress badBitmaskCheck
    const char *dDateR_field = stbStop["dDateR"] | (const char *)nullptr;
    // cppcheck-suppress badBitmaskCheck
    const char *dTimeS = stbStop["dTimeS"] | (const char *)nullptr;
    // cppcheck-suppress badBitmaskCheck
    const char *dTimeR = stbStop["dTimeR"] | (const char *)nullptr;
    const char *date_for_S = dDateS_field ? dDateS_field : jny_date;
    const char *date_for_R = dDateR_field ? dDateR_field : jny_date;

    time_t when = 0;
    DepartureSource source = DepartureSource::Plan;
    if (dTimeR && date_for_R) {
      when = parseHafasDateTime(date_for_R, dTimeR);
      if (when != 0)
        source = DepartureSource::Realtime;
    }
    if (when == 0 && dTimeS && date_for_S) {
      when = parseHafasDateTime(date_for_S, dTimeS);
      source = DepartureSource::Plan;
    }
    if (when == 0)
      continue;

    // `jny.prodX` ist der Index in svcResL[0].res.common.prodL[]. Plan-
    // Anhang A schrieb fälschlich `prodL[0]`; das ist auf jny-Ebene ein
    // Objekt (`{ prodX, fLocX, tLocX, fIdx, tIdx }`), kein Index. Pre-Phase
    // 2026-05-19 verifiziert: jny.prodX === jny.prodL[0].prodX.
    // cppcheck-suppress badBitmaskCheck
    int prod_idx = jny["prodX"] | -1;
    const char *nameS = nullptr;
    if (prod_idx >= 0 && !common_prodL.isNull()) {
      JsonObjectConst prod = common_prodL[prod_idx].as<JsonObjectConst>();
      // cppcheck-suppress badBitmaskCheck
      nameS = prod["nameS"] | (const char *)nullptr;
    }

    Departure &slot = out_stream.slot[matched];
    slot.when = when;
    slot.source = source;
    slot.valid = true;
    copyLineLabelStripped(nameS, slot.line_label, Departure::LINE_LABEL_CAP);
    ++matched;
  }

  result.filter_matched = (matched > 0);
  return true;
}

} // namespace bustaferl
