# `main.cpp` – Refactor-Plan

Stand: 2026-05-18 (Rev. 7, Umsetzungs-Vorgehensmodell + Schritt 12 + Sub-Step-Checkboxen) · Arbeitsstand vor Code-Eingriff.

## 1. Status quo (Findings)

### 1.1 Größe und Rolle

| Datei                              | LOC | Soll-Rolle laut [`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) |
|---|---|---|
| [src/main.cpp](../src/main.cpp)    | **578** | „Top-level setup()/loop(), Verdrahtung" |
| nächstgrößte `*.cpp` (`Esp32Network`) | 274 | HAL-Implementierung |
| nächstgrößte `logic/*.cpp` (`schedule_fetcher`) | 179 | reine Logik |

`main.cpp` ist 2× so groß wie das nächstgrößte File und enthält nicht nur Verdrahtung, sondern auch Logik, Logging-Format, Batch-Orchestrierung und Button-Treiberzeug. Das widerspricht der eigenen Schichtenregel der Architektur.

### 1.2 Was im anonymen Namespace von `main.cpp` lebt

| Funktion / Symbol            | LOC ~ | Art                          | Wo hingehört                                       | Heute testbar? |
|------------------------------|-------|------------------------------|----------------------------------------------------|----------------|
| `g_clock`,`g_net`,`g_sleep`,`g_store`,`g_display` | – | HAL-Singletons          | bleibt in `main.cpp` (Verdrahtung)                | n/a            |
| `g_frame_new`,`g_frame_prev` | –     | je 15 kB Framebuffer (BSS)   | bleibt in `main.cpp` (BSS, nicht Stack — §1.4)    | n/a            |
| `buildFilters`               | 6     | reine Daten-Map              | → `logic/filter_builder.{h,cpp}`                  | nein           |
| `buildScheduleFilters`       | 8     | reine Daten-Map              | → `logic/filter_builder.{h,cpp}`                  | nein           |
| `STOPIDS_PER_QUERY`, `FETCH_ORDER` | 4 | Konstanten                | → `logic/snapshot_fetcher.{h,cpp}`                | n/a            |
| `apiUrlForBatch`             | 9     | Pure-Function String-Bau     | → `logic/snapshot_fetcher.cpp` (intern)           | nein           |
| `logSlot`                    | 11    | Serial-Logging               | → `logic/snapshot_logger.{h,cpp}`                  | nein           |
| `fetchSnapshot`              | **88**| Batch-Schleife + Parse + Log | → `logic/snapshot_fetcher.{h,cpp}`                | nein           |
| `registerWifiCredentials`    | 6     | HAL-Wiring                   | bleibt in `main.cpp`                              | n/a            |
| `refreshSchedule`            | 23    | Logik (Merge-Regel im Tail)  | → `logic/schedule_refresh.{h,cpp}`                | nein           |
| `needScheduleRefresh`        | 22    | **pure** Logik               | → `logic/schedule_refresh.{h,cpp}`                | nein (heute)   |
| `renderAndPush`              | 26    | Render+Diff+Persist          | → `logic/cycle_runner.cpp` (private Helper)       | nein           |
| `doSleepOrLoop`              | 12    | HAL-Glue (Sleep+Save)         | → `logic/cycle_runner.cpp`                        | nein           |
| `coldBootPath`               | 56    | Orchestrierung               | → `logic/cycle_runner.cpp` (`runColdCycle`)       | nein           |
| `warmCyclePath`              | 111   | Orchestrierung               | → `logic/cycle_runner.cpp` (`runWarmCycle`)       | nein           |
| `measureButtonPress`         | 18    | Arduino-GPIO + Klassifikation| → `logic/button_classifier.{h,cpp}` + Adapter     | teilweise       |
| `runBwReset`                 | 21    | Display-Pfad                 | → `logic/cycle_runner.cpp`                        | nein           |
| `handleButtonIfPressed`      | 14    | Arduino-GPIO + Dispatch       | → `main.cpp` (bleibt, dünner)                     | nein           |
| `setup` / `loop`             | 28+3  | Dispatch                      | bleibt in `main.cpp`                              | nein           |

Damit landen ~370 von 578 Zeilen aus `main.cpp` heute außerhalb der Schichtenregel.

### 1.3 Konkrete Code-Smells

1. **Duplikation `buildFilters` / `buildScheduleFilters`** – wortgleich auch in [test/test_device_fetch/test_main.cpp](../test/test_device_fetch/test_main.cpp) Z. 59–66. Bei Topologie-Änderung (z. B. v2 ÖBB-Stream, [CONCEPT.md §v2-5.1](../CONCEPT.md)) drei Stellen synchron zu pflegen.

2. **`fetchSnapshot` mischt vier Sorgen** – Batch-Schleife · Retry-Aufruf · Parse · 30-Zeilen-Serial-Summary fest verdrahtet auf alle 5 Streams (Z. 165–188). Das letzte Stück muss bei jeder Topologie-Änderung von Hand angefasst werden.

3. **`coldBootPath` / `warmCyclePath` haben ~40 % gemeinsamen Code** – beide: fetchSnapshot · refreshSchedule (falls fällig) · mergeSlots · renderFrame · planSleep. Reine Copy-Variation, kein gemeinsamer Pfad.

4. **NTP-„clock < 1.7e9"-Heuristik fünfmal im Repo** – `main.cpp:368`, `test_device_fetch:164/204/258`, `Esp32Clock.cpp:29`. Effektiver `clock.isSynced()`-Check, gehört in `IClock`.

5. **Magic-Numbers** – `4 * 3600` Schwelle „langer Schlaf" in `warmCyclePath` Z. 439, `20 * 3600` Deep-Clean-Intervall Z. 440. Beide tragen Semantik, sind aber nirgendwo benannt.

6. **`static FilterHealth fh` mit gepushter-aus-Meta State** – `warmCyclePath:391` hält ein `static`-Objekt, dessen einziger Zustand (`streak_`) gleichzeitig aus `meta.filter_miss_streak` rekonstruiert wird. Das `static` ist sinnlos und macht den Codepfad unnötig globalsensitiv.

7. **Button-Dispatch zweifach implementiert** – `setup()` (Z. 552–559) und `handleButtonIfPressed()` (Z. 519–533) tun mit minimaler Variation dasselbe.

8. **Overlay-Stale-Suppression liegt beim Caller, 3× wiederholt** – [main.cpp:255–257](../src/main.cpp#L255), Z. 331, Z. 434–435. Implizite Konvention zwischen Caller und Merger.

9. **Doppel-Merge im Warm-Cycle** – `warmCyclePath` baut `merged` für `planSleep` (Z. 434), ruft danach `renderAndPush(snap, overlay, …)` (Z. 452), das *intern denselben Merge erneut* macht.

10. **Inkonsistente Sleep-Argumente** – `doSleepOrLoop` benutzt `sd.seconds` für DeepSleep, hartkodiert `POLL_INTERVAL_S` für LightSleep — letzteres kommt nicht aus `SleepConfig`. `warmCyclePath` benutzt mehrfach direkte `deepSleep`-Calls außerhalb von `planSleep`. Das umgeht den Sleep-Planner und ist nirgendwo zentral dokumentiert.

11. **Schedule-Persist-Schreibrate** – `coldBootPath` Z. 327–329 und `warmCyclePath` Z. 425–428 rufen `saveSchedule` immer wenn Refresh ok. RTC-RAM (kein Flash) — kein Bug, aber merken.

12. **Sekundenscharfer Dedup im `slot_merger` (echter Bug)** – [slot_merger.cpp:20](../src/logic/slot_merger.cpp#L20) vergleicht `when` mit `==`. Realtime kann eine Departure mit Sekunden-Präzision liefern (z. B. 05:14:32), Hint kommt aus EFA minutenscharf (05:15:00). Beide werden als verschiedene Departures behandelt und können nebeneinander im Display landen — derselbe Bus, zwei Slots.

13. **`slot_merger` nutzt nur `first_tomorrow`, nicht heutige Spät-Abfahrten** – [slot_merger.cpp:65–76](../src/logic/slot_merger.cpp#L65). Folge: zwischen `now()+70min` und „letzte realtime-bekannte Abfahrt heute" zeigt das Display Striche, obwohl der EFA-Plan die Daten kennt. **Verletzt direkt die Nutzer-Vorgabe „zeige die nächsten zwei Abfahrten".** Implementierungs-Lücke, kein Konzept-Feature.

14. **OGD-spezifische Benennungen in protokoll-agnostischen Strukturen** – `StreamData::rbl_responded` ([StreamSnapshot.h:23](../src/data/StreamSnapshot.h#L23)) ist generisch gemeint („hat der Endpunkt geantwortet"), heißt aber `rbl_…` und ist damit für v2 (ÖBB-Stream über HAFAS) semantisch falsch. Analog würden Enum-Werte `OgdRealtime`/`EfaHint` v2 zwingen, die Quelle in Enums hartzukodieren — Source steckt schon im Stream-Index.

### 1.4 Was bleiben muss (harte Constraints)

- **Arduino-Identifier in `main.cpp`** (`setup`/`loop`, `Serial.begin`, `pinMode`, `digitalRead`, `delay`, `millis`). Nicht auf Native-Build verfügbar.
- **`#ifndef NATIVE_BUILD`-Klammer** um die ganze Datei: `main.cpp` wird im Native-Build vollständig leer gelinkt. Alles, was wir herausziehen, darf das nicht.
- **Framebuffer-Speicherort**: zwei 15-kB-Frames müssen in BSS (file-scope static) bleiben — nicht auf den Stack/in eine kurzlebige Methode (siehe Claude-Code-Memory „Frame must be heap-allocated on ESP32"); file-scope-static ≠ heap, aber landet in BSS, was den Stack ebenfalls verschont.
- **`RTC_DATA_ATTR`-Constraints** — siehe [Anhang A](#anhang-a--rtc_data_attr-constraints). Kein Eingriff in [Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp) **außer** dem geplanten `SCHED_MAGIC`-Bump in Schritt 2.3.
- **Verhalten muss bitidentisch bleiben** für: Sleep-Entscheidungen, Refresh-Stufen, Stale-Verhalten, FilterHealth-Streak, Serial-Log-Format. **Ausnahmen** (geplante Verhaltensänderungen, siehe R10/R11): Minute-Dedup (Schritt 2.2) und Heute-Abend-Schedule-Bridge (Schritt 2.3). Long-term-Tests laufen in 24-h-Skala, semantische Drift fällt erst spät auf.

### 1.5 Test-Lage zu `main.cpp`

| Test                          | Bezug auf `main.cpp`-Logik                                                |
|-------------------------------|---------------------------------------------------------------------------|
| `test_native_*` (12)          | testen `logic/`+`data/`/`render/rle` direkt. Kein Bezug zu `main.cpp`.   |
| `test_device_fetch`           | repliziert `buildFilters`+`apiUrl` von Hand, prüft Cold-Start-NTP-Pfad gegen die Konvention in `warmCyclePath`. |
| `test_device_*` (rest)        | greift HAL einzeln an, nicht `main.cpp`.                                  |
| `test_longterm_*`             | laufen die echte Firmware ab. **Aber: `IDisplay` ist nicht im Test-Loop** — Heap-Issues aus `renderFrame` und Display-Pfad bleiben unsichtbar (eigener Bug, siehe Schritt 0a). |

Konsequenz: **Refactor von `main.cpp` ist im Native-Build heute völlig ungeprüft.** Wenn wir Logik aus `main.cpp` herausziehen, schaffen wir gleichzeitig Test-Anker — vorher haben wir keine.

---

## 2. Zielbild

```text
src/
├── main.cpp                        ~80 LOC: HAL-Globals, setup/loop, Dispatch
│
├── data/
│   ├── Departure.h                 + DepartureSource-Enum (Realtime/Plan/Hint)
│   ├── ScheduleHint.h              + next_today[2]
│   ├── StreamSnapshot.h            rbl_responded → endpoint_responded
│   ├── stream_labels.h             NEU — eine Tabelle pro Stream, für Logger + Tests
│   └── string_util.h               NEU — startsWith() (heute in zwei Parsern dupliziert)
│
├── logic/
│   ├── filter_builder.{h,cpp}      buildStreamFilters / buildScheduleFilters
│   ├── snapshot_fetcher.{h,cpp}    fetchSnapshot (Batch + Retry + Parse)
│   ├── snapshot_logger.{h,cpp}     formatSnapshotSummary, formatSlot (host-test)
│   ├── schedule_refresh.{h,cpp}    needScheduleRefresh, applyFetchResult
│   ├── render_input.{h,cpp}        composeRenderInput (Overlay × Merge-Rule)
│   ├── slot_merger.{h,cpp}         + next_today-Konsum, Minute-Dedup
│   ├── button_classifier.{h,cpp}   IButton-Adapter + classifyHeld
│   └── cycle_runner.{h,cpp}        CycleDeps-Struct + freie runColdCycle/
│                                   runWarmCycle/runButtonWake/runBwReset
│
├── hal/
│   ├── IClock.h                    + MIN_PLAUSIBLE_EPOCH + isSynced() default
│   ├── IRenderer.h                 NEU — abstrahiert renderFrame
│   ├── Esp32Renderer.{h,cpp}       NEU — dünner Adapter um renderFrame
│   └── Esp32Button.{h,cpp}         NEU — GPIO-Adapter für IButton
│
└── render/
    └── (error_overlay.{h,cpp} entfällt — Smell „toter Wrapper", siehe Schritt 7)

test/
└── test_native_runtime/            NEU (Schritt 10) — Headless-Loop gegen echte API
    ├── main.cpp                    Loop-Treiber, ruft runWarmCycle in Wandzeit-Cadence
    ├── WallClockClock.{h,cpp}
    ├── HttpsNet.{h,cpp}             via libcurl
    ├── NoOpSleep.{h,cpp}
    ├── DiskStore.{h,cpp}            .tmp/native-runtime/persist.bin
    ├── NoOpDisplay.{h,cpp}
    └── RecordingRenderer.{h,cpp}    Pseudo-Raster + diff-getriebener PGM-Dump
```

`main.cpp` schrumpft auf:

```cpp
#ifndef NATIVE_BUILD
#include "logic/cycle_runner.h"
#include "hal/Esp32*.h"

namespace {
  Esp32Clock g_clock{NTP_SERVER, TZ_INFO};
  Esp32Network g_net;
  Esp32Sleep g_sleep;
  Esp32PersistentStore g_store;
  Esp32Display g_display;
  Esp32Renderer g_renderer;
  Frame g_frame_new;
  Frame g_frame_prev;
}

static CycleDeps makeDeps() {
  return {g_clock, g_net, g_sleep, g_store, g_display, g_renderer,
          g_frame_new, g_frame_prev};
}

void setup() {
  Serial.begin(115200); delay(100);
  pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
  registerWifi(); g_display.init();
  CycleDeps deps = makeDeps();
  PersistedMeta meta = g_store.loadMeta();

  switch (g_sleep.wakeupCause()) {
    case WakeCause::ColdBoot: runColdCycle(deps, meta); break;
    case WakeCause::Button:   runButtonWake(deps, meta); break;
    default:                  runWarmCycle(deps, meta); break;
  }
}

void loop() {
  CycleDeps deps = makeDeps();
  PersistedMeta meta = g_store.loadMeta();
  pollButtonAndRunWarm(deps, meta);
}
#endif
```

Keine Engine-Klasse, kein Singleton, kein Static-Init-Order-Risiko. HAL bleibt file-scope-Globals — **Konstruktion-pro-Cycle ist wegen `WiFiMulti`- und GxEPD2-Panel-Init-Kosten nicht praktikabel**. (`RTC_DATA_ATTR` lebt ausschließlich in [Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp) und wird hier nicht angefasst — siehe [Anhang A](#anhang-a--rtc_data_attr-constraints).) HAL wird pro Cycle in einen lokalen `CycleDeps` gepackt und an die Logik gereicht.

### 2.1 Sub-Module im Detail

**`logic/filter_builder.{h,cpp}`**

```cpp
void buildStreamFilters(StreamFilter (&)[STREAM_COUNT]);
void buildScheduleFilters(ScheduleStreamFilter (&)[STREAM_COUNT]);
```

Single source of truth. `test_device_fetch` lädt aus dem Header statt eigenem Copy. Name bewusst neutral: v2 wird OGD-Streams und HAFAS-Stream parallel führen, der Builder bleibt der zentrale Einstieg.

**`logic/snapshot_fetcher.{h,cpp}`**

```cpp
struct FetchSummary {
  int total_batches = 0, failed_batches = 0;
  int attempts_total = 0;
};
FetchSummary fetchSnapshot(INetwork&, const std::string& base_url,
                           StreamSnapshot& out);
```

Stream-Topologie-spezifische Logging wandert in `snapshot_logger`. Iteration über `streamLabel(i)` aus `data/stream_labels.h`:

```cpp
std::string formatSnapshotSummary(const StreamSnapshot&, const FetchSummary&);
std::string formatSlot(const Departure&);   // RT / PLAN / HINT
```

Damit ist die 30-Zeilen-Druckblock-Schleife im Caller drei `Serial.printf`-Zeilen, und der Formatter ist host-testbar — Topologie-Changes (auch v2) ändern *nur* `data/stream_labels.h`.

**`data/stream_labels.h`**

```cpp
namespace bustaferl {
constexpr const char* STREAM_LABEL[STREAM_COUNT] = {
  "58A-Atz", "58A-Hie", "58B-Atz", "U1-Leo", "U1-Obe",
};
constexpr const char* streamLabel(int s) { return STREAM_LABEL[s]; }
}
```

Eine Tabelle, von Logger und Tests genutzt. v2 §5.1 ersetzt die letzten zwei Einträge — eine einzige Datei.

**`logic/schedule_refresh.{h,cpp}`**

```cpp
bool needScheduleRefresh(const ScheduleSnapshot&, time_t now);   // pure
void applyFetchResult(ScheduleSnapshot& inout,
                      const ScheduleFetchResult&, time_t now);    // pure
```

Macht den heutigen „cheap-and-correct"-Merge (Z. 215–224) explizit und testbar.

**`data/Departure.h` — `DepartureSource`-Enum**

```cpp
enum class DepartureSource : uint8_t {
  Unknown   = 0,
  Realtime,             // Live-Wert aus dem Realtime-Endpoint des jeweiligen Streams
  Plan,                 // Plan-Wert aus dem Realtime-Endpoint (kein Echtzeit-Update verfügbar)
  Hint,                 // injiziert aus ScheduleSnapshot (next_today oder first_tomorrow)
};

struct Departure {
  time_t when = 0;
  bool valid = false;
  DepartureSource source = DepartureSource::Unknown;
};
```

`is_realtime` wird *ersetzt* (nicht parallel gehalten). Bewusst protokoll-agnostisch: ob `Realtime` aus OGD oder HAFAS kommt, ergibt sich aus dem Stream-Index — kein Bedarf, die Quelle pro Departure zu wiederholen. CONCEPT-§12.4-Vorgabe „keine Unterscheidung **dem Benutzer gegenüber**" bleibt erfüllt (Renderer ignoriert das Feld); intern wird die Forensik präziser, ohne v2 zu binden.

**`data/StreamSnapshot.h` — Umbenennung**

```cpp
struct StreamData {
  Departure slot[SLOTS_PER_STREAM];
  bool endpoint_responded = false;   // war: rbl_responded
  bool filter_matched = false;
};
```

Semantisch unverändert („hat der Stream-Endpunkt einen well-formed Payload geliefert?"), aber neutral für jeden Protokoll-Typ. Touch-Sites: 2 in [wienerlinien_parse.cpp](../src/data/wienerlinien_parse.cpp), 1 in [filter_health.cpp](../src/logic/filter_health.cpp), Log-Block in main.cpp (verschwindet eh in Schritt 3), ~10 in Tests.

**`data/ScheduleHint.h` — `next_today[2]`**

```cpp
struct ScheduleHint {
  time_t last_today = 0;             // bleibt Refresh-Trigger
  time_t next_today[2] = {0, 0};     // NEU: nächste zwei Plan-Abfahrten heute
                                     //      (ab Hint-Fetch-Zeit, üblicherweise ~22:00)
  time_t first_tomorrow[2] = {0, 0};
};
```

Cutoff in [schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp) trennt heute schon nach `< cutoff` und `≥ cutoff`. `next_today` sind „die letzten N Einträge mit `dateTime < cutoff`", die der Parser heute verwirft. RTC-Speicher-Aufschlag: 16 B pro Stream × 5 = 80 B, bei ~860 B Reserve unkritisch (siehe [Anhang A](#anhang-a--rtc_data_attr-constraints)).

**`logic/render_input.{h,cpp}`**

```cpp
StreamSnapshot composeRenderInput(const StreamSnapshot& snap,
                                  const ScheduleSnapshot& schedule,
                                  OverlayKind overlay,
                                  time_t now);
```

Single source of truth für die Regel „Stale unterdrückt Hints":

- `overlay == Stale` → liefert `snap` unverändert (keine Hints durchsickern)
- sonst → `mergeSlots(snap, schedule, now)`

Eliminiert Smell 8+9 (Caller-Ternär 3× + Doppelmerge).

**`logic/slot_merger.cpp` — expliziter Vertrag und Erweiterungen**

Der Merge-Vertrag, der heute nur als Inline-Kommentar lebt:

> **Regel: Realtime überrult Plan, wenn sie denselben Bus meint.**
>
> *Nicht*: „Realtime überrult Plan immer." Realtime und Hint können nebeneinander stehen, *wenn sie verschiedene Busse* meinen.
>
> Operationalisierung:
>
> 1. Realtime-Slots werden zuerst inseriert → belegen Display-Slots primär.
> 2. Hint-Slots (sowohl `next_today` als auch `first_tomorrow`) werden danach inseriert → füllen, was Realtime nicht belegt hat.
> 3. Dedup: zwei Einträge gelten als „selber Bus" iff `when / 60 == cand.when / 60` (gleiche Minute). Realtime gewinnt qua Insert-Reihenfolge.
> 4. Sortierung: `insertSorted` hält ascending-`when`, die zwei chronologisch frühesten füllen die zwei Display-Slots.

Konkrete Änderungen am Modul:

- `insertSorted`-Dedup auf Minuten-Bucket (Smell 12, Schritt 2.2)
- Zweite Hint-Quelle `next_today` parallel zu `first_tomorrow` (Smell 13, Schritt 2.3)
- Bei Hint-Insertion `source = DepartureSource::Hint` setzen (Schritt 2.1)

Edge-Case `floor((when)/60)` vs. `round((when+30)/60)`: Realtime-Predictions schwingen praktisch nie *früher* als Plan; das einzige Szenario, in dem Truncate-Dedup einen echten Dupe übersieht (RT@23:41:50 gegen Hint@23:42:00, verschiedene Minute-Buckets) tritt mit Wiener-Linien-Daten nicht auf. Long-term-Beobachtungspunkt, kein architektonisches Mitigation-Item.

**`logic/button_classifier.{h,cpp}`**

Trennt Klassifikation von Arduino-IO:

```cpp
struct IButton {
  virtual ~IButton() = default;
  virtual bool isPressed() = 0;
  virtual uint32_t millis() = 0;
  virtual void sleep_ms(uint32_t) = 0;
};
enum class ButtonPress { None, Short, Long };
ButtonPress classifyHeld(IButton&, uint32_t long_press_ms);
```

`Esp32Button` (neuer Adapter, ~15 LOC) implementiert das gegen `digitalRead`/`millis`/`delay`. Host-Tests speisen `FakeButton` mit Press-Pattern.

**`logic/cycle_runner.{h,cpp}` — freie Funktionen, kein State**

```cpp
struct CycleDeps {
  IClock& clock;
  INetwork& net;
  ISleep& sleep;
  IPersistentStore& store;
  IDisplay& display;
  IRenderer& renderer;
  Frame& curr;
  Frame& prev;
};

void runColdCycle(CycleDeps&, PersistedMeta&);
void runWarmCycle(CycleDeps&, PersistedMeta&);
void runButtonWake(CycleDeps&, PersistedMeta&);
void pollButtonAndRunWarm(CycleDeps&, PersistedMeta&);
```

Privat in der .cpp:

- `doFetchCycle(CycleDeps&, …)` — gemeinsamer Sub-Pfad WiFi + NTP-Guard + fetchSnapshot + refreshSchedule. cold und warm rufen ihn auf, machen ihren spezifischen Pre/Post.
- `renderAndPush(CycleDeps&, …)` — nimmt jetzt direkt das `composeRenderInput`-Ergebnis (Smell 9 weg).
- `doSleepOrLoop(CycleDeps&, …)` — HAL-Glue.

`FilterHealth` wird *lokale* Variable in `runWarmCycle`, initialisiert aus `meta.filter_miss_streak`. Das `static`-Objekt verschwindet ersatzlos.

**Magic-Numbers** wandern in [config.h](../src/config.h):

```cpp
#define LONG_SLEEP_FOR_NIGHTLY_CLEAN_S  14400  //  4 h
#define NIGHTLY_DEEP_CLEAN_INTERVAL_S   72000  // 20 h
```

### 2.2 `IClock`-Erweiterung

```cpp
// in IClock.h
namespace bustaferl {
  constexpr time_t MIN_PLAUSIBLE_EPOCH = 1700000000;  // 2023-11-15
  class IClock {
    // ...
    virtual bool isSynced() { return now() >= MIN_PLAUSIBLE_EPOCH; }
  };
}
```

`Esp32Clock` braucht nichts zu überschreiben, der Default reicht. Tests können einen `FakeClock::isSynced` setzen. Die Magic-Konstante steht zentral und wird über alle fünf Vorkommen im Repo eliminiert (inkl. [Esp32Clock.cpp:29](../src/hal/Esp32Clock.cpp#L29), wo die `ntpSync()`-Erfolgsbedingung dieselbe Schwelle verwendet).

**Warum nicht über `lastSync()`:** Das Clock-Objekt wird nach Deep-Sleep frisch konstruiert; sein Member-State verliert sich, `lastSync()` ist dann immer 0 — würde jeden Warm-Wake fälschlich als „braucht NTP" einstufen, obwohl die ESP32-RTC die Wandzeit über den Schlaf *typisch* hält. Der Epoch-Threshold ist semantisch korrekt („ist die Wandzeit plausibel?").

---

## 3. Pros / Cons

### 3.1 Pros

- **Schichtenregel wird eingehalten.** Logik in `logic/`, Wiring in `main.cpp`. Architecture-Diagramm wird wieder wahr.
- **Host-testbar werden**: `fetchSnapshot` (gegen `FakeNet`), `needScheduleRefresh`, `applyFetchResult`, `composeRenderInput`, `classifyHeld`, `formatSnapshotSummary`, *und der ganze `cycle_runner`* (gegen alle Fakes — heute komplett ungeprüft).
- **v2-Migration (ÖBB-Stream) wird billiger.** Heute 3 Stellen für Stream-Topologie (main, test_device_fetch, der Druckblock in fetchSnapshot). Nach Refactor: 1 Tabelle (`stream_labels.h`) + 1 Filter-Builder. Naming protokoll-agnostisch.
- **Duplikation in Tests verschwindet** (`buildFilters` lokal in `test_device_fetch`).
- **Button-Pfad einheitlich** — setup() und loop() benutzen denselben Pfad.
- **Magic-Numbers werden benannt** — `clock.isSynced()`, `LONG_SLEEP_FOR_NIGHTLY_CLEAN_S`, `NIGHTLY_DEEP_CLEAN_INTERVAL_S`.
- **Forensik im Log präziser** — `DepartureSource` unterscheidet RT/Plan/Hint statt RT/„Plan-oder-Hint".
- **Nutzer-Vorgabe „nächste zwei Abfahrten" wird erstmals vollständig erfüllt** — Schritt 2.3 schließt die Heute-Abend-Lücke, die heute Striche zeigt, obwohl der EFA-Plan die Daten kennt.
- **Native-Runtime (Schritt 10)** als echte Test-Plattform — valgrind/massif-Heap-Profile, PGM-Dumps der Frame-Sequenz, headless gegen echte WL/EFA-Endpoints. Heap-Klasse von Bugs wird *erstmals* überhaupt sichtbar.

### 3.2 Cons

- **Risiko subtiler Verhaltensdrift** durch Reorder. Insbesondere:
  - heute zuerst `saveMeta` *vor* `lightSleep` in `doSleepOrLoop`, aber `deepSleep` nimmt der Aufrufer von außen wahr nicht zurück → Ordering muss exakt erhalten bleiben.
  - der Warm-Path entscheidet „nightly deep clean" über *pre*-merge SleepDecision vor dem Rendern; das ist ein subtiler Trick (Sleep-Decision für Render-Entscheidung, nicht für Sleep). Beim Umzug nicht „aufräumen".
- **Zwei geplante Verhaltens-Changes** in den Schritten 2.2 und 2.3, beide *richtungsweisend gewünscht*, aber sie verändern das Long-term-Baseline-Bild messbar. Siehe R10/R11 in [§5](#5-risiken).
- **NTP-Magic-Konstante (1.7e9) wandert in `IClock`** — als benannter `MIN_PLAUSIBLE_EPOCH`-Default. Akzeptabel, weil semantisch eindeutig.
- **Renderer wird abstrahiert (`IRenderer`)** als Vorinvestition für die Native-Runtime. Eine zusätzliche Indirektion auf einem heißen Pfad — virtual call pro Render-Cycle ist ≪ 1 µs, irrelevant gegen 400–600 ms Partial-Refresh.
- **RTC-`SCHED_MAGIC`-Bump** in Schritt 2.3 wirft einmalig die gespeicherten Hints weg. Nächster Cycle re-fetcht. Muss in Release-Notiz.
- **Native-Runtime braucht libcurl** als zusätzliche Build-Abhängigkeit auf dem Host. Distro-paketverfügbar (`libcurl4-openssl-dev` o. ä.), kein Drama, aber CI-Hosts müssen das haben.

### 3.3 Ausdrücklich nicht in Scope (jetzt)

- **Kein** Umzug von `render/layout` auf Native-Build (Adafruit-GFX-Wall). Stattdessen `IRenderer`-Abstraktion — und der Native-Runtime nutzt `RecordingRenderer`, der **kein** Adafruit-GFX-Layout nachbaut, sondern ein deterministisches Pseudo-Raster aus dem `RenderInput`-Hash. Layout-Bugs bleiben Geräte-Test-Stoff.
- **Keine Per-Stream-Slot-Anzahl-Variabilität** (CONCEPT v2 §5.2 Variante B) — bleibt v2-Stoff.
- **Keine Vorbereitung des HAFAS-Parsers / `INetwork::httpPost`** — kommt mit v2. Heute hinzugefügte Methoden würden alle Test-Mocks für etwas durchrütteln, das v1 nie aufruft.
- **Kein `Departure::line_label[6]`** — v2 §5.3 optional-Feld, jetzt YAGNI.
- **Kein** Versuch, `Stream`-Enum für v2 vorzubereiten — Topologie-Wechsel zwingend zur v2-Zeit.

---

## 4. Schritt-für-Schritt-Plan

### 4.0 Vorgehensmodell für die Umsetzung

Ziel: zusammenhängende Umsetzung in einem Schwung. Der Auftraggeber wird **am Anfang** für die offenen Punkte und **am Ende** für den Gesamtreview involviert. Dazwischen läuft die Umsetzung autonom.

#### 4.0.1 — Voraussetzungen (vor Start bestätigt)

- ESP32 ist am Host angeschlossen und reagiert auf `pio test`-Aufrufe → alle automatisierbaren HW-Tests laufen selbständig durch.
- `libcurl4-openssl-dev` (oder Pendant) ist auf dem Host installiert → Schritt 9 `HttpsNet` kann linken.
- Branch `refactor/main-cpp` ist aus `main` abgezweigt und lokal aktiv.

#### 4.0.2 — Autonomie + Eskalation

**Annahmen statt Rückfragen** für Entscheidungen, die zwischen den Schritten auftauchen. Jede Annahme wird in §4.1 „Annahmen während der Umsetzung" mit Schritt-Referenz festgehalten, damit der Gesamtreview am Ende sie nachvollziehen kann.

**Schwerwiegende Fälle** (Auftraggeber sofort kontaktieren — Umsetzung pausiert):

- Irreversible Aktionen außerhalb des Plans (Force-Push, weitere RTC-MAGIC-Bumps ohne Migrations-Plan, Datenverlust).
- Scope-Erweiterungen, die nicht aus dem Plan ableitbar sind.
- Wiederholte Test-Fehler nach Diagnose-Versuch ohne klaren Pfad nach vorn (R1-Drift, der nicht erklärbar ist).
- Verhaltens-Drift im Baseline-Diff (Schritt 0b), der *nicht* zu R10/R11 passt.

#### 4.0.3 — Hardware-Tests

- **Automatisierbare HW-Tests** (`make test-device-*`, `make test-longterm-smoke/soak-*/jitter/wake/horizon-*`) laufen autonom über `pio test -e …` während der Umsetzung. Pass-/Fail-Auswertung ist Teil des jeweiligen Schritt-Gates.
- **Mensch-Interaktion erforderlich** (manueller Button-Smoke, visuelle Display-Inspektion der Overlay-Zustände, finaler 24-h-day-full-Soak auf Wunsch) → in den neuen **Schritt 12 — Manuelle HW-Verifikation am Ende** geparkt. Schritt 6 (`button_classifier`) läuft regulär durch; sein Button-Smoke-Gate wird *dorthin* verschoben.

#### 4.0.4 — Commits + Push

- **Push: nein.** Alle Commits bleiben lokal auf `refactor/main-cpp`, bis der Auftraggeber am Ende explizit pushen oder mergen möchte.
- **Erster Commit**: `Doku: Refactor-Plan einchecken` mit `docs/main-refactor-plan.md` als einzigem Eintrag, **bevor** Schritt 0a beginnt.
- **Plan-Doc-Löschung**: am Ende von Schritt 10 (Doku), wenn die Plan-Inhalte in `docs/ARCHITECTURE.md` und `docs/TESTING.md` überführt sind. Im selben Commit, der die Doku-Konsolidierung vornimmt.

**Commit-Konvention** (analog Bestand `Engine: …`, `Tooling: …`, `Doku: …`):

- `<Kategorie>: <kurze Beschreibung>` als Subject.
- Kategorien: `Engine`, `Tooling`, `Doku`, `Test`, `HAL`, `Data`, `Render`, `WIP` (für Zwischenstände, sollte am Ende konsolidiert werden).
- Kein `Co-Authored-By`-Trailer (siehe Memory `feedback-no-coauthor-trailer`).
- Erweiterte Body-Begründung nur bei nicht-trivialen Schritten.

**Commit-Granularität**: smart batching — Default ist ein Commit pro Checkbox (Top-Level oder Sub-Step, je was passt). Triviale Sub-Steps dürfen gebündelt werden, große Sub-Steps dürfen weiter unterteilt werden, wenn sie atomar nicht grün-lassbar sind. Wenn die Standardgranularität verlassen wird → Annahme in §4.1 dokumentieren.

#### 4.0.5 — Session-Gruppierung

Logische Etappen je ~0.5–4 d. Jede Gruppe ist ein natürlicher Session-Schnitt (Kontextfenster-Größe, HW-Test-Pausen, sinnvolle Pausen-Punkte für den Auftraggeber).

| Group | Schritte                              | Aufwand   | Charakter                                                     |
|-------|----------------------------------------|-----------|----------------------------------------------------------------|
| A     | 0a + 0b + 0c (inkl. 0c.1–5)           | ~2–3 d    | Prereqs + Tooling — komplette Vorbereitung                    |
| B     | 1 + 2 + 2.1 + 2.2                      | ~1.5–2 d  | Daten-Layer-Extraktionen (klein)                              |
| C     | 2.3                                    | ~1.5–2 d  | Schedule-Feature (`next_today`) — eigene Gruppe wegen RTC-Bump |
| D     | 3 + 4 + 5 + 6                          | ~2 d      | Plumbing + Button                                              |
| E     | 7.1 + 7.2 + 7.3                        | ~3–4 d    | Engine-Refactor — das große Stück                              |
| F     | 8                                      | ~0.5 d    | main.cpp schlank                                               |
| G     | 9.1 + 9.2 + 9.3 + 9.4 + 9.5            | ~2.5 d    | Native-Runtime                                                 |
| H     | 10 + 11.1 + 11.2 + 11.3                | ~1.5 d    | Doku + Post-Tooling-Härtung                                    |
| I     | 12                                     | ~0.5 d    | Manuelle HW-Verifikation (am Ende, mit Auftraggeber)           |

Reihenfolge ist linear (A → I). Session-Übergänge dazwischen sind Kontext-Reset-Punkte; Stand wird aus Git-Log + Plan-Doc-Checkboxen rekonstruiert.

### 4.1 Annahmen während der Umsetzung

Wird laufend gefüllt. Format: `**[Schritt X.Y, Datum]** Annahme: …; Begründung: …`

**[Schritt 0a.2, 2026-05-18]** Annahme: `RecordingDisplay.h` lebt in `test/test_longterm_soak/` und wird in `test/test_longterm_smoke/test_main.cpp` per relativem `../test_longterm_soak/RecordingDisplay.h` inkludiert. Begründung: PlatformIO 6.x bietet kein offizielles `test_common/`-Verzeichnis; Alternativen wären Header-Duplikation (rot-tendiert) oder ein neues `src/test_support/`-Modul, das im Native-Build mitkompiliert würde. Cross-Include lokalisiert die Kopplung auf zwei Files und bleibt bei späterer Refactor-Bewegung sofort sichtbar.

**[Schritt 0b, 2026-05-18]** Annahme: Die horizon-evening-Baseline wird *nicht* synchron in Group A erfasst, sondern getrennt vor Group C (Schritt 2.3) abends ab 20:00 lokal gestartet. Begründung: Der Test enforced `local.tm_hour >= 20 || local.tm_hour <= 3` als Pre-Condition; die Group-A-Umsetzung lief am Nachmittag. Die zwei anderen Baselines (`soak-15min`, `device-fetch`) sind regulär erfasst. horizon-evening ist nur für Schritt 2.3 (Smell 13, „Heute-Abend-Bridge") kritisch — wird vor 2.3 nachgeholt.

**[Schritt 0c.3 / 0c.4 / 0c.5, 2026-05-18]** Anpassung der Plan-Reihenfolge:

- **0c.5 (cppcheck-Level hoch)**: in Group A erledigt. `make lint` läuft mit `--enable=warning,style,performance,portability --inconclusive --std=c++17` und ist clean.
- **clang-tidy aus Schritt 11.1**: in Group A vorgezogen. `.clang-tidy` eingecheckt, `make tidy` integriert in `make ci`. Library-Pfade per `-isystem` ausgeklammert (PlatformIO-`compile_commands.json` wird via `sed` rewritten, sodass `.pio/libdeps/*` als System-Header ohne Diagnose-Output gelten). `HeaderFilterRegex` ergänzt die Filterung auf `src/(data|logic|hal|render)/.*\.h$`. Alle ~60 ursprünglichen `src/`-Findings wurden behoben (Magic Numbers zu `DEFAULT_*`-`constexpr`, `enum class : std::uint8_t`, `cppcoreguidelines-init-variables`, `modernize-loop-convert`, `bugprone-implicit-widening-of-multiplication-result`); 5 verbleibende `readability-function-size`/`-cognitive-complexity`-Findings sind mit `NOLINTNEXTLINE`/`NOLINTBEGIN..END` und Begründungs-Kommentar markiert (Parser-Strukturen + Heap-Wächter-Schleife — Refactor-Anker für Schritte 4/5 + post-refactor TODO §7.1). `WarningsAsErrors: '*'` ist scharfgeschaltet.
- **Pre-Commit-Hook (Schritt 11.3)**: ebenfalls vorgezogen. `scripts/install-pre-commit.sh` installiert einen Hook, der `make ci` vor jedem Commit fährt.
- **0c.3 (strikte Compiler-Warnings + `-Werror`)** und **0c.4 (ASan + UBSan)**: bleiben für Schritt 11. Begründung: 0c.3 ist auf eine PlatformIO-Eigenheit gestoßen (`build_src_flags` greift auch auf Test-Builds, Unity's `unity_config.c` ist C nicht C++, Unity-Macros expandieren Old-Style-Casts in Tests). Saubere Auflösung erfordert einen SCons-Hook für selektives Flag-Routing, der den Tooling-Aufwand des Refactors substanziell vergrößert. clang-tidy (jetzt eingebaut) deckt dieselben Klassen mit dem `-isystem`-Mechanismus bereits ab — das Compiler-Strict-Set ist damit weniger dringend.
- **0c.1 (clang-format) + 0c.2 (`make ci` im CI)** waren reibungslos und bleiben in Group A erledigt.

**[Schritt 2.3, 2026-05-18]** Anpassungen während der Umsetzung:

- **horizon-evening-Baseline (0b) übersprungen**: ein 5-h-Soak vor jedem Refactor-Schritt ist nicht praktikabel im Tagesablauf. Auftraggeber hat explizit verzichtet. Diff-Vergleich gegen Baseline entfällt für diesen Schritt; Validierung läuft über die R11-Assertumstellung im Test selbst (`g_bridge_hits > 0`).
- **Mapping in `schedule_fetcher.cpp` (Plan-Schritt 3)**: nicht nötig. `fetchSchedule` ruft `parseEfaResponse(..., out.hint)` direkt mit einer Referenz auf den Ergebnis-`ScheduleHint`-Array auf. `next_today` wandert transparent durch — keine Mapping-Schicht dazwischen. Plan-Schritt 3 ist damit ein No-op.
- **`test_longterm_horizon_evening`-Erweiterung**: Test fetcht jetzt einmal zu Beginn einen echten EFA-Schedule, mergt ihn pro Cycle und zählt `g_bridge_hits` (Cycles wo realtime leer aber merged via Hint gefüllt). Neuer Assert: `g_bridge_hits > 0`. Der `planSleep`-Aufruf bleibt bewusst auf dem *unmerged* `snap` (NO_DATA-Assert unabhängig vom Bridge-Verhalten — Test-Diskrepanz zur Prod-Pipeline ist im Test-Kommentar dokumentiert).

**[Schritt 3, 2026-05-18]** Drift gegen 0b-Baseline in der Summary-Header-Zeile akzeptiert: das alte `fetchSnapshot`-Log nannte den 58B-Stream im Header inkonsistent „58B" (Per-Slot-Logs benutzten schon „58B-Atz[0]"). `streamLabel()` ist jetzt die einzige Quelle der Stream-Bezeichnung; alle Vorkommen lesen „58B-Atz". Begründung: §4.0.4 erlaubt opportunistische Konsolidierungen im jeweiligen Touch-Set, und Plan-Schritt 3 macht explizit die Single-Source-Tag-Ablage zum Ziel. Die zwei externen Stellen mit eigener `58B`-Kopie (`test_longterm_soak/test_main.cpp:165`, `test_device_fetch/test_main.cpp:102`) bleiben unangefasst, weil sie ihren eigenen Summary-String bauen und kein Baseline-Vergleich auf sie zeigt.

**[Schritt 7.2, 2026-05-18]** `[[noreturn]]` auf `ISleep::deepSleep` entfernt (bleibt auf `Esp32Sleep`-Override). Begründung: Tier-2/3-Recording-Tests in 7.3 brauchen eine Fake-`ISleep`, deren `deepSleep` zurückkehrt; mit `[[noreturn]]` an der virtuellen Funktion ist das UB. Auf Hardware kehrt `deepSleep` real nicht zurück — der `[[noreturn]]` auf `Esp32Sleep` bleibt für lokale Dead-Code-Optimierung. Konsequenz für `runColdCycle`/`runWarmCycle`: nach jedem `deepSleep`-Aufruf jetzt explizites `return;` statt impliziter Unreachability.

**[Schritt 7.2, 2026-05-18]** `CycleConfig`-Defaults reuse existing module-level `DEFAULT_*`-constexpr: `DEFAULT_WIFI_TIMEOUT_MS` + `DEFAULT_COLD_BOOT_MAX_RETRIES` aus `boot_sequencer.h`, `DEFAULT_WAKE_BEFORE_BUS_S` / `DEFAULT_BOOT_MARGIN_S` / `DEFAULT_ACTIVE_THRESHOLD_S` / `DEFAULT_NO_DATA_SLEEP_S` / `DEFAULT_API_FAILURE_RETRY_S` aus `sleep_planner.h`. Nur die cycle-spezifischen Werte (poll-interval, stale-threshold, nightly-clean-fenster, button-long-press, NTP-intervall, filter-health-streak, cold-boot-retry/giveup) sind neu in `cycle_runner.h` definiert. Begründung: Schritt-0c.5-Magic-Number-Regel verlangt Single-Source, und das hätte sonst zwei Quellen für dieselben Konstanten geschaffen.

**[Schritt 8, 2026-05-18]** No-op am `main.cpp`: die Datei stand am Ende von Schritt 7 bereits bei ~75 net-LOC (HAL-Globals + `makeCycleConfig()` + `makeDeps()` + `registerWifiCredentials()` + `setup()` + `loop()`), alle Includes sind aktiv genutzt. `makeCycleConfig()` bleibt bewusst in `main.cpp` als Übersetzungsschicht der `config.h`-Compile-Time-Macros zur Laufzeit-`CycleConfig` — sie nach `logic/` zu ziehen würde dort eine `config.h`-Kopplung schaffen, ohne LOC-Einsparung in `main.cpp`. Validation reduziert sich auf `make test` (138/138 grün). HW-Validation (`test-device`, `test-longterm-smoke`, `test-longterm-soak-15min`) wurde verschoben (Auftraggeber-Entscheidung: ESP32 aktuell nicht angesteckt, separate Session). Im Zuge der `make test-device`-Diagnose ist ein latenter Build-Bug aufgefallen: `test/test_device_render/test_main.cpp:94` initialisierte `Departure` noch mit dem 2-Feld-Layout (`{time_t, bool, bool}`), was seit Group B (Einführung `DepartureSource`) nicht mehr kompiliert. Fix in derselben Group F: `{1704108660, DepartureSource::Realtime, true}`. `--without-uploading --without-testing`-Build des `device-render`-Envs läuft grün.

**[Schritt 7.3, 2026-05-18]** Tier-2-Tests sind strukturelle (`assertOrdered` über Substring-Marker im Trace + per-fake-Counter), nicht exakte `TEST_ASSERT_EQUAL_STRING_ARRAY` über die volle Sequenz wie im Plan-Sketch §5.2. Begründung: Die Default-Retry-Policy von `fetchWithRetry` (3 Versuche × 3 Batches × 2 Fetches pro Cycle) erzeugt ~30-Element-Traces mit `http_ok=false`, und die exakte Anzahl der HTTP-Calls hängt von Implementation-Details der Retries ab — ein exakter String-Vergleich würde bei jeder Retry-Justierung brechen, ohne dass die R1-Drift-Eigenschaft wirklich verletzt wurde. Die strukturellen Asserts (`renderer.render` kommt VOR `store.saveFramebuffer` VOR `sleep.deepSleep`; `save_meta_calls <= 1`; `last_deep_sleep_seconds < 24h`) fangen exakt die R1-Bug-Klasse. Recording-Fakes sind in `test/test_native_cycle_runner_warm/recording_fakes.h`; die cold + invariants-Tests inkludieren cross-relativ — gleiches Muster wie test_longterm_soak/RecordingDisplay.h vs. test_longterm_smoke/ (§0a.2-Annahme).

**[Schritt 9.1, 2026-05-18]** `NoOpSleep::deepSleep` kehrt zurück, ruft *nicht* `std::exit(0)` + Re-Start-Skript wie im Plan-Sketch. Begründung: der `native-runtime-smoke`-Target soll den kumulativen Heap-Profil über N Cycles in *einem* Prozess profilen (valgrind/massif sehen sonst keinen Leak über Cycle-Grenzen). Cold-vs-Warm-Boot wird stattdessen out-of-band signalisiert: erster `wakeupCause()`-Call liefert `ColdBoot`, danach `Timer`. Konsequenz: Button-Wake bleibt im Host-Pfad ausgeklammert (R1-Plan §9 "GPIO not covered").

**[Schritt 9, 2026-05-18]** Build-System: kein zweites PlatformIO-Env `native-runtime`. Plan-Sketch §9.5 schlug ein PIO-Env vor; in der Umsetzung ist das ein direktes `g++`-Target im Makefile (`make native-runtime-build`, `-smoke`, `-day`). Begründung: PIO's `platform = native` ist auf das Test-Framework Unity zugeschnitten, nicht auf Standalone-Executables; ein zweites Env hätte SCons-Hooks gebraucht, um die `test_dir`-Konvention zu umgehen — Tooling-Aufwand gegen die Memory `feedback-no-tooling-rabbit-holes`. Die ArduinoJson-Library wird trotzdem über PIO's `lib_deps`-Cache (`.pio/libdeps/native/`) bezogen, der Make-Target stellt sicher, dass das Cache vorhanden ist.

**[Schritt 9.5, 2026-05-18]** `make ci` bleibt unverändert (~30-45 s, pre-commit-tauglich). Neuer separater Target `make ci-heavy` ruft `ci + native-runtime-smoke` (~5-6 min). Begründung: Plan §9.5 schlug Smoke-in-`ci` vor, aber das macht den Pre-Commit-Hook (installiert via `scripts/install-pre-commit.sh`) auf ~6 min — klares Anti-Pattern (Memory `feedback-no-tooling-rabbit-holes`). Auftraggeber hat in der Vor-Umsetzungs-Frage explizit für die `ci-heavy`-Trennung entschieden.

**[Schritt 10, 2026-05-18]** Group H reduziert sich auf den Doku-Schritt: 11.1–11.3 wurden in Group A vorgezogen (siehe §4.1-Annahmen oben), das Plan-übergeordnete `[ ] erledigt` an Schritt 11 ist deshalb mit dem Group-A-Verweis abgehakt. CONCEPT.md §12.4 ist konsistent (Update kam in Schritt 2.3); die übrigen Schritte berühren CONCEPT nicht. `ARCHITECTURE.md` bekam die elf neuen `logic/`-Module in der Modulkarte plus einen Host-Engine-Abschnitt (Adapter-Tabelle `test/test_native_runtime/`); die `POLL_INTERVAL_S` + `NTP_INTERVAL_S`-Zeilen der Konstanten-Tabelle wurden von `main.cpp` auf `logic/cycle_runner` verschoben (entspricht dem Schritt-7-Ergebnis). `TESTING.md` bekam (a) den `native-runtime`-Bucket in die Bucket-Übersicht + einen eigenen Abschnitt zwischen Long-term und Testbar/nicht-testbar, (b) drei neue Zeilen in der Test-Pair-Tabelle (`runtime_diskstore`, `runtime_renderer`, `cycle_runner_*`); die Bucket-Zahl `(12)` wurde auf `(25)` korrigiert.

### 4.2 Schritt-Reihenfolge

Optimiert auf: **früh Tests, klein zerlegt, jeder Schritt grün lassbar**.

### Schritt 0 — Prerequisites (kein Code-Refactor, aber blockierend)

#### Schritt 0a — Long-term-Tests benutzen `IDisplay`

- [x] erledigt

Heute fehlt der Display-Pfad in den Long-term-Tests; Heap-Spitzen aus `renderFrame`+`drawPartial`/`lightFull` bleiben damit unsichtbar. Vor dem Refactor fixen, sonst sind Schritte 7+8-Validierungen blind.

- **0a.1** [test_longterm_horizon_evening/](../test/test_longterm_horizon_evening/), [test_longterm_horizon_scan/](../test/test_longterm_horizon_scan/), [test_longterm_horizon_mock/](../test/test_longterm_horizon_mock/), [test_longterm_day_full/](../test/test_longterm_day_full/) umbauen, sodass pro Cycle `renderFrame` + echtes `g_display.drawPartial`/`lightFull`/`deepClean` läuft. Schlägt aufs Panel.
- **0a.2** Soak-Familie (`test_longterm_soak`) bleibt Software-Heap-Test: ein neuer `RecordingDisplay`-Stub, der RLE-Save/Load und Partial-Bbox-Berechnung im Heap-Pfad äquivalent durchläuft, aber das Panel nicht abnutzt. Begründung: Soak-Tests laufen 5 min – 1 h, dauerhafte Belastung des e-Paper-Panels mit synthetischen Mustern ist Lebensdauer-Verschwendung.

**Validation**: `make test-longterm-smoke` grün; ein voller `make test-longterm-soak-15min` als Baseline.

#### Schritt 0b — Golden-Master-Baseline

- [x] erledigt (soak-15min + device-fetch); horizon-evening: siehe §4.1-Annahme

Vor jedem Code-Eingriff einen Referenz-Snapshot anlegen, gegen den nach jedem Schritt diff-getrieben verglichen wird:

- `make test-longterm-soak-15min` → Output nach `.tmp/baseline/soak-15min.log`
- `make test-device-trace ENV=device-fetch` → Output nach `.tmp/baseline/device-fetch.log`
- `make test-longterm-horizon-evening` → Output nach `.tmp/baseline/horizon-evening.log` (kritisch für 2.3-Verhaltensvergleich)

Nach jedem Refactor-Schritt: `diff` der Reproduktion gegen Baseline. Zeile-für-Zeile-Drift fällt sofort auf.

#### Schritt 0c — Pre-Refactor Tooling-Härtung

- [ ] erledigt

Tools, die *während* des Refactors greifen und Regressionen fangen. Bewusst **ohne** Funktions-Längen-Regel (kommt in [Schritt 11](#schritt-11--post-refactor-tooling-härtung), sonst können die Zwischenzustände der Refactor-Schritte nicht committet werden).

##### 0c.1 — `.clang-format` einchecken

- [x] erledigt

Heute läuft `make format` ohne Config-File — `clang-format` nimmt seinen LLVM-Default. Über Maschinen-/Versionen-Grenzen reproduzierbar nur durch explizite Config.

```yaml
# .clang-format
BasedOnStyle: LLVM
IndentWidth: 2
ColumnLimit: 80
PointerAlignment: Right
SpacesBeforeTrailingComments: 1
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
```

Vorgehen: einmal `make format` damit, Diff prüfen (sollte ≈ 0 sein, weil der heutige Code dem LLVM-Default folgt), committen.

##### 0c.2 — CI ruft `make ci` (Bug-Fix)

- [x] erledigt

[.github/workflows/ci.yml:27-30](../.github/workflows/ci.yml#L27) ruft `make test` und `make build` separat — überspringt damit `make format-check` + `make lint`. Format- und Lint-Violations blocken heute *nichts*.

Fix: ein einziger Step `make ci` ersetzt beide. Voraussetzung: `make ci` ist auf `main` heute grün — vor Commit verifizieren.

##### 0c.3 — Strikte Compiler-Warnings + `-Werror`

- [ ] erledigt — **verschoben nach Schritt 11**, siehe §4.1-Annahme.

Heute in [platformio.ini:18](../platformio.ini#L18) nur `-Wall -Wextra`. Erweiterung in `[env]` build_flags:

```ini
[env]
build_flags =
    -std=gnu++17
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wcast-qual
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wundef
    -Winit-self
    -Wzero-as-null-pointer-constant
    -Wsuggest-override
build_unflags =
    -std=gnu++11
```

**Erwartete Baseline-Bereinigung:**

- **`-Wconversion` / `-Wsign-conversion`** werden Treffer bei `time_t`/`int`/`unsigned`-Mix-Ups landen — überall in der Codebase. Sauber: explizite `static_cast<...>` an den realen Konvertierungs-Sites, *nicht* casts-überall-streuen. Geschätzt 20–40 Touch-Sites.
- **`-Wold-style-cast`** trifft jedes `(int)x`/`(time_t)x` (z. B. [main.cpp:99](../src/main.cpp#L99): `static_cast<long long>(d.when)` ist schon ok, aber `(time_t)-1` in [efa_parse.cpp:53](../src/data/efa_parse.cpp#L53) nicht). 5–10 Sites.
- **`-Wpedantic`** kann auf C-Bibliotheken-Header-Includes feuern (Arduino-Framework hat einige ISO-Verstöße). Falls die Framework-Header die Warning durchschlagen → Library-Pfade per `-isystem` markieren statt `-I`. PlatformIO-Mechanik: `-isystem $PROJECT_LIBDEPS_DIR/$PIOENV` in den build_flags. Nur wenn nötig — vermutlich nicht, weil Framework-Header durch Compiler-Aufruf des Frameworks selber kompiliert werden, nicht durch unseren.
- **`-Wsuggest-override`** trifft jede HAL-Implementierung, die nicht `override` benutzt. Wir benutzen es schon konsistent (in [Esp32Sleep.h](../src/hal/Esp32Sleep.h), [Esp32Network.h](../src/hal/Esp32Network.h) etc.), sollte 0 Findings sein.

**Risiko**: Library-Code (ArduinoJson, GxEPD2) emittiert intern Warnings, die durch unsere Includes lecken. Mitigation in zwei Stufen:

1. Library-Pfade als System-Header markieren (siehe oben).
2. Wenn einzelne Lib-Header sich nicht zähmen lassen: lokal `#pragma GCC diagnostic push/pop` um den `#include`-Block (hässlich, aber lokal).

##### 0c.4 — ASan + UBSan in `env:native`

- [ ] erledigt — **verschoben nach Schritt 11**, siehe §4.1-Annahme.

Catches OOB-Access, Use-after-free, Integer-Overflow, Null-Deref *at-test-time*. Native-Tests werden ~2× langsamer (5 s → 10 s), irrelevant.

```ini
[env:native]
build_flags =
    ${env.build_flags}
    -DNATIVE_BUILD
    -DUNITY_INCLUDE_DOUBLE
    -I src
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all
build_unflags =
    -std=gnu++11
```

`-fno-sanitize-recover=all` — Sanitizer hard-fail bei jedem Finding, kein „warnen-und-weiter-laufen". Hosts brauchen `libasan` / `libubsan` (auf Debian/Ubuntu in `libc6-dbg` bzw. `libasan6` enthalten, CI-Image `ubuntu-latest` hat's).

##### 0c.5 — `cppcheck` Level hoch

- [x] erledigt — `--enable=warning,style,performance,portability --inconclusive --std=c++17`, `make lint` clean.

[Makefile:145–147](../Makefile#L145):

```text
lint:                  ## run cppcheck
    cppcheck --enable=warning,style,performance,portability \
             --inconclusive \
             --error-exitcode=1 \
             --suppress=missingIncludeSystem --inline-suppr \
             --std=c++17 \
             -q src/
```

(Achtung beim Übernehmen: echte Makefile-Recipes brauchen Tab-Indent, nicht Spaces.)

Drei zusätzliche Check-Klassen, plus `--inconclusive` für tiefere Analyse (mehr False-Positives — wir haben Inline-Suppress, also leistbar).

**Aufwand Schritt 0c**: 0.5–1.5 d, davon der Großteil für die Baseline-Bereinigung der neuen Warnings (vor allem `-Wconversion`/`-Wsign-conversion`).
**Validation**: `make ci` grün auf `refactor/main-cpp`-Branch, GitHub-Actions-CI grün am Push.

### Schritt 1 — `logic/filter_builder.{h,cpp}` + Test-Konsolidierung

- [ ] erledigt

- Verschiebe `buildFilters` → `buildStreamFilters`, `buildScheduleFilters` 1:1. Funktionsname bewusst protokoll-neutral (v2-Vorbereitung).
- `main.cpp` und `test_device_fetch` benutzen den Header.
- Neuer Host-Test `test_native_filter_builder` (Strukturcheck: alle 5 Stream-Indices belegt, RBLs nicht 0).

**Validation**: `make test` grün, `make test-device` (wenn HW da) grün, Baseline-Diff sauber.
**Reversibel**: trivial — Header weg, Inline zurück.

### Schritt 2 — `logic/schedule_refresh.{h,cpp}`

- [ ] erledigt

- Extrahiere `needScheduleRefresh` 1:1.
- Extrahiere den per-stream Merge-Tail aus `refreshSchedule` als `applyScheduleFetchResult`.
- `main.cpp::refreshSchedule` wird Wrapper, der die Fetcher-Funktion aufruft und dann `applyScheduleFetchResult`.
- Host-Test `test_native_schedule_refresh`: Sentinel-Werte, 48-h-Cap, „nur überschreiben wenn Daten vorhanden".

**Validation**: `make test` grün.

### Schritt 2.1 — `DepartureSource`-Enum + `endpoint_responded`-Umbenennung

- [ ] erledigt

- `data/Departure.h`: `DepartureSource`-Enum (`Unknown` / `Realtime` / `Plan` / `Hint`), `Departure::is_realtime` durch `source` ersetzen. **Bewusst protokoll-agnostische Benennung** (kein `Ogd…`/`Efa…`-Präfix), v2-tauglich.
- `data/StreamSnapshot.h`: `rbl_responded` → `endpoint_responded`.
- [wienerlinien_parse.cpp](../src/data/wienerlinien_parse.cpp): setzt `source = Realtime` (wenn `timeReal` da) oder `Plan`; ersetzt `rbl_responded` durch `endpoint_responded`.
- [efa_parse.cpp](../src/data/efa_parse.cpp): **kein Touch nötig** — der EFA-Parser schreibt direkt in `ScheduleHint::last_today`/`first_tomorrow[]`, nicht in `Departure`s. Der `source = Hint` wird im Merger gesetzt, nicht hier.
- [slot_merger.cpp](../src/logic/slot_merger.cpp): bei Hint-Insertion `source = Hint` setzen.
- **Opportunistisch** (wir sind eh in beiden Parsern): `startsWith()` ist in [wienerlinien_parse.cpp:59–65](../src/data/wienerlinien_parse.cpp#L59) und [efa_parse.cpp:14–20](../src/data/efa_parse.cpp#L14) dupliziert. In neue Mini-Header `data/string_util.h` konsolidieren, beide Parser inkludieren.
- [filter_health.cpp](../src/logic/filter_health.cpp): Parameter heißt jetzt `endpoint_responded`.
- Tests in `test_native_wienerlinien_parse`, `test_native_efa_parse`, `test_native_slot_merger`, `test_native_filter_health` auf neue Feldnamen umstellen.

**Aufwand**: 0.5 d.
**Validation**: `make test` grün; Baseline-Diff zeigt veränderte Slot-Tags im Log (RT/PLAN/HINT), ansonsten identisch.

### Schritt 2.2 — `logic/render_input.{h,cpp}` + Minute-Dedup

- [ ] erledigt

- Neues Modul `logic/render_input.{h,cpp}` mit `composeRenderInput(snap, schedule, overlay, now)`.
- `slot_merger.cpp::insertSorted` Dedup auf Minute (`when/60 == cand.when/60`).
- Neue Tests:
  - `test_native_render_input`: drei Overlay-Pfade (None/Stale/FilterDead)
  - `test_native_slot_merger`: `test_minute_bucket_dedup_realtime_wins`
- Caller-Anpassung erfolgt in Schritt 7 (cycle_runner). Bis dahin ist `composeRenderInput` ungenutzt; `main.cpp` ruft weiter direkt `mergeSlots`.

**Aufwand**: 0.5 d.
**Validation**: `make test` grün. Long-term-Smoke gegen Baseline aus 0b — Drift im Minute-Dedup-Pfad wird erwartet (z. B. morgens, wenn Realtime ins 70-min-Fenster kriecht), Drift muss erklärbar sein.

### Schritt 2.3 — `ScheduleHint::next_today[2]` (Lücke aus Smell 13 schließen)

- [x] erledigt

Das ist der substantielle Schritt — eine **Anforderungslücke** schließen, nicht refactoring.

1. `data/ScheduleHint.h`: Feld `time_t next_today[2] = {0, 0}` hinzu.
2. [efa_parse.cpp](../src/data/efa_parse.cpp): behält die letzten 2 Einträge mit `dateTime < cutoff` (heute werden alle verworfen außer `last_today`).
3. [schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp): Mapping in `ScheduleFetchResult.hint[i].next_today` schreiben.
4. [slot_merger.cpp](../src/logic/slot_merger.cpp): zweite Hint-Quelle parallel zu `first_tomorrow`:

   ```cpp
   if (use_schedule) {
     for (int i = 0; i < 2; ++i)
       insertSorted(merged, hintDep(schedule.hint[s].next_today[i], now));
     for (int i = 0; i < 2; ++i)
       insertSorted(merged, hintDep(schedule.hint[s].first_tomorrow[i], now));
   }
   ```

   Reihenfolge egal — `insertSorted` sortiert nach `when`.
5. [hal/Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp): `SCHED_MAGIC` bumpen (`0x5CEDB051` → `0x5CEDB052`). Einmaliger Hint-Verlust beim Update, nächster Cycle re-fetcht.
6. `needScheduleRefresh`: bleibt unverändert. `last_today` ist semantisch redundant zu `next_today[1]` ab fetch-time, aber bleibt erhalten — Umbenennung in eigener PR.
7. Tests:
   - `test_native_efa_parse`: neue Fixture-Variante mit 4+ Today-Einträgen, Assert auf `next_today[0..1]`-Belegung
   - `test_native_schedule_fetcher`: Mapping testen
   - `test_native_slot_merger`: neuer Test „Realtime leer, `next_today[0]` zwei Stunden voraus → Display zeigt's"
   - `test_longterm_horizon_evening`: Asserts umstellen — heute prüft der Test „nach 21:00 Display zeigt `—:—`". Wird zu „nach 21:00 Display zeigt die nächsten Plan-Abfahrten" (Verhaltensänderung, R11).

**Doku-Folge-Item**: [CONCEPT.md §12.4](../CONCEPT.md) muss aktualisiert werden, weil die Hint-Quelle nicht mehr nur `first_tomorrow` ist. Findet im selben Commit statt.

**Aufwand**: 1.5–2 d.
**Validation**: `make test` grün; `make test-longterm-horizon-evening` grün mit umgestellten Asserts; Long-term-Baseline-Diff zeigt erwartete Drift in Abend-Fenstern.

### Schritt 3 — `logic/snapshot_logger.{h,cpp}` + `data/stream_labels.h`

- [x] erledigt

- Neue Mini-Datei `data/stream_labels.h` mit `STREAM_LABEL[]` und `streamLabel(int)`. **Einzige Stelle**, an der v2 die zwei letzten Stream-Labels ersetzt.
- Extrahiere `logSlot` als `formatSlot(const Departure&) → std::string`. Drei Tags (`RT`/`PLAN`/`HINT`) dank Schritt 2.1.
- Extrahiere den 30-Zeilen-Summary-Block (`fetchSnapshot` Z. 165–188) als `formatSnapshotSummary(...)`. Schleife über `streamLabel(i)` statt 10 hartkodierte Zeilen.
- `main.cpp` ruft `Serial.printf("%s", formatXxx(...).c_str())`.
- Host-Test `test_native_snapshot_logger` prüft Format gegen Fixture-StreamSnapshots.

**Validation**: `make test` grün. **Das Serial-Log wird hier per Test festgenagelt** — gegen die Baseline aus Schritt 0b prüfen, ob identisch (modulo die `RT/PLAN/HINT`-Tag-Differenz aus 2.1).

### Schritt 4 — `logic/snapshot_fetcher.{h,cpp}`

- [x] erledigt

- Verschiebe `apiUrlForBatch` (intern) und `fetchSnapshot` (public) ohne Logik-Änderung.
- Argumente: `(INetwork&, const StreamFilter (&)[N], StreamSnapshot& out, FetchSummary& summary)`.
- Logging im Inneren bleibt vorerst per `Serial.printf` — die pro-Batch-Logs sind anders strukturiert als der Summary-Block, und „Logger als Callback" wäre Overkill.
- Host-Test `test_native_snapshot_fetcher` mit `FakeNet`, der canned Bodies pro URL liefert.

**Validation**: `make test` grün; auf HW `make test-device-trace ENV=device-fetch` gegen Baseline (Schritt 0b) diff'en.

### Schritt 5 — `IClock::isSynced()` + NTP-Magic eliminieren + tote Konstanten

- [x] erledigt

- `MIN_PLAUSIBLE_EPOCH` + Default-Implementation in [IClock.h](../src/hal/IClock.h).
- Alle fünf Vorkommen der `1700000000`-Konstante ersetzen:
  - `main.cpp:368`
  - `test_device_fetch:164/204/258` (3×)
  - [Esp32Clock.cpp:29](../src/hal/Esp32Clock.cpp#L29) — die `ntpSync()`-Erfolgsschwelle wird zu `now() >= MIN_PLAUSIBLE_EPOCH`.
- **Opportunistisch**: `RLE_BUDGET_BYTES` aus [config.h:99](../src/config.h#L99) entfernen. Wird im ganzen Repo nirgends gelesen — nur `RLE_HARDCAP_BYTES` ist tatsächlich verdrahtet ([Esp32PersistentStore.cpp:16](../src/hal/Esp32PersistentStore.cpp#L16)). 1-Zeilen-Delete, kein Verhaltens-Impact.

**Validation**: `make test` grün, `test_device_fetch` grün.

### Schritt 6 — `logic/button_classifier.{h,cpp}` + `Esp32Button` Adapter

- [x] erledigt

- `classifyHeld(IButton&, uint32_t long_press_ms) → ButtonPress`.
- `Esp32Button` Adapter mit `pinMode/digitalRead/millis/delay`.
- `main.cpp` behält nur das Erst-Polling (Pin-Read, ob LOW) — die Klassifikation läuft über `classifyHeld`.
- Host-Test `test_native_button_classifier` mit FakeButton-Sequenz.

**Validation**: `make test` grün. Der manuelle Button-Smoke (kurz + lang) wandert nach **Schritt 12 — Manuelle HW-Verifikation am Ende** — Schritt 6 selbst läuft ohne Mensch-Interaktion durch.

### Schritt 7 — `logic/cycle_runner.{h,cpp}` (das große Stück)

In drei Sub-Schritten aufgegliedert. **Native-Clean-Pflicht für alle drei**: kein `cycle_runner`- oder `IRenderer`-Header darf `Arduino.h`/`<WiFi.h>`/`Esp32*` direkt oder transitiv inkludieren. CI-`make ci` baut `native` und fängt Regression früh. Voraussetzung für Schritt 9 (Native-Runtime).

#### Schritt 7.1 — `IRenderer`-Abstraktion + `Esp32Renderer`

- [x] erledigt

Isolierter erster Schnitt: nur die HAL-Abstraktion für den Renderer, damit `cycle_runner` (7.2) ohne `render/layout.cpp`-Touch native-kompilierbar wird.

Neuer Header `hal/IRenderer.h`:

```cpp
class IRenderer {
 public:
  virtual ~IRenderer() = default;
  virtual void render(const RenderInput&, Frame&) = 0;
};
```

- Neuer Adapter `hal/Esp32Renderer.{h,cpp}` — 1-Funktion-Wrapper um `renderFrame(in, fb)`.
- `main.cpp` (interim) instanziiert `Esp32Renderer g_renderer;` zusätzlich zu den anderen HAL-Globals. Bleibt vorerst ungenutzt, weil `main.cpp::renderAndPush` heute noch direkt `renderFrame` aufruft.

**Validation**: `make test` grün; `make build` grün; CI grün (native-build prüft, dass `hal/IRenderer.h` ohne `Arduino.h` kompiliert).

#### Schritt 7.2 — `cycle_runner`-Grundgerüst + Tier 1 Tests

- [x] erledigt

Das eigentliche Tragwerk: HAL-Globals → CycleDeps → freie Funktionen, Code-Wanderung aus `main.cpp`.

- `CycleDeps`-Struct + freie Funktionen `runColdCycle`/`runWarmCycle`/`runButtonWake`/`pollButtonAndRunWarm`/`runBwReset` in `logic/cycle_runner.{h,cpp}`.
- Ziehe `renderAndPush`, `doSleepOrLoop`, `coldBootPath`, `warmCyclePath` in die `cycle_runner.cpp` um. `composeRenderInput` (Schritt 2.2) wird *jetzt* eingebaut — kein Doppelmerge mehr, ein Overlay-Branch pro Cycle.
- Gemeinsamer Pfad `doFetchCycle()` — kein Copy/Paste mehr.
- Static `FilterHealth fh` → lokale Variable, im Cycle aus Meta hydratisiert.
- `LONG_SLEEP_FOR_NIGHTLY_CLEAN_S` + `NIGHTLY_DEEP_CLEAN_INTERVAL_S` in `config.h`.
- **Toter Stale-Render-Call entfernen**: heute ruft [main.cpp:357](../src/main.cpp#L357) `renderStaleFrame(g_frame_new)` und im *nächsten* Statement `renderAndPush({}, OverlayKind::Stale, …)`, das `g_frame_new` sofort überschreibt. Der erste Aufruf ist literally redundant — entfällt ersatzlos im neuen `cycle_runner`-Pfad.
- **`render/error_overlay.{h,cpp}` ganz löschen, sobald `renderStaleFrame` weg ist**. Der verbleibende `renderStartFailedFrame` ist ein 4-Zeilen-Wrapper um `renderFrame({StreamSnapshot{}, OverlayKind::StartFailed}, fb)` — Cold-Boot-Give-Up-Pfad ruft direkt, Module verschwindet. 2 Dateien gelöscht.

**Tier 1 Tests** (in diesem Sub-Schritt):

- `test_native_cycle_runner_helpers` — die privaten Helper (`doFetchCycle` bzw. dessen extrahierter Unter-Schnitt, `LONG_SLEEP_FOR_NIGHTLY_CLEAN`-Logik) gegen Fakes.

**Validation**: `make test` grün; `make ci` (native-build clean); `make test-device` grün; `make test-longterm-smoke` für HW-Sanity.

#### Schritt 7.3 — Tier 2 + Tier 3 Tests

- [x] erledigt

Die Schwergewicht-Tests, die den eigentlichen R1-Drift-Killer aufstellen.

- **Tier 2** — Call-Sequenz-Recording mit Recording-Fakes (siehe [§5.2](#52-tier-2-call-sequenz-recording-der-eigentliche-r1-killer) für Detailskizze): `test_native_cycle_runner_warm_happy`, `_wifi_down`, `_ntp_fail`, `_fetch_fail_prestale`, `_fetch_fail_stale`, `_cold_happy`. Jede Variante mit erwarteter Call-Sequenz als String-Array.
- **Tier 3** — Property-Tests (siehe [§5.3](#53-tier-3-property-tests)): `test_native_cycle_runner_invariants` — „nie `deepSleep > 24 h`", „`saveMeta` genau 1× pro Cycle", „`renderer.render` höchstens 1× pro Cycle".

**Validation**: `make test` grün; `make ci`; `make test-longterm-soak-15min` als Heap-Confidence (jetzt mit Display-Pfad, dank Schritt 0a).

### Schritt 8 — `main.cpp` schlank

- [x] erledigt

- File auf ~80 LOC reduzieren: HAL-Globals, `makeDeps()`, setup/loop, registerWifi.
- Header-Includes ausdünnen.

**Validation**: `make test`, `make test-device`, `make test-longterm-smoke`, `make test-longterm-soak-15min` mit Baseline-Diff.

### Schritt 9 — `test/test_native_runtime/` — Native-Device-Loop einrichten

- [x] erledigt

Headless-Loop, die `runWarmCycle`/`runColdCycle` auf dem Host gegen *echte* WL/EFA-Endpoints fährt. Erst jetzt möglich, weil Schritte 7+8 `cycle_runner` native-clean gemacht haben. In fünf Sub-Schritten aufgegliedert; Gesamt-Aufwand ~2.5 d (~650 LOC).

#### Schritt 9.1 — Trivial-Adapter (Clock + Sleep + Display + Store)

- [x] erledigt

Die vier HAL-Implementations ohne externe Dependencies, ~250 LOC, 0.5 d.

- **`WallClockClock`** (IClock): wraps `std::chrono::system_clock`. `isSynced()` immer true. `ntpSync()` no-op (Host hat NTP via OS).
- **`NoOpSleep`** (ISleep): `deepSleep(s)` → `std::exit(0)` + Re-Start-Skript (oder: `std::this_thread::sleep_for`, je nach Mode); `lightSleep(s)` → `sleep_for`; `wakeupCause()` → `Timer` (Cold-Boot nur via leerem `persist.bin`).
- **`NoOpDisplay`** (IDisplay): zählt nur Aufrufe, kein Pixel-Output. Die Pixel-Inspektion läuft über den `RecordingRenderer` (siehe 9.3).
- **`DiskStore`** (IPersistentStore): persistiert `PersistedMeta`, `ScheduleSnapshot`, und das RLE-komprimierte Framebuffer in `.tmp/native-runtime/persist.bin`. Cold-Boot = Datei löschen.

**Validation**: kleine Host-Unit-Tests für `DiskStore`-Roundtrip (analog `test_native_rle`); ASan-clean.

#### Schritt 9.2 — `HttpsNet` via libcurl

- [x] erledigt

INetwork-Implementation gegen die echten Endpoints, ~100 LOC, 0.5 d.

- libcurl gegen `wienerlinien.at/ogd_realtime/monitor` und `…/XSLT_DM_REQUEST` (`useRealtime=0`).
- Cert-Chain über System-Default; Fallback `CURLOPT_CAINFO` auf bundled PEM (siehe R12).
- Optional ein Mock-Mode für `MOCK_API_BASE`-URL (analog `test_longterm_horizon_mock`) — Default: live.
- Timeout-Verhalten mirrorisch zum [Esp32Network](../src/hal/Esp32Network.cpp) (8 s).

**Validation**: 3 manuelle Smoke-Calls gegen die Produktions-URL, jeweils Body-Size > 1 kB und JSON-`monitors`-Key vorhanden.

#### Schritt 9.3 — `RecordingRenderer` mit PGM-Output

- [x] erledigt

IRenderer-Implementation mit deterministischem Pseudo-Raster + diff-getriebenem Dump, ~150 LOC, 0.5 d.

```cpp
class RecordingRenderer : public IRenderer {
  Frame prev_;
  int seq_ = 0;
  bool first_ = true;
public:
  void render(const RenderInput& in, Frame& fb) override {
    renderDeterministic(in, fb);   // Pseudo-Raster aus Slot-Inhalts-Hash
    if (first_ || memcmp(prev_.data(), fb.data(), Frame::bytes) != 0) {
      writePgm(".tmp/native-runtime/frame-" + zeropad(seq_++, 6) + ".pgm", fb);
      memcpy(prev_.data(), fb.data(), Frame::bytes);
      first_ = false;
    }
  }
};
```

Diff-getrieben: PGM-Anzahl entspricht der Anzahl der Display-Updates → Heap-Leak im Render-Pfad korrelierbar gegen PGM-Count.

**Validation**: Unit-Test gegen denselben Input zweimal → genau 1 PGM geschrieben (Dedup-Check).

#### Schritt 9.4 — Treiber `test/test_native_runtime/main.cpp`

- [x] erledigt

Loop-Treiber, ~100 LOC, 0.5 d.

- Lädt Config (Endpunkte, Poll-Intervall).
- Baut `CycleDeps` aus den fünf Native-HAL-Adaptern aus 9.1–9.3.
- Ruft `runColdCycle` einmal, dann `runWarmCycle` in Wandzeit-Cadence (`POLL_INTERVAL_S` Sleep zwischen Polls).
- Signal-Handler für SIGTERM/SIGINT, sauberer Shutdown.

**Validation**: 3 Cycles laufen lassen, Output in `.tmp/native-runtime/` plausibel.

#### Schritt 9.5 — Build-Setup + CI-Wiring + README

- [x] erledigt

Glue + Doku, ~50 LOC, 0.5 d.

- Neuer `platformio.ini`-Env `native-runtime` (separat von `native`, weil andere Source-Set und libcurl-Linkage).
- Build-Define `BUSTAFERL_NATIVE_RUNTIME` zur Conditional-Compilation in `cycle_runner.cpp`, falls dort native-spezifische Anpassungen nötig werden (Default: keine).
- Make-Targets:
  - `make native-runtime-smoke` — 10 Cycles, ~5 min, asserts: keine Crashes, `valgrind --error-exitcode=1` exit 0.
  - `make native-runtime-day` — 24 h, manuell gestartet, schreibt `.tmp/native-runtime/`-Sammlung für Review.
- CI-Wiring: `native-runtime-smoke` in `make ci` aufnehmen (~5 min budget).
- README in `test/test_native_runtime/` erklärt: was es testet, wie ausführen, was die PGMs bedeuten.

**Validation Schritt 9 gesamt**:

- `make native-runtime-smoke` grün (10 Cycles, valgrind sauber).
- Auf einem Test-Host: `make native-runtime-day` über 24 h. PGM-Verzeichnis manuell inspizieren auf Plausibilität (Frame-Anzahl im Tag-Mittel < 200, Heap-Profil über massif gleichmäßig).

#### Was Schritt 9 *nicht* leistet (aus §3.3 herübergezogen)

- **Adafruit-GFX-Layout** — RecordingRenderer baut nur Pseudo-Raster. Display-Layout-Bugs sind weiter Geräte-Test-Stoff.
- **e-Paper-Refresh-Artefakte** — kein Panel im Native-Pfad.
- **GPIO-Verhalten** — Button-Pfad wird nicht durch Schritt 9 gefahren.

### Schritt 10 — Doku

- [x] erledigt

- [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) §Modulkarte um die neuen `logic/`-Module und das `test_native_runtime/`-Setup ergänzen.
- [docs/TESTING.md](../docs/TESTING.md) Test-Pair-Tabelle ergänzen, `native-runtime`-Bucket aufnehmen.
- `CONCEPT.md §12.4`: Update bereits in Schritt 2.3 vorgenommen — hier nur prüfen, dass die übrigen Schritte konsistent sind.
- `markdownlint-cli2 --fix` auf alle berührten Files.

### Schritt 11 — Post-Refactor Tooling-Härtung

- [x] erledigt (alle drei Unter-Schritte 11.1/11.2/11.3 in Group A vorgezogen, siehe §4.1)

Erst *nach* Schritt 8 (`main.cpp` schlank), weil sonst die ≤60-LOC-Regel die Zwischenzustände der Schritte 1–7 blockieren würde.

#### 11.1 — `.clang-tidy` einchecken

- [x] erledigt — **vorgezogen in Group A**, siehe §4.1-Annahme. Alle ursprünglichen `src/`-Findings behoben, `WarningsAsErrors: '*'` scharf.

```yaml
# .clang-tidy
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  readability-function-size,
  readability-function-cognitive-complexity,
  readability-magic-numbers,
  readability-identifier-naming,
  readability-redundant-*,
  readability-simplify-*,
  cppcoreguidelines-pro-type-cstyle-cast,
  cppcoreguidelines-pro-type-static-cast-downcast,
  cppcoreguidelines-init-variables,
  cppcoreguidelines-slicing,
  modernize-use-nullptr,
  modernize-use-override,
  modernize-use-default-member-init,
  modernize-loop-convert,
  modernize-use-equals-default,
  modernize-use-equals-delete,
  performance-*
WarningsAsErrors: '*'
HeaderFilterRegex: '^src/(data|logic|hal|render)/.*\.h$'
CheckOptions:
  readability-function-size.LineThreshold: 60
  readability-function-size.StatementThreshold: 50
  readability-function-size.BranchThreshold: 10
  readability-function-size.ParameterThreshold: 6
  readability-function-size.NestingThreshold: 5
  readability-function-cognitive-complexity.Threshold: 25
  readability-magic-numbers.IgnorePowersOf2IntegerValues: true
  readability-magic-numbers.IgnoredIntegerValues: '0;1;2;3;-1;100;1000'
```

**Begründung der Funktions-Schwellen:**

- `LineThreshold: 60` — `fetchSnapshot` (heute 88) und `warmCyclePath` (111) reißen das mit Anlauf. Nach Refactor sollten alle Funktionen ≤ 50 sein; 60 als Puffer.
- `NestingThreshold: 5` — vier OK, fünf grenzwertig, sechs splittet sich selbst.
- `BranchThreshold: 10` — `planSleep` hat heute ~6 Branches, `planRefresh` ~5. Headroom × 2.
- `ParameterThreshold: 6` — `CycleDeps`-Struct ist genau dafür da, dass `runWarmCycle` nicht 8 Parameter hat.
- `cognitive-complexity Threshold: 25` — moderater Default; trifft sehr verschachtelte Logik.
- `IgnoredIntegerValues` für `magic-numbers` lässt 0/1/2/3 in Schleifen-Indizes, -1 als Error-Sentinel, 100/1000 für ms-Konvertierungen durch.

#### 11.2 — `make tidy` + CI-Wiring

- [x] erledigt — **vorgezogen in Group A**, siehe §4.1-Annahme. `make tidy` läuft `pio run -t compiledb` → `-isystem`-Rewrite für Lib-Pfade → clang-tidy nur auf host-kompatible TUs. `make ci` ruft jetzt `format-check + lint + tidy + test-native + build`.

```text
tidy:                  ## run clang-tidy on changed translation units
    @$(PIO) run -e native -t compiledb
    @find src -name '*.cpp' -not -path '*/fixtures/*' \
      | xargs clang-tidy -p .pio/build/native --quiet
```

`make ci` erweitert um `tidy`:

```text
ci: format-check lint tidy test-native build
```

#### 11.3 — Pre-Commit-Hook automatisch installieren

- [x] erledigt — **vorgezogen in Group A**. `scripts/install-pre-commit.sh` installiert den `make ci`-Hook (~30 s Pre-Commit-Runtime).

`scripts/install-pre-commit.sh`:

```bash
#!/bin/sh
set -e
cat > .git/hooks/pre-commit <<'EOF'
#!/bin/sh
exec make format-check lint tidy test-native
EOF
chmod +x .git/hooks/pre-commit
echo "pre-commit hook installiert; entfernen mit: rm .git/hooks/pre-commit"
```

Hook-Dauer ~10–20 s (`make format-check` + `make lint` + `make tidy` + `make test-native`). Innerhalb des akzeptablen Pre-Commit-Budgets. Schritt 11.3 läuft das Script einmal — danach läuft der Hook automatisch.

**Aufwand Schritt 11**: 1 d, davon der Großteil für die Baseline-Bereinigung neuer clang-tidy-Findings auf dem refactorierten Code (`magic-numbers` für ms/sleep-Konstanten, `cognitive-complexity` für Stellen, die der Refactor übersehen hat).
**Validation**: `make ci` grün inkl. `tidy`; alle Funktionen erfüllen die Schwellen.

### Schritt 12 — Manuelle HW-Verifikation am Ende

- [ ] erledigt

Sammelt alle Test-Schritte, die zwingend Mensch-Interaktion benötigen und während der autonomen Umsetzung gepark wurden. Der Auftraggeber ist hier wieder im Loop.

#### 12.1 — Manueller Button-Smoke

- [ ] erledigt

Verschoben aus Schritt 6 (R4-Mitigation): Button-Pfad-Regression ist nicht in CI fangbar.

- Gerät frisch geflasht mit dem refactorierten Build.
- **Kurzer Druck** auf BOOT-Button → Anzeige aktualisiert sich (warmCycle wird ausgelöst).
- **Langer Druck (≥ 2 s)** auf BOOT-Button → B/W-Deep-Clean läuft sichtbar (3× Flash), letzter Frame wird neu gezeichnet.

#### 12.2 — Visuelle Display-Verifikation Overlay-Zustände

- [ ] erledigt

Sichtprüfung, ob die Layout-Ausgabe nach dem Refactor unverändert ist (Schichten-Test gegen Adafruit-GFX-Wand).

- Normaler Zustand (Realtime + Hint): Slots, Stream-Beschriftungen, Trennlinien wie heute.
- Stale-Overlay (WiFi-Down provozieren, 3+ min warten): `??:??` überall, Banner „VERALTET".
- FilterDead-Overlay (per Test-Build-Define o. Ä. erzwingen): Banner „58B Filter ungueltig".
- StartFailed-Overlay (per leerer `secrets.h` o. Ä. erzwingen): Banner „Start fehlgeschlagen".

#### 12.3 — Optional: Finaler 24-h-day-full-Soak

- [ ] erledigt

Wenn vor einem Tag-Release gewünscht (kein Pflicht-Gate für den Refactor selbst):

- `make test-longterm-day-full` (24 h, unbeaufsichtigt).
- Auswertung am nächsten Tag.

**Aufwand**: 0.5 d (Schritt 12.1 + 12.2 zusammen ~30 min mit Auftraggeber; 12.3 ist Wand-Uhr-Zeit, nicht Arbeits-Zeit).

---

## 5. Risiken

| #  | Risiko                                                             | Wahrscheinlichkeit | Impact | Mitigation                                                                              |
|----|---------------------------------------------------------------------|--------------------|--------|------------------------------------------------------------------------------------------|
| R1 | Subtile Verhaltensdrift im Sleep/Render-Pfad (Reorder)              | mittel             | hoch   | Schritt 0b Baseline-Capture; Schritt 7 Call-Sequenz-Recording-Tests (§5.2); Long-term-Soak vor Commit; Schritt 9 valgrind als zusätzlicher Korrektheits-Check. |
| R3 | Framebuffer auf Stack rutscht                                       | niedrig            | hoch   | `g_frame_new`/`g_frame_prev` bleiben file-scope in `main.cpp`. `CycleDeps` hält nur Referenzen. PR-Checkliste: kein lokales `Frame f;`. |
| R4 | Button-Pfad-Regression (Long-Press wird nicht mehr erkannt)         | mittel             | mittel | `test_native_button_classifier` mit Held-Pattern, **plus** manueller Smoke-Test in Schritt 12.1 — nicht in CI. |
| R5 | `fetchSnapshot`-Logbild ändert sich versehentlich                  | mittel             | niedrig| Schritt 3 fixiert Format als Host-Test. Schritt 4 ändert das nicht. Baseline-Diff (Schritt 0b) fängt es.  |
| R6 | Native-Build bricht (`cycle_runner` zieht Arduino transitiv rein)   | mittel             | mittel | **Schritt 7 ist Pflicht-native-clean**, **Schritt 9 validiert es real** (Build-Failure = Test-Failure). CI-`make ci` baut `native` und fängt es. |
| R7 | RTC-Datenlayout-Bumps fehlerhaft                                    | niedrig            | hoch   | Schritt 2.3 bumpt `SCHED_MAGIC`; `MAGIC` (Framebuffer) bleibt unangetastet. Test: erster Boot nach Update zeigt korrekt re-fetchten Hint statt Müll. |
| R8 | Long-term-Tests müssen ggf. neu kalibriert werden                  | niedrig            | mittel | Soak/Heap-Tests messen Steady-State; Refactor sollte sie nicht verschieben. Falls doch: kalibrieren *bevor* tag/release.                  |
| R9 | Ein Schritt ist zu groß für „eine sitzende Commit-Einheit"          | hoch               | niedrig| Plan ist in 18 Top-Level-Schritte (0a, 0b, 0c, 1, 2, 2.1, 2.2, 2.3, 3–12) mit feingranularen Sub-Schritten (0c.1–5, 7.1–3, 9.1–5, 11.1–3, 12.1–3) zerlegt — bis zu ~33 atomare Commits möglich. Top-Level-Checkbox dient als „alle Sub-Schritte erledigt"-Rollup; Sub-Step-Commits dürfen smart batched werden (siehe §4.0.4). |
| R10 | Minute-Dedup (2.2) ändert Verhalten in Übergangs-Minuten            | mittel             | niedrig | Erwartete Drift im Baseline-Diff (0b). Manuell prüfen, dass Drift nur in Realtime-kreuzt-Hint-Minuten auftritt, sonst unverändert. |
| R11 | Heute-Abend-Bridge (2.3) füllt Striche-Slots                        | mittel             | niedrig | Erwartete Drift im Baseline-Diff (0b) und in `horizon_evening`. Asserts dort umstellen. Verhalten ist gewünscht (Smell 13). Doku-Update CONCEPT §12.4 zwingend mit. |
| **R12** | **libcurl-Cert-Bundle auf CI-Host fehlt** (Schritt 9)            | **niedrig**        | **niedrig** | **`HttpsNet` defaultet auf System-Cert-Store; CI-Image-Hinweis im README**. Fallback: explizit `CURLOPT_CAINFO` auf bundled PEM. |
| **R13** | **`-Wconversion`/`-Wsign-conversion`-Baseline ist umfangreicher als geschätzt** (Schritt 0c) | **mittel** | **niedrig** | Schätzung 20–40 Touch-Sites; reale Zahl kann höher sein. Mitigation: Schritt 0c.3 ist explizit als 0.5–1.5 d budgetiert. Falls einzelne Stellen unfixbar (Library-Header), lokal mit `#pragma GCC diagnostic` einkapseln statt globales Disable. |
| **R14** | **Library-Header (ArduinoJson, GxEPD2) emittieren Pedantic-Warnings** (Schritt 0c) | **niedrig**        | **mittel** | Library-Pfade per `-isystem` markieren — PlatformIO-Default ist `-I`. Fallback: `#pragma GCC diagnostic push/pop` um problematische Includes. Falls beides scheitert: einzelne `-W…` selektiv deaktivieren mit `-Wno-…` und Begründung im platformio.ini-Kommentar. |

(R2 „Static-Init-Order der Engine-Singleton" entfällt — `CycleDeps`-Variante hat keine globale Engine.)

### 5.1 Test-Gates pro Schritt (Stand 2026-05-18)

| Schritt | Vor-Commit-Gate                          | Wenn HW verfügbar                                       |
|---------|-------------------------------------------|----------------------------------------------------------|
| 0a      | `make test-longterm-smoke`               | `make test-longterm-soak-15min` als neue Baseline       |
| 0b      | n/a                                      | drei Baseline-Capture-Läufe (soak + device-fetch + horizon-evening) |
| **0c**  | **`make ci` grün (incl. neue Warnings + ASan)** | GitHub-Actions-CI grün am Branch-Push           |
| 1, 2    | `make test`                              | (optional `make test-device`)                            |
| 2.1     | `make test`                              | `make test-device` (Slot-Tag-Diff erwartet)              |
| 2.2     | `make test`                              | `make test-longterm-smoke` + Baseline-Diff               |
| 2.3     | `make test`                              | `make test-longterm-horizon-evening` (umgestellte Asserts)|
| 3       | `make test`                              | (optional `make test-device-trace`)                      |
| 4       | `make test`                              | `make test-device-trace ENV=device-fetch` (Diff vs. Baseline) |
| 5       | `make test`                              | (optional `make test-device`)                            |
| 6       | `make test`                              | `make test-device-sleep` (Button-Wake-Cause-Pfad)        |
| 7.1     | `make test` + `make ci` (native-build prüft IRenderer-Header) | `make build` grün                    |
| 7.2     | `make test` + `make ci` (Tier 1 Tests)   | `make test-device` + `make test-longterm-smoke`         |
| 7.3     | `make test` + `make ci` (Tier 2+3 Tests) | `make test-longterm-soak-15min` (Display-Pfad + Drift-Diff) |
| 8       | `make test` + `make ci`                  | `make test-device` + `make test-longterm-soak-15min` (Diff vs. Baseline) |
| **9**   | **`make native-runtime-smoke` (valgrind)** | **`make native-runtime-day` auf Test-Host** |
| 10      | `markdownlint-cli2 --fix`                | n/a                                                     |
| **11**  | **`make ci` (incl. `make tidy`)**        | GitHub-Actions-CI grün; Funktions-Längen-Schwellen erfüllt |
| **12**  | **Auftraggeber im Loop**                 | **Button-Smoke (12.1) + visuelle Overlay-Inspektion (12.2)** |

### 5.2 Tier-2: Call-Sequenz-Recording (der eigentliche R1-Killer)

Im Hosttest baut man Fakes, die nicht nur Daten zurückgeben, sondern jede Methode in einen `std::vector<std::string>` als Trace-Zeile aufnehmen:

```cpp
class RecordingNet : public INetwork {
  std::vector<std::string>& trace_;
  bool ok_;
  std::string canned_body_;
public:
  RecordingNet(std::vector<std::string>& t, bool ok = true) : trace_(t), ok_(ok) {}
  bool connect(unsigned ms) override {
    trace_.push_back("net.connect(" + std::to_string(ms) + ")");
    return ok_;
  }
  bool httpGet(const std::string& url, std::string& out) override {
    trace_.push_back("net.httpGet(" + truncate(url, 40) + ")");
    out = canned_body_;
    return ok_;
  }
  bool isConnected() override { trace_.push_back("net.isConnected"); return ok_; }
};
```

Analog `RecordingClock`, `RecordingSleep`, `RecordingStore`, `RecordingDisplay`, `RecordingRenderer`. Dann:

```cpp
void test_warm_cycle_ordering_happy_path() {
  std::vector<std::string> trace;
  RecordingNet net{trace, /*ok=*/true};
  RecordingClock clk{trace, /*synced=*/true, /*now=*/1736000000};
  RecordingSleep sleep{trace};
  RecordingStore store{trace};
  RecordingDisplay display{trace};
  RecordingRenderer renderer{trace};
  Frame curr, prev;
  CycleDeps deps{clk, net, sleep, store, display, renderer, curr, prev};
  PersistedMeta meta;

  runWarmCycle(deps, meta);

  std::vector<std::string> expected = {
    "store.loadSchedule",
    "net.connect(10000)",
    "clock.now",
    "clock.isSynced -> true",
    "net.httpGet(...stopId=...)",   // batch 1
    "net.httpGet(...stopId=...)",   // batch 2
    "net.httpGet(...stopId=...)",   // batch 3
    "clock.now",
    "renderer.render",
    "store.loadFramebuffer",
    "display.drawPartial(bbox=...)",
    "store.saveFramebuffer",
    "store.saveMeta",
    "sleep.deepSleep(...)",
  };
  TEST_ASSERT_EQUAL_STRING_ARRAY(expected, trace);
}
```

Plus negative Varianten: `_wifi_down`, `_ntp_fail`, `_fetch_fail_prestale`, `_fetch_fail_stale`, `_cold_happy`. Jede mit eigener erwarteter Sequenz.

### 5.3 Tier-3: Property-Tests

```cpp
void test_warm_cycle_never_calls_deepSleep_with_huge_value() {
  for (auto preset : {ClockState::synced, ClockState::unsynced_pre_boot}) {
    // ...
    runWarmCycle(deps, meta);
    TEST_ASSERT_LESS_THAN(86400U, sleep.last_deep_sleep_seconds);
  }
}
void test_warm_cycle_saves_meta_exactly_once() {
  // ...
  TEST_ASSERT_EQUAL(1, store.save_meta_count);
}
```

Fängt genau die Klasse von Bugs, die der `[engine]`-Bug aus [test/test_device_fetch/test_main.cpp](../test/test_device_fetch/test_main.cpp) Z. 157–195 ursprünglich verursacht hat.

### 5.4 Aufwands-Schätzung

| Block                              | Aufwand     |
|------------------------------------|-------------|
| Schritt 0a + 0b (Prereqs)           | 1.5–2 d     |
| **Schritt 0c (Tooling-Härtung pre)**| **0.5–1.5 d** |
| Schritte 1, 2, 2.1, 2.2             | 1.5–2 d     |
| Schritt 2.3 (`next_today` feature)  | 1.5–2 d     |
| Schritte 3–6                       | 1.5–2 d     |
| Schritt 7 (`cycle_runner` + Tests)  | 3–4 d       |
| Schritt 8 (main.cpp schlank)        | 0.5 d       |
| Schritt 9 (Native-Runtime)          | ~2.5 d      |
| Schritt 10 (Doku)                  | 0.5 d       |
| **Schritt 11 (Tooling-Härtung post)** | **~1 d**  |
| **Schritt 12 (Manuelle HW-Verifikation)** | **~0.5 d** |
| **Total**                           | **~16.5–21 d** |

---

## 6. Zwei native-Pfade — Begrenzungen

Die in Schritt 9 entstehende Native-Runtime ersetzt **nicht** die Long-term-Hardware-Tests. Klare Trennung:

| Pfad                    | Zeit                | API                      | Wofür                                                       |
|-------------------------|---------------------|--------------------------|-------------------------------------------------------------|
| **native-runtime-live** | Wandzeit, ehrlich   | echte WL/EFA-Endpoints   | Heap-Leak via valgrind, RLE-Overflow, Render-Drift          |
| **native-mock-horizon** | beschleunigt        | Python-Mock (wie heute)  | EFA-Fallback-Logik, Übergänge — `horizon_mock`-Pattern      |

**Wichtig:** Zeit kann nicht beschleunigt werden, wenn gegen die echte API gesprochen wird — der API-Response zur Sprung-Zeit wäre „dieselbe Antwort wie vor 5 s". Die zwei Pfade sind nicht mischbar. Long-term-Tests auf Hardware bleiben unverzichtbar in der **Dauer**; Native-Runtime gewinnt in der **Diagnostik-Tiefe** (valgrind, massif, PGM-Dumps, kein Flash-Zyklus).

---

## 7. Post-Refactor TODOs (nicht in diesem Branch)

Sammlung von Befunden aus dem Code-Sweep außerhalb `main.cpp`, die *nicht* in den Refactor mitfahren, aber nach Abschluss systematisch betrachtet gehören.

### 7.1 Heap-Wächter in `schedule_fetcher.cpp` — Verdachtsmoment, kein Fix-Plan

[logic/schedule_fetcher.cpp:11–36](../src/logic/schedule_fetcher.cpp#L11) baut eine ungewöhnlich aufwendige Verteidigung gegen ESP32-TLS-Heap-Fragmentation:

- `SCHED_MIN_FREE_HEAP 90000u` — bail before-the-call, wenn freier Heap < 90 KB.
- `SCHED_MIN_LARGEST_BLOCK 60000u` — bail, wenn kein 60-KB-Block zusammenhängend frei ist.
- Zusätzlich [delay(150)](../src/logic/schedule_fetcher.cpp#L171) nach jedem Call („give the WiFi/mbedtls allocator a moment to actually reclaim TLS context memory").
- Custom `StringAppender`/`ChunkedDecodingStream` in [Esp32Network.cpp](../src/hal/Esp32Network.cpp), um die Body-Allokation zu umgehen.

Das ist Defense-in-depth — viel davon. Verdachtsmoment: **was, wenn die ursprüngliche Implementierung einen Bug hatte und die Mitigations sind Pflaster über Pflaster, nicht eine Wurzel-Korrektur?**

**Aktion nach Refactor:**

- Mit der Native-Runtime aus Schritt 9 (valgrind + massif) den EFA-Cold-Boot-Pfad analysieren, *ohne* dass die Heap-Wächter und der `delay(150)` greifen können (sie sind ESP32-only).
- Wenn valgrind einen klaren Leak oder eine Über-Allokation zeigt → Root-Cause fixen, Pflaster zurückbauen.
- Wenn valgrind sauber ist → die Heap-Wächter sind tatsächlich ESP32-spezifische Reaktion auf mbedtls/HTTPClient/PHY-Eigenschaften, nicht auf einen unsererseits behebbaren Bug. Dann *dokumentieren statt entfernen*.

Eigenständiges Vorhaben, ~0.5–2 d je nach Befund. Bewusst aus dem Refactor herausgehalten, weil:

1. Risiko-Profil ist anders (Heap-Bugs gegen TLS-Stack — sehr fies zu reproduzieren).
2. Die Schutzwälle haben hart erarbeitet, das System läuft heute. Vor einer Demontage will man den Diagnose-Beweis.
3. Schritt 9 *liefert genau diesen Diagnose-Beweis* — also chronologisch *nach* dem Refactor.

### 7.2 `Esp32Network.cpp` — Streaming-Parse-Custom-Code

Die `StringAppender` / `BlockingClientStream` / `ChunkedDecodingStream`-Klassen in [Esp32Network.cpp](../src/hal/Esp32Network.cpp) sind nicht-trivial und tief mit `HTTPClient`/`WiFiClientSecure` verschränkt. **Kein Aktions-Item** — solange die Klassen tun, was sie sollen (Heap-Spitze halbieren), bleiben sie. Die übliche Falle „wir wissen besser als die Library, wie man X macht" gilt: Refactor-Versuche an dieser Stelle gehen 15 von 10 Fällen schief.

Im Rahmen des v2-Mergings (HAFAS-POST-Pfad) wird hier ohnehin angefasst — *dann* fällt der Refactor-Anlass von selbst an.

---

## Anhang A — `RTC_DATA_ATTR`-Constraints

`RTC_DATA_ATTR` ist ein ESP-IDF-Attribut (`__attribute__((section(".rtc.data")))`), das eine Variable in den **RTC Slow Memory** legt — 8 KB beim klassischen ESP32.

**Eigenschaften:**

- **Überlebt Deep Sleep**, sowohl Timer- als auch GPIO-Wake. Das ist genau der Punkt, warum unser RLE-Framebuffer dort liegt — sonst ginge das Diff-Render kaputt.
- **Verliert sich bei Power-Loss / Brown-out / hartem Reset.** Das ist der Cold-Boot-Trigger (`g_magic != MAGIC` ⇒ frischer Start).
- **Nicht kontiguierlich heap-fähig.** Die Allokation ist statisch zur Link-Zeit. Heap-`malloc` kann das nicht.
- **Muss file-scope sein.** Nicht in einer Klasse, nicht auf dem Stack, nicht in Templates, die in mehreren TUs instanziiert werden. Genau einmal pro Variable im ganzen Binary.
- **Kein direktes Native-Build-Pendant.** `RTC_DATA_ATTR` expandiert auf ESP32 zu der Section-Direktive; auf Native expandiert es zu — nichts, weil der Header gar nicht kompiliert wird. Die Native-Runtime (Schritt 9) ersetzt das durch `DiskStore` mit `persist.bin`.

**Aktuelle Belegung** in [Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp):

| Variable        | Bytes (ca.) | Zweck                                          |
|-----------------|-------------|------------------------------------------------|
| `g_magic`       | 4           | Cold-Boot-Detektion für Meta + Framebuffer    |
| `g_meta`        | ~32         | `PersistedMeta`                                |
| `g_rle_len`     | 4           | Belegte Länge im RLE-Buffer                    |
| `g_rle`         | 7168        | RLE-komprimierter Framebuffer (Hardcap)        |
| `g_sched_magic` | 4           | Cold-Boot-Detektion für Schedule              |
| `g_sched`       | ~120        | `ScheduleSnapshot`                             |
| **Total**       | **~7332**   | von 8192 nutzbar — **~860 B Reserve**          |

**Konsequenzen für den Refactor:**

1. **Refactor (Schritte 1–8):** [Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp) bleibt unangetastet **mit Ausnahme** des `SCHED_MAGIC`-Bumps in Schritt 2.3.
2. **Schritt 2.3 Speicher-Aufschlag:** `ScheduleHint` wächst um `time_t next_today[2]` = 16 B pro Stream × 5 Streams = +80 B in `g_sched`. Neuer Total: ~7412 B, Reserve: ~780 B. Noch komfortabel.
3. **Schritt 9 Native-Pendant:** `DiskStore` schreibt dasselbe Layout (PersistedMeta + RLE-Framebuffer + ScheduleSnapshot) in `.tmp/native-runtime/persist.bin`. „Cold Boot" = Datei löschen.
4. **Soft-Constraint:** Das Binary darf nicht versehentlich noch *eine* `RTC_DATA_ATTR`-Verwendung dazubekommen, die die 8 KB sprengt. Nach 2.3 sind ~780 B Reserve — kein Freibrief.
