#include "oebb_hafas_parse.h"

#include "../config.h"
#include "time_constants.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace bustaferl {

namespace {

// HAFAS nests prodL/opL/himL fairly deep; lift ArduinoJson's default cap (10).
constexpr int OEBB_JSON_NESTING_LIMIT = 20;

// HAFAS dDateS = "YYYYMMDD", dTimeS = "HHMMSS" (HHMMSS can exceed 24h when a
// service crosses midnight relative to the board's base date — mktime
// normalises the overflow). Both are local Vienna time. Returns 0 on failure.
time_t parseHafasDateTime(const char *date, const char *time_s) {
  if (!date || !time_s)
    return 0;
  if (std::strlen(date) < 8 || std::strlen(time_s) < 6)
    return 0;
  int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
  if (std::sscanf(date, "%4d%2d%2d", &y, &mo, &d) != 3)
    return 0;
  if (std::sscanf(time_s, "%2d%2d%2d", &h, &mi, &s) != 3)
    return 0;
  struct tm tm{};
  tm.tm_year = y - TM_YEAR_BASE;
  tm.tm_mon = mo - 1;
  tm.tm_mday = d;
  tm.tm_hour = h;
  tm.tm_min = mi;
  tm.tm_sec = s;
  tm.tm_isdst = -1; // let libc apply Vienna DST from $TZ
  time_t t = mktime(&tm);
  return (t == static_cast<time_t>(-1)) ? 0 : t;
}

// Copy a HAFAS product name into the fixed line_label buffer, abbreviating
// anything longer than 5 glyphs to "xx" (keeps the narrow column intact).
void setLineLabel(char (&dst)[6], const char *name) {
  if (!name || !*name) {
    dst[0] = '\0';
    return;
  }
  if (std::strlen(name) > 5) {
    std::strcpy(dst, "xx");
    return;
  }
  std::strncpy(dst, name, sizeof(dst) - 1);
  dst[sizeof(dst) - 1] = '\0';
}

// Resolve a journey's product index into common.prodL. HAFAS StationBoard
// answers carry it either as the scalar `jny.prodX` or as `jny.prodL[0]`
// (which may itself be an int index or an object with a `prodX` field).
int productIndex(JsonObjectConst jny) {
  int idx = jny["prodX"] | -1;
  if (idx >= 0)
    return idx;
  JsonVariantConst p0 = jny["prodL"][0];
  if (p0.is<int>())
    return p0.as<int>();
  return p0["prodX"] | -1;
}

} // namespace

std::string buildOebbRequest(const OebbStreamFilter &f) {
  JsonDocument doc;
  doc["id"] = "bustaferl";
  doc["ver"] = OEBB_HAFAS_VER;
  doc["lang"] = "deu";
  JsonObject auth = doc["auth"].to<JsonObject>();
  auth["type"] = "AID";
  auth["aid"] = OEBB_HAFAS_AID;
  // client is a verbatim JSON fragment from config.h — inject it raw rather
  // than re-parsing it into the document.
  doc["client"] = serialized(OEBB_HAFAS_CLIENT_JSON);
  doc["formatted"] = false;

  JsonObject svc = doc["svcReqL"].to<JsonArray>().add<JsonObject>();
  svc["meth"] = "StationBoard";
  JsonObject req = svc["req"].to<JsonObject>();
  req["type"] = "DEP";
  JsonObject stb = req["stbLoc"].to<JsonObject>();
  stb["type"] = "S";
  stb["extId"] = f.stb_eva;
  JsonObject dir = req["dirLoc"].to<JsonObject>();
  dir["type"] = "S";
  dir["extId"] = f.dir_eva;
  req["maxJny"] = f.max_jny;
  JsonObject flt = req["jnyFltrL"].to<JsonArray>().add<JsonObject>();
  flt["type"] = "PROD";
  flt["mode"] = "INC";
  flt["value"] = f.products;

  std::string out;
  serializeJson(doc, out);
  return out;
}

bool parseOebbStationBoard(const std::string &json, StreamData &out_stream) {
  out_stream = StreamData{};

  JsonDocument doc;
  auto err = deserializeJson(
      doc, json, DeserializationOption::NestingLimit(OEBB_JSON_NESTING_LIMIT));
  if (err)
    return false;

  // Top-level err: "OK" expected. Anything else (e.g. "AID") means the
  // endpoint rejected us — leave endpoint_responded false so auth-health trips.
  const char *top_err = doc["err"] | "OK";
  if (std::strcmp(top_err, "OK") != 0)
    return true;

  JsonObjectConst res = doc["svcResL"][0]["res"];
  if (res.isNull())
    return true;

  out_stream.endpoint_responded = true;

  JsonArrayConst prodL = res["common"]["prodL"].as<JsonArrayConst>();
  JsonArrayConst jnyL = res["jnyL"].as<JsonArrayConst>();
  if (jnyL.isNull())
    return true;

  int slot = 0;
  for (JsonObjectConst jny : jnyL) {
    if (slot >= SLOTS_PER_STREAM)
      break;
    JsonObjectConst stbStop = jny["stbStop"];
    if (stbStop["dCncl"] | false)
      continue; // cancelled — skip, line stays hidden

    const char *dDateR = stbStop["dDateR"] | static_cast<const char *>(nullptr);
    const char *dTimeR = stbStop["dTimeR"] | static_cast<const char *>(nullptr);
    const char *dDateS = stbStop["dDateS"] | static_cast<const char *>(nullptr);
    const char *dTimeS = stbStop["dTimeS"] | static_cast<const char *>(nullptr);

    time_t t = 0;
    bool rt = false;
    if (dTimeR && *dTimeR) {
      // dDateR is often omitted when same-day — fall back to dDateS.
      t = parseHafasDateTime(dDateR ? dDateR : dDateS, dTimeR);
      rt = (t != 0);
    }
    if (!t && dTimeS && *dTimeS) {
      t = parseHafasDateTime(dDateS, dTimeS);
      rt = false;
    }
    if (!t)
      continue;

    Departure &dep = out_stream.slot[slot];
    dep.when = t;
    dep.source = rt ? DepartureSource::Realtime : DepartureSource::Plan;
    dep.valid = true;

    int prod_idx = productIndex(jny);
    if (prod_idx >= 0 && !prodL.isNull() &&
        prod_idx < static_cast<int>(prodL.size())) {
      JsonObjectConst prod = prodL[prod_idx];
      const char *name =
          prod["name"] | (prod["nameS"] | static_cast<const char *>(nullptr));
      setLineLabel(dep.line_label, name);
    }
    ++slot;
  }

  out_stream.filter_matched = (slot > 0);
  return true;
}

} // namespace bustaferl
