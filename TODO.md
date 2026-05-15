# bustaferl — Umsetzungsplan

Reihenfolge ist die empfohlene Bearbeitungsabfolge. Phasen 1–3 sind Voraussetzung für Phase 4 (Hardware-Bring-up). Phasen 5–8 können nach erster lauffähiger Version parallel laufen.

---

## Phase 1 — Projekt-Setup

- [ ] Toolchain entscheiden: **PlatformIO** als Build-System (Arduino-Framework für ESP32), `make` als Wrapper für gängige Targets
- [ ] `platformio.ini` anlegen mit zwei Environments:
  - [ ] `env:esp32dev` — Target-Build für die echte Hardware
  - [ ] `env:native` — Host-Build für Unit-Tests (PlatformIO Unity, kein ESP32-Code im Pfad)
- [ ] Library-Deps in `platformio.ini` fixieren:
  - [ ] `zinggjm/GxEPD2`
  - [ ] `adafruit/Adafruit GFX Library`
  - [ ] `bblanchon/ArduinoJson` (für OGD-Parsing)
  - [ ] `WiFi`, `HTTPClient`, `WiFiClientSecure` (Core, keine Version-Pin nötig)
- [ ] `.gitignore` für `.pio/`, `.vscode/`, `secrets.h`, `*.bin`, `build/`, `.cache/`
- [ ] `secrets.h.example` ins Repo, `secrets.h` lokal anlegen
- [ ] `README.md` Stub (wird in Phase 8 ausgefüllt)
- [ ] `LICENSE` ist da — passt
- [ ] CI-Workflow `.github/workflows/ci.yml`: `make build` + `make test` bei jedem Push

## Phase 2 — Software-Architektur

Ziel: testbarer Kern, dünne Hardware-Adapter. ESP32-spezifische Calls bleiben in `hal/`, alles Geschäftliche ist plattform­neutral kompilierbar (für `env:native`).

### Modulgrenzen

- [ ] `src/main.cpp` — `setup()` / `loop()`, Verdrahtung der Module, sonst minimal
- [ ] `src/hal/` — Hardware-Abstraktionen mit Interfaces, je 1 ESP32-Impl + 1 Mock
  - [ ] `IClock` (`now()`, `ntpSync()`)
  - [ ] `INetwork` (`connect()`, `isConnected()`, `httpGet()`)
  - [ ] `IDisplay` (`drawFull()`, `drawPartial(bbox)`, `lightFull()`, `deepClean()`)
  - [ ] `ISleep` (`deepSleep(seconds)`, `wakeupCause()`)
  - [ ] `IPersistentStore` (RTC-RAM Read/Write für Framebuffer-RLE + Metadaten)
- [ ] `src/data/` — Datenstrukturen und Parser, plattformneutral
  - [ ] `Departure` (struct: zeit_unix, ist_realtime_bool)
  - [ ] `StreamSnapshot` (3 Streams × 2 Slots)
  - [ ] `wienerlinien_parse.{h,cpp}` — JSON → `StreamSnapshot`, inkl. `towards`-Filter
- [ ] `src/logic/` — Geschäftslogik, **keine** Hardware-Includes
  - [ ] `stale_policy.{h,cpp}` — entscheidet anhand `last_success_ts` + `now()` ob stale
  - [ ] `sleep_planner.{h,cpp}` — implementiert §6 (t_ref, wake_at, delta-Fallunterscheidung)
  - [ ] `refresh_planner.{h,cpp}` — Diff zweier Frames, Bounding Box auf 8-px-Grid, Entscheidung Partial vs. Light Full vs. Deep Clean
  - [ ] `filter_health.{h,cpp}` — zählt aufeinanderfolgende erfolgreiche Calls ohne `towards`-Match (für 58B-Drift-Erkennung)
  - [ ] `boot_sequencer.{h,cpp}` — Cold-Boot-Zustandsmaschine nach §8
- [ ] `src/render/` — Layout und Rasterisierung
  - [ ] `layout.{h,cpp}` — feste Positionen für Block-Überschriften, Linien-Zeilen, Slot-Boxen
  - [ ] `frame_buffer.{h,cpp}` — 400×300/8 = 15 000 Byte
  - [ ] `rle.{h,cpp}` — Lauflängenkompression für RTC-RAM, mit Overflow-Erkennung
  - [ ] `error_overlay.{h,cpp}` — „veraltet", „58B Filter ungültig", „Start fehlgeschlagen"
- [ ] `src/config.h` — alle `#define`-Konstanten aus §10 des Konzepts
- [ ] `src/secrets.h` (gitignored) — WiFi-Credentials

### Wichtige Architekturentscheidungen

- [ ] Logik-Module bekommen Abhängigkeiten via Konstruktor-Injection (Interface-Pointer), nicht via globale Singletons → ermöglicht Mocks im Test
- [ ] Keine `String`-Klassen in der Logik, nur `const char*` und `std::string_view` wo Arduino-frei möglich → Host-Build einfacher
- [ ] Zeit überall als `time_t` (Unix-Sekunden), Zeitzone nur zur Display-Formatierung
- [ ] Framebuffer-Diff arbeitet auf entpackten Bytes; RLE nur an der RTC-Persistierungsgrenze

### Top-Level-Ablauf in `main.cpp`

- [ ] `setup()`:
  - [ ] `wakeupCause()` lesen
  - [ ] Cold Boot → `BootSequencer::run()`
  - [ ] Wake from Sleep → Framebuffer aus RTC dekomprimieren → reguläre Schleife
- [ ] `loop()` (eigentlich einmaliger Durchlauf, am Ende Deep Sleep):
  - [ ] WiFi up, API holen, JSON parsen
  - [ ] `StalePolicy` entscheiden
  - [ ] Frame rendern
  - [ ] `RefreshPlanner` befragen, entsprechende Display-Aktion
  - [ ] Framebuffer komprimieren, RTC schreiben
  - [ ] `SleepPlanner.plan()` → Deep Sleep oder weiter aktiv

## Phase 3 — Unit-Tests

PlatformIO Unity, ausgeführt im `env:native` (Host-Build, kein Hardware-Zugriff). Ein Testfile pro Logik-Modul.

- [ ] `test/test_wienerlinien_parse/` — feste JSON-Fixtures unter `test/fixtures/`
  - [ ] glücklicher Fall: 3 Streams, jeweils 2 Echtzeit-Departures
  - [ ] Plan-Fallback wenn Echtzeit fehlt
  - [ ] leere Departures-Liste
  - [ ] `towards`-Filter matcht / matcht nicht
  - [ ] kaputtes JSON → definierter Fehler statt Crash
- [ ] `test/test_stale_policy/`
  - [ ] frischer Call → ok
  - [ ] älter als 180 s → stale
  - [ ] genau 179 / 181 s — Grenzfälle
- [ ] `test/test_sleep_planner/`
  - [ ] delta > 2 min → Deep Sleep mit korrekter Dauer
  - [ ] delta 0–2 min → Active
  - [ ] delta < 0 → Active
  - [ ] keine Departures → 1800 s Sleep
- [ ] `test/test_refresh_planner/`
  - [ ] identische Frames → kein Refresh
  - [ ] kleine Änderung → Partial mit korrekt auf 8 px gerundeter Bbox
  - [ ] 2 h vergangen → Light Full erzwungen
  - [ ] Partial-Counter erreicht Hardcap → Light Full
- [ ] `test/test_rle/`
  - [ ] roundtrip: kompress → dekompress = identisch
  - [ ] weißes Bild komprimiert auf < 100 Byte
  - [ ] worst-case Bild meldet Overflow
- [ ] `test/test_filter_health/`
  - [ ] 1 Fehl-Match → nicht meldepflichtig
  - [ ] 3 in Folge → Filter-ungültig-Flag gesetzt
  - [ ] 1 Treffer dazwischen → Counter resettet
- [ ] `test/test_boot_sequencer/` — über Mocks von INetwork/IClock
  - [ ] glücklicher Pfad
  - [ ] WiFi schlägt fehl → Retry, dann Fehlerbild
  - [ ] NTP schlägt fehl → wie WiFi
- [ ] Coverage-Ziel: alle `logic/`-Module ≥ 90 %, `data/parse` ≥ 80 %; HAL/Render werden manuell auf Hardware geprüft

## Phase 4 — Makefile

Dünner Wrapper um PlatformIO, damit `make` für jeden Entwickler ohne PIO-Memorisierung funktioniert.

- [ ] Targets:
  - [ ] `make build` → `pio run -e esp32dev`
  - [ ] `make upload` → `pio run -e esp32dev -t upload`
  - [ ] `make monitor` → `pio device monitor -b 115200`
  - [ ] `make flash` → `upload` + `monitor`
  - [ ] `make test` → `pio test -e native`
  - [ ] `make test-verbose` → `pio test -e native -v`
  - [ ] `make clean` → `pio run -t clean` + `rm -rf .pio build`
  - [ ] `make format` → `clang-format -i src/**/*.{h,cpp} test/**/*.{h,cpp}`
  - [ ] `make lint` → `cppcheck --enable=warning,style src/`
  - [ ] `make size` → `pio run -e esp32dev -t size`
  - [ ] `make secrets` → kopiert `secrets.h.example` nach `secrets.h` wenn nicht da
  - [ ] `make help` → listet alle Targets
- [ ] `.PHONY` für alle Targets
- [ ] Default-Target = `help`
- [ ] `make ci` → `format-check` + `lint` + `test` + `build` (für GitHub Actions)

## Phase 5 — Hardware-Bring-up

Erst nach grünen Tests aus Phase 3.

- [ ] Verkabelung gemäß §1 Pin-Tabelle, BS-Jumper prüfen
- [ ] „Hello World"-Sketch: einmaliger Full Refresh mit GxEPD2-Beispiel — verifiziert Verkabelung, Treiberklasse, Modul-Revision
- [ ] WiFi-Verbindung mit echten Credentials
- [ ] Erster echter API-Call gegen Wiener Linien — JSON ins Log dumpen, `towards`-Strings für alle 3 RBLs verifizieren (Konzept §11)
- [ ] Erster vollständiger Render mit echten Daten
- [ ] Partial Refresh praktisch verifizieren — Layout so wählen, dass Bboxes klein bleiben (Slots vertikal separieren)
- [ ] Deep Sleep + Wake mit RTC-RAM Framebuffer
- [ ] Strommessung im Deep Sleep — Zielwert < 50 µA

## Phase 6 — Integrations- und Langzeittest

- [ ] 24-h-Lauf an echter Hardware, Logs mitschneiden
- [ ] Light-Full alle 2 h sichtbar passiert
- [ ] Deep Clean nachts passiert
- [ ] NTP-Sync sichtbar passiert
- [ ] Ghosting nach 24 h visuell beurteilen
- [ ] Stale-Verhalten durch WiFi-AP-Aus erzwingen
- [ ] 58B-Filter-Drift simulieren (FILTER_TOWARDS temporär auf Nonsens setzen) → Fehlertext erscheint nach 3 Calls

## Phase 7 — Entwickler-Dokumentation

- [ ] `docs/DEVELOPMENT.md`
  - [ ] Toolchain-Setup (PlatformIO, Python, USB-Treiber)
  - [ ] Repo klonen, `make secrets`, Credentials eintragen
  - [ ] Build / Flash / Monitor mit `make`
  - [ ] Tests laufen lassen
- [ ] `docs/ARCHITECTURE.md`
  - [ ] Modulkarte (Phase 2)
  - [ ] Dataflow-Diagramm (ASCII): API → Parse → Logik → Render → Display
  - [ ] Zustandsmaschine Cold Boot / Sleep / Active
  - [ ] Wo welche Konstante wirkt
- [ ] `docs/HARDWARE.md`
  - [ ] Stückliste mit Links
  - [ ] Verkabelung mit Foto/Diagramm
  - [ ] Modul-Revisionen UC8176 vs. SSD1683 und welche `GxEPD2`-Klasse jeweils
- [ ] `docs/TESTING.md`
  - [ ] wie neue Tests anlegen
  - [ ] wie Mocks für HAL-Interfaces nutzen
  - [ ] Fixtures-Konvention
- [ ] Code-Kommentare nur an nicht-offensichtlichen Stellen (8-px-Bbox-Alignment, RTC-RAM-Lifetime, `esp_sleep_get_wakeup_cause()`-Semantik)

## Phase 8 — Benutzer-Dokumentation

- [ ] `README.md` als Einstieg
  - [ ] Was ist das Bustaferl (Foto)
  - [ ] Was zeigt es (Screenshot des Displays)
  - [ ] Quick Start: Hardware + Flashen in 5 Schritten
  - [ ] Link auf `docs/`
- [ ] `docs/USER.md`
  - [ ] Erst-Inbetriebnahme: WiFi eintragen, RBLs eintragen, `towards`-Strings
  - [ ] Wo finde ich meine RBL-Nummer (Anleitung mit Screenshot der WL-OGD-Seite)
  - [ ] Wie WiFi später ändern (neu flashen)
  - [ ] Was bedeuten die Anzeigen: `HH:MM`, `—:—`, „veraltet", „58B Filter ungültig", „Start fehlgeschlagen"
  - [ ] Troubleshooting:
    - [ ] Display bleibt leer
    - [ ] Display zeigt nur Striche
    - [ ] Display friert ein
    - [ ] Akku/Netzteil-Anforderungen
- [ ] `CHANGELOG.md` ab Release 1.0

## Phase 9 — Release-Kandidat

- [ ] Version 1.0 taggen
- [ ] Pre-built `.bin` als GitHub Release anhängen (optional)
- [ ] Repo-Description und Topics auf GitHub setzen
