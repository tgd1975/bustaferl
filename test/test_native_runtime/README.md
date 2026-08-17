# test_native_runtime — Host-Loop für `cycle_runner`

Headless-Treiber, der `runColdCycle` und `runWarmCycle` aus
[../../src/logic/cycle_runner.h](../../src/logic/cycle_runner.h) auf dem Host
gegen die echten Wiener-Linien-Endpoints fährt. Refactor-Plan §9.

## Was es testet

- Cycle-Logik unter realer HTTP-Latenz und realen JSON-Responses (deckt
  Klassen ab, die der `test_native_*`-Bucket mit Fake-`INetwork` nicht
  sehen kann — Header-Encoding-Edge-Cases, EFA-Mapping gegen Live-Daten,
  Timing der `pause`/`deepSleep`-Übergänge in Wandzeit).
- Heap-Profil über viele Cycles in einem Prozess (valgrind +
  `make native-runtime-smoke`). Leaks im Render-Pfad korrelieren mit der
  PGM-Anzahl im Dump-Verzeichnis.

## Was es **nicht** testet

- e-Paper-Refresh-Artefakte (kein Panel im Host-Pfad; Waveform/Ghosting
  bleibt Sache des Geräts).
- GPIO/Button-Verhalten — wird durch Schritt 9 nicht gefahren.

Seit dem HostCanvas-Parity-Commit (`b47ca14`) rendert `RecordingRenderer`
das **echte Produktions-Layout** — die PGM-Dumps zeigen exakt, was das
Panel angezeigt hätte.

## Ausführen

| Target                              | Was             | Dauer        |
|-------------------------------------|-----------------|--------------|
| `make native-runtime-build`         | Treiber bauen   | ~5–10 s      |
| `make native-runtime-https-smoke`   | 3 Live-Calls    | ~3 s         |
| `make native-runtime-smoke`         | 10 Cycles + valgrind | ~5 min       |
| `make native-runtime-day`           | 24-h-Soak       | bis 24 h     |

`ci-heavy` zieht den Smoke automatisch mit (`make ci` bleibt schnell für
den Pre-Commit-Hook).

## Konfiguration via ENV

| Variable                  | Default                                                   | Effekt                                          |
|---------------------------|-----------------------------------------------------------|-------------------------------------------------|
| `BUSTAFERL_API_BASE`      | `https://www.wienerlinien.at/ogd_realtime/monitor`        | Realtime-Endpoint                               |
| `BUSTAFERL_EFA_BASE`      | `https://www.wienerlinien.at/ogd_routing/XSLT_DM_REQUEST` | EFA-Schedule-Endpoint                           |
| `BUSTAFERL_INSECURE`      | `0`                                                       | `1` → TLS-Verify aus (für lokalen Mock-Runner)  |
| `BUSTAFERL_TIME_SCALE`    | `1.0`                                                     | Sleep-Multiplikator (`0.1` = 10× schneller)     |
| `BUSTAFERL_MAX_CYCLES`    | `0` (∞)                                                   | Stop nach N Warm-Cycles                         |
| `BUSTAFERL_PERSIST_PATH`  | `.tmp/native-runtime/persist.bin`                         | DiskStore-File                                  |
| `BUSTAFERL_FRESH_BOOT`    | `1`                                                       | Persist-File beim Start löschen (= Cold-Boot)   |
| `BUSTAFERL_LOG_PATH`      | `.tmp/native-runtime/run.log`                             | Timestamped Run-Log (append, flush pro Zeile)   |

## Output

- `.tmp/native-runtime/run.log` — timestamped Trace jedes Cycles: gerenderte
  Slots (HH:MM + Quelle RT/PLAN/HINT pro Stream), Panel-Calls
  (partial/lightFull/deepClean inkl. Bbox), geplante Sleeps. Ein Freeze
  zeigt sich als Zeitlücke nach der letzten Zeile; ein falscher Slot-Wert
  steht mit Kontext im Log.
- `.tmp/native-runtime/frame-NNNNNN.pgm` — P5-PGM, einer pro
  Display-Update. Anzahl = Anzahl der echten Frame-Wechsel; identische
  Frames werden dedupliziert.
- `.tmp/native-runtime/persist.bin` — `PersistedMeta` + RLE-Framebuffer +
  `ScheduleSnapshot`. Format spiegelt `Esp32PersistentStore` (zwei
  Magics, kein Quer-Plattform-Anspruch).
- `.tmp/native-runtime/valgrind.log` — bei `make native-runtime-smoke`.

PGMs sind 1bpp-auf-8bpp expandiert (Schwarz `0x00`, Weiß `0xFF`) und
mit `eog`, `feh`, `display`, oder per `xdg-open` ansehbar. Das
Pseudo-Raster ist ein 20×20-Cell-Tiling über einen FNV-Hash der
Slot-Timestamps — kein Layout-Test, sondern eine **Diff-Sentinel**: wenn
zwei Frames identisch aussehen, war auch der Input identisch.

## Dateien

- `main.cpp` — Treiber (siehe oben).
- `WallClockClock.h`, `NoOpSleep.h`, `NoOpDisplay.h` — Trivial-Adapter (9.1).
- `DiskStore.{h,cpp}` — File-backed `IPersistentStore` (9.1).
- `HttpsNet.{h,cpp}` — libcurl-`INetwork` (9.2).
- `RecordingRenderer.{h,cpp}` — Dump-Renderer (9.3).
- `https_smoke.cpp` — Standalone-Live-Call-Smoke für `HttpsNet`.

## Tests

Die einzelnen Adapter haben eigene Unity-Tests im `env:native`-Bucket:

- `test_native_runtime_diskstore` — DiskStore-Roundtrip.
- `test_native_runtime_renderer` — RecordingRenderer-Dedup + Determinismus.

Beide laufen mit `make test-native` automatisch mit.
