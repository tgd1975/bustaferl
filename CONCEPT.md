# bustaferl — Konzept

Das Bustaferl ist eine e-Paper-Anzeige fürs Vorzimmer. Es zeigt die nächsten Abfahrten der Linie 58A (Tullnertalgasse, beide Richtungen) und der Linie 58B (Endemanngasse, nur Richtung Atzgersdorf nach der Schleife). Es beantwortet die Frage: „muss ich aufbrechen, mich beeilen, warten oder zu Fuß gehen?" durch reine Echtzeit-Abfahrtsdaten — die Bewertung („genug Zeit?") macht der Nutzer selbst anhand bekannter Gehzeiten.

## 1. Hardware

- Waveshare 4.2" e-Paper Modul, 400×300 Pixel, schwarz/weiß
- Treiber-IC: UC8176 (Module vor 2022) — Revision auf der Rückseite des Moduls prüfen, neuere Chargen mit SSD1683 brauchen eine andere GxEPD2-Klasse
- Microcontroller: ESP32 (wegen Deep-Sleep-Eignung)
- Bibliothek: GxEPD2, Treiberklasse `GxEPD2_420`
- SPI-Verkabelung, BS-Jumper auf 0 (4-line SPI)

### Pin-Belegung

Reihenfolge am Modul-Stecker: `BUSY · RST · DC · CS · CLK · DIN · GND · 3.3V`.
Kabelfarben entsprechen dem mitgelieferten Waveshare-Adapterkabel.

| ePaper | Kabel   | ESP32 GPIO | DevKit-Label | Bemerkung           |
|--------|---------|------------|--------------|---------------------|
| 3.3V   | rot     | 3V3        | `3V3`        |                     |
| GND    | schwarz | GND        | `GND`        |                     |
| DIN    | blau    | 23         | `D23`        | HW-SPI MOSI (VSPI)  |
| CLK    | gelb    | 18         | `D18`        | HW-SPI SCK (VSPI)   |
| CS     | orange  | 5          | `D5`         | SPI-CS              |
| DC     | grün    | 17         | `TX2`        |                     |
| RST    | weiß    | 16         | `RX2`        |                     |
| BUSY   | violett | 4          | `D4`         | Input               |

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

**Bewusste Vereinfachung:** Stale löscht *alle* Slots, auch noch nicht abgelaufene. Alternative wäre „letzte bekannte Werte mit Zeitstempel" — verworfen, weil veraltete Minutenangaben mehr verwirren als ein klares Striche-Bild. Die Anzeige soll entweder vertrauenswürdig sein oder eindeutig kaputt aussehen.

### API-Polling im Wachzustand

Poll-Intervall: **30 s**. Damit hat man bei Stale-Schwelle 180 s mindestens 5 Versuche, bevor die Anzeige umschlägt. Ein Wachzustand entsteht entweder im Vorlauf zur frühsten Abfahrt (siehe §6) oder als Übergangsmodus zwischen Polls — d. h. zwischen zwei Polls darf das Gerät kurz schlafen (Light Sleep, kein Deep Sleep mit Reboot), um Strom zu sparen, aber der Stale-Timer läuft weiter.

## 5. Refresh-Strategie

Diff-basiert: jeden Wake-Zyklus neues Bild rendern, mit aktuell angezeigtem vergleichen. Identisch → nichts tun.

Bei Unterschied: Bounding Box der Änderungen ermitteln, X-Achse auf 8-Pixel-Grenzen ausrichten, Partial Refresh.

### Framebuffer-Persistenz

Das zuletzt gerenderte Bild muss Deep Sleep überleben, sonst gibt es nichts zum Diffen. Speicherort: **RTC Slow Memory** (`RTC_DATA_ATTR`), 8 kB nutzbar. 400 × 300 px / 8 = 15 000 Byte — passt nicht roh hinein. Lösung: einfache RLE-Kompression über horizontale Bytes; bei einem typischen, weit überwiegend weißen Bustaferl-Layout reduziert das auf ~1–3 kB. Bei Kompressionsüberlauf (Notfall) wird der nächste Render als Light Full erzwungen und der Buffer verworfen.

### Stufen-Modell gegen Ghosting

- **Partial Refresh**: bei jeder Änderung, kein Blinken, ~400–600 ms
- **Light Full Refresh** (1× S/W-Flash + Bild): primär zeitgesteuert alle ~2 h. Partial-Zähler ist nur Sicherheitsnetz (Hard-Cap: 80), nicht der reguläre Trigger.
- **Deep Clean** (3× S/W-Flash + Bild): einmal pro Nacht, eingebettet als Vor-Aktion des längsten Schlaf-Zeitraums

## 6. Deep-Sleep-Logik

Nach jedem Render Berechnung des nächsten Wake-Zeitpunkts:

```
t_ref   = min(alle angezeigten Abfahrtszeiten über alle 3 Streams)
wake_at = t_ref − 15 min − 30 s Boot-Margin
delta   = wake_at − now()
```

`t_ref` ist die *gemeinsame* früheste Abfahrt aller drei Streams — egal welche Linie, welche Richtung. Sobald eine davon naht, soll die Anzeige aktuell sein. Annahme: angezeigte Abfahrtszeiten sind nicht zu früh. Die 15 Minuten sind Sicherheitsfenster zum Aufbruch; die 30 Sekunden decken Boot + WiFi + API + Render ab.

### Fallunterscheidung nach `delta`

| `delta` | Verhalten |
|---|---|
| ≥ 2 min | **Deep Sleep** bis `wake_at` |
| 0 – 2 min | **Nicht schlafen**, Light Sleep zwischen Polls (30 s, siehe §4) |
| < 0 (Wake-Punkt liegt in der Vergangenheit) | **Aktiver Wachzustand**: 15-min-Vorlauf bereits angebrochen oder überschritten. Kein Sleep, regulärer 30-s-Poll bis `t_ref` erreicht ist. |

Damit gibt es keine Sleep-Loop mit 0-Sekunden-Schlaf: sobald `delta < 2 min`, bleibt das Gerät durchgängig wach.

### Weitere Sonderfälle

- **API liefert keine relevanten Abfahrten** (Wochenende, Betriebspause, alle Linien out of service) → 30 min schlafen, dann erneut versuchen
- **Längster Sleep der Nacht** (z. B. nach letztem Bus bis ~5:00) → Deep Clean davor, NTP-Sync danach beim Wake
- **`t_ref` weniger als 30 s entfernt**: durchaus möglich beim Boot — kein Sleep, sofort weiterrendern

## 7. NTP-Sync

Mindestens 1× alle 24 h. Bei jedem Wake prüfen: wenn `now() - last_ntp_sync > 24h` → syncen. Natürlicher Slot: gemeinsam mit dem nächtlichen Deep Clean.

ESP32-RTC driftet über Stunden um Sekunden, über Tage um Minuten — täglicher Sync reicht für Minuten-Granularität.

## 8. Cold Boot (Stromausfall, erstes Flashen)

Reihenfolge nach Power-on-Reset, wenn weder Framebuffer noch RTC-Zeit gültig sind:

1. **WiFi** verbinden (WiFiMulti, Timeout 10 s)
2. **NTP-Sync** — zwingend, weil RTC bei 1970 startet und ohne korrekte Zeit kein sinnvolles `t_ref` berechnet werden kann
3. **API-Call** für alle 3 Streams
4. **Deep Clean** (3× S/W-Flash) — billiges Reset des Panel-Zustands, gibt sauberes Startbild ohne Ghost-Reste vom letzten Betrieb
5. **Voller Render** des Initialbilds
6. Framebuffer in RTC-RAM ablegen
7. Reguläre Sleep-Logik nach §6

Wenn Schritt 1 oder 2 fehlschlägt: 60 s warten, retry. Nach 5 Fehlversuchen: einmaliges Striche-Bild rendern mit Hinweis „Start fehlgeschlagen", dann 5 min schlafen und alles neu versuchen.

Erkennung „Cold Boot vs. Wake from Deep Sleep": `esp_sleep_get_wakeup_cause()`. Bei `ESP_SLEEP_WAKEUP_UNDEFINED` ist es Cold Boot.

## 9. Edge Cases

- eine Richtung ohne Daten → `—:—` nur an der betroffenen Stelle, Rest unverändert
- API komplett unerreichbar im Wachzustand > 3 min → volles Striche-Bild
- während Deep Sleep: letzter Render bleibt, kein Stale-Trigger
- Pre-Sleep liefert API bereits erste Morgenbusse → bleiben über Nacht korrekt sichtbar
- **`towards`-Filter für 58B greift nicht** (Wiener Linien hat den String geändert): sichtbarer Fehlertext „58B Filter ungültig" in der Endemanngasse-Zeile statt stilles `—:—`. Erkennbar daran, dass die API zwar Daten für den RBL liefert, aber keine einzige Departure mehr auf das `FILTER_TOWARDS`-Muster matcht — über mindestens 3 aufeinanderfolgende erfolgreiche Calls.
- RTC-Framebuffer-RLE überschreitet 7 kB → nächster Render erzwingt Light Full, Buffer wird verworfen und neu aufgebaut

## 10. Konfiguration

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
#define POLL_INTERVAL_S           30     // API-Poll im Wachzustand
#define ACTIVE_THRESHOLD_S       120     // unter delta=2 min nicht mehr deep-sleepen
#define NO_DATA_SLEEP_S         1800     // 30 min, wenn API keine Abfahrten liefert
#define PARTIAL_HARDCAP           80     // Sicherheitsnetz, regulär zeitgesteuert
#define LIGHT_FULL_INTERVAL_S   7200     // 2 h
#define NTP_INTERVAL_S         86400     // 24 h
#define COLD_BOOT_RETRY_S         60     // WiFi/NTP-Retry beim Cold Boot
#define COLD_BOOT_MAX_RETRIES      5

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

## 11. Beim ersten Flashen auszufüllen

- WiFi-Daten in `secrets.h` (Vorlage kopieren)
- 3 RBL-Nummern in `config.h`
- Exakte `towards`-Strings aus realer API-Antwort verifizieren (z.B. „Hietzing" vs. „Hietzing S+U")

## Schwellwert-Defaults im Überblick

| Variable | Wert | Bedeutung |
|---|---|---|
| `STALE_THRESHOLD_S` | 180 | Sekunden bis Stale-Zustand |
| `WAKE_BEFORE_BUS_S` | 900 | Wake-Zeitpunkt vor Abfahrt |
| `BOOT_MARGIN_S` | 30 | Boot+WiFi+Render-Reserve |
| `POLL_INTERVAL_S` | 30 | API-Poll im Wachzustand |
| `ACTIVE_THRESHOLD_S` | 120 | unter dieser delta kein Deep Sleep mehr |
| `NO_DATA_SLEEP_S` | 1800 | Sleep, wenn API keine Abfahrten liefert |
| `PARTIAL_HARDCAP` | 80 | Sicherheitsnetz für Light Full |
| `LIGHT_FULL_INTERVAL_S` | 7200 | Sekunden zwischen Light Fulls |
| `NTP_INTERVAL_S` | 86400 | Sekunden zwischen NTP-Syncs |
| `COLD_BOOT_RETRY_S` | 60 | Retry-Intervall WiFi/NTP beim Cold Boot |
| `COLD_BOOT_MAX_RETRIES` | 5 | Retries beim Cold Boot bis Fehlerbild |
