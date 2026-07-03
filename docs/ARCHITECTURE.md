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
│   ├── INetwork.h
│   ├── IDisplay.h
│   ├── ISleep.h
│   ├── IPersistentStore.h
│   └── Esp32*.{h,cpp}        ESP32-Implementierungen
│
├── data/                     plattformneutral, parsing/structs
│   ├── Departure.h           + line_label[6] (S-Bahn: variable Linie pro Slot)
│   ├── StreamSnapshot.h      4 Streams (STREAM_SBAHN_HBF statt 2× U1)
│   ├── wienerlinien_parse.{h,cpp}   OGD-Monitor (Bus, GET)
│   └── oebb_hafas_parse.{h,cpp}     ÖBB HAFAS mgate.exe (S-Bahn, POST)
│
├── logic/                    plattformneutral, reine Funktionen + kleine Klassen
│   ├── stale_policy.{h,cpp}         §4
│   ├── sleep_planner.{h,cpp}        §6
│   ├── refresh_planner.{h,cpp}      §5
│   ├── filter_health.{h,cpp}        §9
│   ├── boot_sequencer.{h,cpp}       §8
│   ├── filter_builder.{h,cpp}       Default-Filter-Set (`towards`-Mapping)
│   ├── api_fetcher.{h,cpp}          Retry-Wrapper um `INetwork::httpGet`
│   ├── snapshot_fetcher.{h,cpp}     pro Cycle: OGD-Bus-Batch (GET) + ÖBB-S-Bahn (POST)
│   ├── snapshot_logger.{h,cpp}      einheitliches `[snapshot]`-Log-Format
│   ├── schedule_fetcher.{h,cpp}     EFA-Endpoint → `ScheduleHint`
│   ├── schedule_refresh.{h,cpp}     entscheidet, ob EFA neu geladen wird
│   ├── slot_merger.{h,cpp}          Realtime ∪ `next_today` ∪ `first_tomorrow`
│   ├── render_input.{h,cpp}         Snapshot + Overlay → `RenderInput`
│   ├── display_apply.{h,cpp}        Frame-Diff → `IDisplay::partial/full`
│   ├── button_classifier.{h,cpp}    Press-Klassifikation (Short / Long)
│   └── cycle_runner.{h,cpp}         `runColdCycle` / `runWarmCycle` — Engine
│
└── render/                   Layout und Rasterisierung
    ├── frame_buffer.h        Template FrameBuffer<W,H>, 1-bpp
    ├── layout.{h,cpp}        Block-Layout inkl. Overlays (Stale/StartFailed/
    │                         Boot), Adafruit GFX
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

## Dataflow (warm cycle)

```text
              ┌─────────┐
              │WiFi up  │
              └────┬────┘
                   │
              ┌────▼────┐
              │httpGet  │  (INetwork)
              └────┬────┘
                   │ JSON body
              ┌────▼────────┐
              │parseMonitor │  (data/wienerlinien_parse)
              └────┬────────┘
                   │ StreamSnapshot
        ┌──────────┼──────────┐
        ▼          ▼          ▼
   stale_policy filter_health  ──┐
        │          │              │
        └────┬─────┘              │
             │ overlay decision   │
             ▼                    │
        ┌─────────┐               │
        │ render  │  (Frame)      │
        └────┬────┘               │
             │                    │
        ┌────▼─────────┐          │
        │refresh_planner│         │
        └────┬──────────┘         │
             │ kind + bbox        │
        ┌────▼────┐               │
        │IDisplay │               │
        └─────────┘               │
                                  ▼
                          ┌──────────────┐
                          │sleep_planner │
                          └──────┬───────┘
                                 │ mode + seconds
                          ┌──────▼──┐
                          │ ISleep  │
                          └─────────┘
```

## Zustandsmaschine

```text
                  ┌──────────────┐
       power-on   │  Cold Boot   │
       ─────────► │ §8 Sequencer │
                  └──────┬───────┘
                         │ Ok
                         ▼
                  ┌──────────────┐
                  │ First Render │
                  │ + Deep Clean │
                  └──────┬───────┘
                         │
            ┌────────────┴────────────┐
            ▼                         ▼
   ┌────────────────┐         ┌──────────────┐
   │  Deep Sleep    │◄────────┤  Warm Cycle  │
   │ (timer wakeup) │         │ (poll + draw)│
   └────────┬───────┘         └──────┬───────┘
            │                        │
            └────────────┬───────────┘
                         │
                  delta < 2 min?
                         │ yes
                         ▼
                  ┌──────────────┐
                  │   Active     │
                  │ (poll 30 s)  │
                  └──────┬───────┘
                         │
                         └─► back to Warm Cycle
```

## Wichtige Design-Entscheidungen

### Framebuffer-Diff statt blind-redraw

e-Paper-Partials sind schnell und flickerfrei, kosten aber „Ghosting".
Wir behalten das letzte Bild im RTC-RAM (RLE-komprimiert, ~1–3 kB),
vergleichen byteweise, refreshen nur die Differenz-Bbox auf 8-Pixel-Grenzen.

### Light Full primär zeitgesteuert

Partial-Counter als reines Sicherheitsnetz: nach 2 h sowieso Light Full,
egal wie viele Partials. Vermeidet, dass bei statischen Anzeigen das
Ghosting unkontrolliert wächst.

### Cold-Boot-Erkennung via Wakeup-Cause

`esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED` → frischer
Boot (Power-on oder Brown-out). Sonst → Wake aus Sleep, RTC-Daten gültig.

### Stale-Verhalten ist binär

Bewusste Vereinfachung: entweder vertrauenswürdige Echtzeit oder klar
sichtbares „kaputt". Keine grauen Zwischenzustände mit Zeitstempeln.

### `towards`-Filter-Drift wird sichtbar gemacht

Wenn 58B im Endemann-RBL 3 erfolgreiche API-Calls in Folge keinerlei
Departure mit passendem `towards` liefert, blendet das Bustaferl
`58B Filter ungueltig` ein. Sonst würde der Wegfall stillschweigend zu
`--:--` werden und ewig so bleiben.

Analog für die S-Bahn: antwortet die ÖBB-HAFAS-Schnittstelle 3-mal in Folge mit
`err != "OK"` (typisch ein rotierter AID/Client-Schlüssel), erscheint im
S-Bahn-Block `OEBB-API: Auth ungueltig`; die Bus-Zeilen laufen weiter.

## Wo welche Konstante wirkt

| Konstante                  | Modul                         | Effekt                              |
|----------------------------|-------------------------------|-------------------------------------|
| `STALE_THRESHOLD_S`        | logic/stale_policy            | Sekunden bis Banner „veraltet"      |
| `WAKE_BEFORE_BUS_S`        | logic/sleep_planner           | Vorlauf vor frühster Abfahrt        |
| `BOOT_MARGIN_S`            | logic/sleep_planner           | Boot+WiFi+API-Reserve               |
| `POLL_INTERVAL_S`          | logic/cycle_runner            | Poll-Cadence im Wach-Zustand        |
| `ACTIVE_THRESHOLD_S`       | logic/sleep_planner           | Schwelle DeepSleep vs. Active       |
| `NO_DATA_SLEEP_S`          | logic/sleep_planner           | Sleep wenn API leer                 |
| `PARTIAL_HARDCAP`          | logic/refresh_planner         | erzwungenes Light Full bei Cap      |
| `LIGHT_FULL_INTERVAL_S`    | logic/refresh_planner         | Light Full alle 2 h                 |
| `NTP_INTERVAL_S`           | logic/cycle_runner            | NTP-Resync täglich                  |
| `FILTER_HEALTH_DEAD_AFTER` | logic/filter_health           | Misses bis „Filter ungültig"        |
| `OEBB_HAFAS_AID` / `_CLIENT_JSON` | data/oebb_hafas_parse  | ÖBB-HAFAS-Auth (pre-flash verifizieren) |
| `OEBB_JNYFLTR_PRODUCTS`    | data/oebb_hafas_parse         | Produktfilter S-Bahn+Regio+REX      |
| `RLE_HARDCAP_BYTES`        | hal/Esp32PersistentStore      | maximaler RLE-Buffer im RTC-RAM     |

## Speicher-Layout (ESP32)

- **Flash:** Firmware (~600 kB) + Arduino + GxEPD2 + ArduinoJson
- **DRAM:** zwei Framebuffer à 15 kB (`g_frame_new`, `g_frame_prev`) + Stack + Heap
- **RTC slow memory (8 kB):** `PersistedMeta` + RLE-Framebuffer (Budget 3 kB,
  Hardcap 7 kB)
- **RTC fast memory:** ungenutzt

## Host-Engine (`test/test_native_runtime/`)

Die `runColdCycle` / `runWarmCycle`-Engine aus `logic/cycle_runner` läuft auch
auf dem Host. `test/test_native_runtime/` ist ein Standalone-Treiber, der die
Engine gegen die echten Wiener-Linien-Endpoints fährt — kein PIO-Test-Bucket,
sondern ein direktes `g++`-Target via `make native-runtime-*`.

| Adapter                              | Ersetzt …            | Verhalten                                                                 |
|--------------------------------------|----------------------|---------------------------------------------------------------------------|
| `WallClockClock`                     | `Esp32Clock`         | `time(nullptr)` + `gmtime_r`; `isSynced()` immer `true`                   |
| `HttpsNet` (libcurl)                 | `Esp32Network`       | HTTPS GET/POST gegen den realen Endpoint; ENV-Overrides für Base + TLS-Verify  |
| `DiskStore`                          | `Esp32PersistentStore` | `PersistedMeta` + RLE-Frame + `ScheduleSnapshot` in `persist.bin`        |
| `NoOpDisplay`                        | `Esp32Display`       | verschluckt `partial/full/clear`; kein Panel                              |
| `NoOpSleep`                          | `Esp32Sleep`         | `deepSleep` kehrt zurück (ein Prozess, valgrind sieht alle Cycles)        |
| `RecordingRenderer`                  | `Esp32Renderer`      | 1bpp-Pseudo-Raster + Dedup; schreibt P5-PGM pro Frame-Wechsel             |

Der Treiber existiert für drei Klassen, die `test_native_*` mit Fake-INetwork
nicht abdeckt: HTTPS-Edge-Cases gegen den Live-Endpoint, EFA-Mapping mit
realen Responses, und Heap-Verlauf über N Cycles in einem Prozess
(`make native-runtime-smoke` → valgrind). Details in
[../test/test_native_runtime/README.md](../test/test_native_runtime/README.md).
