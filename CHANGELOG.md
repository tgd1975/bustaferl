# Changelog

## Unreleased

### Zustandsmaschine vereinfacht

- **`Stale`, `Night` und `Quiet` als Display-States entfernt.** Jeder Screen
  mit Daten ist jetzt das `Normal`-Board; die Datenlage (nur Echtzeit, nur
  Plan, gemischt, leer) bestimmt das Bild, kein Modus-Label. Grundregel: Die
  nächste Abfahrt (Echtzeit **oder** Plan) wird immer mit ihrer echten Uhrzeit
  gezeigt — auch Stunden im Voraus; `--:--` nur, wenn auch der Fahrplan nichts
  hat. `DisplayState` hat nur noch fünf Werte (Boot, Normal, Offline, Auth,
  WifiAuth). Bei Fetch-Fehlern behält der Warm-Zyklus das letzte gute Bild.
- Entfernt: `QUIET_HORIZON_S`, `NIGHT_FIRST_DEP_MIN_AHEAD_S`,
  `SERVICE_WINDOW_*`, `drawQuiet`, `allDeparturesBeyond`,
  `outsideServiceWindow`, `nextDepartureFarAway`, das `stale`-Flag im Renderer
  sowie die `CYC_STALE`/`StaleEnter`/`StaleExit`-Trace-Einträge
  (`TRACE_MAGIC`-Bump). Device-Mockviews 2/3/4 gelöscht.

### Fehlerbehebungen

- **Boot-Screen im Warmbetrieb:** Ein Nicht-Deep-Sleep-Reset (Brownout,
  Watchdog) wurde als Kaltstart fehlgeroutet. Routing jetzt in host-getesteter
  `selectCycle()` — mit vorhandenem Board geht ein Reset direkt ins Board.
- **Falsches-Passwort-Fehlauslöser:** WLAN-Disconnect-Reason 15 /
  `AUTH_EXPIRE` lösten den terminalen WifiAuth-Screen auch bei korrektem
  Passwort aus. Reason-Set eingeengt auf `{AUTH_FAIL, MIC_FAILURE}`.
- **Button:** Long-Press feuert jetzt beim Erreichen des 3-s-Timeouts (nicht
  erst beim Loslassen); RTC-Pullup auf GPIO 0 im Deep Sleep gehalten gegen
  sporadisch verpasste Wakes.
- **58A-Zeit-Ausrichtung:** Rechtsbündige Zeiten wackelten ±2 px je nach
  letzter Ziffer (Tinten- statt Advance-Breite); Ausrichtung an fixer
  Referenzbreite.

## v2.0 — S-Bahn Atzgersdorf (sichtbarer Display-Rewrite)

**Breaking:** RTC-`MAGIC` wurde gebumpt — beim ersten Boot nach dem
Firmware-Update wird der gespeicherte Frame verworfen und ein voller
Cold-Boot-Cycle ausgelöst (~10–20 s). Kein Anwender-Eingriff nötig.

### Neue Datenquelle

- ÖBB-HAFAS-Anbindung (`mgate.exe` POST → `oebb_hafas_parse`): neuer
  S-Bahn-Stream Atzgersdorf → Wien Hbf mit S2/S3/S4 + REX, ersetzt die
  beiden U1-Streams (Leopoldau, Oberlaa) am Südtirolerplatz.
- `Stream`-Enum reduziert auf vier Einträge (`STREAM_COUNT = 4`); alte
  `STREAM_U1_*` ohne Backward-Compat-Aliasse entfernt.
- Auth-Tripwire (`PersistedMeta::auth_error_seen`) erkennt
  HAFAS-`AID`-Rotation (sofort, `err: "AID"` / `err: "AUTH"`) und
  OGD-401-Drift (debounced, 3× in Folge).

### Display-Rewrite

- Komplett neues Layout (4-Spalten-Board statt 3-Block-Liste), eigene
  Render-Module `bitmap_fonts`, `badge`, `plan_marker`, `network_plan`,
  `display_state`, abstrakter `Canvas` mit Host- und Adafruit-GFX-Variante.
- **Sieben Display-States** mit eigenem State-Selector
  (`logic/render_input::selectDisplayState`): Normal, Stale, Night,
  Quiet, Offline, Auth, Boot. Ersetzt die alten Banner-Overlays.
- **Plan-Marker `□`**: 5×5-Pixel-Rahmen vor Slot-Zeiten, die aus
  Plan-Daten statt Echtzeit stammen.
- **Netzplan**: kleines Diamant-/Big-/Dot-Schema unten rechts mit
  „you are here"-Marker am Bahnhof Atzgersdorf.
- Mockview-Firmware-Variante (`mockview-1` … `mockview-8`) für die
  HW-Sichtkontrolle der einzelnen States.

### Konfiguration

- Neue Konstanten in `config.h`: `OEBB_EXTID_ATZG`, `OEBB_EXTID_WIENHBF`,
  `OEBB_HAFAS_AID`, `OEBB_MGATE_URL`, `OEBB_JNYFLTR_PRODUCTS`,
  `OEBB_MAX_JNY`, `OFFLINE_THRESHOLD_S`, `QUIET_HORIZON_S`,
  `OGD_AUTH_STREAK_TRIPWIRE`.
- EFA-Schedule-Pfad fetcht nur noch 2 distinkte DIVAs statt 3 (S-Bahn
  hat keinen Plan-Hint-Pfad in v2).

### Tests

- Neue Native-Buckets: `test_native_oebb_hafas_parse`,
  `test_native_badge`, `test_native_plan_marker`,
  `test_native_network_plan`, `test_native_render_all_states`.
- Bestehende Native-Buckets durchgängig auf `STREAM_COUNT = 4`
  migriert (cycle_runner_*, slot_merger, filter_*, render_input, …).
- Device-Buckets (`test_device_fetch`, `test_device_schedule`,
  `test_device_persistent`) ziehen die v2-Streams nach.
- Longterm-Buckets (smoke, horizon_*, day_full, soak, jitter) auf
  3-Bus-Stream-URL umgestellt.

### Doku

- Komplette Synchronisation: CONCEPT (v2-Sektion + EVA→HAFAS-extId
  Korrektur), README, ARCHITECTURE, HANDBUCH (Display-Abschnitt
  Rewrite), USER (Symbol-Cheatsheet, AID-Erneuerung), TESTING.
- Autoritative Layout-Referenz: die Host-Screenshots in
  `docs/screenshots/` (`make test-native-png`).

## v1.0 — Wiener Linien

- Erste Implementierung gemäß `CONCEPT.md`:
  - HAL-Abstraktion (Clock/Network/Display/Sleep/PersistentStore)
  - Wiener-Linien-OGD-Parser mit `towards`-Filter
  - Logik-Module: stale, sleep planning, refresh planning,
    filter health, cold-boot sequencer
  - Renderer mit Adafruit GFX, RLE-komprimierter Framebuffer im RTC-RAM
  - Unit-Test-Suite (Unity, host-only, alle Logik-Module)
  - PlatformIO + Makefile + GitHub-Actions CI
  - Entwickler- und Benutzer-Dokumentation
