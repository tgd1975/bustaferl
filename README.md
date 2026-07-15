# bustaferl

E-Paper-Anzeige fürs Vorzimmer: nächste Abfahrten der Wiener-Linien-Buslinien
58A (Tullnertalgasse, beide Richtungen), 58B (Endemanngasse → Atzgersdorf,
nach der Schleife) und der ÖBB-S-Bahn Atzgersdorf → Wien Hauptbahnhof
(S2/S3/S4 plus gelegentliche REX). Rohe Echtzeitdaten, keine
Empfehlungslogik — die Entscheidung „aufbrechen oder warten" trifft der
Mensch selbst.

## Was es zeigt

Das Hauptbild zeigt vier Spalten — drei für die Buslinien, eine für die
S-Bahn — auf einem 400×300-px-Panel. Die autoritativen Host-Screenshots
stehen in [`docs/screenshots/`](docs/screenshots/) (`01-boot.png` …
`08-wifi-auth.png`, erzeugt von `make test-native-png`). Eine Kurzfassung des
Layouts findet sich in [`docs/USER.md`](docs/USER.md), die ausführliche in
[`docs/HANDBUCH.md`](docs/HANDBUCH.md).

**Solange irgendeine Abfahrt bekannt ist — Echtzeit oder Fahrplan — zeigt
das Gerät das Abfahrts-Board mit ihrer echten Uhrzeit**, auch Stunden im
Voraus. Es gibt keinen eigenen „Veraltet"-, „Nacht"- oder „Keine
Abfahrten"-Screen: das Board wird allein von der Datenlage bestimmt (nur
Echtzeit, nur Plan, gemischt, oder leer → `--:--`). Nur vier
Fehler-/Platzhalter-Screens ersetzen das Board ganz:

- **Boot** — Splash-Bild beim ersten Hochfahren, bis der erste Cycle
  Daten gerendert hat.
- **Kein Empfang (Offline)** — WiFi weg und länger als 5 min kein Erfolg.
- **Auth-Fehler** — HAFAS- oder OGD-Endpunkt liefert wiederholt
  401/Auth-Drift (AID rotiert → Firmware-Update nötig).
- **WLAN-Passwort falsch (WifiAuth)** — WPA-Handshake endgültig
  gescheitert; `secrets.h` korrigieren.

Symbole im Detail:

- `HH:MM` — nächste Abfahrt (Echtzeit)
- `□ HH:MM` — Plan-Marker: Slot ist Plan-Daten, nicht Echtzeit
- `--:--` — weder Echtzeit noch Fahrplan haben eine Abfahrt für den Slot

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
