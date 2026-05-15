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

| ePaper  | ESP32 GPIO | Bemerkung               |
|---------|------------|-------------------------|
| 3.3V    | 3V3        |                         |
| GND     | GND        |                         |
| DIN     | GPIO 23    | HW-SPI MOSI (VSPI)      |
| CLK     | GPIO 18    | HW-SPI SCK (VSPI)       |
| CS      | GPIO 5     | klassischer SPI-CS      |
| DC      | GPIO 17    | frei                    |
| RST     | GPIO 16    | frei                    |
| BUSY    | GPIO 4     | frei, Input             |

GPIO-Belegung in `src/config.h` änderbar (Defines `EPD_CS`, `EPD_DC`,
`EPD_RST`, `EPD_BUSY`).

## BS-Jumper

Auf der Rückseite des Waveshare-Moduls sitzt ein BS-Jumper, der zwischen
3-line SPI (1) und 4-line SPI (0) wählt. **Muss auf 0 stehen.**

## Verkabelungs-Hinweise

- Möglichst kurze Drähte (< 20 cm), insb. CLK und DIN
- Bei Brown-Out beim Boot: Netzteil mit mehr Strom (≥ 500 mA) oder
  zusätzlichen Elko (470 µF) zwischen 3V3 und GND
- ESP32-DevKits mit USB-zu-Serial-Bridge sind beim ersten Flashen einfacher
  als bare Module
