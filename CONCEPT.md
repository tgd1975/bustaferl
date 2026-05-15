# bustaferl — Konzept

Das Bustaferl ist eine e-Paper-Anzeige fürs Vorzimmer. Es zeigt die nächsten Abfahrten der Linie 58A (Tullnertalgasse, beide Richtungen) und der Linie 58B (Endemanngasse, nur Richtung Atzgersdorf nach der Schleife). Es beantwortet die Frage: „muss ich aufbrechen, mich beeilen, warten oder zu Fuß gehen?" durch reine Echtzeit-Abfahrtsdaten — die Bewertung („genug Zeit?") macht der Nutzer selbst anhand bekannter Gehzeiten.

## 1. Hardware

- Waveshare 4.2" e-Paper Modul, 400×300 Pixel, schwarz/weiß
- Treiber-IC: UC8176 (Module vor 2022)
- Microcontroller: ESP32 (wegen Deep-Sleep-Eignung)
- Bibliothek: GxEPD2, Treiberklasse `GxEPD2_420`
- SPI-Verkabelung, BS-Jumper auf 0 (4-line SPI)

### Pin-Belegung

| ePaper | ESP32 GPIO | Bemerkung |
|---|---|---|
| 3.3V | 3V3 | |
| GND | GND | |
| DIN | 23 | HW-SPI MOSI (VSPI) |
| CLK | 18 | HW-SPI SCK (VSPI) |
| CS | 5 | klassischer SPI-CS |
| DC | 17 | frei |
| RST | 16 | frei |
| BUSY | 4 | frei, Input |

## 2. Datenquelle und Filter

Wiener-Linien-Echtzeit-API (OGD): `https://www.wienerlinien.at/ogd_realtime/monitor?rbl=…`. Kein API-Key, kostenfrei.

Drei Datenflüsse:
- **Tullnertalgasse**, Linie 58A, Richtung Atzgersdorf
- **Tullnertalgasse**, Linie 58A, Richtung Hietzing
- **Endemanngasse**, Linie 58B, gefiltert über `towards = "Atzgersdorf"` (das ist genau der Steig-Durchgang nach der Schleife)

Pro Abfahrt wird der Echtzeit-Wert verwendet. Liefert die API nur den Plan-Wert, wird dieser still als Fallback genommen — keine Unterscheidung sichtbar.

## 3. Display-Inhalt

Drei nach Linie und Richtung gruppierte Blöcke:

```
TULLNERTALGASSE
58A → Atzgersdorf      HH:MM  HH:MM
58A → Hietzing         HH:MM  HH:MM

ENDEMANNGASSE
58B → Atzgersdorf      HH:MM  HH:MM
   (nach Schleife)
```

Pro Richtung die nächsten zwei Abfahrten als absolute Uhrzeit. Keine Countdowns, keine aktuelle Uhrzeit, keine Aufbruchszeit, keine Niederflur-Info, keine Empfehlungslogik.

Leerwert pro Slot (API liefert nichts): `—:—`.

## 4. Aktualität (Stale-Verhalten)

Hart binär. Während das System wach ist und versucht, die API zu erreichen: wenn der letzte erfolgreiche Call ≥ 3 Minuten zurückliegt → Stale-Zustand:

- alle Uhrzeiten zu `—:—`
- deutlich sichtbarer Hinweis „veraltet"

Der Stale-Check gilt **nur im Wachzustand**. Während Deep Sleep nicht — die Vorbedingung „wir konnten nicht" ist nicht erfüllt, weil wir nicht wollten. Der letzte Render bleibt über die ganze Schlafphase sichtbar.

Bei erfolgreichem API-Call nach Stale: automatischer Rückwechsel.

## 5. Refresh-Strategie

Diff-basiert: jeden Wake-Zyklus neues Bild rendern, mit aktuell angezeigtem vergleichen. Identisch → nichts tun.

Bei Unterschied: Bounding Box der Änderungen ermitteln, X-Achse auf 8-Pixel-Grenzen ausrichten, Partial Refresh.

Stufen-Modell gegen Ghosting:

- **Partial Refresh**: bei jeder Änderung, kein Blinken, ~400–600 ms
- **Light Full Refresh** (1× S/W-Flash + Bild): alle ~2 h ODER nach 60–80 Partials seit dem letzten Full Refresh
- **Deep Clean** (3× S/W-Flash + Bild): einmal pro Nacht, eingebettet als Vor-Aktion des längsten Schlaf-Zeitraums

## 6. Deep-Sleep-Logik

Nach jedem Render Berechnung des nächsten Wake-Zeitpunkts:

```
wake_at = frühste_erreichbare_Abfahrt - 15 min - 30 s Boot-Margin
```

Annahme: angezeigte Abfahrtszeiten sind nicht zu früh. Die 15 Minuten sind Sicherheitsfenster zum Aufbruch; die 30 Sekunden decken Boot + WiFi + API + Render ab.

Sonderfälle:
- Differenz < 2 min → nicht schlafen, weiter aktiv
- API liefert keine relevanten Abfahrten → 30 min schlafen, dann erneut versuchen
- Längster Sleep der Nacht (z.B. nach letztem Bus bis ~5:00) → Deep Clean davor

## 7. NTP-Sync

Mindestens 1× alle 24 h. Bei jedem Wake prüfen: wenn `now() - last_ntp_sync > 24h` → syncen. Natürlicher Slot: gemeinsam mit dem nächtlichen Deep Clean.

ESP32-RTC driftet über Stunden um Sekunden, über Tage um Minuten — täglicher Sync reicht für Minuten-Granularität.

## 8. Edge Cases

- eine Richtung ohne Daten → `—:—` nur an der betroffenen Stelle, Rest unverändert
- API komplett unerreichbar im Wachzustand > 3 min → volles Striche-Bild
- während Deep Sleep: letzter Render bleibt, kein Stale-Trigger
- Pre-Sleep liefert API bereits erste Morgenbusse → bleiben über Nacht korrekt sichtbar
- kalte Umgebungstemperaturen (< 10 °C) → optional engerer Full-Refresh-Schwellwert (UC8176 ist temperaturempfindlich)

## 9. Konfiguration

### Im Source-Code (committed, im Repo)

RBLs, Pins, Schwellwerte. Z.B. `config.h`:

```cpp
// Wiener Linien RBLs
#define RBL_TULL_ATZGERSDORF  0   // TODO eintragen
#define RBL_TULL_HIETZING     0   // TODO eintragen
#define RBL_ENDEMANN          0   // TODO eintragen
#define FILTER_TOWARDS        "Atzgersdorf"

// e-Paper GPIO
#define EPD_CS    5
#define EPD_DC    17
#define EPD_RST   16
#define EPD_BUSY  4

// Verhalten
#define STALE_THRESHOLD_S        180     // 3 min
#define WAKE_BEFORE_BUS_S        900     // 15 min
#define BOOT_MARGIN_S             30
#define PARTIAL_BUDGET            70
#define LIGHT_FULL_INTERVAL_S   7200     // 2 h
#define NTP_INTERVAL_S         86400     // 24 h

// Zeit (Wien mit DST)
#define NTP_SERVER  "at.pool.ntp.org"
#define TZ_INFO     "CET-1CEST,M3.5.0,M10.5.0/3"
```

### In `secrets.h` (gitignore'd)

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID_PRIMARY      "DEINE_SSID"
#define WIFI_PASSWORD_PRIMARY  "DEIN_PASSWORT"

// Optional zweites Netz — Zeilen aktivieren wenn gewünscht:
// #define WIFI_SSID_SECONDARY      "..."
// #define WIFI_PASSWORD_SECONDARY  "..."

#endif
```

Repo enthält `secrets.h.example` als Template. README erklärt das Kopier-Ritual.

### WiFi-Failover

`WiFiMulti` aus dem ESP32-Arduino-Core:

```cpp
wifiMulti.addAP(WIFI_SSID_PRIMARY, WIFI_PASSWORD_PRIMARY);
#ifdef WIFI_SSID_SECONDARY
  wifiMulti.addAP(WIFI_SSID_SECONDARY, WIFI_PASSWORD_SECONDARY);
#endif

if (wifiMulti.run(10000) != WL_CONNECTED) {
    // beide unerreichbar → API-Call scheitert → Stale greift nach 3 min
}
```

## 10. Beim ersten Flashen auszufüllen

- WiFi-Daten in `secrets.h` (Vorlage kopieren)
- 3 RBL-Nummern in `config.h`
- Exakte `towards`-Strings aus realer API-Antwort verifizieren (z.B. „Hietzing" vs. „Hietzing S+U")

## Schwellwert-Defaults im Überblick

| Variable | Wert | Bedeutung |
|---|---|---|
| `STALE_THRESHOLD_S` | 180 | Sekunden bis Stale-Zustand |
| `WAKE_BEFORE_BUS_S` | 900 | Wake-Zeitpunkt vor Abfahrt |
| `BOOT_MARGIN_S` | 30 | Boot+WiFi+Render-Reserve |
| `PARTIAL_BUDGET` | 70 | Partials bis Light Full |
| `LIGHT_FULL_INTERVAL_S` | 7200 | Sekunden zwischen Light Fulls |
| `NTP_INTERVAL_S` | 86400 | Sekunden zwischen NTP-Syncs |
