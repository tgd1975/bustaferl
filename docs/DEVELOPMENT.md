# Entwickler-Doku

## Toolchain

- **PlatformIO Core** ≥ 6.1 (`pip install platformio` oder via VSCode-Extension)
- **Python 3.11+** (für PlatformIO)
- **clang-format** (optional, für `make format`)
- **cppcheck** (optional, für `make lint`)
- USB-Treiber für deinen ESP32 (CP210x oder CH340 je nach Board)

PlatformIO zieht beim ersten Build automatisch:

- ESP32-Toolchain (xtensa-esp32)
- Arduino-Core für ESP32
- GxEPD2, Adafruit GFX, ArduinoJson, WiFiMulti

## Erstmaliges Setup

```bash
git clone https://github.com/tgd1975/bustaferl
cd bustaferl
make secrets             # legt src/secrets.h aus dem Template an
$EDITOR src/secrets.h    # WiFi-Daten eintragen
$EDITOR src/config.h     # RBLs und towards-Strings eintragen
```

## Build, Flash, Monitor

```bash
make build         # nur kompilieren
make upload        # flashen
make monitor       # serielle Konsole, 115200 baud
make flash         # upload + monitor
```

## Tests

Alle Logik- und Datenmodule sind plattformneutral und laufen auf dem Host:

```bash
make test          # alle Unit-Tests
make test-verbose  # mit Unity-Detail-Output
```

Tests laufen im `env:native` mit Unity. Siehe [`TESTING.md`](TESTING.md) für
Details zu Mocks, Fixtures und neuen Test-Cases.

## CI

`.github/workflows/ci.yml` läuft bei jedem Push auf `main` oder `claude/**`:

1. Unit-Tests (host)
2. Firmware-Build für ESP32

Falls CI bricht, das gleiche lokal nachstellen:

```bash
make ci
```

## Code-Stil

- C++17, GNU-Dialekt für Arduino-Kompatibilität
- 4-Space-Indent, keine Tabs
- Header guards `BUSTAFERL_<MODUL>_H`
- Namespace `bustaferl` für alles
- Hardware-Includes (`<Arduino.h>`, `<WiFi.h>`, GxEPD2) **nur** in
  `src/hal/Esp32*.cpp` und `src/main.cpp` — alles andere muss im
  `env:native` kompilieren
- Kommentare nur dort, wo das „warum" nicht offensichtlich ist

## Module-Layer-Regel

```text
main.cpp          → darf alles
hal/Esp32*.cpp    → darf Arduino-Core und Treiber
hal/*.h           → reines Interface, nur std::
data/             → plattformneutral, darf ArduinoJson nutzen
logic/            → plattformneutral, KEINE Bibliotheken außer std::
render/           → layout.cpp/error_overlay.cpp brauchen Adafruit GFX
                    (deshalb `#ifndef NATIVE_BUILD`)
                    rle.cpp und frame_buffer.h sind plattformneutral
```

Wenn die CI rot wird wegen „undefined symbol" beim Native-Build:
vermutlich hat ein Logik-Modul einen Arduino-Header geerbt. Layer prüfen.

## Konfigurationsschwellen anpassen

Alle Magic-Numbers leben in `src/config.h`. Eine Änderung an
`STALE_THRESHOLD_S`, `WAKE_BEFORE_BUS_S` etc. erfordert nur einen Rebuild,
keine Quelltext-Änderung.

## Debugging

- `Serial.printf("[tag] msg\n", …)` — landet bei 115200 baud im Monitor
- Cold-Boot-Pfad zwingen: ESP32 vom Strom trennen, neu einstecken
- Stale-Verhalten erzwingen: WiFi-Router ausschalten und 4 min warten
- Filter-Drift simulieren: `FILTER_TOWARDS_58B` temporär auf `"XXX"` setzen
- Deep Sleep abkürzen: `WAKE_BEFORE_BUS_S` runter, neu flashen
