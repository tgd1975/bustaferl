# Tests

Alle Tests laufen auf dem Host (`env:native`), nicht auf dem ESP32. Das
Test-Framework ist [Unity](https://github.com/ThrowTheSwitch/Unity), das
PlatformIO mitbringt.

```bash
make test          # alle Suites
make test-verbose  # mit Unity-Detail-Output
pio test -e native -f test_sleep_planner   # nur eine Suite
```

## Testbar / nicht testbar

| Schicht          | Host-Tests             | Hardware-Tests          |
|------------------|------------------------|-------------------------|
| `logic/`         | ✅ vollständig         | nein                    |
| `data/`          | ✅ Parser mit Fixtures | nein                    |
| `render/rle.cpp` | ✅ Roundtrip-Tests     | nein                    |
| `render/layout`  | ❌ braucht GFX-Stack   | manuell auf Gerät       |
| `hal/Esp32*`     | ❌ ESP32-only          | nur auf Gerät           |
| `main.cpp`       | ❌ Verdrahtung         | Integrationstest        |

## Neue Tests anlegen

Jede Suite ist ein eigenes Verzeichnis unter `test/` mit genau einer
`test_main.cpp`:

```
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

| Modul           | Ziel    | Stand                                    |
|-----------------|---------|------------------------------------------|
| `logic/*`       | ≥ 90 %  | erreicht (Grenzfälle abgedeckt)          |
| `data/parse`    | ≥ 80 %  | erreicht (Happy/Plan-Fallback/Error/Empty)|
| `render/rle`    | hoch    | erreicht (Roundtrip + Overflow + Format) |

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
