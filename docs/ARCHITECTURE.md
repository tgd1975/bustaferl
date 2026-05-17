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
│   ├── Departure.h
│   ├── StreamSnapshot.h
│   └── wienerlinien_parse.{h,cpp}
│
├── logic/                    plattformneutral, reine Funktionen + kleine Klassen
│   ├── stale_policy.{h,cpp}      §4
│   ├── sleep_planner.{h,cpp}     §6
│   ├── refresh_planner.{h,cpp}   §5
│   ├── filter_health.{h,cpp}     §9
│   └── boot_sequencer.{h,cpp}    §8
│
└── render/                   Layout und Rasterisierung
    ├── frame_buffer.h        Template FrameBuffer<W,H>, 1-bpp
    ├── layout.{h,cpp}        Block-Layout, Adafruit GFX
    ├── error_overlay.{h,cpp} Stale-/StartFailed-Frames
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

## Wo welche Konstante wirkt

| Konstante                  | Modul                         | Effekt                              |
|----------------------------|-------------------------------|-------------------------------------|
| `STALE_THRESHOLD_S`        | logic/stale_policy            | Sekunden bis Banner „veraltet"      |
| `WAKE_BEFORE_BUS_S`        | logic/sleep_planner           | Vorlauf vor frühster Abfahrt        |
| `BOOT_MARGIN_S`            | logic/sleep_planner           | Boot+WiFi+API-Reserve               |
| `POLL_INTERVAL_S`          | main.cpp (active phase)       | Poll-Cadence im Wach-Zustand        |
| `ACTIVE_THRESHOLD_S`       | logic/sleep_planner           | Schwelle DeepSleep vs. Active       |
| `NO_DATA_SLEEP_S`          | logic/sleep_planner           | Sleep wenn API leer                 |
| `PARTIAL_HARDCAP`          | logic/refresh_planner         | erzwungenes Light Full bei Cap      |
| `LIGHT_FULL_INTERVAL_S`    | logic/refresh_planner         | Light Full alle 2 h                 |
| `NTP_INTERVAL_S`           | main.cpp                      | NTP-Resync täglich                  |
| `FILTER_HEALTH_DEAD_AFTER` | logic/filter_health           | Misses bis „Filter ungültig"        |
| `RLE_HARDCAP_BYTES`        | hal/Esp32PersistentStore      | maximaler RLE-Buffer im RTC-RAM     |

## Speicher-Layout (ESP32)

- **Flash:** Firmware (~600 kB) + Arduino + GxEPD2 + ArduinoJson
- **DRAM:** zwei Framebuffer à 15 kB (`g_frame_new`, `g_frame_prev`) + Stack + Heap
- **RTC slow memory (8 kB):** `PersistedMeta` + RLE-Framebuffer (Budget 3 kB,
  Hardcap 7 kB)
- **RTC fast memory:** ungenutzt
