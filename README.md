# bustaferl

E-Paper-Anzeige fürs Vorzimmer: nächste Abfahrten der Wiener-Linien-Buslinien
58A (Tullnertalgasse, beide Richtungen), 58B (Endemanngasse → Atzgersdorf,
nach der Schleife) und der ÖBB-S-Bahn Atzgersdorf → Wien Hauptbahnhof
(S2/S3/S4 plus gelegentliche REX). Rohe Echtzeitdaten, keine
Empfehlungslogik — die Entscheidung „aufbrechen oder warten" trifft der
Mensch selbst.

## Was es zeigt

Das Hauptbild zeigt vier Spalten — drei für die Buslinien, eine für die
S-Bahn — auf einem 400×300-px-Panel. Die autoritativen Mockups stehen in
[`docs/design_handoff_display/`](docs/design_handoff_display/):
`screen-1-normal.png` zeigt den Normalfall, `screen-2-veraltet.png` …
`screen-7-boot.png` die jeweiligen Sonderzustände. Eine Kurzfassung des
Layouts findet sich auch in [`docs/USER.md`](docs/USER.md).

Es gibt sieben Display-Zustände, die der State-Selector aus Datenlage,
NTP-Sync, Uhrzeit und letzter erfolgreicher Abfrage ableitet:

- **Normal** — frische Daten, alle Slots aktuell.
- **Veraltet** — Daten älter als ~5 min; alle Slots zeigen `??:??`.
- **Nachtbetrieb** — außerhalb der Service-Zeit, leere Slots werden
  als Plan-Hinweise auf den Morgen aufgefüllt.
- **Keine Abfahrten** — Service-Zeit, aber kein Stream liefert eine
  Fahrt (Pause/Wendezeit).
- **Kein Empfang** — WiFi/NTP fehlgeschlagen oder beide Endpunkte
  schweigen.
- **Auth-Fehler** — HAFAS- oder OGD-Endpunkt liefert wiederholt
  401/Auth-Drift (AID rotiert → Firmware-Update nötig).
- **Boot** — Splash-Bild beim ersten Hochfahren, bis der erste Cycle
  Daten gerendert hat.

Symbole im Detail:

- `HH:MM` — nächste Abfahrt (Echtzeit, mit stillem Fallback auf Plan)
- `□ HH:MM` — Plan-Marker: Slot ist Plan-Daten, nicht Echtzeit
- `--:--` — Stream antwortet, hat aber keine Abfahrt im Horizont
- `??:??` — Daten sind älter als die Stale-Schwelle (Display zeigt
  weiter, aber als veraltet markiert)

## Quick Start

1. Hardware verkabeln gemäß [`docs/HARDWARE.md`](docs/HARDWARE.md)
2. `make secrets` — legt `src/secrets.h` aus dem Template an
3. WiFi-Daten in `src/secrets.h` eintragen
4. RBL-Nummern, `towards`-Strings sowie HAFAS-Location-IDs / AID-Wert
   in `src/config.h` eintragen (siehe [`docs/USER.md`](docs/USER.md) für
   Anleitung)
5. `make flash` — kompiliert, flasht, öffnet Serial-Monitor

## Dokumentation

- [`CONCEPT.md`](CONCEPT.md) — Architektur-Entscheidungen, Verhalten im Detail
- [`docs/HANDBUCH.md`](docs/HANDBUCH.md) — Anzeigen-Bedeutung mit Screenshots,
  Refresh-Rhythmus, Echtzeit vs. Plandaten, Offline-Verhalten
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
