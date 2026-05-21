# bustaferl — Konzept

Das Bustaferl ist eine e-Paper-Anzeige fürs Vorzimmer. Es beantwortet die Frage: „muss ich aufbrechen, mich beeilen, warten oder zu Fuß gehen?" durch reine Echtzeit-Abfahrtsdaten — die Bewertung („genug Zeit?") macht der Nutzer selbst anhand bekannter Gehzeiten.

Das Dokument ist in zwei Versionen gegliedert:

- **v1 — Wiener Linien** beschreibt den umgesetzten Stand: Linien 58A (Tullnertalgasse, beide Richtungen), 58B (Endemanngasse, nur Atzgersdorf-Schleife) und U1 (Südtiroler Platz, beide Richtungen) ausschließlich über die Wiener-Linien-Schnittstellen.
- **v2 — S-Bahn Atzgersdorf** ersetzt die beiden U1-Streams durch einen ÖBB-S-Bahn-Stream Richtung Wien Hauptbahnhof. Hardware und Verhaltenslogik aus v1 bleiben unverändert; nur Datenquelle, Stream-Topologie und Layout-Block 3 ändern sich.

## Inhaltsverzeichnis

v1 — Wiener Linien:

- [1. Hardware](#1-hardware)
- [2. Datenquelle und Filter](#2-datenquelle-und-filter)
- [3. Display-Inhalt](#3-display-inhalt)
- [4. Aktualität (Stale-Verhalten)](#4-aktualität-stale-verhalten)
- [5. Refresh-Strategie](#5-refresh-strategie)
- [6. Deep-Sleep-Logik](#6-deep-sleep-logik)
- [7. NTP-Sync](#7-ntp-sync)
- [8. Cold Boot (Stromausfall, erstes Flashen)](#8-cold-boot-stromausfall-erstes-flashen)
- [9. Edge Cases](#9-edge-cases)
- [10. Konfiguration](#10-konfiguration)
- [11. Beim ersten Flashen auszufüllen](#11-beim-ersten-flashen-auszufüllen)
- [12. Plan-Erstabfahrten am Abend („Bus-in-der-Früh"-Anzeige)](#12-plan-erstabfahrten-am-abend-bus-in-der-früh-anzeige)
- [Schwellwert-Defaults im Überblick](#schwellwert-defaults-im-überblick)

v2 — S-Bahn Atzgersdorf:

- [1. Motivation und Geltungsbereich](#1-motivation-und-geltungsbereich)
- [2. Datenquellen-Recherche](#2-datenquellen-recherche)
- [3. Stationen-IDs](#3-stationen-ids)
- [4. Request-Schema (Primär: mgate.exe)](#4-request-schema-primär-mgateexe)
- [5. Stream- und Datenmodell-Änderungen](#5-stream--und-datenmodell-änderungen)
- [6. API-Polling, Quoten und Wake-Verhalten](#6-api-polling-quoten-und-wake-verhalten)
- [7. Layout-Block 3 (Display)](#7-layout-block-3-display)
- [8. Plan-Hints (Analogie zu v1 §12)](#8-plan-hints-analogie-zu-v1-12)
- [9. Edge Cases und Fehlerbilder](#9-edge-cases-und-fehlerbilder)
- [10. Pre-Flash-Verifikation & Open Questions](#10-pre-flash-verifikation--open-questions)
- [11. Migrationsschritte (umgesetzt)](#11-migrationsschritte-umgesetzt-siehe-docsv2-rolloutv2-sbahn-migration-planmd)
- [Quellen (Web-Recherche zu v2)](#quellen-web-recherche-zu-v2)

---

## v1 — Wiener Linien

Das Bustaferl zeigt in dieser Version die nächsten Abfahrten der Linien 58A (Tullnertalgasse, beide Richtungen), 58B (Endemanngasse, nur Richtung Atzgersdorf nach der Schleife) und U1 (Südtiroler Platz, beide Richtungen).

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

```text
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

```text
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

## 12. Plan-Erstabfahrten am Abend („Bus-in-der-Früh"-Anzeige)

Die OGD-Realtime-API liefert nur ein Fenster von ~70 Minuten vor jeder Abfahrt. Damit am Abend „wann fährt der Bus in der Früh?" sichtbar wird, müssen wir die Plandaten dazumischen.

### 12.1 Datenquelle: EFA-Departure-Monitor

`https://www.wienerlinien.at/ogd_routing/XSLT_DM_REQUEST` mit `outputFormat=JSON`, `useRealtime=0`. Liefert pro Haltestelle (DIVA-ID) eine Liste planmäßiger Abfahrten ab einer gewünschten Stichzeit.

Pro Departure-Eintrag relevant: `dateTime` (geplante Zeit), `servingLine.number` (Linie), `servingLine.direction` (Richtungsname).

Direction-Strings unterscheiden sich von der OGD-API — z. B. EFA `"Wien Atzgersdorf"` vs. OGD `"Bhf. Atzgersdorf S (üb. Atzgersdorfer Str.)"`. Eigene `FILTER_TOWARDS_EFA_*`-Konstanten parallel zu den OGD-Konstanten.

### 12.2 Was gespeichert wird

Pro Stream (in RTC Slow Memory, ~120 Byte gesamt):

```cpp
struct ScheduleHint {
  time_t last_today;        // letzte planmäßige Abfahrt heute (Refresh-Trigger)
  time_t next_today[2];     // die letzten zwei planmäßigen Abfahrten vor cutoff
  time_t first_tomorrow[2]; // erste zwei planmäßige Abfahrten morgen
};
ScheduleHint hint[STREAM_COUNT];
time_t schedule_fetched_at;
```

`last_today` ist Trigger für Refresh-Logik, kein direkter Display-Wert. `next_today` schließt die Lücke am Abend: heute wäre `slot[]` nach Ende des 70-Min-Realtime-Fensters leer, obwohl der Plan noch Abfahrten kennt — diese Bridge wird nun mitgemischt.

### 12.3 Refresh-Strategie

**Cold Boot** (§8): nach dem Realtime-Fetch, vor Deep Clean, ein EFA-Fetch pro Haltestelle. Best Effort — schlägt es fehl, bleibt `schedule_fetched_at = 0` und der Renderer fällt auf reines Realtime-Verhalten zurück.

**Warm Cycle**: getriggert, nicht zeitgesteuert. Bedingung: `now() > min(hint[*].last_today)` *und* `schedule_fetched_at < heute_00:00`. Das fällt natürlich mit dem nächtlichen Deep-Clean-Slot zusammen (beides „WiFi ohnehin an, lange Wachphase"). Sicherheits-Fallback: wenn `now() - schedule_fetched_at > 48 h` → erzwungener Refresh.

**Call-Schema** pro Haltestelle: ein einziger Call mit `itdTime=22:00` (heute) und `limit=50`. Die Response deckt typischerweise die letzten Abfahrten heute + die ersten morgen ab. Daraus client-seitig je Stream (Line+Direction-Filter):

- `last_today` = letzter Eintrag mit `dateTime` < morgen 03:00
- `next_today[0..1]` = die *letzten zwei* Einträge mit `dateTime` < morgen 03:00 (chronologisch); der Slot-Merger filtert die in der Vergangenheit liegenden via `t < now`, übrig bleiben die noch ausstehenden Abend-Abfahrten
- `first_tomorrow[0..1]` = erste zwei Einträge mit `dateTime` ≥ morgen 03:00

Drei Haltestellen → drei Calls pro Tag.

### 12.4 Verwendung im Display

Eine Regel — keine Sondertypografie, keine zusätzlichen Zeilen, kein Hinweis „Plan vs. Echtzeit":

```text
slot[0..1] = die nächsten zwei Departures ab now() aus:
             realtime
               ∪ {hint.next_today[0], hint.next_today[1]}
               ∪ {hint.first_tomorrow[0], hint.first_tomorrow[1]}
             nach Zeit sortiert, ersten beiden zeigen
```

Bewusst keine Unterscheidung: sobald eine Abfahrt ins 70-Minuten-Realtime-Fenster rutscht, ersetzt der Realtime-Wert den Hint auf demselben Slot — exakt das gewünschte „schrittweise" Verhalten. Nutzer sieht keinen Bruch zwischen Plan und Realtime, weder morgens noch abends.

`hint`-Werte werden ignoriert, wenn `schedule_fetched_at == 0` (nie geladen) oder älter als 48 h. Stale-Mechanik aus §4 bleibt unverändert.

### 12.5 Edge Cases

- **EFA unreachable** während Cold Boot oder geplantem Refresh → alte `hint`-Werte bleiben gültig (bis Alters-Cap); kein Display-Effekt. Retry beim nächsten regulären Anlass.
- **EFA-`direction`-Filter greift nicht** (analog zum bestehenden FilterHealth für OGD): `hint`-Werte für betroffenen Stream bleiben 0; Renderer verhält sich für diesen Slot wie heute (`—:—` außerhalb des Realtime-Fensters). Reicht für jetzt — eigene FilterHealth-Instanz für EFA wäre über-engineered.
- **Wochenende/Feiertag**: EFA berücksichtigt Kalendervarianten automatisch — wir bekommen die richtigen Werte für den jeweils nächsten Verkehrstag.
- **DST-Übergang**: `dateTime` aus EFA wird zu `time_t` (UTC) normalisiert; intern alles `time_t`, keine HH:MM-Strings.
- **Schema-Änderung in RTC-Layout**: `MAGIC` in `Esp32PersistentStore` bumpen, damit alte Strukturen nach Update nicht falsch interpretiert werden.

### 12.6 Pre-flash-Konfiguration

Zusätzlich zu §11:

- **DIVA-Stop-IDs** für die drei Haltestellen in `config.h` (verifiziert via `XSLT_STOPFINDER_REQUEST?name_sf=…`):
  - Tullnertalgasse → `60201395`
  - Endemanngasse → `60200278`
  - Südtiroler Platz / Hauptbahnhof → `60201349`
- **EFA-Direction-Strings** für die fünf Streams — einmal an einem echten Cold-Boot-Call verifizieren, ggf. korrigieren (analog zu den OGD-Towards-Konstanten).

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

---

## v2 — S-Bahn Atzgersdorf

Ersetzt die beiden U1-Streams (Leopoldau, Oberlaa) am Südtirolerplatz durch **einen** Stream: **Bhf. Wien Atzgersdorf → Wien Hbf**, mit den nächsten **2–3** Zügen (S2/S3/S4 plus gelegentlich REX).

Diese Version ergänzt v1. Sie ändert nur Datenquelle, Stream-Topologie und Layout-Block 3 — die Sleep-, Refresh-, Stale- und Cold-Boot-Logik aus v1 (§§4–9) bleibt unverändert übernommen.

## 1. Motivation und Geltungsbereich

Was bleibt gleich (aus v1):

- Hardware, Pin-Belegung, e-Paper-Stufenmodell
- 58A Tullnertalgasse (beide Richtungen) und 58B Endemanngasse als Streams 0–2, weiterhin über Wiener-Linien-OGD
- EFA-Plan-Hints für 58A/58B (v1 §12)
- Stale-Schwelle, Wake-Logik, Deep-Sleep, RLE-Persistenz, Diff-Refresh

Was sich ändert:

- Streams 3 + 4 (U1 Leopoldau/Oberlaa) entfallen. Neuer Stream **`STREAM_SBAHN_HBF`** ersetzt sie. `STREAM_COUNT` sinkt von 5 auf 4.
- Neue Datenquelle: **ÖBB Scotty (HAFAS mgate.exe)** für Echtzeit, plus optional ein paralleler Hint-Mechanismus für Spät-/Frühabfahrten.
- Layout-Block 3 zeigt eine Linie mit Linienkennung (S2/S3/S4/REX) je Slot, statt zwei Richtungen mit fixer Linie.

Nicht-Ziele:

- Keine Anzeige von Gleisnummer, Verspätungsminuten als Zahl, Zuglaufnummer oder Echtzeit-Zwischenhalten. Anzeige bleibt „nur Abfahrtsuhrzeit".
- Keine Mischung mit S-Bahnen Richtung Liesing/Mödling — nur Richtung Hauptbahnhof.

## 2. Datenquellen-Recherche

Die Wiener-Linien-OGD-Schnittstelle (RBL-Monitor) deckt **keine ÖBB-S-Bahn ab** — der RBL-Monitor liefert nur Tram/Bus/U-Bahn der Wiener Linien. Der in v1 §12 genutzte EFA-Endpunkt (`XSLT_DM_REQUEST` auf `wienerlinien.at`) zeigt am Bahnhof Atzgersdorf empirisch nur die Buslinien 10A/63A in der Umgebung, keine Züge — die EFA-Instanz der Wiener Linien ist auf den eigenen Verbundausschnitt zugeschnitten. ÖBB-seitige Quellen sind also Pflicht.

### 2.1 Vergleichstabelle

| Quelle | Endpoint | Format | Auth | Echtzeit | Risiko | Eignung ESP32 |
|---|---|---|---|---|---|---|
| **ÖBB Scotty mgate (HAFAS)** | `fahrplan.oebb.at/bin/mgate.exe` | JSON POST | reverse-engineerter `aid=OWDL4fE4ixNiPBBm`, `client={id:"OEBB",type:"WEB",name:"webapp"}` | ja | inoffiziell, AID/Client kann jederzeit rotieren | gut: kompaktes JSON, HTTPS, ein Call pro Refresh |
| **ÖBB stboard.exe (Web-Bahnhofstafel)** | `fahrplan.oebb.at/bin/stboard.exe/dn?input=8100634&boardType=dep&…` | HTML, optional `tableOXML` | keine | ja (im HTML) | offiziell, aber HTML-Layout undokumentiert und kann sich ändern | mittel: 30–80 kB HTML, parser-anfällig |
| **VAO REST (Verkehrsauskunft Österreich)** | `routenplaner.verkehrsauskunft.at` | JSON | API-Key nach signiertem Vertrag, 100 Calls/Tag | ja | offiziell, vertraglich gedeckt | gut, aber 100/Tag ist eng — siehe §6 Quoten |
| **GTFS-RT data.oebb.at** | data.oebb.at | Protobuf | keine | ja, ganz Österreich | offiziell | schlecht: Feed ist viele MB, kein Filter auf Station serverseitig |
| **VOR EFA (`efa.vor.at`)** | wie der Wiener-Linien-EFA, aber Verbund-weit | JSON | keine | unsicher | inoffiziell-toleriert | gut wenn realtime tatsächlich vorhanden — empirisch zu prüfen |

### 2.2 Empfehlung

**Primär: ÖBB Scotty (HAFAS mgate.exe).** Begründung:

- Liefert in einem einzigen Request alle Abfahrten ab einer Station mit Echtzeit, Direction-Filter und Produkt-Filter (S-Bahn vs. RJ vs. Nightjet).
- JSON ist ESP32-tauglich (~5 kB pro Antwort, ArduinoJson kommt damit zurecht — vgl. bestehende EFA-Parser).
- Wird von der offiziellen ÖBB-Webapp verwendet, also auch ohne Mobile-App gut beobachtbar (DevTools).
- Risikoabwehr durch denselben `FilterHealth`-Mechanismus, der bereits für Wiener-Linien-`towards` existiert: ändert ÖBB die Antwortstruktur, fällt das nach 3 erfolglosen Matches auf einen sichtbaren Fehlertext.

**Fallback / Plan B: stboard.exe** mit `L=vs_java3&tpl=stbResult2json` (URL-Parametervariante, die einige HAFAS-Instanzen JSON statt HTML liefern lassen — vor Flash an einer echten Antwort verifizieren). Falls Scotty-mgate.exe AID/Client ändert, kann man hier weiter ernten ohne neuen Vertragsabschluss.

**VAO ist die saubere Langzeitlösung**, aber: 100 Calls/Tag ÷ 18 h Wachfenster ≈ 5,5 Calls/h — zu wenig für 30-s-Polling im Aktivzustand. Erst sinnvoll, wenn ein Tarifmodell mit mehr Quota dazukommt oder das Polling-Schema gröber wird.

## 3. Stationen-IDs

ÖBB verwendet zwei verschiedene ID-Räume:

| Verwendung | Station | Wert | Beleg |
|---|---|---|---|
| Legacy `stboard.exe` (HTML, optional) | Wien Atzgersdorf | `8100634` | `fahrplan.oebb.at/bin/stboard.exe/dn?input=8100634` — offizieller Stationsbutton |
| Legacy `stboard.exe` (HTML, optional) | Wien Hauptbahnhof | `8100002` | Standard-HAFAS-Mapping, auch in DB-EVA-Listen geführt |
| **`mgate.exe` (Produktivpfad)** | Wien Atzgersdorf | `1292301` | `mgate.exe` LocMatch-Query (Beleg siehe §10) |
| **`mgate.exe` (Produktivpfad)** | Wien Hauptbahnhof | `1290401` | `mgate.exe` LocMatch-Query (Beleg siehe §10) |

Die 8-stelligen EVA-Nummern gelten ausschließlich für die Legacy-HTML-Schnittstelle. Für `mgate.exe` benötigt HAFAS die **internen Location-IDs** (`1292301` / `1290401`), wie die ÖBB-Webapp sie im LocMatch-Aufruf liefert (Pre-Phase 2026-05-19). Beide IDs sind Pre-Flash-Konstanten und gehören in `config.h` (`OEBB_EXTID_ATZG` / `OEBB_EXTID_WIENHBF`). Es gibt keine sinnvolle Laufzeit-Suche.

Reproduktionsweg, falls IDs erneut beschafft werden müssen: gegen `mgate.exe` einen `LocMatch`-Request mit `input.loc.name` = `Wien Atzgersdorf*` schicken (Body-Skelett wie unten, nur `meth: "LocMatch"`); der Response enthält `match.locL[].extId`, das ist der Wert für `stbLoc.extId` im StationBoard-Aufruf.

## 4. Request-Schema (Primär: mgate.exe)

POST `https://fahrplan.oebb.at/bin/mgate.exe` mit folgendem Body (Beispiel; konkrete Werte vor Flash gegen DevTools-Mitschnitt der Webapp gegenprüfen, da HAFAS-Profile rotieren):

```json
{
  "id": "bustaferl",
  "ver": "1.67",
  "lang": "deu",
  "auth": { "type": "AID", "aid": "OWDL4fE4ixNiPBBm" },
  "client": { "id": "OEBB", "type": "WEB", "name": "webapp", "l": "vs_webapp" },
  "formatted": false,
  "svcReqL": [{
    "meth": "StationBoard",
    "req": {
      "type": "DEP",
      "stbLoc": { "type": "S", "extId": "1292301" },
      "dirLoc": { "type": "S", "extId": "1290401" },
      "maxJny": 6,
      "jnyFltrL": [{ "type": "PROD", "mode": "INC", "value": "63" }]
    }
  }]
}
```

Felder:

- `stbLoc.extId` → HAFAS-Location-ID Atzgersdorf (`1292301`, **nicht** die 8-stellige EVA)
- `dirLoc.extId` → HAFAS-Location-ID Wien Hbf (`1290401`). Filtert HAFAS-seitig auf Züge, die Hbf als kommenden Halt haben. Das schließt automatisch die Gegenrichtung (Mödling/Liesing) aus, ohne dass wir auf Endstation-Strings filtern müssen.
- `jnyFltrL` → bitmask `value="63"` = S-Bahn + Regio + REX (Bits 0–5). Die echten Bitwerte für das ÖBB-Profil sind vor Flash zu verifizieren — siehe §10 Open Questions.
- `maxJny: 6` → 6 Abfahrten anfragen, im Client die ersten ≤3 Richtung Hbf nehmen. Puffer, falls `dirLoc` einzelne Fahrten doch nicht ausfiltert (z. B. Triebwagenwende).

Antwort enthält pro Eintrag mindestens:

- `jnyL[i].stbStop.dTimeS` — Plan-Abfahrt `HHMMSS`
- `jnyL[i].stbStop.dTimeR` — Echtzeit-Abfahrt (optional, fehlt im Stör-/Ausfall-Fall)
- `jnyL[i].stbStop.dCncl` — Bool, Fahrt entfällt
- `jnyL[i].prodL[0]` → Index in `common.prodL` → Linienname (`S2`, `S3`, `S4`, `REX1` …)
- `jnyL[i].stbStop.dPltfS.txt` — Bahnsteig (nicht angezeigt, aber für künftige Erweiterungen geparst lassen)

Datum/Heute-Logik: HAFAS liefert `date` Tagesfeld auf `svcResL[0].res.fpB`/`fpE`; das Tagesfeld pro Eintrag steht im `stbStop.dDateS`. Konvertierung wie EFA-Parser bereits in [efa_parse.cpp](src/data/efa_parse.cpp): YYYYMMDD + HHMMSS in `time_t` mit `TZ_INFO`.

## 5. Stream- und Datenmodell-Änderungen

### 5.1 Enum und Konstanten

[StreamSnapshot.h](src/data/StreamSnapshot.h):

```cpp
enum Stream {
  STREAM_58A_ATZ = 0,
  STREAM_58A_HIETZING = 1,
  STREAM_58B_ATZ = 2,
  STREAM_SBAHN_HBF = 3,
  STREAM_COUNT = 4
};
```

`STREAM_U1_LEOPOLDAU` und `STREAM_U1_OBERLAA` entfallen vollständig (keine Backward-Compat-Aliasse — der RTC-`MAGIC` in [Esp32PersistentStore](src/hal/Esp32PersistentStore.h) wird gebumpt, alte Frames sind dann ungültig und werden beim ersten Boot verworfen).

[config.h](src/config.h):

```cpp
// ÖBB Wien Atzgersdorf → Wien Hauptbahnhof
#define EVA_WIEN_ATZGERSDORF "8100634"
#define EVA_WIEN_HBF         "8100002"
#define OEBB_MGATE_URL       "https://fahrplan.oebb.at/bin/mgate.exe"
#define OEBB_HAFAS_AID       "OWDL4fE4ixNiPBBm"   // TODO: gegen aktuelle webapp gegenchecken
#define OEBB_MAX_JNY         6
#define OEBB_DISPLAY_SLOTS   3                    // Stream darf bis 3 Slots füllen, andere weiter 2
```

Die bisherigen `RBL_SUEDTIROLER_*`, `TOWARDS_U1_*`, `EFA_TOWARDS_U1_*`, `DIVA_SUEDTIROLER_PLATZ` werden entfernt.

### 5.2 Slot-Anzahl pro Stream

`StreamData::slot` ist im Bestand ein 2er-Array. Der Benutzerwunsch ist „2–3 Züge". Zwei Optionen:

- **A — bei 2 Slots bleiben.** Konsistent zum Rest, kein Code-Eingriff an `Departure`/Merger/Render. „2–3" wird zu „immer 2". Einfachst.
- **B — `slot`-Array auf 3 vergrößern**, im Layout pro Stream konfigurieren, wie viele Slots tatsächlich gerendert werden. Mehr Display-Information, aber rührt an alle Reducer und macht den `slot_merger` komplexer.

**Empfehlung: A**, mit der Option, später auf B zu erweitern, wenn der freie Layout-Platz (durch den Wegfall der zweiten U1-Zeile) das nahelegt. Begründung: Die Strecke Atzgersdorf→Hbf wird in der Hauptverkehrszeit alle 7–8 Minuten bedient, ein dritter Slot wäre meist nur 15 Minuten in der Zukunft — kaum Mehrwert über zwei.

Diese Entscheidung wird festgehalten und kann später revidiert werden — siehe §10.

### 5.3 Linienkennung pro Slot

Der einzige Stream im System, bei dem die **Linie pro Slot variieren kann** (mal S2, mal S3, mal S4, manchmal REX). Bisher ist die Linie pro Stream konstant (`LINE_58A`, `LINE_U1`).

`Departure` (in [Departure.h](src/data/Departure.h)) wird um ein **optionales** Feld `line_label[6]` erweitert. Nur der S-Bahn-Stream nutzt es; die Bus-Streams lassen es leer und der Renderer fällt auf die statische Stream-Linie zurück. Kein neuer Stream-Typ, keine Polymorphie.

## 6. API-Polling, Quoten und Wake-Verhalten

Bestehendes Schema bleibt: `POLL_INTERVAL_S=30` im Wachzustand, `WAKE_BEFORE_BUS_S=900` Vorlauf vor `t_ref`.

Neu zu beachten:

- mgate.exe ist nicht rate-limited dokumentiert; die ÖBB-Webapp pollt ihre eigene Bahnhofstafel ca. alle 30 s. 30 s sind also unauffällig.
- HTTPS-Handshake ist auf ESP32 spürbar — bestehender HTTPS-Code (für `wienerlinien.at`) funktioniert weiter, kein neues Root-Cert nötig (Let's Encrypt / DigiCert sind im Bundle).
- Die ÖBB-Antwort kann größer sein als die OGD-Antwort (~5–8 kB statt ~2 kB). `ApiFetcher` braucht einen größeren Response-Buffer; Heap-Spitze im wachen Zustand prüfen.

`t_ref` wird wie gehabt aus dem Minimum aller Stream-Abfahrtszeiten gebildet — der neue Stream nimmt schlicht teil.

## 7. Layout-Block 3 (Display)

> **Aktueller Stand seit v2.0**: Block 3 (und das gesamte Display-Layout) ist in [docs/design_handoff_display/](docs/design_handoff_display/) ausgelagert. Die Screenshots `screen-1-normal.png` … `screen-7-boot.png` sind die autoritative Referenz; das untenstehende ASCII-Mockup ist nur noch historischer Snapshot des frühen Entwurfs.

Bisheriger Südtirolerplatz-Block:

```text
SÜDTIROLER PLATZ
U1 → Leopoldau         HH:MM  HH:MM
U1 → Oberlaa           HH:MM  HH:MM
```

Neuer Block:

```text
ATZGERSDORF S-BAHN
→ Hauptbahnhof
S2  HH:MM   S3  HH:MM
```

Begründung dieser Form:

- Eine Richtung (Hbf) → Richtung steht einmal in der Überschrift, spart Tinte.
- Linie wandert vor die Uhrzeit, weil sie pro Slot variiert. Layout muss damit umgehen, dass `S2` (2 Glyphen) und `REX1` (4 Glyphen) unterschiedlich breit sind — bestehende Glyphen aus dem GxEPD2-Font sind monospace-freundlich, aber `REX` zerstört die schmale Spalte. Lösung: Spaltenbreite an die längste tatsächlich auftretende Liniennummer kalibrieren (typisch `S3` / `REX1`), und Edge-Case `Nightjet/RJX` → mit `xx` darstellen (sichtbarer Hinweis statt Layout-Bruch).
- Fällt eine Fahrt aus (`dCncl=true`), Slot wird zu `—:—`, Linie unsichtbar.

Layout-Code in [layout.cpp](src/render/layout.cpp): neue Spaltenstruktur, eigene Hilfsfunktion `drawSBahnSlot(line_label, hhmm)`. Vorhandene `drawSlotPair(hhmm, hhmm)` wird für die anderen Streams nicht angefasst.

## 8. Plan-Hints (Analogie zu v1 §12)

Der Wert eines Hint-Mechanismus für die S-Bahn ist anders gelagert als beim Bus: Stammstrecke fährt von ~5:00 bis ~0:30, das Echtzeit-70-Minuten-Fenster reicht praktisch immer für die nächste Fahrt aus, **außer in der Mittagsnacht-Lücke**. Ob das den Aufwand für einen ÖBB-seitigen Hint-Pfad lohnt, ist eine offene Designfrage:

- **Variante 1 (empfohlen): kein Hint-Pfad für S-Bahn.** In der Nacht zeigt der Stream `—:—` und im Cold-Boot direkt nach Strecken-Schließung ebenfalls. Vertretbar, weil das Vorzimmer-Gerät morgens für 58A geweckt wird (Bus fährt früher) und der S-Bahn-Block dann durch den normalen 70-Minuten-Vorlauf wieder gefüllt ist.
- **Variante 2: HAFAS mgate mit `time=05:00` ein zweites Mal abrufen** beim nächtlichen Refresh-Slot (parallel zum bestehenden EFA-Hint-Lauf für die Busse). Doppelt-API, aber konsistenter Anzeigeneindruck.

Festlegung wird in §10 als offen markiert; Default-Implementierung: Variante 1.

## 9. Edge Cases und Fehlerbilder

Übernommen oder neu:

- **AID/Client veraltet** → mgate antwortet `errTxt: "AID"` mit HTTP 200. Detektion: Response-Feld `err != "OK"` über 3 aufeinanderfolgende Calls → sichtbarer Fehlertext „ÖBB-API: Auth ungültig" im S-Bahn-Block (analog zum „58B Filter ungültig" in v1 §9). Behebung: AID in `config.h` aktualisieren, neu flashen.
- **mgate liefert HTTP-Fehler / Timeout** → bestehender `fetchWithRetry` greift; nach 3 fehlgeschlagenen Calls Stale-Mechanik wie für Wiener-Linien.
- **Direction-Filter greift nicht** (HAFAS ignoriert `dirLoc`, retourniert Gegenrichtung) → erkennbar daran, dass im Filter-Healthcheck keine Departure den Richtungsmatch besteht (eigenes Kriterium: Plan-Zeit-Abstand zu Hbf-Ankunft sinnvoll). Vorerst weicher Ansatz: kein Strenge-Filter clientseitig, weil `dirLoc` in den Test-Antworten zuverlässig wirkt. Bei Pre-Flash-Verifikation mit aufzeichnen.
- **Bauarbeiten / Schienenersatzverkehr** → ÖBB zeigt SEV-Buslinien (`SEV1` etc.) im selben Feed. Diese mit `jnyFltrL` ausschließen, sonst stehen plötzlich Buslinien im S-Bahn-Block. Pre-Flash: Verhalten einmal in einer realen SEV-Phase prüfen, falls nicht akut: spätestens beim ersten Auftreten anpassen.
- **Cold Boot in der Nachtbetriebs-Lücke** → mgate liefert Einträge erst ab Betriebsstart morgens; bis dahin `—:—`. Kein Sonderfall nötig, deckt §6 ab.

## 10. Pre-Flash-Verifikation & Open Questions

Vor dem ersten Flash auf der echten Hardware durchzuführen:

1. **AID/Client gegen die laufende ÖBB-Webapp prüfen.** DevTools öffnen auf `fahrplan.oebb.at/webapp`, einen Departure-Request abfangen, die Header `auth`/`client` mit `config.h` abgleichen. Wenn abweichend: in `config.h` korrigieren und in dieser Datei als Quelle dokumentieren.
2. **`jnyFltrL` Bitmask** für S-Bahn+Regio+REX endgültig festschreiben. Den Wert aus dem abgefangenen Webapp-Request übernehmen (die Webapp setzt diese Maske selbst beim Toggeln der Produktfilter).
3. **`dirLoc`-Verhalten** an drei verschiedenen Tageszeiten gegenchecken: Hauptverkehr / abends / Wochenende. Wenn Gegenrichtung jemals durchsickert, brauchen wir clientseitigen Filter über `jnyL[i].dirTxt` (heuristisch: Richtungen Hbf/Floridsdorf/Praterstern akzeptieren, Mödling/Wr. Neustadt verwerfen).
4. **2 oder 3 Slots?** §5.2-Entscheidung: Variante A vorgesehen; im echten Betrieb beobachten, ob ein dritter Slot Mehrwert hätte.
5. **Hint-Variante.** §8: Default 1 (kein Hint). Vor Roll-out kurz im Vorzimmer beobachten, ob das morgendliche Verhalten reicht.
6. **HTTP-Heap-Spitze** unter realer Antwort-Größe messen. Bestehende Tests in [test_device_render](test/test_device_render/test_main.cpp) um einen Heap-Check für den S-Bahn-Pfad erweitern.

## 11. Migrationsschritte (umgesetzt, siehe [docs/v2-rollout/v2-sbahn-migration-plan.md](docs/v2-rollout/v2-sbahn-migration-plan.md))

Die ursprüngliche Skizze ist in den ausführlichen Plan überführt worden;
Stand und Reihenfolge werden dort gepflegt. Die hier ehemals aufgeführten
Schritte sind über Sessions B–F vollständig in den Code geflossen
(`MAGIC`-Bump, Stream-Enum-Umstellung, `oebb_hafas_parse.{h,cpp}`, `httpPost`
in `INetwork`/`Esp32Network`, `filter_builder::buildOebbFilter`,
`render/`-Module für das neue Layout, Tests in `test_native_*` und
`test_device_*`). Layout-Block-3 ist außerdem nach `docs/design_handoff_display/`
ausgelagert — `screen-*.png` sind die autoritative Referenz, dieser §-Block
hier verweist nur darauf.

## Quellen (Web-Recherche zu v2)

- [ÖBB Stationsinformation (Web-Bahnhofstafel) — Beleg für EVA 8100634 = Wien Atzgersdorf](https://fahrplan.oebb.at/bin/stboard.exe/dn?ld=19&L=vs_scotty&input=8100634&boardType=dep&time=now&selectDate=today&maxJourneys=20&productsFilter=1111111111111111&start=yes)
- [ÖBB Open Data — GTFS / API-Galerie](https://data.oebb.at/de/api-galerie)
- [ÖBB Stationsverzeichnis (PDF) — EVA-Liste](https://www.oebb.at/de/dam/jcr:ab3012f5-0960-4e30-b2d3-24417c723eb3/stationsverzeichnis-oebb.pdf)
- [Wien Atzgersdorf Bahnhof (offizielle Seite)](https://bahnhof.oebb.at/de/wien/wien-atzgersdorf)
- [Wien Atzgersdorf railway station — Wikipedia (Liniennetz S2/S3/S4)](https://en.wikipedia.org/wiki/Wien_Atzgersdorf_railway_station)
- [public-transport/hafas-client — ÖBB-Profil (mgate.exe, AID, client)](https://github.com/public-transport/hafas-client)
- [juliuste/oebb-hafas — ÖBB HAFAS JavaScript-Client](https://github.com/juliuste/oebb-hafas)
- [Dave2ooo/oebb-monitor — Home-Assistant-Anzeige via Scotty](https://github.com/Dave2ooo/oebb-monitor)
- [List of HAFAS API Endpoints — derhuerst Gist](https://gist.github.com/derhuerst/2b7ed83bfa5f115125a5)
- [VAO Start — REST-API, Vertrag und 100-Calls-Limit](https://www.verkehrsauskunft.at/produkte)
- [VOR — Verkehrsverbund Ost-Region (Region und EFA)](https://www.vor.at/)
- [Schnellbahn Wien — Fahrplan & Streckennetz (S2/S3/S4 über Atzgersdorf)](https://www.schnellbahn-wien.at/netz/)
