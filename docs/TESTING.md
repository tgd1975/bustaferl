# Tests

Zwei Test-Sorten — Host-Suite (`env:native`, schnell, Mocks) und
On-Device-Suites (`env:esp32-test-*`, exakte Hardware-Pfade über Unity-Serial).

```bash
make test          # Host-Suite
make test-verbose  # mit Unity-Detail-Output
pio test -e native -f test_sleep_planner   # nur eine Suite

pio test -e esp32-test-fetch       # WiFi + HTTPS + Parse, NTP, Engine-Recovery
pio test -e esp32-test-persistent  # RTC slow memory + RLE round-trip
pio test -e esp32-test-render      # renderFrame() + error_overlay
pio test -e esp32-test-sleep       # Esp32Sleep wakeupCause mapping
pio test -e esp32-test-longterm    # 60 fetch cycles / 60 s = ~1 h Stress
```

## Testbar / nicht testbar

| Schicht                    | Host-Tests             | On-Device-Tests               |
|----------------------------|------------------------|-------------------------------|
| `logic/`                   | ✅ vollständig         | im fetch-Test mit-exerziert   |
| `data/`                    | ✅ Parser mit Fixtures | live-Antwort im fetch-Test    |
| `render/rle.cpp`           | ✅ Roundtrip-Tests     | im persistent-Test indirekt   |
| `render/layout`            | ❌ braucht GFX-Stack   | ✅ esp32-test-render          |
| `render/error_overlay`     | ❌                     | ✅ esp32-test-render          |
| `hal/Esp32Network`         | ❌ ESP32-only          | ✅ esp32-test-fetch           |
| `hal/Esp32Clock`           | ❌ ESP32-only          | ✅ esp32-test-fetch           |
| `hal/Esp32PersistentStore` | ❌ ESP32-only          | ✅ esp32-test-persistent      |
| `hal/Esp32Display`         | ❌ ESP32-only          | nur visuell (Panel-Output)    |
| `hal/Esp32Sleep`           | ❌ ESP32-only          | nur über Wake-Path im Betrieb |
| `main.cpp`                 | ❌ Verdrahtung         | Engine-Sequenz im fetch-Test  |

## Neue Tests anlegen

Jede Suite ist ein eigenes Verzeichnis unter `test/` mit genau einer
`test_main.cpp`:

```text
test/test_meine_logik/test_main.cpp
```

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

PlatformIO findet die Suite automatisch — kein zusätzlicher Eintrag in
`platformio.ini` nötig.

## Mocks für HAL-Interfaces

Beispiel `test/test_boot_sequencer/test_main.cpp`:

```cpp
class FakeNet : public INetwork {
public:
    bool will_connect = true;
    bool connect(unsigned) override { return will_connect; }
    bool isConnected() override { return will_connect; }
    bool httpGet(const std::string&, std::string&) override { return false; }
};
```

Alle HAL-Interfaces sind so geschnitten, dass Stubs in 5–10 Zeilen passen.
Wenn ein Interface nur eine Methode hat, kann sie inline implementiert
werden — keine eigene Header-Datei nötig.

## Fixtures

Hardcoded als `R"JSON(...)JSON"`-Literale im Test-File. Vorteil: keine
Dateisystem-Abhängigkeit im Host-Build, Test ist hermetisch. Nachteil: nicht
geeignet für sehr große Payloads. Für unsere Stichproben passt es.

## Coverage-Ziele

| Modul        | Ziel   | Stand                                      |
|--------------|--------|--------------------------------------------|
| `logic/*`    | ≥ 90 % | erreicht (Grenzfälle abgedeckt)            |
| `data/parse` | ≥ 80 % | erreicht (Happy/Plan-Fallback/Error/Empty) |
| `render/rle` | hoch   | erreicht (Roundtrip + Overflow + Format)   |

`gcov`-Auswertung ist nicht eingerichtet — bei Bedarf:

```bash
pio test -e native --verbose 2>&1 | tee test.log
# in env:native build_flags ergänzen: -fprofile-arcs -ftest-coverage
gcov src/logic/*.cpp
```

## Tests gegen das echte Gerät

Manuell, in dieser Reihenfolge:

1. `GxEPD2_HelloWorld`-Beispiel flashen → bestätigt Verkabelung und Treiberklasse
2. Voller bustaferl-Flash mit korrekten RBLs
3. Serial-Monitor offen lassen, 30 min beobachten:
   - mindestens 1× Render
   - Deep Sleep wird ausgelöst und kommt zurück
4. Stale-Test: WiFi aus, 4 min warten → Banner muss erscheinen
5. Filter-Drift-Test: `FILTER_TOWARDS_58B` auf Nonsens, flashen, 3 erfolgreiche Calls → Banner

Diese Schritte sind in [`TODO.md`](../TODO.md) Phase 6 als Checkliste.
