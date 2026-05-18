# Tests

Drei Buckets, drei Make-Targets, glob-basierte Filter. Make ist der
kanonische Einstieg — `make help` listet alles.

| Bucket | Wo läuft's | Wann | Make-Target |
| --- | --- | --- | --- |
| `test_native_*` (25) | Host (`env:native`) | nach jedem Edit, ~5 s | `make test-native` (alias `make test`) |
| `test_device_*` (5) | ESP32 (`env:device-*`) | pre-commit mit Gerät, ~5–10 min | `make test-device` |
| `test_longterm_*` (8 Sources / 10 Targets) | ESP32 (oder PC-driven Mock) | opt-in, 3 min … 24 h | `make test-longterm-*` |
| `native-runtime` (Host-Engine) | Host (direktes `g++`) | opt-in, ~3 s … 24 h | `make native-runtime-*` |

Direkter `pio test -e …` ist Debug-Pfad für eine einzelne Env. Die
Folder-Übersicht mit Concern je Test steht in
[test/README.md](../test/README.md).

```bash
make test                      # Host-Suite, ~5 s
make test-device               # alle device-* (skip wenn kein ESP32)
make test-all                  # host + device (smart skip)
make test-longterm-smoke       # 3 min nach Hardware-Eingriff
make test-longterm-soak-15min  # 15 min pre-tag (Tier 2)
make test-longterm-day-full    # 24 h pre-major-release (Tier 3)
```

## Drei Tiers — Routine-Workflow

**Tier 1 — pre-commit (billig, automatisierbar):**

| Anlass | Was läuft | Dauer |
| --- | --- | --- |
| `git commit` | `make test` (host) | ~5 s |
| nach Hardware-Eingriff | `make test-longterm-smoke` | 3 min |
| Quick-Sanity nach Engine-Änderung | `make test-longterm-soak-5min` | 5 min |

**Tier 2 — pre-tag (ritualisiert via `/release`):**

| Anlass | Was läuft | Dauer |
| --- | --- | --- |
| `git tag` (jeder Tag) | `make test-all` + `make test-longterm-soak-15min` | ~25 min |
| Network-Layer-Änderung | + `make test-longterm-jitter` | +10 min |
| Horizon/EFA-Änderung | + `make test-longterm-horizon-mock` | +10 min |
| Sleep/Persistent-Änderung | + `make test-longterm-wake` | +30 min |

**Tier 3 — pre-major-release (unattended overnight):**

| Anlass | Was läuft | Dauer |
| --- | --- | --- |
| Major/Minor Version-Tag | Tier 2 + `make test-longterm-day-full` | + 24 h |
| Weekly Soak-Baseline | `make test-longterm-soak-1h` | 1 h |

Niemand stellt einen Wecker. Niemand startet einen Test um 03:30 —
`day-full` wird abends gestartet und läuft bis zum nächsten Abend.

### Definition: Hardware-Eingriff

Alles was die **physische** Seite anfasst, nicht den Code:

- Nachlöten / Verkabelung geändert
- ESP32-Modul oder Display getauscht / neu aufgesteckt
- Buttons umgepinnt
- Andere Stromversorgung / neue Batterie
- Gehäuse geöffnet, Board ist mechanisch unterwegs gewesen
- Board runtergefallen / Stoß bekommen

Heuristik: wenn du den Zustandswechsel nicht sicher mit *nur
Software-Diff* erklären könntest → Hardware-Eingriff. `test-longterm-smoke`
(3 min) ist dann der „läuft die Kiste überhaupt noch"-Check vor
Code-Iteration.

## Long-term Test-Set — 8 Sources / 10 Targets

Eigene Source je Test — **Ausnahme**: die `soak`-Familie teilt eine
Source und parametrisiert per Build-Define (`LONGTERM_SOAK_CYCLES`).

| Test | Dauer | Klasse | Concern |
| --- | --- | --- | --- |
| `test_longterm_smoke` | ~3 min | Routine | HW-Sanity nach Flash |
| `test_longterm_jitter` | ~10 min | Routine | Retry/Reconnect unter WiFi-Drop |
| `test_longterm_horizon_mock` | ~10 min | Routine | EFA-Fallback Logic (PC-driven Mock) |
| `test_longterm_wake_cycle` | ~30 min | Routine | Deep-Sleep + Persistent Store |
| `test_longterm_soak` (5min) | ~5 min | Routine | Heap-Quick-Check |
| `test_longterm_soak` (15min) | ~15 min | Routine | Heap-Confidence Pre-Commit |
| `test_longterm_soak` (1h) | ~1 h | Routine | Heap-Leak canonical |
| `test_longterm_horizon_scan` | ~90 min | Iteration | Rolling-Window-Cliff (Tageslicht) |
| `test_longterm_horizon_evening` | ~5 h | Iteration | Evening-Dry-Up + Nacht-Stille |
| `test_longterm_day_full` | ~24 h | Pre-release | beide Live-Übergänge + Refresh-Budget |

## `native-runtime` — Host-Engine gegen Live-Endpoints

Standalone-Treiber, der `runColdCycle` / `runWarmCycle` aus
[../src/logic/cycle_runner.h](../src/logic/cycle_runner.h) auf dem Host
gegen die echten Wiener-Linien-Endpoints fährt — kein PIO-Test, ein direktes
`g++`-Target im Makefile (siehe Annahme in `main-refactor-plan.md` §4.1
Schritt 9).

| Target | Was | Dauer |
| --- | --- | --- |
| `make native-runtime-build` | Treiber bauen | ~5–10 s |
| `make native-runtime-https-smoke` | 3 Live-Calls via `HttpsNet` | ~3 s |
| `make native-runtime-smoke` | 10 Cycles unter valgrind | ~5 min |
| `make native-runtime-day` | 24-h-Soak | bis 24 h |

`make ci` bleibt absichtlich schnell (~30–45 s, pre-commit-tauglich) und
ruft den Runtime-Smoke nicht; `make ci-heavy` zieht ihn mit. Adapter-Tabelle,
ENV-Variablen und Output-Layout: siehe
[../test/test_native_runtime/README.md](../test/test_native_runtime/README.md).

Concern-Abdeckung:

| Concern | Wer fängt's |
| --- | --- |
| HTTPS-Edge-Cases gegen Live-Endpoint | `native-runtime-smoke` |
| EFA-Mapping gegen reale Responses | `native-runtime-smoke` |
| Heap-Verlauf über viele Cycles (valgrind) | `native-runtime-smoke` |
| 24-h-Soak ohne ESP32-Bindung | `native-runtime-day` |

## Testbar / nicht testbar

| Schicht | Host-Tests | On-Device-Tests |
| --- | --- | --- |
| `logic/` | ✅ vollständig | im `device-fetch` mit-exerziert |
| `data/` | ✅ Parser mit Fixtures | live-Antwort im `device-fetch` |
| `render/rle.cpp` | ✅ Roundtrip-Tests | im `device-persistent` indirekt |
| `render/layout` | ❌ braucht GFX-Stack | ✅ `device-render` |
| `render/error_overlay` | ❌ | ✅ `device-render` |
| `hal/Esp32Network` | ❌ ESP32-only | ✅ `device-fetch` |
| `hal/Esp32Clock` | ❌ ESP32-only | ✅ `device-fetch` |
| `hal/Esp32PersistentStore` | ❌ ESP32-only | ✅ `device-persistent` |
| `hal/Esp32Display` | ❌ ESP32-only | nur visuell (Panel-Output) |
| `hal/Esp32Sleep` | ❌ ESP32-only | ✅ `device-sleep` (Light-Sleep-Proxy) + `longterm-wake` (echte Resets) |
| `main.cpp` | ❌ Verdrahtung | Engine-Sequenz im `device-fetch` |

Long-term-Coverage:

| Concern | Test |
| --- | --- |
| Heap-Leak | `longterm-soak-*` |
| Persistent-Pfad über echten Reset | `longterm-wake` |
| Retry/Reconnect | `longterm-jitter` |
| EFA-Fallback-Logic | `longterm-horizon-mock` |
| Rolling-Window-Cliff | `longterm-horizon-scan` |
| Evening-Dry-Up live | `longterm-horizon-evening` |
| Live-Übergänge + Refresh-Budget | `longterm-day-full` |

**Migration-Phase-2-Hinweis**: ein Versuch, `render/layout` und
`render/error_overlay` auf `env:native` umzuziehen, ist gescheitert —
`Adafruit_GFX.h` includet hart `Arduino.h`/`Print.h`/`WProgram.h`, die
auf `platform = native` nicht existieren. Render-Tests bleiben deshalb
on-device.

## Test-Pair-Konvention (host ↔ host-äquivalent + device-Integration)

| Host-pure | Device-Integration |
| --- | --- |
| `test_native_rle` | `test_device_persistent` |
| `test_native_wienerlinien_parse` | `test_device_fetch` |
| `test_native_efa_parse` | `test_device_schedule` |
| `test_native_runtime_diskstore` | `test_device_persistent` |
| `test_native_runtime_renderer` | `test_device_render` |
| `test_native_cycle_runner_*` (cold/warm/helpers/invariants) | `test_device_fetch` + `test_device_sleep` |

Konvention: jeder device-Test, der eine host-Variante hat, erwähnt
diese im Top-of-File-Kommentar — eine Zeile.

## Neue Tests anlegen

Präfix bestimmt den Bucket — die Filter-Regeln in `platformio.ini`
greifen automatisch (glob):

```text
test/test_native_<x>/test_main.cpp     → läuft in env:native
test/test_device_<x>/test_main.cpp     → braucht env:device-<x>
test/test_longterm_<x>/test_main.cpp   → braucht env:longterm-<x>
```

Für device/longterm noch je ein Env in `platformio.ini` mit
`test_filter = test_<folder>` ergänzen. Pure host-Tests sind reine
`git add`.

Minimales Skelett:

```cpp
#include <unity.h>
#include "logic/meine_logik.h"

using namespace bustaferl;

void test_happy() {
    TEST_ASSERT_EQUAL(42, doSomething());
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_happy);
    return UNITY_END();
}
```

## Mocks für HAL-Interfaces

Alle HAL-Interfaces sind so geschnitten, dass Stubs in 5–10 Zeilen
passen. Beispiel `test/test_native_boot_sequencer/test_main.cpp`:

```cpp
class FakeNet : public INetwork {
public:
    bool will_connect = true;
    bool connect(unsigned) override { return will_connect; }
    bool isConnected() override { return will_connect; }
    bool httpGet(const std::string&, std::string&) override { return false; }
};
```

Wenn ein Interface nur eine Methode hat, kann sie inline implementiert
werden — keine eigene Header-Datei nötig.

## Fixtures

Hardcoded als `R"JSON(...)JSON"`-Literale im Test-File. Vorteil: keine
Dateisystem-Abhängigkeit im Host-Build, Test ist hermetisch. Nachteil:
nicht geeignet für sehr große Payloads — für unsere Stichproben passt
es.

## `.tmp/` — projektlokales Scratch

`make test-device` und alle `test-longterm-*`-Targets schreiben JSON
nach `.tmp/test-results.json` bzw. `.tmp/longterm-<name>.json`. `.tmp/`
ist gitignored. Forensik-Stream für eine einzelne Env:

```bash
make test-device-trace ENV=device-fetch
# → .tmp/traces/device-fetch.log  (volles Serial-Log)
```

Sidecar-Meta für den `/release`-Skill:

```text
.tmp/test-all.meta.json              {"commit":"...", "timestamp":..., "pass":true}
.tmp/longterm-soak-15min.meta.json   gleiche Form
```

Der Skill liest die Meta + vergleicht `commit` gegen `git rev-parse
HEAD`; ist der SHA veraltet oder pass=false, refused der Skill den Tag.

## Mock-API-Runner für `horizon_mock`

PC-driven Integration-Test
([test/test_longterm_horizon_mock/runner.py](../test/test_longterm_horizon_mock/runner.py)):

- Python-Skript (~250 LOC) startet lokalen HTTP-Server auf Loopback
- Drei Phasen, wallclock-getriggert:
  1. **Cycle 1–3**: voller Departure-Set → Device zeigt live-Daten
  2. **Cycle 4–6**: leere Streams → EFA-Hint soll aktivieren
  3. **Cycle 7–9**: wieder voller Set → EFA-Hint soll deaktivieren
- Device wird mit `MOCK_API_BASE=http://<host>:8080/...` gebaut
  (Build-Define via Test-Env `longterm-horizon-mock-firmware`)
- Runner parst `[engine]` Log-Tags via Serial, asserts auf Übergänge
- Pass/Fail-Logik im Python-Skript, nicht auf dem Device

Vorteil: ~10 min statt 12+ h für die Logik-Verifikation. Deterministisch.
Jederzeit lauffähig.

## Enforcement

- **Pre-commit**: `make test` lokal vor jedem Commit (manuell oder via
  selbst gewähltem Hook).
- **CI**: `make ci` (= `format-check` + `lint` + `test-native` + `build`)
  — CI hat keine HW, deshalb host-only.
- **Pre-tag**: `/release` Skill (siehe
  [.claude/skills/release/SKILL.md](../.claude/skills/release/SKILL.md))
  blockt den Tag wenn `.tmp/test-all.meta.json` / `longterm-soak-15min.meta.json`
  veraltet oder rot sind.

## Coverage-Ziele

| Modul | Ziel | Stand |
| --- | --- | --- |
| `logic/*` | ≥ 90 % | erreicht (Grenzfälle abgedeckt) |
| `data/parse` | ≥ 80 % | erreicht (Happy / Plan-Fallback / Error / Empty) |
| `render/rle` | hoch | erreicht (Roundtrip + Overflow + Format) |

`gcov`-Auswertung ist nicht eingerichtet — bei Bedarf:

```bash
pio test -e native --verbose 2>&1 | tee .tmp/test.log
# in env:native build_flags ergänzen: -fprofile-arcs -ftest-coverage
gcov src/logic/*.cpp
```

## Tests gegen das echte Gerät — Erstinbetriebnahme

Die alte manuelle Checkliste (`GxEPD2_HelloWorld` flashen, 30 min
beobachten, etc.) ist durch konkrete Long-term-Targets ersetzt. Bei
echter Erstinbetriebnahme:

1. `make test-longterm-smoke` — bestätigt WiFi + HTTPS + Parse + Heap
   in 3 Minuten
2. `make test-longterm-soak-15min` — bestätigt die Heap-Stabilität
   über eine Pre-Commit-relevante Dauer
3. Volle Geräte-Verifikation: `make test-all` (host + alle device-*)
