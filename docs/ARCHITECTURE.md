# Architektur

## Modulkarte

```text
src/
├── main.cpp                  Top-level setup()/loop(), Verdrahtung
├── config.h                  alle Schwellwerte und Pins (CONCEPT.md §10)
├── secrets.h                 (gitignored) WiFi-Credentials
│
├── hal/                      Hardware-Abstraktion
│   ├── IClock.h              Interface
│   ├── INetwork.h            + `connectionInfo()` (SSID/IP/RSSI für Diagnose)
│   ├── NetInfo.h             Leaf-Struct: WLAN-Assoziationsdetails
│   ├── IDisplay.h
│   ├── ISleep.h
│   ├── IButton.h             Boot-Taster (isPressed / nowMs / sleepMs)
│   ├── IPersistentStore.h    + `loadTrace()` / `saveTrace()` (CycleTrace)
│   └── Esp32*.{h,cpp}        ESP32-Implementierungen
│
├── data/                     plattformneutral, parsing/structs
│   ├── Departure.h
│   ├── StreamSnapshot.h
│   ├── PersistedMeta.h
│   ├── ScheduleHint.h
│   ├── CycleTrace.h          RTC-Ringpuffer: Zyklus- + Fehler-Historie
│   ├── DiagView.h            transientes Bündel für Diagnose-/Boot-Check-Seiten
│   ├── wienerlinien_parse.{h,cpp}   OGD-JSON → bus-StreamData
│   ├── efa_parse.{h,cpp}            EFA-DM-Response → ScheduleHint
│   └── oebb_hafas_parse.{h,cpp}     mgate.exe-Antwort → S-Bahn-StreamData
│
├── logic/                    plattformneutral, reine Funktionen + kleine Klassen
│   ├── stale_policy.{h,cpp}         §4
│   ├── sleep_planner.{h,cpp}        §6
│   ├── refresh_planner.{h,cpp}      §5
│   ├── filter_health.{h,cpp}        §9
│   ├── boot_sequencer.{h,cpp}       §8
│   ├── filter_builder.{h,cpp}       Default-Filter-Set + `buildOebbFilter`
│   ├── api_fetcher.{h,cpp}          Retry-Wrapper um `httpGet` / `httpPost`
│   ├── rescue_policy.{h,cpp}        Rescue-Fetch-Fenster (§6): Nachhol-Logik
│   ├── snapshot_fetcher.{h,cpp}     pro Cycle: OGD-Batch + HAFAS-Call
│   ├── snapshot_logger.{h,cpp}      einheitliches `[snapshot]`-Log-Format
│   ├── schedule_fetcher.{h,cpp}     EFA-Endpoint → `ScheduleHint`
│   ├── schedule_refresh.{h,cpp}     entscheidet, ob EFA neu geladen wird
│   ├── slot_merger.{h,cpp}          Realtime ∪ `next_today` ∪ `first_tomorrow`
│   ├── render_input.{h,cpp}         Snapshot + `selectDisplayState` → `RenderInput`
│   ├── display_apply.{h,cpp}        Frame-Diff → `IDisplay::partial/full`
│   ├── button_classifier.{h,cpp}    Press-Klassifikation (Short / Long / Double)
│   ├── cycle_trace.{h,cpp}          Ring-Push/-Zugriff auf CycleTrace (pure)
│   ├── diag_mode.{h,cpp}            Pager-Automat (diagNext) für Diagnose-Seiten
│   └── cycle_runner.{h,cpp}         `runColdCycle` / `runWarmCycle` / `runDiagMode` — Engine
│
└── render/                   Layout und Rasterisierung
    ├── frame_buffer.h        Template FrameBuffer<W,H>, 1-bpp
    ├── canvas.{h,cpp}        Abstrakter Canvas + HostCanvas / AdafruitGfxCanvas
    ├── bitmap_fonts.{h,cpp}  U8g2-Font-Roles für die sieben Display-States
    ├── badge.{h,cpp}         Header-Badges (sm/md/lg)
    ├── plan_marker.{h,cpp}   5×5-Plan-Marker `□`
    ├── network_plan.{h,cpp}  Diamond/Big/Dot-Marker + Atzg-Linie
    ├── display_state.{h,cpp} Fullscreen-Renderer für Boot/Offline/Auth/Quiet
    ├── diag_page.{h,cpp}     Text-Renderer der Diagnose-Seiten + Boot-Check
    ├── layout.{h,cpp}        Spalten-Layout für Normal/Stale/Night + `renderFrame`
    └── rle.{h,cpp}           Lauflängenkompression für RTC-RAM
```

## Schichten-Regel

```text
              main.cpp
                 │
        ┌────────┼────────┐
        ▼        ▼        ▼
       hal/    logic/   render/
        │        │        │
        │        ▼        │
        │      data/      │
        │        │        │
        └────────┴────────┘
                 ▼
               std::
```

- Pfeile nur nach unten (oder zur Seite)
- `logic/` und `data/` dürfen **keine** Hardware-Header inkludieren
- HAL-Implementierungen werden über Konstruktor-Injection an die Logik
  übergeben → Logik mit Mocks testbar

## Dataflow (Warm Cycle)

### Happy Case — Überblick

Ein Timer-Wakeup aus dem Deep Sleep, ein erfolgreicher Fetch, ein
Partial-Update, zurück in den Schlaf. Das ist der Zyklus, den das Gerät
tagsüber alle 30 s bis n Minuten fährt.

```mermaid
sequenceDiagram
    autonumber
    participant SL as Deep Sleep
    participant CR as cycle_runner
    participant NET as WiFi + APIs
    participant LG as Logik
    participant RD as Renderer
    participant EP as e-Paper
    participant ST as RTC-Store

    SL->>CR: Timer-Wakeup → runWarmCycle()
    CR->>NET: WiFi verbinden (a-net2)
    CR->>NET: Echtzeit holen (WL OGD + ÖBB HAFAS)
    NET-->>CR: StreamSnapshot (4 Streams, je 3 Slots)
    CR->>LG: DisplayState wählen + Slots mit EFA-Hints mergen
    LG-->>CR: Normal + merged Snapshot
    CR->>RD: renderFrame(RenderInput)
    RD-->>CR: Frame (400×300, 1 bpp)
    CR->>ST: voriges Frame laden
    CR->>LG: planRefresh (Byte-Diff) → Partial + Bbox
    CR->>EP: drawPartial(bbox) — nur die geänderten Ziffern
    CR->>ST: Frame (Delta-RLE) + Meta sichern
    CR->>LG: planSleep (nächster Bus − 15 min Vorlauf)
    CR->>SL: deepSleep(n s)
```

### Happy Case — Detail

Gleicher Zyklus, aufgelöst auf die realen Komponenten. Nummerierung folgt
`runWarmCycle()` in [cycle_runner.cpp](../src/logic/cycle_runner.cpp).

```mermaid
sequenceDiagram
    autonumber
    participant M as main / Esp32Sleep
    participant CR as cycle_runner
    participant ST as Esp32PersistentStore
    participant NET as Esp32Network
    participant CLK as Esp32Clock
    participant SF as snapshot_fetcher
    participant SCH as schedule_fetcher
    participant SEL as render_input
    participant MRG as slot_merger
    participant SP as sleep_planner
    participant LY as layout
    participant RP as refresh_planner
    participant DA as display_apply
    participant EP as Esp32Display

    M->>CR: wakeupCause()==Timer → runWarmCycle(meta)
    CR->>ST: loadSchedule() — EFA-Hints aus RTC
    CR->>NET: connect(15 s)
    NET-->>CR: WL_CONNECTED (Regulatory AT, ch 1–13)
    CR->>CLK: isSynced()? + Drift-Guard (now ≤ expected_wake_at + 30 min?)
    note right of CR: unplausibel weit gesprungene Uhr → NTP-Resync erzwungen

    rect rgba(127,127,127,0.12)
        note over CR,SF: doFetchCycle — Datenschicht
        CR->>SF: fetchSnapshot(filters, oebb_filter)
        SF->>NET: GET OGD-Monitor Batch 1 (2 stopIds)
        SF->>NET: GET OGD-Monitor Batch 2
        note right of SF: wienerlinien_parse → Bus-Slots (RT/PLAN)
        SF->>NET: POST mgate.exe (HAFAS)
        note right of SF: oebb_hafas_parse → 3 S-Bahn-Slots,<br/>Minuten-Dedup, line_label S2/S3
        SF-->>CR: snap (api_ok=1) + FetchSummary
        CR->>CR: meta.last_success_at = now
        CR->>SEL: selectDisplayState(snap, meta, signals)
        SEL-->>CR: Normal
        CR->>MRG: mergeSlots(snap, schedule, now)
        note right of MRG: Vergangene raus, Hints füllen Lücken,<br/>Minuten-Duplikate raus (RT gewinnt)
        MRG-->>CR: merged
    end

    opt Schedule-Refresh fällig (needScheduleRefresh)
        CR->>SCH: fetchSchedule() — EFA XSLT_DM_REQUEST je DIVA
        SCH-->>CR: ScheduleHints (next_today / first_tomorrow)
        CR->>ST: saveSchedule()
    end

    CR->>SP: planSleep(merged, now)
    SP-->>CR: DeepSleep(n) oder Active

    rect rgba(127,127,127,0.12)
        note over CR,EP: renderAndPush — Darstellungsschicht
        CR->>SEL: composeRenderInput(state, snap, schedule)
        CR->>LY: renderFrame(in, curr)
        CR->>LY: drawUpdateStamp(curr, meta.last_display_update)
        note right of LY: alter Stempel → unverändertes Board<br/>bleibt byte-identisch (None-Skip)
        CR->>ST: loadFramebuffer(prev) — Delta-RLE decode
        CR->>RP: planRefresh(prev, curr, partial_count, last_light_full)
        RP-->>CR: Partial + Bbox (oder LightFull nach 15 Partials / 1 h)
        CR->>LY: drawUpdateStamp(curr, now) + planRefresh erneut
        note right of LY: Stempel tickt nur mit echtem Update mit
        CR->>DA: applyDisplayDecision(d)
        DA->>EP: drawPartial(fb, bbox)
        DA-->>CR: partial_count++
        CR->>ST: saveFramebuffer(curr) — Delta-RLE ≤ 7168 B
    end

    opt Snapshot unvollständig (≥1 Batch fehlte) — runRescueFetch
        note over CR,NET: Fenster 20–40 s nach dem Update, max 3 Versuche
        CR->>NET: erneuter Fetch (bis vollständig)
        CR->>EP: genau ein Extra-Refresh bei erstem vollständigen Snapshot
        CR->>SP: planSleep neu aus dem gerettetem Merge
    end

    CR->>ST: saveMeta(meta)
    CR->>M: deepSleep(n s) bzw. lightSleep(30 s) wenn Active
```

## Zustandsmaschine — Sonderfälle

Der Gerätelebenszyklus mit allen Fehler- und Randpfaden. Jede Kante, die
in `DeepSleep` mündet, trägt die Schlafdauer — genau diese Kanten
entscheiden, ob das Gerät "eingefroren" wirkt (siehe Coma-Fix in
[sleep_planner.cpp](../src/logic/sleep_planner.cpp): ein fehlgeschlagener
Fetch darf nie einen langen Schlaf planen).

```mermaid
stateDiagram-v2
    [*] --> ColdBoot: Power-on / MAGIC-Bump

    state ColdBoot {
        [*] --> BootSequenz: WiFi + NTP
        BootSequenz --> BootCheck: Ok
        BootCheck --> FirstRender: 15 s Info-Screen (Taste überspringt)
        FirstRender --> [*]: LightFull/DeepClean + Stempel
    }

    ColdBoot --> DeepSleep: Ok → planSleep
    ColdBoot --> RetrySleep60: RetryLater (WiFi/NTP down, Versuch unter 5)
    RetrySleep60 --> ColdBoot: Timer-Wakeup
    ColdBoot --> GiveUp: 5 Versuche erschöpft
    GiveUp --> DeepSleep: Offline-Screen + langer Schlaf

    DeepSleep --> WarmCycle: Timer-Wakeup
    DeepSleep --> ButtonWake: BOOT-Taste
    ButtonWake --> WarmCycle: kurzer Druck → Update-Zyklus
    ButtonWake --> BwReset: langer Druck (ab 3 s) → DeepClean + Redraw
    ButtonWake --> DiagMode: Doppelklick → Diagnose-Seiten
    Active --> DiagMode: Doppelklick beim Poll
    DiagMode --> WarmCycle: langer Druck / 10-min-Timeout → Board neu

    state WarmCycle {
        [*] --> WifiCheck
        WifiCheck --> ClockCheck: verbunden
        ClockCheck --> Fetch: synced + Uhr plausibel (sonst NTP-Resync)
        Fetch --> RenderPush: api_ok — Normal/Night/Quiet
        Fetch --> KeepFrame: Fetch-Fehler, unter 10 min alt (pre-stale)
        Fetch --> RenderPush: Fetch-Fehler, über 10 min → Stale
        RenderPush --> Rescue: Snapshot unvollständig
        Rescue --> [*]: 1 Extra-Refresh im 20–40 s-Fenster, dann Schlaf
        WifiCheck --> StaleOffline: WiFi down + Schwelle gerissen
        StaleOffline --> [*]
        KeepFrame --> [*]
        RenderPush --> [*]
    }

    WarmCycle --> DeepSleep: Fetch-Fehler → 60 s Retry (Coma-Fix)
    WarmCycle --> DeepSleep: keine Abfahrten (api_ok) → 30 min
    WarmCycle --> DeepSleep: nächster Bus fern → bis 15 min davor
    WarmCycle --> Active: nächster Bus unter 2 min
    Active --> WarmCycle: lightSleep 30 s, dann neu pollen
    WarmCycle --> NightlyClean: langer Schlaf geplant + 20 h ohne DeepClean
    NightlyClean --> DeepSleep: DeepClean statt Partial
```

Auf dem Panel entscheidet pro Zyklus der State-Selector, **welcher der
sieben Screens** gezeigt wird — eine Prioritätskette, kein Automat
(`selectDisplayState` in
[render_input.cpp](../src/logic/render_input.cpp), erste zutreffende
Bedingung gewinnt):

```mermaid
flowchart TD
    A{"auth_error_seen?"} -->|ja| AUTH["Auth — §9-Screen"]
    A -->|nein| B{"erster Render überhaupt?"}
    B -->|ja| BOOT["Boot — lädt Fahrplan…"]
    B -->|nein| C{"WiFi down + Erfolg über 5 min her?"}
    C -->|ja| OFF["Offline — Kein Empfang"]
    C -->|nein| D{"letzter Erfolg über 10 min her?"}
    D -->|ja| STALE["Stale — alle Slots maskiert"]
    D -->|nein| E{"alle Abfahrten über 20 min entfernt?"}
    E -->|ja| QUIET["Quiet — Keine Abfahrten"]
    E -->|nein| F{"01:00 bis 05:00 + erste Abfahrt fern?"}
    F -->|ja| NIGHT["Night — Nachtbetrieb"]
    F -->|nein| NORM["Normal — volles Board"]
```

## Wichtige Design-Entscheidungen

### Framebuffer-Diff statt blind-redraw

e-Paper-Partials sind schnell und flickerfrei, kosten aber „Ghosting".
Wir behalten das letzte Bild im RTC-RAM (RLE-komprimiert, ~1–3 kB),
vergleichen byteweise, refreshen nur die Differenz-Bbox auf 8-Pixel-Grenzen.

### Light Full gegen Ghosting

Ein Light Full Refresh (1× S/W-Flash + Bild) läuft, sobald der letzte
mindestens `LIGHT_FULL_INTERVAL_S` (1 h) her ist **oder** seither
`PARTIAL_HARDCAP` (15) Partials liefen — je nachdem, was zuerst eintritt.
Beides zusammen hält das Ghosting bei statischen wie bei sich häufig
ändernden Anzeigen in Schach.

### Cold-Boot-Erkennung via Wakeup-Cause

`esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED` → frischer
Boot (Power-on oder Brown-out). Sonst → Wake aus Sleep, RTC-Daten gültig.

### Deep-Wake erzwingt Full Refresh

Ein Wake aus Deep Sleep verliert den schnellen Partial-RAM des Panels. Der
erste Render danach wird deshalb auf einen Full Refresh promotet
(`deep_wake` → `planRefresh(panel_ram_untrusted)`), sonst zeigen sich weiße
Ränder und Garbage. Nur der allererste Render pro Wake ist betroffen.

### Drift-Guard gegen korrupte RTC-Uhr

`isSynced()` erzwingt nur eine untere Zeitschranke (> 2023). Weckt das Gerät
mit einer `now()`, die mehr als `MAX_WAKE_OVERSHOOT_S` (30 min) über dem beim
Einschlafen gespeicherten `expected_wake_at` liegt, gilt die Uhr als korrupt
und ein NTP-Resync läuft vor dem Rendern — sonst würden Plan-Hints gegen eine
um Stunden versetzte Uhr gerendert (Feld-Symptom „58B-Coma").

### Stale-Verhalten ist binär

Bewusste Vereinfachung: entweder vertrauenswürdige Echtzeit oder klar
sichtbares „kaputt". Keine grauen Zwischenzustände mit Zeitstempeln.

### Sieben Display-States

Ein zentrales `DisplayState`-Enum mit sieben Werten steuert das Fullscreen-
Bild statt einzelner Overlay-Banner (`VERALTET`, `Filter ungueltig`,
`Start fehlgeschlagen`):

```text
                    SelectorSignals
                          │
                          ▼
         ┌──────────────────────────────────┐
         │     selectDisplayState()         │
         │     (logic/render_input.cpp)     │
         └────────────────┬─────────────────┘
                          │
        ┌─────┬─────┬─────┼─────┬─────┬─────┐
        ▼     ▼     ▼     ▼     ▼     ▼     ▼
      Boot  Normal Stale Night Quiet Offline Auth
```

`render/display_state.cpp` rendert für jeden State entweder den
Spalten-Board (`Normal`/`Stale`/`Night`) oder ein dediziertes
Fullscreen-Bild (`Boot`/`Quiet`/`Offline`/`Auth`). Die State-Auswahl
ist pure Funktion → vollständig host-testbar in `test_native_render_input`.

### `towards`-Filter-Drift wird sichtbar gemacht

Wenn 58B im Endemann-RBL 3 erfolgreiche API-Calls in Folge keinerlei
Departure mit passendem `towards` liefert, blendet das Bustaferl
`58B Filter ungueltig` ein. Sonst würde der Wegfall stillschweigend zu
`--:--` werden und ewig so bleiben.

### Diagnose-Modus statt serieller Logs

Das Gerät hängt im Betrieb an keiner seriellen Konsole. Um eine im Feld
beobachtete Anomalie („warum stand da `--:--`?") ohne Log verstehen zu können,
führt jeder Warm-Zyklus einen kompakten **CycleTrace** mit: pro Zyklus ein
12-B-`CycleRecord` (Zeit, Auslöser, Stream-OK-Bits, fehlgeschlagene Batches,
Rescue-/Stale-Flags, Schlafdauer) plus 6-B-`ErrorRecord`s für Anomalien — zwei
RTC-Ringpuffer (je 16 Einträge, ~292 B), die den Tiefschlaf überstehen.

Ein **Doppelklick** öffnet `runDiagMode`: einmal frisch fetchen, dann vier
schlichte Text-Seiten (STATUS / ZYKLEN / FEHLER / DATEN-DETAILS), vorwärts per
Kurzdruck (mit Umlauf), zurück per Langdruck, Sicherheits-Timeout nach 10 min.
Die Aufteilung ist bewusst geschichtet: `logic/cycle_trace` (Ring, pure),
`logic/diag_mode` (`diagNext`-Pager-Automat, pure) und `render/diag_page`
(Text auf abstraktem `render::Canvas`) sind alle host-testbar; `cycle_runner`
verdrahtet nur Fetch, HAL-Probe (`connectionInfo`, Heap/Uptime) und den
Button-Loop.

### Boot-Check nach dem Kaltstart

Nach erfolgreicher Boot-Sequenz zeigt `runColdCycle` für `BOOT_INFO_SHOW_S`
(15 s, per Taste überspringbar) denselben STATUS-Screen plus Start-Zeilen
(RTC-Restore-Status, Batch-Tally, WLAN&NTP-Anlauf) — ein Selbsttest, den man
direkt nach dem Einschalten lesen kann. Erst danach rendert das Board. Weil der
Boot-Check bereits deep-cleant, genügt dem Board danach ein Light-Full.

## Wo welche Konstante wirkt

| Konstante                  | Wert | Modul                         | Effekt                              |
|----------------------------|------|-------------------------------|-------------------------------------|
| `STALE_THRESHOLD_V2_S`     | 600  | logic/render_input            | letzter Erfolg älter → `Stale`-Screen (`??:??`) |
| `OFFLINE_THRESHOLD_S`      | 300  | logic/render_input            | WiFi down + Schwelle → `Offline`    |
| `QUIET_HORIZON_S`          | 1200 | logic/render_input            | alle Abfahrten weiter → `Quiet`     |
| `NIGHT_FIRST_DEP_MIN_AHEAD_S` | 1800 | logic/render_input         | erste Abfahrt weiter (Nachtfenster) → `Night` |
| `WAKE_BEFORE_BUS_S`        | 900  | logic/sleep_planner           | Vorlauf vor frühster Abfahrt        |
| `BOOT_MARGIN_S`            | 30   | logic/sleep_planner           | Boot+WiFi+API-Reserve               |
| `POLL_INTERVAL_S`          | 30   | logic/cycle_runner            | Poll-Cadence im Wach-Zustand        |
| `ACTIVE_THRESHOLD_S`       | 120  | logic/sleep_planner           | Schwelle DeepSleep vs. Active       |
| `NO_DATA_SLEEP_S`          | 1800 | logic/sleep_planner           | Sleep wenn API leer (aber `api_ok`) |
| `API_FAILURE_RETRY_S`      | 60   | logic/sleep_planner           | Kurz-Retry nach fehlgeschlagenem Fetch (Coma-Fix) |
| `PARTIAL_HARDCAP`          | 15   | logic/refresh_planner         | erzwungenes Light Full nach N Partials |
| `LIGHT_FULL_INTERVAL_S`    | 3600 | logic/refresh_planner         | Light Full spätestens alle 1 h      |
| `RESCUE_WINDOW_START/END_S` | 20/40 | logic/rescue_policy          | Nachhol-Fenster nach unvollständigem Fetch |
| `RESCUE_MAX_ATTEMPTS`      | 3    | logic/rescue_policy           | max. Komplett-Fetches im Rescue-Fenster |
| `NTP_INTERVAL_S`           | 86400 | logic/cycle_runner           | NTP-Resync täglich                  |
| `BTN_LONG_PRESS_MS`        | 3000 | logic/button_classifier       | Halten bis Long → S/W-Reset         |
| `BTN_DOUBLE_CLICK_MS`      | 400  | logic/button_classifier       | Fenster für Doppelklick → Diagnose  |
| `DIAG_MAX_S`               | 600  | logic/cycle_runner            | Sicherheits-Timeout des Diagnose-Modus |
| `BOOT_INFO_SHOW_S`         | 15   | logic/cycle_runner            | Dauer des Boot-Check-Screens (0 = aus) |
| `MAX_WAKE_OVERSHOOT_S`     | 1800 | logic/cycle_runner            | Wake über `expected_wake_at` hinaus → Drift-Guard erzwingt NTP |
| `FILTER_HEALTH_DEAD_AFTER` | 3    | logic/filter_health           | Misses bis „Filter ungültig"        |
| `OGD_AUTH_STREAK_TRIPWIRE` | 3    | logic/snapshot_fetcher        | 3× OGD-401 → `auth_error_seen`      |
| `OEBB_EXTID_ATZG/_WIENHBF` | —    | data/oebb_hafas_parse + cfg   | HAFAS-`stbLoc`/`dirLoc`             |
| `OEBB_HAFAS_AID`           | —    | config.h → mgate.exe-Body     | HAFAS-Auth-Token (rotiert)          |
| `RLE_HARDCAP_BYTES`        | 7168 | hal/Esp32PersistentStore      | maximaler RLE-Buffer im RTC-RAM     |

## Speicher-Layout (ESP32)

- **Flash:** Firmware gesamt ~1,08 MB (~83 % der 1,31-MB-App-Partition):
  eigener Code + Arduino + GxEPD2 + ArduinoJson + U8g2-Font-Daten (7
  Font-Roles aus `render/bitmap_fonts`) + Custom-Glyphen
- **DRAM:** zwei Framebuffer à 15 kB (`g_frame_new`, `g_frame_prev`) + Stack + Heap
- **RTC slow memory (8 kB):** `PersistedMeta` + `StreamSnapshot` (inkl.
  `Departure::line_label` pro Slot) + `ScheduleHint` + `CycleTrace`
  (Zyklus-/Fehler-Historie, ~292 B) + RLE-Framebuffer
  (Budget ~3 kB, Hardcap `RLE_HARDCAP_BYTES` = 7168 B) — Bilanz in
  [rtc-memory-budget.md](rtc-memory-budget.md)
- **RTC fast memory:** ungenutzt

## Host-Engine (`test/test_native_runtime/`)

Die `runColdCycle` / `runWarmCycle`-Engine aus `logic/cycle_runner` läuft auch
auf dem Host. `test/test_native_runtime/` ist ein Standalone-Treiber, der die
Engine gegen die echten Wiener-Linien-Endpoints fährt — kein PIO-Test-Bucket,
sondern ein direktes `g++`-Target via `make native-runtime-*`.

| Adapter                              | Ersetzt …            | Verhalten                                                                 |
|--------------------------------------|----------------------|---------------------------------------------------------------------------|
| `WallClockClock`                     | `Esp32Clock`         | `time(nullptr)` + `gmtime_r`; `isSynced()` immer `true`                   |
| `HttpsNet` (libcurl)                 | `Esp32Network`       | HTTPS-GET gegen den realen Endpoint; ENV-Overrides für Base + TLS-Verify  |
| `DiskStore`                          | `Esp32PersistentStore` | `PersistedMeta` + RLE-Frame + `ScheduleSnapshot` in `persist.bin`        |
| `NoOpDisplay`                        | `Esp32Display`       | verschluckt `partial/full/clear`; kein Panel                              |
| `NoOpSleep`                          | `Esp32Sleep`         | `deepSleep` kehrt zurück (ein Prozess, valgrind sieht alle Cycles)        |
| `RecordingRenderer`                  | `Esp32Renderer`      | 1bpp-Pseudo-Raster + Dedup; schreibt P5-PGM pro Frame-Wechsel             |

Der Treiber existiert für drei Klassen, die `test_native_*` mit Fake-INetwork
nicht abdeckt: HTTPS-Edge-Cases gegen den Live-Endpoint, EFA-Mapping mit
realen Responses, und Heap-Verlauf über N Cycles in einem Prozess
(`make native-runtime-smoke` → valgrind). Details in
[../test/test_native_runtime/README.md](../test/test_native_runtime/README.md).
