# v2 — S-Bahn Atzgersdorf · Migrationsplan

Stand: 2026-05-19 · Vor-Umsetzungs-Stand. Setzt das in [CONCEPT.md §v2](../CONCEPT.md#v2--s-bahn-atzgersdorf) beschriebene Zielbild um.

Vorlage und Stil: [docs/main-refactor-plan.md](main-refactor-plan.md). Dieses Dokument plant nur die v2-Migration; Schichten-Refactor von `main.cpp` und Engine-Extraktion sind im main-refactor abgeschlossen und bleiben unberührt.

## Inhaltsverzeichnis

- [1. Status quo](#1-status-quo)
- [2. Zielbild](#2-zielbild)
- [3. Pros / Cons / Out-of-Scope](#3-pros--cons--out-of-scope)
- [4. Schritt-für-Schritt-Plan](#4-schritt-für-schritt-plan)
- [5. Risiken](#5-risiken)
- [6. Tests](#6-tests)
- [Anhang A — HAFAS-Request-/Response-Vertrag](#anhang-a--hafas-request-response-vertrag)
- [Anhang B — RTC-Bilanz nach v2](#anhang-b--rtc-bilanz-nach-v2)
- [Anhang C — Open Questions vor Schritt 0](#anhang-c--open-questions-vor-schritt-0)

---

## 1. Status quo

### 1.1 Was heute steht

| Komponente | Stand | Quelle |
|---|---|---|
| Stream-Topologie | **5 Streams** (`STREAM_58A_ATZ`, `…_HIETZING`, `STREAM_58B_ATZ`, `STREAM_U1_LEOPOLDAU`, `STREAM_U1_OBERLAA`) | [src/data/StreamSnapshot.h:11-18](../src/data/StreamSnapshot.h#L11-L18) |
| Realtime-Backend | Wiener-Linien-OGD (`monitor?stopId=…`), **nur GET** | [src/data/wienerlinien_parse.cpp](../src/data/wienerlinien_parse.cpp), [src/hal/Esp32Network.h:17](../src/hal/Esp32Network.h#L17) |
| Hint-Backend | Wiener-Linien-EFA (`XSLT_DM_REQUEST`), per DIVA-Stop | [src/logic/schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp), [src/data/efa_parse.h](../src/data/efa_parse.h) |
| Filter-Quelle | Single source of truth: `buildStreamFilters` + `buildScheduleFilters` | [src/logic/filter_builder.cpp](../src/logic/filter_builder.cpp) |
| Stream-Labels | Einzige Stelle: `streamLabel()` | [src/data/stream_labels.h](../src/data/stream_labels.h) |
| Layout Block 3 | „SÜDTIROLER PLATZ" + zwei `U1`-Zeilen, jeweils zwei Slots | [src/render/layout.cpp:127-131](../src/render/layout.cpp#L127-L131) |
| Departure-Struktur | `when`, `valid`, `source ∈ {Unknown, Realtime, Plan, Hint}` — Linie/Richtung implizit über Stream-Index | [src/data/Departure.h](../src/data/Departure.h) |
| RTC-Bilanz | ~7412 / 8192 Byte belegt, ~780 B Reserve nach Schritt 2.3 des main-refactors | [docs/main-refactor-plan.md Anhang A](main-refactor-plan.md) |
| Test-Buckets | 23 `test_native_*`, 5 `test_device_*`, 9 `test_longterm_*` | [test/](../test/) |

### 1.2 Was sich für v2 ändert (laut [CONCEPT.md §v2-1](../CONCEPT.md#1-motivation-und-geltungsbereich))

- Streams 3 + 4 (beide U1) entfallen vollständig. Ein neuer Stream `STREAM_SBAHN_HBF` ersetzt sie → `STREAM_COUNT = 4`.
- Neue Datenquelle **ÖBB Scotty mgate.exe (HAFAS, POST/JSON)** für den S-Bahn-Stream; Bus-Streams bleiben auf OGD.
- Layout-Block 3 wechselt von „Richtung pro Zeile, Linie fix" zu „Linie pro Slot, Richtung in der Überschrift".
- `Departure` braucht ein optionales `line_label`, weil im S-Bahn-Stream pro Slot die Linie variiert (S2/S3/S4/REX).

### 1.3 Was unverändert bleibt (harte Constraints)

- **Sleep-Planner, Refresh-Planner, Stale-Policy, Cold-Boot-Sequencer** kennen den S-Bahn-Stream nur über `t_ref = min(stream[*].slot[*].when)`. Kein Eingriff.
- **HAL-Interfaces** `IClock`, `IDisplay`, `IPersistentStore`, `ISleep`, `IRenderer`, `IButton` bleiben strukturell. Einzige Erweiterung: `INetwork::httpPost` (Schritt 1).
- **Schichten-Regel** aus [docs/ARCHITECTURE.md](ARCHITECTURE.md). HAFAS-Parser landet in `data/`, kein HAL-Touch außer Esp32Network.
- **`main.cpp`** (~80 LOC nach main-refactor Schritt 8) wird nicht angefasst — `cycle_runner` iteriert über `STREAM_COUNT` und ist von der Reduktion 5→4 selbst nicht betroffen.

### 1.4 Touch-Site-Inventar (heute → v2)

| Wo | Heute | v2 |
|---|---|---|
| [src/data/StreamSnapshot.h:11-18](../src/data/StreamSnapshot.h#L11-L18) | `STREAM_U1_LEOPOLDAU=3`, `STREAM_U1_OBERLAA=4`, `STREAM_COUNT=5` | `STREAM_SBAHN_HBF=3`, `STREAM_COUNT=4` |
| [src/data/Departure.h](../src/data/Departure.h) | `{when, source, valid}` | + `char line_label[6] = ""` (optional, S-Bahn füllt) |
| [src/data/stream_labels.h](../src/data/stream_labels.h) | switch über 5 Indices | switch über 4 Indices, neuer Eintrag `"SBahn-Hbf"` |
| [src/config.h:13-14](../src/config.h#L13-L14) | `RBL_SUEDTIROLER_LEOPOLDAU`, `RBL_SUEDTIROLER_OBERLAA` | entfernen |
| [src/config.h:39-40](../src/config.h#L39-L40) | `TOWARDS_U1_LEOPOLDAU`, `TOWARDS_U1_OBERLAA` | entfernen |
| [src/config.h:47](../src/config.h#L47) | `DIVA_SUEDTIROLER_PLATZ` | entfernen |
| [src/config.h:55-56](../src/config.h#L55-L56) | `EFA_TOWARDS_U1_LEOPOLDAU`, `EFA_TOWARDS_U1_OBERLAA` | entfernen |
| [src/config.h](../src/config.h) (neu) | — | `EVA_WIEN_ATZGERSDORF`, `EVA_WIEN_HBF`, `OEBB_MGATE_URL`, `OEBB_HAFAS_AID`, `OEBB_HAFAS_CLIENT_JSON`, `OEBB_JNYFLTR_PRODUCTS`, `OEBB_MAX_JNY` |
| [src/logic/filter_builder.cpp:11-13](../src/logic/filter_builder.cpp#L11-L13) | zwei U1-Zeilen | eine S-Bahn-Zeile (neuer Filter-Typ — siehe Schritt 4) |
| [src/logic/filter_builder.cpp:21-24](../src/logic/filter_builder.cpp#L21-L24) | zwei U1-Schedule-Zeilen | weggelassen (S-Bahn-Stream hat keinen Schedule-Filter) |
| [src/logic/snapshot_fetcher.cpp:16-19](../src/logic/snapshot_fetcher.cpp#L16-L19) | `FETCH_ORDER` über 5 Streams | über 4 Streams; S-Bahn-Stream getrennt gefetcht (anderer Endpoint, POST) |
| [src/logic/snapshot_fetcher.cpp](../src/logic/snapshot_fetcher.cpp) | nur OGD-Batch-Loop | + ÖBB-Single-Call (Sub-Funktion `fetchOebbStream`) |
| [src/render/layout.cpp:127-131](../src/render/layout.cpp#L127-L131) | `drawHeader "SUEDTIROLER PLATZ"` + zwei `drawStreamLine` für U1 | `drawHeader "ATZGERSDORF S-BAHN"` + Untertitel `→ Hauptbahnhof` + neuer `drawSBahnSlot(line_label, hhmm)` × 2 |
| [src/hal/INetwork.h](../src/hal/INetwork.h) | `httpGet`, `httpGetStream` | + `httpPost(url, body, content_type, out)` (mind. nicht-streaming) |
| [src/hal/Esp32Network.{h,cpp}](../src/hal/Esp32Network.h) | nur GET | + POST-Pfad, gleicher TLS-Kontext |
| [src/hal/Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp) | `MAGIC`, `SCHED_MAGIC` | beide bumpen (STREAM_COUNT-Änderung invalidiert RLE-Frame *und* Schedule-Layout) |
| Tests: `test_native_filter_builder`, `…_filter_health`, `…_slot_merger`, `…_wienerlinien_parse`, `…_efa_parse`, `…_schedule_fetcher`, `…_snapshot_fetcher`, `…_snapshot_logger`, `…_render_input`, `…_cycle_runner_*`, `test_device_fetch`, `test_device_schedule`, `test_longterm_*` | jeweils `STREAM_COUNT=5`-implizit (Index 3/4 erwartet U1-Daten) | auf 4 Streams umstellen; ggf. Fixture-Anpassung |

Zähle ich pro Datei einmal: ~14 Quellfiles und ~16 Testfiles werden angefasst.

### 1.5 Was offen ist (vor Code-Eingriff zu klären)

CONCEPT §v2-10 nennt sechs Pre-Flash-Verifikationspunkte. Drei davon **müssen** vor Schritt 1 beantwortet sein, weil sie das Datenmodell oder den Request-Body bestimmen:

1. AID/Client-Werte aus aktueller ÖBB-Webapp ([CONCEPT §v2-10 Punkt 1](../CONCEPT.md#10-pre-flash-verifikation--open-questions)).
2. `jnyFltrL`-Bitmask für S-Bahn+Regio+REX (Punkt 2).
3. `dirLoc`-Verhalten — reicht es, Gegenrichtung HAFAS-seitig zu unterdrücken, oder brauchen wir clientseitig einen Direction-Healthcheck (Punkt 3).

Die restlichen drei (Slot-Anzahl 2 vs. 3, Hint-Variante, Heap-Spitze) sind nach Schritt 3 entscheidbar und werden in §4.1 (Annahmen) festgehalten — siehe [Anhang C](#anhang-c--open-questions-vor-schritt-0).

---

## 2. Zielbild

### 2.1 Struktur nach v2

```text
src/
├── config.h                       U1-Konstanten weg; ÖBB-Konstanten dazu
├── data/
│   ├── Departure.h                + char line_label[6] (optional)
│   ├── StreamSnapshot.h           Stream-Enum: 4 Werte, neuer STREAM_SBAHN_HBF
│   ├── stream_labels.h            4 Einträge statt 5
│   ├── oebb_hafas_parse.{h,cpp}   NEU — Parser für mgate.exe-Antwort
│   ├── wienerlinien_parse.*       unverändert (außer STREAM_COUNT-Größe)
│   └── efa_parse.*                unverändert (außer STREAM_COUNT-Größe)
├── logic/
│   ├── filter_builder.{h,cpp}     OGD-Filter: 3 Einträge; Schedule-Filter: 3 Einträge
│   ├── snapshot_fetcher.{h,cpp}   + fetchOebbStream() für STREAM_SBAHN_HBF;
│   │                              FETCH_ORDER nur noch über die 3 OGD-Streams
│   ├── schedule_fetcher.*         unverändert (S-Bahn-Stream wird übersprungen)
│   ├── slot_merger.cpp            keine Änderung (mergt über STREAM_COUNT)
│   ├── render_input.*             unverändert
│   └── cycle_runner.*             unverändert (iteriert über STREAM_COUNT)
├── hal/
│   ├── INetwork.h                 + httpPost()
│   └── Esp32Network.{h,cpp}       + httpPost-Impl
└── render/
    └── layout.cpp                 Block 3: drawHeader+Untertitel+drawSBahnSlot×2
```

`main.cpp` bleibt — alle Stream-Konstanten kommen aus `config.h`, alle Stream-Iterationen aus `STREAM_COUNT`.

### 2.2 Neue Module im Detail

**`data/oebb_hafas_parse.{h,cpp}`**

```cpp
namespace bustaferl {

struct OebbStreamFilter {
  std::string stb_eva;   // EVA, Departure board station
  std::string dir_eva;   // EVA, dirLoc → must touch this station downstream
  std::string products;  // jnyFltrL "value", e.g. "63" (= S-Bahn+Regio+REX)
  int max_jny = OEBB_MAX_JNY;
};

// Builds the mgate.exe POST body for one StationBoard request. Pure function,
// host-testable. AID/Client come from config.h.
std::string buildOebbRequest(const OebbStreamFilter &f);

// Parses an mgate.exe StationBoard response into `slot[0..N-1]`. Sets
// `endpoint_responded` iff the response carries `err == "OK"` and a non-null
// `svcResL[0].res`. Sets `filter_matched` iff at least one entry survives
// (i.e. dirLoc reached, product accepted, not cancelled). Each slot gets
// `source = Realtime` (when `dTimeR` present) or `Plan` (only `dTimeS`); the
// line-name (`prodL[…].name`) is copied into `line_label`.
bool parseOebbStationBoard(const std::string &json, StreamData &out_stream);

} // namespace bustaferl
```

Begründung gegen `OebbStreamFilter` als `struct`-Familie pro Stream (wie bei OGD): es gibt nur **einen** S-Bahn-Stream. Eine `StreamFilter`-Tabelle der Größe `STREAM_COUNT` für genau einen aktiven Eintrag wäre Boilerplate, der unbenutzte Einträge mitführt. Stattdessen: `OebbStreamFilter` als Single-Value-Filter, vom `filter_builder` direkt ausgegeben.

**`logic/snapshot_fetcher.cpp` — erweitert um Sub-Funktion `fetchOebbStream`**

```cpp
namespace bustaferl {

// Internal: one mgate.exe POST → parseOebbStationBoard → write into
// out.stream[STREAM_SBAHN_HBF]. Updates summary like the OGD batch loop
// (counts as one batch). Logs analogously via SNAP_LOG.
static bool fetchOebbStream(INetwork &net, const std::string &mgate_url,
                            const OebbStreamFilter &f, StreamSnapshot &out,
                            FetchSummary &summary);

} // namespace bustaferl
```

`fetchSnapshot` ruft beide Pfade hintereinander: erst die bestehende OGD-Batch-Schleife (über die 3 verbleibenden OGD-Streams), dann `fetchOebbStream` für `STREAM_SBAHN_HBF`. Reihenfolge bewusst OGD-zuerst, weil der mgate-Call größer und teurer ist und ein OGD-Fehler den S-Bahn-Stream nicht verbergen soll.

`api_ok` bleibt definiert als „mindestens ein Batch hat geparst" — wird durch den S-Bahn-Call automatisch toleranter, weil ein OGD-Totalausfall noch eine S-Bahn-Antwort haben kann (und umgekehrt). Das ist erwünschtes Verhalten.

**`logic/filter_builder.{h,cpp}` — erweitert um S-Bahn-Filter-Getter**

```cpp
namespace bustaferl {

void buildStreamFilters(StreamFilter (&f)[STREAM_COUNT]);            // OGD
void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]);  // EFA
OebbStreamFilter buildOebbFilter();                                   // NEU
} // namespace bustaferl
```

Die ersten drei Indices der `StreamFilter`-Tabelle werden mit den OGD-Streams belegt; **`f[STREAM_SBAHN_HBF]` bleibt default-leer** (`rbl = 0`). Das ist absichtlich: `wienerlinien_parse::findFilterForRbl` matcht nie auf `rbl=0`, weil die OGD-Antwort kein RBL 0 enthält. Der S-Bahn-Slot wird ausschließlich von `fetchOebbStream` befüllt.

Analog `ScheduleStreamFilter[STREAM_SBAHN_HBF]` mit `diva = 0` — `schedule_fetcher` iteriert über die distinct DIVAs (`fetchSchedule` sieht heute schon eine DIVA als „kein Schedule" wenn niemand sie nennt). Test in `test_native_schedule_fetcher` bestätigt.

**`hal/INetwork.h` + `Esp32Network` — `httpPost`**

```cpp
// in INetwork.h
virtual bool httpPost(const std::string &url, const std::string &body,
                      const std::string &content_type, std::string &out) = 0;
```

Streaming-POST (analog zu `httpGetStream`) wird **nicht** in der ersten Iteration hinzugefügt. Begründung: HAFAS-Antworten sind 5–8 KB ([CONCEPT §v2-6](../CONCEPT.md#6-api-polling-quoten-und-wake-verhalten)), eine Größenordnung kleiner als die EFA-Antworten (~38 KB), die heute Streaming brauchen. Erst wenn Schritt 9 (Heap-Profiling) zeigt, dass die `std::string`-Variante die Heap-Reserve verletzt, kommt `httpPostStream` nach. YAGNI bis dahin.

`Esp32Network::httpPost` reused das bestehende `WiFiClientSecure` und die `setInsecure()`-Policy aus dem GET-Pfad — Let's-Encrypt-Cert für `fahrplan.oebb.at` liegt im Bundle, kein neues Root-Cert nötig (verifiziert via `openssl s_client -connect fahrplan.oebb.at:443` vor Schritt 1).

**`data/Departure.h` — `line_label`**

```cpp
struct Departure {
  time_t when = 0;
  DepartureSource source = DepartureSource::Unknown;
  bool valid = false;
  char line_label[6] = "";   // NEU: "S2", "S3", "S4", "REX1", "" (Bus-Streams)

  bool operator==(const Departure &o) const {
    return valid == o.valid && when == o.when && source == o.source
        && std::strcmp(line_label, o.line_label) == 0;
  }
};
```

`char[6]` statt `std::string`: `Departure` lebt in `StreamData::slot[]`, das wiederum heute in `StreamSnapshot` und nach Frame-Render kompakt in RLE persistiert wird. Heap-Allokation pro Slot wäre vermeidbarer Heap-Druck (4 × 4 × pointer + len + cap = 64 B Overhead), `char[6]` ist 6 B. Maximal-Länge 5 + null-terminator deckt `REX1` (4), `RJX1` (4), `NJ123` (5) ab; alles Längere wird abgekürzt zu `"xx"` (siehe [CONCEPT §v2-7](../CONCEPT.md#7-layout-block-3-display)).

Touch-Sites: `Departure`-Vergleich, `slot_merger::insertSorted` (kopiert via aggregate-assign — funktioniert), `frame_buffer`-Diff vergleicht Bytes (line_label wandert mit). Bus-Streams setzen `line_label[0] = '\0'` per Default-Init; der Renderer wertet `line_label` nur im S-Bahn-Block aus.

**`render/layout.cpp` — Block 3 neu**

```cpp
void drawSBahnSlot(ExternalCanvas &c, int x, int y, const Departure &d,
                   bool stale) {
  char buf[8];
  if (stale)
    std::snprintf(buf, sizeof(buf), "??:??");
  else if (d.valid)
    formatHHMM(d.when, buf, sizeof(buf));
  else
    std::snprintf(buf, sizeof(buf), "--:--");

  // Linie links der Uhrzeit, breitenkalibriert. Ein leeres Label oder
  // ungültiger Slot blendet die Linie aus (sonst stünde sie ohne Zeit da).
  if (!stale && d.valid && d.line_label[0]) {
    drawText(c, x, y, 2, d.line_label);
  }
  drawText(c, x + 56, y, 2, buf);  // Linien-Spalte breit genug für "REX1"
}

// In renderFrame() statt der beiden U1-Zeilen:
drawHeader(c, 196, "ATZGERSDORF S-BAHN");
drawText(c, 8, 224, 1, "-> Hauptbahnhof");
const auto &sbahn = in.snapshot.stream[STREAM_SBAHN_HBF];
drawSBahnSlot(c, 8, 246, sbahn.slot[0], stale);
drawSBahnSlot(c, 200, 246, sbahn.slot[1], stale);
```

Eine Zeile mit zwei Slots, jeweils mit Linien-Präfix. Y-Koordinaten halten den Block innerhalb des bestehenden 196–270-Fensters; Overlay-Banner-Position (y=270) bleibt unverändert.

### 2.3 Stream-Layout im Detail

```text
STREAM_58A_ATZ       (0) — Tullnertalgasse 58A → Atzgersdorf       (OGD)
STREAM_58A_HIETZING  (1) — Tullnertalgasse 58A → Hietzing          (OGD)
STREAM_58B_ATZ       (2) — Endemanngasse   58B → Atzgersdorf       (OGD)
STREAM_SBAHN_HBF     (3) — Bhf. Atzgersdorf S-Bahn → Wien Hbf      (ÖBB HAFAS)
STREAM_COUNT = 4
```

`SLOTS_PER_STREAM = 2` bleibt für alle Streams — Variante A aus [CONCEPT §v2-5.2](../CONCEPT.md#52-slot-anzahl-pro-stream). Eine Erweiterung auf 3 für den S-Bahn-Stream allein wäre eine eigene PR nach v2-Roll-out, sobald Beobachtungsdaten vorliegen.

### 2.4 Konfiguration

Neue Konstanten in `config.h`:

```cpp
// ÖBB Wien Atzgersdorf → Wien Hauptbahnhof
#define EVA_WIEN_ATZGERSDORF      "8100634"
#define EVA_WIEN_HBF              "8100002"
#define OEBB_MGATE_URL            "https://fahrplan.oebb.at/bin/mgate.exe"
#define OEBB_HAFAS_AID            "OWDL4fE4ixNiPBBm"   // TODO §0.1
#define OEBB_HAFAS_CLIENT_JSON \
  "{\"id\":\"OEBB\",\"type\":\"WEB\",\"name\":\"webapp\",\"l\":\"vs_webapp\"}"
#define OEBB_HAFAS_VER            "1.67"
#define OEBB_JNYFLTR_PRODUCTS     "63"                 // TODO §0.2
#define OEBB_MAX_JNY              6
```

`OEBB_HAFAS_AID`, `OEBB_HAFAS_CLIENT_JSON`, `OEBB_HAFAS_VER`, `OEBB_JNYFLTR_PRODUCTS` werden in Schritt 0 (Pre-Verifikation) aus der laufenden ÖBB-Webapp übernommen und in derselben Datei dokumentiert (`// confirmed against webapp on 2026-MM-DD`).

Entfernt:

```cpp
#define RBL_SUEDTIROLER_LEOPOLDAU 4105
#define RBL_SUEDTIROLER_OBERLAA   4124
#define TOWARDS_U1_LEOPOLDAU     "Leopoldau"
#define TOWARDS_U1_OBERLAA       "Oberlaa"
#define DIVA_SUEDTIROLER_PLATZ    60201349
#define EFA_TOWARDS_U1_LEOPOLDAU "Leopoldau"
#define EFA_TOWARDS_U1_OBERLAA   "Oberlaa"
// LINE_U1 bleibt vorerst stehen — siehe §4.1 Annahme [Schritt 2]
```

---

## 3. Pros / Cons / Out-of-Scope

### 3.1 Pros

- **Konzept-Versprechen aus CONCEPT §v2 wird eingelöst.** S-Bahn statt zwei U1-Streams ist der vorzimmer-praktische Use-Case (zum Bahnhof, nicht in die Stadt mit dem Umweg).
- **Schichtenregel wird nicht verletzt.** HAFAS-Parser landet sauber in `data/`, ein neuer HAL-Methodenstub (`httpPost`) ist die einzige Cross-Layer-Bewegung.
- **Single-source-of-truth-Architektur des main-refactors trägt:** Stream-Topologie-Änderung berührt genau `stream_labels.h`, `filter_builder.cpp`, `config.h`, `snapshot_fetcher.cpp` und `layout.cpp`. Kein Suchen-und-Ersetzen quer durchs Repo.
- **Bestehende Test-Infrastruktur skaliert mit:** Pro-Stream-Tests werden Index-3-Erwartung ergänzt, ein neuer `test_native_oebb_hafas_parse` analog zu `test_native_wienerlinien_parse` mit Fixture-Pattern.
- **`DepartureSource` bleibt protokoll-agnostisch** ([docs/main-refactor-plan.md §2.1, „bewusst protokoll-agnostische Benennung"](main-refactor-plan.md)) — exakt der Punkt, für den der Refactor die Benennung neutral gewählt hat. v2 nutzt dieselben drei Werte ohne Erweiterung.

### 3.2 Cons

- **AID/Client/jnyFltrL-Brüchigkeit.** ÖBB rotiert diese Werte ohne Vorankündigung. Mitigation: Pre-Flash-Verifikation in Schritt 0 + `FilterHealth` für S-Bahn-Stream + sichtbarer Banner `ÖBB-API: Auth ungültig` ([CONCEPT §v2-9](../CONCEPT.md#9-edge-cases-und-fehlerbilder)).
- **Heap-Spitze beim HTTPS-POST** noch nicht profiled. Bestehende Heap-Wächter in [src/logic/schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp) sind auf den EFA-Fall zugeschnitten; HAFAS-Pfad braucht eigene Messung (Schritt 9). Risiko mittel — die Antwortgröße ist 5–8 KB statt 38 KB.
- **RTC-Frame-MAGIC-Bump** verwirft beim Update den letzten gerenderten Frame → erstes Wake nach Update macht Light Full statt Partial. Einmalig, akzeptabel.
- **`Departure::line_label`** wächst die Struktur um 6 B. `StreamData::slot[2]` × 4 Streams = 48 B mehr in RTC-RAM, gut innerhalb der ~780 B Reserve aus [main-refactor Anhang A](main-refactor-plan.md).
- **Layout-Kalibrierung** für Linien-Längen `S2` / `REX1` ist visuell zu prüfen — `test_device_render`-Suite hat heute keinen Pixel-Match, nur Pass/Fail auf API-Pfade. Schritt 7-Validation muss per Mensch erfolgen (Schritt 11).
- **Doku-Sync notwendig in vier Files** (CONCEPT.md, README.md, ARCHITECTURE.md, HANDBUCH.md). Schritt 10 dafür eingeplant.

### 3.3 Ausdrücklich nicht in Scope

- **Keine HAFAS-Hint-Quelle** (Variante 2 aus [CONCEPT §v2-8](../CONCEPT.md#8-plan-hints-analogie-zu-v1-12)). Default Variante 1 — kein Hint-Pfad für S-Bahn. Begründung dort: Stammstrecke fährt 5:00–0:30, das 70-Minuten-Realtime-Fenster reicht praktisch immer. Nachgelagerte PR wenn Bedarf.
- **Kein dritter Slot** für den S-Bahn-Stream (Variante B in CONCEPT §v2-5.2). Nach v2-Roll-out zu beobachten, dann ggf. separate PR.
- **Kein clientseitiger Richtungs-Healthcheck.** `dirLoc` wird vertraut, solange Schritt 0 Punkt 3 (drei Tageszeiten gegengeprüft) das bestätigt. Wenn nicht: Heuristik via `jnyL[i].dirTxt` in Schritt 3 (siehe Open Questions [Anhang C](#anhang-c--open-questions-vor-schritt-0)).
- **Kein `httpPostStream`** (siehe §2.2 Begründung). Erst bei Bedarf nach Schritt 9.
- **Keine Vorbereitung weiterer Stationen** (Tullnerfeld o.ä.). v2 ist exakt eine S-Bahn-Strecke.
- **Kein Bahnsteig-Display.** `dPltfS.txt` wird geparst und liegt ungenutzt im Snapshot herum, um spätere Erweiterung billig zu halten — Renderer ignoriert das Feld.
- **Keine Verspätungsanzeige** als Zahl. `dTimeR` ersetzt `dTimeS` still — derselbe Modus wie bei der OGD-Realtime/Plan-Fallback.

---

## 4. Schritt-für-Schritt-Plan

### 4.0 Vorgehensmodell

Analog [main-refactor-plan §4.0](main-refactor-plan.md). Zusammenhängende Umsetzung in einem Schwung; Auftraggeber wird **am Anfang** (Schritt 0 — AID/Client/jnyFltrL klären) und **am Ende** (visuelle Layout-Inspektion + Roll-out) involviert.

- **Branch**: neuer Branch `v2/sbahn-atzgersdorf` aus `main` abgezweigt (erst nach Abschluss des main-refactors auf `main` gemergt).
- **Commits**: Konvention wie main-refactor — `<Kategorie>: <kurze Beschreibung>`, Kategorien `Data`, `HAL`, `Engine`, `Render`, `Doku`, `Test`, `Tooling`. Kein Co-Authored-By-Trailer.
- **Granularität**: ein Commit pro Schritt-Checkbox (Sub-Steps dürfen gebündelt sein).
- **Push: nein.** Lokal bleiben bis Auftraggeber explizit grünes Licht gibt.
- **Annahmen während der Umsetzung** werden in §4.1 mit Schritt-Referenz festgehalten.

#### Session-Gruppierung

| Group | Schritte | Aufwand | Charakter |
|---|---|---|---|
| A | 0 | ~0.5–1 d | Pre-Verifikation (DevTools, AID/Client/jnyFltrL/dirLoc) |
| B | 1 | ~0.5 d | `INetwork::httpPost` + `Esp32Network::httpPost` |
| C | 2 + 3 | ~1.5–2 d | Datenmodell (`Departure::line_label`, Stream-Enum, RTC-Bumps) + HAFAS-Parser |
| D | 4 + 5 | ~1 d | `filter_builder` + `snapshot_fetcher`-Erweiterung |
| E | 6 + 7 | ~0.5–1 d | `schedule_fetcher`-Skip + Layout Block 3 |
| F | 8 | ~1 d | Tests umstellen (Stream-Indices, Fixtures, neuer Parser-Test) |
| G | 9 | ~0.5 d | Heap-Profiling über Native-Runtime + Device-Smoke |
| H | 10 | ~0.5 d | Doku-Sync (CONCEPT-Markdown-Wechsel „v2 ist gefahren") |
| I | 11 | ~0.5 d | Manuelle Layout-Inspektion + finaler 24h-Soak (Auftraggeber dabei) |

**Total**: ~5.5–7 d, abhängig davon wie viel in Schritt 0 hängen bleibt.

### 4.1 Annahmen während der Umsetzung

Format: `**[Schritt X, Datum]** Annahme: …; Begründung: …`

Wird gefüllt sobald Umsetzung beginnt.

### 4.2 Schritte

#### Schritt 0 — Pre-Flash-Verifikation der HAFAS-Parameter

- [ ] erledigt

Blocker für alle nachfolgenden Schritte. Ergebnis sind drei verifizierte Konstanten, die in `config.h` einziehen.

- **0.1** **AID/Client-Werte abfangen.** DevTools öffnen auf `https://fahrplan.oebb.at/webapp`, eine beliebige Abfahrtsabfrage Atzgersdorf → Wien Hbf machen, im Network-Tab den `mgate.exe`-Request finden, Request-Body als JSON parsen. Felder `auth.aid`, `client.id`, `client.type`, `client.name`, `client.l`, `ver` notieren. Werte als HEREDOC-Block in `docs/v2-sbahn-migration-plan.md` Anhang A einchecken (Beleg für künftige Updates), und in `config.h` als `OEBB_HAFAS_AID`, `OEBB_HAFAS_CLIENT_JSON`, `OEBB_HAFAS_VER` setzen.
- **0.2** **`jnyFltrL`-Bitmask bestimmen.** In derselben DevTools-Session den Produktfilter der Webapp einmal toggeln (S-Bahn aus, S-Bahn an). Vergleichen, welches `jnyFltrL[].value`-Feld wechselt — das ist der Bitvektor für „nur S-Bahn", umgekehrt für die Maske inkl. Regio+REX. Wert in `config.h` als `OEBB_JNYFLTR_PRODUCTS` setzen.
- **0.3** **`dirLoc` an drei Tageszeiten gegenchecken.** Drei mgate-Requests mit `stbLoc.extId = EVA_WIEN_ATZGERSDORF`, `dirLoc.extId = EVA_WIEN_HBF`, `maxJny = 10` zu Hauptverkehr (~7:30), Mittag (~13:00) und Spätabend (~22:00). Antworten aus dem Network-Tab in `.tmp/hafas-fixtures/` ablegen. Pro Antwort: enthält die Liste nur Züge Richtung Hauptbahnhof, oder mischen sich Gegenrichtungs-Departures (Mödling/Wr. Neustadt) durch? Wenn ja → Schritt 3 erweitert um clientseitigen Direction-Healthcheck via `jnyL[i].dirTxt`.
- **0.4** **Cert-Check.** `openssl s_client -connect fahrplan.oebb.at:443 -servername fahrplan.oebb.at < /dev/null 2>/dev/null | openssl x509 -noout -issuer` → Issuer notieren. Wenn nicht in `WiFiClientSecure`-Bundle (Let's Encrypt, DigiCert, ISRG) → in Schritt 1 explizit ein Root-Cert mitgeben statt `setInsecure()`.
- **0.5** **SEV-Behandlung.** Wenn im Fixture-Sweep aus 0.3 ein SEV-Bus auftaucht: notieren, welche `prodL[].cls` oder welches `jnyL[i].prodL[].name`-Pattern ihn erkennbar macht. In Schritt 3 als Hard-Filter implementieren.

**Aufwand**: 0.5–1 d, abhängig davon wie schnell die ÖBB-Webapp im DevTools-Mitschnitt kooperiert.

**Validation**: Drei Antwort-Fixtures unter `.tmp/hafas-fixtures/`, `docs/v2-sbahn-migration-plan.md` Anhang A mit konkreten Werten überschrieben.

**Reversibel**: trivial — Pre-Flash, kein Code-Eingriff.

#### Schritt 1 — `INetwork::httpPost` + `Esp32Network::httpPost`

- [ ] erledigt

- **1.1** [src/hal/INetwork.h](../src/hal/INetwork.h): Methode `httpPost(url, body, content_type, out)` hinzufügen. Default-Implementierung nicht möglich (rein virtuell). Native-Tests müssen `httpPost` ab jetzt mit-mocken — `FakeNet`-Klassen in `test/test_native_runtime/` und in den jeweiligen Test-Buckets ergänzen.
- **1.2** [src/hal/Esp32Network.{h,cpp}](../src/hal/Esp32Network.h): `httpPost`-Impl analog zum bestehenden `httpGet`. Wieder `WiFiClientSecure` mit `setInsecure()` (oder Cert aus Schritt 0.4); `HTTPClient::POST(body)` statt `GET()`. Response-Body wie heute in `out` einsammeln. Content-Type Default `application/json`.
- **1.3** Host-Test `test_native_api_fetcher`: existiert schon für GET → eine zweite Test-Funktion für POST mit `FakeNet`, der canned Response liefert. Verifiziert Retry-Logik (`fetchWithRetry`-Wrapper kommt in Schritt 5 dazu, hier nur die Direct-Call-Variante).

**Aufwand**: 0.5 d.

**Validation**: `make test` grün; `make build` für `env:esp32dev` grün; ein temporärer Smoke-Test gegen die ÖBB-API kann manuell mit `curl --data-binary @body.json https://fahrplan.oebb.at/bin/mgate.exe` cross-validiert werden (Smoke-Test nicht ins Repo).

**Reversibel**: trivial — POST-Methode entfernen, Native-Mocks zurückbauen.

#### Schritt 2 — Datenmodell-Änderungen + RTC-MAGIC-Bumps

- [ ] erledigt

Atomarer Schritt: Stream-Enum + `Departure::line_label` + beide MAGICs zusammen, damit zwischen den Sub-Steps kein inkonsistenter Zustand committet wird.

- **2.1** [src/data/StreamSnapshot.h:11-18](../src/data/StreamSnapshot.h#L11-L18): U1-Werte raus, `STREAM_SBAHN_HBF = 3` rein, `STREAM_COUNT = 4`. Kein Aliasing der alten Werte (würde Test-Drift maskieren).
- **2.2** [src/data/Departure.h](../src/data/Departure.h): `char line_label[6] = ""` ergänzen; `operator==` um `strcmp(line_label, …) == 0` erweitern. `<cstring>`-Include hinzu.
- **2.3** [src/data/stream_labels.h](../src/data/stream_labels.h): die zwei `U1-…`-cases entfernen, ein neuer case `STREAM_SBAHN_HBF: return "SBahn-Hbf";`.
- **2.4** [src/config.h:13-14, 39-40, 47, 55-56](../src/config.h#L13): U1-bezogene Konstanten entfernen. `LINE_U1` bleibt **vorerst** stehen — wenn nach Schritt 8 keine Test-Fixture mehr darauf referenziert, in einem Cleanup-Commit mit-entfernen. Annahme [Schritt 2]: `LINE_U1`-Cleanup wird *nicht* in derselben Diff committet, weil das die Fokussierung auf Stream-Topologie verwässert.
- **2.5** Neue Konstanten aus [§2.4](#24-konfiguration) in `config.h` einsetzen. Werte aus Schritt 0.1/0.2.
- **2.6** [src/hal/Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp): sowohl `MAGIC` (RLE-Frame) als auch `SCHED_MAGIC` bumpen. Begründung MAGIC: `STREAM_COUNT`-Änderung verändert `StreamData::slot[]`-Layout *nicht*, aber `Departure::line_label` schon — und der RLE-komprimierte Frame entspricht nicht mehr dem neuen Layout-Block 3. Begründung SCHED_MAGIC: `ScheduleHint` ist strukturell identisch, aber Index 3/4 enthielt vorher U1-Schedules; ein Reuse würde im S-Bahn-Stream falsche Hint-Werte injizieren.

**Aufwand**: 0.5 d.

**Validation**: `make ci` grün (vermutlich rot — viele Tests erwarten `STREAM_COUNT == 5`). Die Roten sind in Schritt 8 zu fixen; vor Schritt 3 muss `make build` für `env:esp32dev` und der reine Compile-Pfad von `env:native` grün sein. Wenn Touch-Sites in `test/`-Files identifiziert werden, in Schritt 8 abarbeiten.

**Reversibel**: ja, aber MAGIC-Bumps haben Hardware-Konsequenzen (gespeicherte Frames + Hints werden verworfen). Wenn rollback, dann zusätzlich `MAGIC` nochmal bumpen, damit das neuere Layout nicht von der alten Firmware fehlinterpretiert wird.

#### Schritt 3 — `data/oebb_hafas_parse.{h,cpp}` + Tests

- [ ] erledigt

- **3.1** [src/data/oebb_hafas_parse.h](../src/data/oebb_hafas_parse.h) anlegen mit Signaturen aus [§2.2](#22-neue-module-im-detail) (`OebbStreamFilter`, `buildOebbRequest`, `parseOebbStationBoard`).
- **3.2** [src/data/oebb_hafas_parse.cpp](../src/data/oebb_hafas_parse.cpp): `buildOebbRequest` als String-Bau mit `ArduinoJson::serializeJson` über einen `JsonDocument`. Body-Schema aus [Anhang A](#anhang-a--hafas-request-response-vertrag).
- **3.3** `parseOebbStationBoard`: `deserializeJson` mit `NestingLimit(20)` (HAFAS verschachtelt `prodL`/`opL`/`himL` tief). Reihenfolge:
  1. `doc["err"]` lesen — wenn nicht `"OK"`, `endpoint_responded = false`, return true (parsing erfolgreich, aber API-Fehler).
  2. `doc["svcResL"][0]["res"]["jnyL"]` als Array — wenn null → `endpoint_responded = true`, `filter_matched = false`, return true.
  3. Pro `jny`: `jny["stbStop"]["dCncl"]` → Cancelled skippen; `jny["stbStop"]["dTimeS"]` + `["dDateS"]` lesen, optional `dTimeR` + `dDateR`. Konvertieren via Howard-Hinnant + `TZ_INFO` (analog [src/data/wienerlinien_parse.cpp:32-79](../src/data/wienerlinien_parse.cpp#L32)).
  4. `jny["prodL"][0]` als Index in `doc["svcResL"][0]["res"]["common"]["prodL"]`, daraus `name` (oder `nameS`) lesen → `line_label` (mit `strncpy`, null-terminieren, abkürzen zu `"xx"` wenn länger als 5).
  5. SEV-Filter (wenn aus Schritt 0.5 nötig): Slot überspringen wenn `prodL[…].cls` SEV-class entspricht.
  6. Slot füllen, bis `SLOTS_PER_STREAM` erreicht.
  7. `endpoint_responded = true`, `filter_matched = (matched_count > 0)`.
- **3.4** Fixtures: `.tmp/hafas-fixtures/*.json` aus Schritt 0.3 nach `test/test_native_oebb_hafas_parse/fixtures/oebb_live_{morning,noon,evening}.h` konvertieren (raw-string-literal-Embed analog zu [test/test_native_wienerlinien_parse/](../test/test_native_wienerlinien_parse/)).
- **3.5** Tests in `test/test_native_oebb_hafas_parse/test_main.cpp`:
  - `test_buildRequest_includes_aid_and_client`
  - `test_buildRequest_includes_stbLoc_dirLoc`
  - `test_buildRequest_includes_products_filter`
  - `test_parse_morning_fixture_three_slots_realtime`
  - `test_parse_noon_fixture_line_labels_S2_S3`
  - `test_parse_evening_fixture_includes_REX`
  - `test_parse_cancelled_skipped`
  - `test_parse_err_not_ok_sets_endpoint_not_responded`
  - `test_parse_empty_jnyL_sets_filter_unmatched`
  - `test_parse_long_line_label_abbreviates_xx`

**Aufwand**: 1–1.5 d (Parser plus Tests).

**Validation**: `make test` (Native) grün — neuer Test-Bucket ohne Device-Abhängigkeit, läuft im `env:native`.

**Reversibel**: ja, isoliert.

#### Schritt 4 — `filter_builder.{h,cpp}` umstellen + S-Bahn-Filter-Getter

- [ ] erledigt

- **4.1** [src/logic/filter_builder.cpp](../src/logic/filter_builder.cpp): U1-Zeilen in `buildStreamFilters` und `buildScheduleFilters` entfernen. `f[STREAM_SBAHN_HBF]` bleibt default-konstruiert (`rbl = 0` bzw. `diva = 0`).
- **4.2** [src/logic/filter_builder.h](../src/logic/filter_builder.h): `OebbStreamFilter buildOebbFilter();` hinzufügen.
- **4.3** Implementierung in `filter_builder.cpp`:

  ```cpp
  OebbStreamFilter buildOebbFilter() {
    OebbStreamFilter f;
    f.stb_eva = EVA_WIEN_ATZGERSDORF;
    f.dir_eva = EVA_WIEN_HBF;
    f.products = OEBB_JNYFLTR_PRODUCTS;
    f.max_jny = OEBB_MAX_JNY;
    return f;
  }
  ```

- **4.4** Test `test_native_filter_builder` erweitern:
  - alte U1-Erwartungen entfernen
  - Assert: `f[STREAM_SBAHN_HBF].rbl == 0` für OGD-Filter (default)
  - Assert: `f[STREAM_SBAHN_HBF].diva == 0` für Schedule-Filter (default)
  - Neuer Test: `buildOebbFilter()` liefert die ÖBB-Konstanten

**Aufwand**: 0.5 d.

**Validation**: `make test` grün (zumindest dieser Bucket).

**Reversibel**: ja.

#### Schritt 5 — `snapshot_fetcher.cpp` um `fetchOebbStream` erweitern

- [ ] erledigt

- **5.1** [src/logic/snapshot_fetcher.cpp:16-19](../src/logic/snapshot_fetcher.cpp#L16-L19): `FETCH_ORDER` reduzieren auf die 3 OGD-Streams. Index `STREAM_SBAHN_HBF = 3` taucht hier nicht auf.
- **5.2** Neue interne Funktion `fetchOebbStream(net, mgate_url, filter, out, summary)`:
  - `buildOebbRequest(filter)` → POST-Body
  - `fetchWithRetry`-ähnlicher Wrapper mit POST (entweder `api_fetcher` um eine `fetchPostWithRetry`-Variante erweitern, oder Inline-3-Retry-Schleife im Fetcher — vorgezogen: Erweiterung in `api_fetcher`, damit beide Pfade die Retry-Policy teilen).
  - Erfolgreiche Antwort → `parseOebbStationBoard(body, out.stream[STREAM_SBAHN_HBF])`.
  - `summary.total_batches++`; bei Fail `summary.failed_batches++`.
  - Logging-Konvention analog OGD-Batch: `[api] oebb httpPost failed after %d attempts`, `[api] oebb succeeded on attempt %d/%d`.
- **5.3** Public `fetchSnapshot` ruft nach der OGD-Schleife einmal `fetchOebbStream(net, oebb_mgate_url, …)`. Signatur erweitern: `fetchSnapshot(INetwork&, const std::string &ogd_base, const std::string &mgate_url, const StreamFilter (&)[STREAM_COUNT], const OebbStreamFilter &oebb_filter, StreamSnapshot &out, FetchSummary &summary)`.
- **5.4** Caller `cycle_runner` anpassen — `CycleConfig` braucht ein neues Feld `std::string mgate_url`, das in [main.cpp::makeCycleConfig](../src/main.cpp#L31) auf `OEBB_MGATE_URL` gesetzt wird.
- **5.5** Test `test_native_snapshot_fetcher` erweitern: `FakeNet` mit zwei kanonischen Antworten (OGD-JSON für die Bus-Streams, HAFAS-JSON für den S-Bahn-Stream). Assertion: nach `fetchSnapshot` ist `out.stream[0..2]` aus OGD befüllt, `out.stream[3]` aus HAFAS, `summary.total_batches == 2` (1 OGD-Batch bei 3 Streams + 1 OEBB-Call).
- **5.6** [src/logic/api_fetcher.{h,cpp}](../src/logic/api_fetcher.h): `fetchPostWithRetry(INetwork &, url, body, content_type, out, FetchConfig)` als zweite Funktion. `fetchWithRetry` (GET) bleibt.

**Aufwand**: 0.5–1 d.

**Validation**: `make test` grün; `make build` für `env:esp32dev` grün; `test_device_fetch` muss erweitert werden, um die zwei-Endpunkt-Welt zu testen (kommt in Schritt 8).

**Reversibel**: ja, `fetchOebbStream` ist ein zusätzlicher Aufruf am Ende; entfernen → Stream 3 bleibt leer, Display zeigt `--:--`.

#### Schritt 6 — `schedule_fetcher` skippt den S-Bahn-Stream

- [ ] erledigt

Trivial — `schedule_fetcher::fetchSchedule` iteriert heute schon über distinct DIVAs aus `filters[]`. Mit `filters[STREAM_SBAHN_HBF].diva == 0` (Default aus Schritt 4) wird kein Call für diesen Stream gemacht.

- **6.1** Bestätigen durch Lesen: [src/logic/schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp) — der DIVA-Distinct-Loop muss `diva == 0` als „skip" behandeln. Wenn nicht (heute wird das geprüft, aber double-check): Guard hinzufügen.
- **6.2** Test `test_native_schedule_fetcher` erweitern: Filter mit `diva == 0` an Index 3 — Erwartung: `result.calls_attempted == 1` (nur einer von beiden DIVAs aus den Bus-Streams, da Tullnertalgasse 58A→Atz und 58A→Hie sich die DIVA teilen, bleibt es bei 2 distinct DIVAs für Tullnertalgasse + Endemann).

**Aufwand**: 0.25 d.

**Validation**: `make test` grün.

#### Schritt 7 — `render/layout.cpp` Block 3 neu

- [ ] erledigt

- **7.1** [src/render/layout.cpp](../src/render/layout.cpp): neue Funktion `drawSBahnSlot(c, x, y, d, stale)` aus [§2.2](#22-neue-module-im-detail). `static`-intern im anonymen Namespace.
- **7.2** `renderFrame`: alten Block 3 (`drawHeader "SUEDTIROLER PLATZ"` + zwei `drawStreamLine` U1) ersetzen durch:
  - `drawHeader(c, 196, "ATZGERSDORF S-BAHN");`
  - `drawText(c, 8, 224, 1, "-> Hauptbahnhof");`
  - `drawSBahnSlot(c, 8, 246, sbahn.slot[0], stale);`
  - `drawSBahnSlot(c, 200, 246, sbahn.slot[1], stale);`
- **7.3** Visuelle Kalibrierung der x-Offsets pro Slot. Linie + 56 px Lücke + HH:MM → maximal 200 px pro Slot bei `text size = 2`. `S2` (24 px) bis `REX1` (48 px) müssen in die Linien-Spalte passen, ohne dass HH:MM überlappt.
- **7.4** Edge-Case: leerer `line_label` (Bus-Stream-Schiebung; sollte nicht passieren, aber defense-in-depth) — Linien-Spalte überspringen, HH:MM steht alleine.
- **7.5** Edge-Case: `stale == true` → keine Linie zeichnen (sonst stünde sie über einem `??:??`, was den User-Eindruck „kaputt" verwässert).

**Aufwand**: 0.5 d (Code) + visuelle Inspektion in Schritt 11.

**Validation**: `pio run -e device-render -t test` grün — die `test_device_render`-Suite muss um den S-Bahn-Block ergänzt werden (Schritt 8).

**Reversibel**: ja.

#### Schritt 8 — Tests umstellen

- [ ] erledigt

Sammelschritt für alle Test-Touch-Sites, die durch STREAM_COUNT-Änderung und Layout-Wechsel rot wurden.

- **8.1** `test_native_*`-Buckets, die `STREAM_COUNT == 5` annehmen:
  - `test_native_filter_builder` ([Schritt 4](#schritt-4--filter_builderhcpp-umstellen--s-bahn-filter-getter))
  - `test_native_filter_health` — Stream-Index 3 testet jetzt S-Bahn-Verhalten, nicht U1
  - `test_native_slot_merger` — Hint-Tests an Index 3/4 entfallen (S-Bahn hat keinen Hint, Variante 1)
  - `test_native_wienerlinien_parse` — Fixtures, die U1-RBLs erwartet hatten: U1-Section in Fixture entfernen, Assertion `endpoint_responded[3..4]` weg
  - `test_native_efa_parse` — Fixtures, die DIVA `60201349` (Südtirolerplatz) referenzieren: entfernen
  - `test_native_schedule_fetcher` — Erwartung: 2 statt 3 distinct DIVA-Calls
  - `test_native_schedule_refresh` — Stream-Index 3/4 entfällt
  - `test_native_snapshot_fetcher` — siehe Schritt 5.5
  - `test_native_snapshot_logger` — Summary-Format hat jetzt 4 Stream-Zeilen statt 5
  - `test_native_render_input` — Index 3 ist S-Bahn (line_label gesetzt)
  - `test_native_cycle_runner_*` — alle drei Buckets: cold + warm + invariants — Fakes liefern jetzt OGD-Batch + OEBB-Call, Recording-Traces erwarten beide
- **8.2** `test_device_*`-Buckets:
  - `test_device_fetch`: zweite kanonische Antwort (HAFAS-Fixture) + Assertion auf `stream[3].slot[0..1].valid`
  - `test_device_schedule`: U1-Erwartung raus, ÖBB hat keinen Schedule-Call → Erwartung „2 calls instead of 3"
  - `test_device_render`: Block-3-Pixel-Asserts aktualisieren (Position der Header-Zeile, Position der zwei S-Bahn-Slots)
- **8.3** `test_longterm_*`-Buckets:
  - `test_longterm_smoke`: Erwartung „alle 5 Streams" → „alle 4 Streams"; Stream 3 ist jetzt S-Bahn, validiert über `line_label != ""` für mindestens einen Slot
  - `test_longterm_horizon_evening`: Hint-Bridge gilt nur für Bus-Streams (3 statt 5)
  - `test_longterm_horizon_scan`: Cliff-Test gilt für Bus-Streams; S-Bahn-Stream hat keinen Hint, fällt nach 70 min auf `--:--` (nicht auf `next_today`)
  - `test_longterm_day_full`, `test_longterm_jitter`, `test_longterm_wake_cycle`: STREAM_COUNT-Touch-Sites
- **8.4** Fixture-Update: alle `wl_live.h`-Fixtures, die U1-Daten enthielten, müssen mit-gezogen werden (das ist viel `git diff`-Lärm aber keine echte Logik).

**Aufwand**: ~1 d. Mechanisch, aber viel.

**Validation**: `make ci` grün (alle Buckets); `make test-device` grün; `make test-longterm-smoke` grün; `make test-longterm-soak-15min` als Baseline + Diff gegen pre-v2-Baseline (Drift erwartet — andere Streams, andere Logs).

**Reversibel**: theoretisch — aber praktisch ist das Test-Update sehr verflochten mit Schritt 2–7. Rollback würde alle Schritte rollback bedeuten.

#### Schritt 9 — Heap-Profiling

- [ ] erledigt

- **9.1** Native-Runtime ([test/test_native_runtime/](../test/test_native_runtime/)) um den OEBB-Pfad erweitern: `HttpsNet` muss `httpPost` implementieren (libcurl `CURLOPT_POSTFIELDS`).
- **9.2** `make native-runtime-smoke` mit valgrind/massif laufen lassen — `fetchSnapshot` mit beiden Endpunkten, 50 Cycles. Erwartung: kein Leak über Cycle-Grenzen, Peak-Heap < pre-v2-Peak + 8 KB (HAFAS-Antwort).
- **9.3** Auf Device: `test_device_fetch` mit `Serial.printf("free heap before/after oebb call: %u/%u\n", …)` instrumentieren. Wenn Spitze die EFA-Heap-Wächter triggern würde (`< 90 KB free`), `httpPostStream` einplanen — als Follow-up-PR, nicht im selben Branch.

**Aufwand**: 0.5 d.

**Validation**: massif-Report unter `.tmp/native-runtime/massif-v2.out`; Device-Log mit Heap-Werten.

**Reversibel**: nur Diagnostik, kein Code-Refactor.

#### Schritt 10 — Doku-Sync

- [ ] erledigt

- **10.1** [CONCEPT.md §v2-11](../CONCEPT.md#11-migrationsschritte-zur-späteren-umsetzung-nicht-teil-dieses-konzepts): Header von „Migrationsschritte (zur späteren Umsetzung, nicht Teil dieses Konzepts)" auf „Migrationsschritte (umgesetzt, siehe [docs/v2-sbahn-migration-plan.md](docs/v2-sbahn-migration-plan.md))" ändern. Liste der elf Sub-Steps abhaken oder durch Verweis auf diesen Plan ersetzen.
- **10.2** [README.md](../README.md) Zeile 4: „nächste Abfahrten der Wiener-Linien-Buslinien 58A, 58B" um „und der ÖBB-S-Bahn Atzgersdorf → Wien Hbf" ergänzen. Display-ASCII-Block (Zeile 10-18) um den S-Bahn-Block erweitern. Banner-Liste (Zeile 23) ergänzen um `ÖBB-API: Auth ungültig`.
- **10.3** [docs/ARCHITECTURE.md](ARCHITECTURE.md) Modulkarte: `oebb_hafas_parse.{h,cpp}` in `data/` eintragen. „Wo welche Konstante wirkt"-Tabelle ergänzen (`OEBB_*`-Konstanten). Speicher-Layout: `Departure::line_label` als zusätzliche 6 B pro Slot vermerken.
- **10.4** [docs/HANDBUCH.md](HANDBUCH.md): Block-3-Beschreibung aktualisieren. Screenshot in `docs/screenshots/` neu machen (nach Schritt 11 manueller Verifikation).
- **10.5** [docs/USER.md](USER.md): „Was bedeuten die Anzeigen" um S-Bahn-Slot-Beschreibung + `ÖBB-API: Auth ungültig`-Banner ergänzen.
- **10.6** [docs/HARDWARE.md](HARDWARE.md): unverändert (keine Hardware-Änderung).
- **10.7** [docs/TESTING.md](TESTING.md): neuer Test-Bucket `test_native_oebb_hafas_parse` in die Bucket-Übersicht.
- **10.8** [CHANGELOG.md](../CHANGELOG.md): Eintrag für Release 2.0 (v2 ist eine Major-Änderung wegen RTC-MAGIC-Bump = Reset-on-Update, sichtbare Layout-Änderung).
- **10.9** Markdown-Lint über alle geänderten `.md`-Files mit `markdownlint-cli2 --fix`.

**Aufwand**: 0.5 d.

**Validation**: `markdownlint-cli2` ohne Fehler; visuelle Sichtkontrolle der Screenshots.

#### Schritt 11 — Manuelle HW-Verifikation am Ende

- [ ] erledigt

Mit Auftraggeber am Gerät:

- **11.1** Flash `make flash` auf ESP32.
- **11.2** Cold-Boot beobachten: Display zeigt `--:--` auf S-Bahn-Slot solange HAFAS noch nicht geantwortet hat, danach `S2 HH:MM   S3 HH:MM`.
- **11.3** Bus-Streams unverändert.
- **11.4** Layout-Sichtkontrolle: Linien-Spalte breit genug für `REX1` ohne HH:MM-Überlappung? Wenn nicht: x-Offsets in `layout.cpp::drawSBahnSlot` nachjustieren, neue PR.
- **11.5** Stale-Test: WiFi-AP aus → nach 3 min Banner `VERALTET`. S-Bahn-Slot zeigt `??:??`, Linien-Label ist ausgeblendet (Schritt 7.5).
- **11.6** Auth-Drift-Test: `OEBB_HAFAS_AID` in `config.h` temporär auf `"INVALID"` setzen, neu flashen. Erwartung: nach 3 erfolglosen Calls Banner `ÖBB-API: Auth ungültig`.
- **11.7** Optional 24h-Soak (`make test-longterm-day-full`) über Nacht — Auftraggeber-Entscheidung ob nötig.

**Aufwand**: 0.5 d + ggf. 24h Warten.

**Validation**: alle vier Beobachtungen passieren wie beschrieben; Screenshot für [docs/HANDBUCH.md](HANDBUCH.md) entstanden.

---

## 5. Risiken

| ID | Risiko | Wahrscheinlichkeit | Auswirkung | Mitigation |
|---|---|---|---|---|
| V1 | AID/Client rotiert vor Release | mittel | Banner „Auth ungültig", manuelle `config.h`-Aktualisierung + Re-Flash | Schritt 0.1 dokumentiert die Werte mit Datum; FilterHealth fängt Drift; Banner ist Auftrag, AID zu aktualisieren — kein stilles Versagen |
| V2 | `dirLoc`-Filter lässt Gegenrichtung durch | gering (Webapp-Verhalten in DevTools sauber) | falsche Züge im Display | Schritt 0.3 sweep über drei Tageszeiten; wenn auch nur einmal Gegenrichtung sichtbar → clientseitiger Direction-Check in Schritt 3 |
| V3 | SEV-Bus erscheint im S-Bahn-Block (Bauarbeiten) | mittel saisonal | irreführende Anzeige (Bus statt Zug) | Schritt 0.5 + Hard-Filter via `prodL[…].cls` in Schritt 3; wenn 0.5 keine SEV-Phase erwischt: Verhalten erst im Realbetrieb beobachtbar, dann Fast-Follow-PR |
| V4 | HAFAS-Antwort-Heap-Spitze > Reserve | gering (5–8 KB Antwort) | OOM-Crash auf ESP32 beim ersten OEBB-Call | Schritt 9 misst; wenn Spitze gefährlich: `httpPostStream` als Follow-up-PR (Streaming-Pendant zu `httpGetStream`) |
| V5 | Layout-Bruch durch lange Liniennamen (`Nightjet 123`) | mittel | überlappende Glyphen, unleserlich | Schritt 3.3: längere als 5 Zeichen werden zu `"xx"` abgekürzt; sichtbare Anomalie, kein Layout-Schaden |
| V6 | RTC-Reserve nach `line_label` zu knapp für künftige Erweiterungen | gering | künftige PRs müssten RLE-Hardcap senken | Bilanz in [Anhang B](#anhang-b--rtc-bilanz-nach-v2): 732 B Reserve nach v2 |
| V7 | TLS-Cert für `fahrplan.oebb.at` nicht im Bundle | gering | `httpPost` schlägt mit Cert-Error fehl | Schritt 0.4: Issuer prüfen, ggf. Cert in `Esp32Network::httpPost` mitgeben |
| V8 | `STREAM_COUNT == 5`-Annahmen in Tests übersehen | mittel | Compile- oder Runtime-Fail im Test-Bucket | Schritt 8 ist Sammelschritt; `make ci` enforced (alle Buckets) |
| V9 | Bestehende Hint-Tests (`test_longterm_horizon_evening`) brechen, weil Index 3 jetzt S-Bahn ist | hoch | Test rot bis Anpassung | Schritt 8.3 stellt explizit um |
| V10 | `Departure::line_label` interagiert subtil mit RLE-Encoding (zusätzliche Bytes pro Slot werden komprimiert?) | gering | RLE-Hardcap-Verletzung bei worst-case-Frames | Schritt 9 misst; line_label ist meist leer (Bus-Slots) oder kurzer ASCII, kompressionsfreundlich |

---

## 6. Tests

Tabellarisches Inventar der neuen und geänderten Test-Pairs:

| Bucket | Status | Touch |
|---|---|---|
| `test_native_oebb_hafas_parse` | **neu** | Parser-Tests aus Schritt 3.5 |
| `test_native_filter_builder` | geändert | + `buildOebbFilter`-Test, U1-Erwartungen weg |
| `test_native_filter_health` | geändert | Index 3 ist S-Bahn |
| `test_native_slot_merger` | geändert | keine Hint-Tests für Index 3 (S-Bahn ohne Hint) |
| `test_native_wienerlinien_parse` | geändert | Fixtures: U1-Section raus |
| `test_native_efa_parse` | geändert | Fixtures: DIVA Südtirolerplatz raus |
| `test_native_schedule_fetcher` | geändert | 2 statt 3 distinct DIVAs |
| `test_native_schedule_refresh` | geändert | STREAM_COUNT |
| `test_native_snapshot_fetcher` | geändert | + OEBB-Call mit FakeNet-Antwort |
| `test_native_snapshot_logger` | geändert | 4 statt 5 Stream-Zeilen |
| `test_native_render_input` | geändert | `line_label`-Assertion auf S-Bahn-Index |
| `test_native_cycle_runner_warm` | geändert | Recording-Trace erwartet beide Endpunkte |
| `test_native_cycle_runner_cold` | geändert | dito |
| `test_native_cycle_runner_invariants` | geändert | dito |
| `test_native_api_fetcher` | geändert | + `fetchPostWithRetry`-Tests |
| `test_native_irenderer` | unverändert | Frame ist `STREAM_COUNT`-agnostisch (template) |
| `test_native_runtime_renderer` | unverändert | dito |
| `test_native_runtime_diskstore` | geändert | RTC-MAGIC-Bumps reflektieren |
| `test_device_fetch` | geändert | + HAFAS-Pfad |
| `test_device_schedule` | geändert | 2 statt 3 DIVA-Calls |
| `test_device_render` | geändert | Block-3-Pixel-Asserts |
| `test_device_persistent` | geändert | RTC-MAGIC + `Departure::line_label` |
| `test_device_sleep` | unverändert | Sleep ist Stream-agnostisch |
| `test_longterm_*` | alle geändert | STREAM_COUNT-Touch-Sites |

Coverage-Ziel bleibt: `logic/` + `data/` ≥ 90 %; neue `oebb_hafas_parse.cpp` braucht eigene Coverage-Messung in Schritt 3.

---

## Anhang A — HAFAS-Request-/Response-Vertrag

Format wird in Schritt 0.1 finalisiert. Hier der Vor-Verifikations-Stand aus [CONCEPT §v2-4](../CONCEPT.md#4-request-schema-primär-mgateexe):

**Request-Body** (POST `https://fahrplan.oebb.at/bin/mgate.exe`):

```json
{
  "id": "bustaferl",
  "ver": "1.67",
  "lang": "deu",
  "auth": { "type": "AID", "aid": "OWDL4fE4ixNiPBBm" },
  "client": { "id": "OEBB", "type": "WEB", "name": "webapp", "l": "vs_webapp" },
  "formatted": false,
  "svcReqL": [{
    "meth": "StationBoard",
    "req": {
      "type": "DEP",
      "stbLoc": { "type": "S", "extId": "8100634" },
      "dirLoc": { "type": "S", "extId": "8100002" },
      "maxJny": 6,
      "jnyFltrL": [{ "type": "PROD", "mode": "INC", "value": "63" }]
    }
  }]
}
```

**Response-Felder, die geparst werden**:

- `err` — `"OK"` erwartet, sonst `endpoint_responded = false`
- `svcResL[0].res.jnyL[i]` — Liste der Departures
  - `.stbStop.dCncl` — Cancelled
  - `.stbStop.dDateS` (`YYYYMMDD`) + `.stbStop.dTimeS` (`HHMMSS`) — Plan-Abfahrt
  - `.stbStop.dDateR` + `.stbStop.dTimeR` — Echtzeit-Abfahrt (optional)
  - `.prodL[0]` — Index in `svcResL[0].res.common.prodL[]`
- `svcResL[0].res.common.prodL[i].name` (oder `nameS`) — Linienkennung wie `"S2"`, `"REX1"`

`Content-Type: application/json; charset=UTF-8`. Header `User-Agent` wird vom `HTTPClient` automatisch gesetzt; HAFAS akzeptiert es ohne Anpassung.

---

## Anhang B — RTC-Bilanz nach v2

Ausgehend von der Bilanz nach main-refactor Schritt 2.3 ([main-refactor-plan Anhang A](main-refactor-plan.md)):

| Posten | Bytes |
|---|---|
| **Stand nach main-refactor** | 7412 / 8192 (~780 B Reserve) |
| – `g_sched.hint[3..4]` entfällt (2 × `sizeof(ScheduleHint) = 40 B`) | −80 |
| – `g_meta.filter_miss_streak[3..4]` entfällt (2 × 1 B falls so groß; konservativ 4 B) | −4 |
| + `Departure::line_label[6]` in `g_rle` (worst-case-Frame: ~bei voll besetzten Slots zusätzliche Bytes vor RLE — sehr klein dank Lauflängenkompression) | +0 (Annahme nach Schritt 9 zu verifizieren) |
| **Stand nach v2** | ~7328 / 8192 (~864 B Reserve) |

Reserve bleibt komfortabel. Genauere Bilanz wird in Schritt 9 (Heap-Profiling) gemessen.

---

## Anhang C — Open Questions vor Schritt 0

| # | Frage | Antwort-Schritt |
|---|---|---|
| C1 | Aktuelle AID/Client/ver-Werte der ÖBB-Webapp? | 0.1 |
| C2 | Bitmask-Wert von `jnyFltrL` für S-Bahn+Regio+REX? | 0.2 |
| C3 | Reicht `dirLoc` als Gegenrichtungs-Filter (Hauptverkehr/Mittag/Spätabend)? | 0.3 |
| C4 | TLS-Cert-Issuer von `fahrplan.oebb.at` im `WiFiClientSecure`-Bundle? | 0.4 |
| C5 | SEV-Erkennungs-Merkmal in HAFAS-Antwort? | 0.5 |
| C6 | Variante 1 (kein Hint) oder Variante 2 (HAFAS-Hint-Call) für S-Bahn? | bleibt bei Variante 1 (Default aus CONCEPT §v2-8); Re-Eval nach v2-Roll-out wenn Bedarf |
| C7 | 2 oder 3 Slots für den S-Bahn-Stream? | bleibt bei 2 (Variante A aus CONCEPT §v2-5.2); Re-Eval nach v2-Roll-out |
| C8 | Heap-Verhalten des HAFAS-Calls? | misst Schritt 9 |
