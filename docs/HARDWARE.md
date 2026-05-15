# Hardware

## Stückliste

| Teil                  | Bezugsquelle (Beispiel)                                 | Anmerkung                                    |
|-----------------------|---------------------------------------------------------|----------------------------------------------|
| Waveshare 4.2" e-Paper (UC8176, 400×300, S/W) | Waveshare-Shop, Amazon, AliExpress      | nicht „V2"; siehe Revisions-Hinweis unten   |
| ESP32 DevKit (z. B. ESP32-WROOM-32) | beliebig                                  | wegen Deep-Sleep-Eignung                     |
| Dupont-Kabel female-female × 8 | Bastelladen                                    |                                              |
| USB-Netzteil 5 V / ≥ 500 mA | beliebig                                          |                                              |
| Optional: 18650-Akku-Shield + LDO | für mobilen Betrieb                            |                                              |

## Modul-Revision: UC8176 vs. SSD1683

Waveshare hat ab ca. 2022 die 4.2"-Module auf den Controller **SSD1683**
umgestellt. Optisch identisch, aber **andere Init-Sequenz**. So erkennst du
deine Revision:

- **Rückseite des Moduls anschauen** — Aufdruck mit Versionsnummer
- **Versionen V1 / V2 mit UC8176** → `GxEPD2_420` (im Code voreingestellt)
- **Version V2 von 2022 oder neuer / „4.2inch e-Paper V2.x" mit SSD1683**
  → Treiberklasse in `src/hal/Esp32Display.cpp` ändern auf `GxEPD2_420_GDEY042T81`
  oder einen anderen GxEPD2-SSD1683-Subtreiber, der zu deinem Modul passt
  (siehe [GxEPD2-README](https://github.com/ZinggJM/GxEPD2#supported-spi-e-paper-panels-from-good-display))

Falls du dir unsicher bist: einfach mit der Voreinstellung flashen und das
GxEPD2-Beispiel `GxEPD2_HelloWorld` ausprobieren. Falsche Treiberklasse →
Display bleibt leer oder zeigt Müll.

## Pin-Belegung (4-line SPI)

Reihenfolge am 8-Pin-JST-Stecker des Moduls (so steht es auf der Platine):
`BUSY · RST · DC · CS · CLK · DIN · GND · 3.3V`. Das mitgelieferte Kabel
hat Standard-Waveshare-Farben.

| ePaper | Kabelfarbe | ESP32 GPIO | Boardlabel* | Bemerkung           |
|--------|------------|------------|-------------|---------------------|
| 3.3V   | rot        | 3V3        | `3V3`       |                     |
| GND    | schwarz    | GND        | `GND`       |                     |
| DIN    | blau       | GPIO 23    | `D23`       | HW-SPI MOSI (VSPI)  |
| CLK    | gelb       | GPIO 18    | `D18`       | HW-SPI SCK (VSPI)   |
| CS     | orange     | GPIO 5     | `D5`        | SPI-CS              |
| DC     | grün       | GPIO 17    | `TX2`       | = GPIO 17           |
| RST    | weiß       | GPIO 16    | `RX2`       | = GPIO 16           |
| BUSY   | violett    | GPIO 4     | `D4`        | Input               |

\* Bezieht sich auf das ESP32-WROOM-32 30-Pin-DevKit (DEVKIT V1) — die
beiden mittleren Pins der Unterreihe heißen dort `RX2`/`TX2` statt
`D16`/`D17`. GPIO-Nummer ist verbindlich, nicht das Label.

Alle GPIO-Zuordnungen sind in `src/config.h` über `EPD_CS`, `EPD_DC`,
`EPD_RST`, `EPD_BUSY` änderbar (MOSI/SCK sind über VSPI fixiert).

## BS-Jumper

Auf der Rückseite des Waveshare-Moduls sitzt ein BS-Jumper, der zwischen
3-line SPI (1) und 4-line SPI (0) wählt. **Muss auf 0 stehen.**

## Verkabelungs-Hinweise

- Möglichst kurze Drähte (< 20 cm), insb. CLK und DIN
- Bei Brown-Out beim Boot: Netzteil mit mehr Strom (≥ 500 mA) oder
  zusätzlichen Elko (470 µF) zwischen 3V3 und GND
- ESP32-DevKits mit USB-zu-Serial-Bridge sind beim ersten Flashen einfacher
  als bare Module
