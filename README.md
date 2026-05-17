# bustaferl

E-Paper-Anzeige fürs Vorzimmer: nächste Abfahrten der Wiener-Linien-Buslinien
58A (Tullnertalgasse, beide Richtungen) und 58B (Endemanngasse → Atzgersdorf,
nach der Schleife). Rohe Echtzeitdaten, keine Empfehlungslogik — die
Entscheidung „aufbrechen oder warten" trifft der Mensch selbst.

## Was es zeigt

```text
TULLNERTALGASSE
58A -> Atzgersdorf      HH:MM  HH:MM
58A -> Hietzing         HH:MM  HH:MM

ENDEMANNGASSE
58B -> Atzgersdorf      HH:MM  HH:MM
   (nach Schleife)
```

- `HH:MM` — nächste Abfahrt (Echtzeit, mit stillem Fallback auf Plan)
- `--:--` — keine Abfahrt im Datenstrom
- Banner `VERALTET` — API-Daten älter als 3 min
- Banner `58B Filter ungueltig` — die Richtungs-Filterung schlägt fehl, RBL prüfen
- Banner `Start fehlgeschlagen` — Cold Boot hat WiFi/NTP nicht hochbekommen

## Quick Start

1. Hardware verkabeln gemäß [`docs/HARDWARE.md`](docs/HARDWARE.md)
2. `make secrets` — legt `src/secrets.h` aus dem Template an
3. WiFi-Daten in `src/secrets.h` eintragen
4. RBL-Nummern und `towards`-Strings in `src/config.h` eintragen
   (siehe [`docs/USER.md`](docs/USER.md) für Anleitung)
5. `make flash` — kompiliert, flasht, öffnet Serial-Monitor

## Dokumentation

- [`CONCEPT.md`](CONCEPT.md) — Architektur-Entscheidungen, Verhalten im Detail
- [`docs/USER.md`](docs/USER.md) — Erstinbetriebnahme, Anzeigen-Bedeutung,
  Troubleshooting
- [`docs/HARDWARE.md`](docs/HARDWARE.md) — Stückliste, Verkabelung,
  Modul-Revisionen
- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — Toolchain, Build, Flash
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Modulkarte, Dataflow,
  Zustandsmaschine
- [`docs/TESTING.md`](docs/TESTING.md) — Tests schreiben und laufen lassen
- [`TODO.md`](TODO.md) — Umsetzungsplan, abhakbar

## Build-Targets

```text
make build      compile firmware for ESP32
make upload     flash to attached ESP32
make flash      upload + open monitor
make monitor    open serial monitor
make test       run unit tests on host
make lint       run cppcheck
make format     run clang-format
make size       firmware size breakdown
make clean      remove build artifacts
make ci         what CI runs (format-check + lint + test + build)
make help       list targets
```

## Lizenz

Siehe [`LICENSE`](LICENSE).
