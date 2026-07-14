# bustaferl — Konzept

Das Bustaferl ist eine e-Paper-Anzeige fürs Vorzimmer. Es beantwortet die Frage: „muss ich aufbrechen, mich beeilen, warten oder zu Fuß gehen?" durch reine Echtzeit-Abfahrtsdaten — die Bewertung („genug Zeit?") macht der Nutzer selbst anhand bekannter Gehzeiten.

Es zeigt die nächsten Abfahrten von vier Streams:

- **58A Tullnertalgasse**, Richtung Atzgersdorf
- **58A Tullnertalgasse**, Richtung Hietzing
- **58B Endemanngasse**, Richtung Atzgersdorf (nur die Schleifen-Durchfahrt)
- **S-Bahn Bhf. Atzgersdorf → Wien Hauptbahnhof** (S2/S3/S4, gelegentlich REX)

Die drei Bus-Streams kommen über die Wiener-Linien-OGD-Schnittstelle, der S-Bahn-Stream über ÖBB Scotty (HAFAS `mgate.exe`).

## Inhaltsverzeichnis

- [1. Hardware](#1-hardware)
- [2. Datenquellen und Filter](#2-datenquellen-und-filter)
- [3. Display-Inhalt](#3-display-inhalt)
- [4. Aktualität (Stale-Verhalten)](#4-aktualität-stale-verhalten)
- [5. Refresh-Strategie](#5-refresh-strategie)
- [6. Deep-Sleep-Logik](#6-deep-sleep-logik)
- [7. NTP-Sync](#7-ntp-sync)
- [8. Cold Boot (Stromausfall, erstes Flashen)](#8-cold-boot-stromausfall-erstes-flashen)
- [9. Edge Cases](#9-edge-cases)
- [10. Plan-Erstabfahrten am Abend („Bus-in-der-Früh"-Anzeige)](#10-plan-erstabfahrten-am-abend-bus-in-der-früh-anzeige)
- [11. ÖBB-S-Bahn: Datenquelle und Request-Schema](#11-öbb-s-bahn-datenquelle-und-request-schema)
- [12. Konfiguration](#12-konfiguration)
- [13. Beim ersten Flashen auszufüllen](#13-beim-ersten-flashen-auszufüllen)
- [Schwellwert-Defaults im Überblick](#schwellwert-defaults-im-überblick)

---

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

## 2. Datenquellen und Filter

### Bus-Streams (Wiener Linien OGD)

Wiener-Linien-Echtzeit-API (OGD): `https://www.wienerlinien.at/ogd_realtime/monitor?rbl=…`. Kein API-Key, kostenfrei.

Drei Datenflüsse:

- **Tullnertalgasse**, Linie 58A, Richtung Atzgersdorf
- **Tullnertalgasse**, Linie 58A, Richtung Hietzing
- **Endemanngasse**, Linie 58B, gefiltert über `towards = "Atzgersdorf"` (das ist genau der Steig-Durchgang nach der Schleife)

### S-Bahn-Stream (ÖBB Scotty / HAFAS)

**ÖBB Scotty (HAFAS `mgate.exe`)** für die S-Bahn Bhf. Atzgersdorf → Wien Hbf. Ein POST pro Refresh liefert die nächsten Abfahrten mit Echtzeit, Richtungs- und Produktfilter. Details zu Request-Schema und Stations-IDs in §11.

Pro Abfahrt wird der Echtzeit-Wert verwendet. Liefert die API nur den Plan-Wert, wird dieser still als Fallback genommen — keine Unterscheidung sichtbar.

## 3. Display-Inhalt

Vier nach Linie und Richtung gruppierte Blöcke:

```text
TULLNERTALGASSE
58A → Atzgersdorf      HH:MM  HH:MM
58A → Hietzing         HH:MM  HH:MM

ENDEMANNGASSE
58B → Atzgersdorf      HH:MM  HH:MM
   (nach Schleife)

ATZGERSDORF S-BAHN
→ Hauptbahnhof
S2  HH:MM   S3  HH:MM
```

Pro Bus-Richtung die nächsten zwei Abfahrten als absolute Uhrzeit; der S-Bahn-Block zeigt die nächsten Abfahrten mit vorangestellter Linienkennung, weil die Linie pro Slot variiert (mal S2, mal S3/S4, manchmal REX). Keine Countdowns, keine aktuelle Uhrzeit, keine Aufbruchszeit, keine Niederflur-Info, keine Empfehlungslogik.

Leerwert pro Slot (API liefert nichts): `—:—`.

> **Autoritatives Layout**: Das gesamte Display-Layout ist in [docs/design_handoff_display/](docs/design_handoff_display/) ausgelagert. Die Screenshots `screen-1-normal.png` … `screen-7-boot.png` sind die verbindliche Referenz; die ASCII-Mockups hier sind nur zur Orientierung.

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

Das zuletzt gerenderte Bild muss Deep Sleep überleben, sonst gibt es nichts zum Diffen. Speicherort: **RTC Slow Memory** (`RTC_DATA_ATTR`), 8 kB nutzbar. 400 × 300 px / 8 = 15 000 Byte — passt nicht roh hinein. Lösung: Row-Delta-RLE-Kompression; bei einem typischen, weit überwiegend weißen Bustaferl-Layout reduziert das auf ~1–3 kB. Bei Kompressionsüberlauf (Notfall) wird der nächste Render als Light Full erzwungen und der Buffer verworfen.

### Stufen-Modell gegen Ghosting

- **Partial Refresh**: bei jeder Änderung, kein Blinken, ~400–600 ms
- **Light Full Refresh** (1× S/W-Flash + Bild): primär zeitgesteuert alle ~1 h. Partial-Zähler ist Sicherheitsnetz (Hard-Cap: 15), auch als reguläres Ghost-Clearing.
- **Deep Clean** (3× S/W-Flash + Bild): einmal pro Nacht, eingebettet als Vor-Aktion des längsten Schlaf-Zeitraums

## 6. Deep-Sleep-Logik

Nach jedem Render Berechnung des nächsten Wake-Zeitpunkts:

```text
t_ref   = min(alle angezeigten Abfahrtszeiten über alle Streams)
wake_at = t_ref − 15 min − 30 s Boot-Margin
delta   = wake_at − now()
```

`t_ref` ist die *gemeinsame* früheste Abfahrt aller Streams — egal welche Linie, welche Richtung. Sobald eine davon naht, soll die Anzeige aktuell sein. Annahme: angezeigte Abfahrtszeiten sind nicht zu früh. Die 15 Minuten sind Sicherheitsfenster zum Aufbruch; die 30 Sekunden decken Boot + WiFi + API + Render ab.

### Fallunterscheidung nach `delta`

| `delta` | Verhalten |
|---|---|
| ≥ 2 min | **Deep Sleep** bis `wake_at` |
| 0 – 2 min | **Nicht schlafen**, Light Sleep zwischen Polls (30 s, siehe §4) |
| < 0 (Wake-Punkt liegt in der Vergangenheit) | **Aktiver Wachzustand**: 15-min-Vorlauf bereits angebrochen oder überschritten. Kein Sleep, regulärer 30-s-Poll bis `t_ref` erreicht ist. |

Damit gibt es keine Sleep-Loop mit 0-Sekunden-Schlaf: sobald `delta < 2 min`, bleibt das Gerät durchgängig wach.

Ein Cycle, der die API **nicht** erreichen konnte (`api_ok == false`), plant nie einen langen Schlaf — er retryt kurz (`API_FAILURE_RETRY_S`, 60 s), auch wenn Plan-Hints eine ferne Abfahrt kennen. Sonst würde ein einzelner fehlgeschlagener Fetch das Gerät auf das Hint-`t_ref` (z. B. „letzter Bus 23:58") stundenlang schlafen legen.

### Weitere Sonderfälle

- **API liefert keine relevanten Abfahrten** (Wochenende, Betriebspause, alle Linien out of service) → 30 min schlafen, dann erneut versuchen
- **Längster Sleep der Nacht** (z. B. nach letztem Bus bis ~5:00) → Deep Clean davor, NTP-Sync danach beim Wake
- **`t_ref` weniger als 30 s entfernt**: durchaus möglich beim Boot — kein Sleep, sofort weiterrendern
- **Rescue-Fetch**: Ein Cycle, der mit einem unvollständigen Snapshot rendern musste (mindestens ein API-Batch fiel weg), holt die Daten im Fenster 20–40 s nach dem Display-Update weiter nach und pusht genau einen Extra-Refresh, sobald ein vollständiger Snapshot ankommt.

## 7. NTP-Sync

Mindestens 1× alle 24 h. Bei jedem Wake prüfen: wenn `now() - last_ntp_sync > 24h` → syncen. Natürlicher Slot: gemeinsam mit dem nächtlichen Deep Clean.

ESP32-RTC driftet über Stunden um Sekunden, über Tage um Minuten — täglicher Sync reicht für Minuten-Granularität.

**Drift-Guard**: Weckt das Gerät mit einer `now()`, die deutlich (> `MAX_WAKE_OVERSHOOT_S`) über dem beim Einschlafen gespeicherten `expected_wake_at` liegt, gilt die RTC-Uhr als korrupt und ein NTP-Resync wird erzwungen, bevor gerendert wird. Das verhindert das Feld-Symptom „Plan-Hints gegen eine um Stunden versetzte Uhr gerendert".

## 8. Cold Boot (Stromausfall, erstes Flashen)

Reihenfolge nach Power-on-Reset, wenn weder Framebuffer noch RTC-Zeit gültig sind:

1. **WiFi** verbinden (WiFiMulti, Timeout 10 s)
2. **NTP-Sync** — zwingend, weil RTC bei 1970 startet und ohne korrekte Zeit kein sinnvolles `t_ref` berechnet werden kann
3. **API-Call** für alle Streams
4. **Boot-Check** (`BOOT_INFO_SHOW_S`, 15 s, per Taste überspringbar): STATUS-Screen + Start-Zeilen (RTC-Restore, Batch-Tally, WLAN&NTP-Anlauf) — Selbsttest zum Mitlesen. Deep-cleant das Panel bereits.
5. **Board-Render** des Initialbilds (Light Full, weil der Boot-Check schon deep-cleant hat)
6. Framebuffer in RTC-RAM ablegen
7. Reguläre Sleep-Logik nach §6

`BOOT_INFO_SHOW_S = 0` schaltet den Boot-Check ab; dann deep-cleant Schritt 5 selbst.

Wenn Schritt 1 oder 2 fehlschlägt (kein WLAN erreichbar): den **KEIN-EMPFANG**-Screen rendern — mit den gesuchten SSIDs, den tatsächlich gefundenen (nicht passenden) Netzen und, falls erkannt, einem Groß-/Kleinschreibungs-Hinweis. Danach 60 s schlafen und erneut versuchen. Solange das Gerät **noch nie** verbunden war (`has_any_data == false`), bleibt es dauerhaft auf dem Cold-Path (siehe `setup()`-Routing): **WLAN wird jede Minute erneut versucht, der Screen aber nur alle 5 Minuten neu gezeichnet** (`no_wifi_cycles % NO_WIFI_REPAINT_EVERY`, erste Anzeige Deep-Clean, danach Light-Full) — E-Paper-Full-Refreshes sind langsam und der Scan ändert sich kaum von Minute zu Minute. Sobald WLAN auftaucht, verbindet sich der nächste Cold-Cycle und läuft die volle Boot-Sequenz durch. Es gibt kein endgültiges „Aufgeben"; der Retry-Zähler steigt nur bis zum Cap (für die Boot-Check-„Versuch N"-Anzeige) und ändert die Kadenz nicht mehr.

Sonderfall **falsches WLAN-Passwort** (WPA-Handshake schlägt fehl, Disconnect-Reason 15 u. a.): terminal — Retry hilft nicht. Eigener **WLAN-PASSWORT**-Screen mit dem betroffenen SSID, danach langer Schlaf (`WIFI_AUTH_SLEEP_S`, 1 h) statt der 60-s-Schleife.

Erkennung „Cold Boot vs. Wake from Deep Sleep": `esp_sleep_get_wakeup_cause()`. Bei `ESP_SLEEP_WAKEUP_UNDEFINED` ist es Cold Boot.

Ein Wake aus Deep Sleep verliert den schnellen Partial-RAM des Panels; der erste Render nach einem Deep-Wake wird deshalb auf einen Full-Refresh promotet (`deep_wake` → `planRefresh(panel_ram_untrusted)`), sonst zeigen sich weiße Ränder und Garbage.

## 9. Edge Cases

- eine Richtung ohne Daten → `—:—` nur an der betroffenen Stelle, Rest unverändert
- API komplett unerreichbar im Wachzustand > 3 min → volles Striche-Bild
- während Deep Sleep: letzter Render bleibt, kein Stale-Trigger
- Pre-Sleep liefert API bereits erste Morgenbusse → bleiben über Nacht korrekt sichtbar
- **`towards`-Filter für 58B greift nicht** (Wiener Linien hat den String geändert): sichtbarer Fehlerzustand statt stilles `—:—`. Erkennbar daran, dass die API zwar Daten für den RBL liefert, aber keine einzige Departure mehr auf das `FILTER_TOWARDS`-Muster matcht — über mindestens 3 aufeinanderfolgende erfolgreiche Calls.
- **ÖBB-AID/Client veraltet** → `mgate.exe` antwortet `err != "OK"` (z. B. `"AID"`) mit HTTP 200. Über 3 aufeinanderfolgende Calls → `auth_error_seen`, sichtbarer Auth-Fehlerzustand. Behebung: AID in `config.h` aktualisieren, neu flashen.
- **Schienenersatzverkehr** → ÖBB zeigt SEV-Buslinien (`SEV1` etc.) im selben Feed; der Produktfilter `jnyFltrL` schließt sie aus, sonst stünden Busse im S-Bahn-Block.
- RTC-Framebuffer-RLE überschreitet das RTC-Slot-Budget → nächster Render erzwingt Light Full, Buffer wird verworfen und neu aufgebaut

## 10. Plan-Erstabfahrten am Abend („Bus-in-der-Früh"-Anzeige)

Die OGD-Realtime-API liefert nur ein Fenster von ~70 Minuten vor jeder Abfahrt. Damit am Abend „wann fährt der Bus in der Früh?" sichtbar wird, müssen wir die Plandaten für die Bus-Streams dazumischen.

### 10.1 Datenquelle: EFA-Departure-Monitor

`https://www.wienerlinien.at/ogd_routing/XSLT_DM_REQUEST` mit `outputFormat=JSON`, `useRealtime=0`. Liefert pro Haltestelle (DIVA-ID) eine Liste planmäßiger Abfahrten ab einer gewünschten Stichzeit.

Pro Departure-Eintrag relevant: `dateTime` (geplante Zeit), `servingLine.number` (Linie), `servingLine.direction` (Richtungsname).

Direction-Strings unterscheiden sich von der OGD-API — z. B. EFA `"Wien Atzgersdorf"` vs. OGD `"Bhf. Atzgersdorf S (üb. Atzgersdorfer Str.)"`. Eigene `EFA_TOWARDS_*`-Konstanten parallel zu den OGD-Konstanten.

### 10.2 Was gespeichert wird

Pro Stream (in RTC Slow Memory):

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

### 10.3 Refresh-Strategie

**Cold Boot** (§8): nach dem Realtime-Fetch, vor Deep Clean, ein EFA-Fetch pro Haltestelle. Best Effort — schlägt es fehl, bleibt `schedule_fetched_at = 0` und der Renderer fällt auf reines Realtime-Verhalten zurück.

**Warm Cycle**: getriggert, nicht zeitgesteuert. Bedingung: `now() > min(hint[*].last_today)` *und* `schedule_fetched_at < heute_00:00`. Das fällt natürlich mit dem nächtlichen Deep-Clean-Slot zusammen (beides „WiFi ohnehin an, lange Wachphase"). Sicherheits-Fallback: wenn `now() - schedule_fetched_at > 48 h` → erzwungener Refresh.

**Call-Schema** pro Haltestelle: ein einziger Call mit `itdTime=22:00` (heute) und `limit=50`. Die Response deckt typischerweise die letzten Abfahrten heute + die ersten morgen ab. Daraus client-seitig je Stream (Line+Direction-Filter):

- `last_today` = letzter Eintrag mit `dateTime` < morgen 03:00
- `next_today[0..1]` = die *letzten zwei* Einträge mit `dateTime` < morgen 03:00 (chronologisch); der Slot-Merger filtert die in der Vergangenheit liegenden via `t < now`, übrig bleiben die noch ausstehenden Abend-Abfahrten
- `first_tomorrow[0..1]` = erste zwei Einträge mit `dateTime` ≥ morgen 03:00

### 10.4 Verwendung im Display

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

### 10.5 S-Bahn und Plan-Hints

Der Wert eines Hint-Mechanismus für die S-Bahn ist geringer als beim Bus: die Stammstrecke fährt von ~5:00 bis ~0:30, das Echtzeit-70-Minuten-Fenster reicht praktisch immer für die nächste Fahrt aus — außer in der Nachtlücke. Default-Verhalten: **kein Hint-Pfad für die S-Bahn**. In der Nacht zeigt der Stream `—:—`; das Vorzimmer-Gerät wird morgens ohnehin für 58A geweckt (Bus fährt früher) und der S-Bahn-Block ist dann durch den normalen 70-Minuten-Vorlauf wieder gefüllt.

### 10.6 Edge Cases

- **EFA unreachable** während Cold Boot oder geplantem Refresh → alte `hint`-Werte bleiben gültig (bis Alters-Cap); kein Display-Effekt. Retry beim nächsten regulären Anlass.
- **EFA-`direction`-Filter greift nicht** (analog zum FilterHealth für OGD): `hint`-Werte für betroffenen Stream bleiben 0; Renderer verhält sich für diesen Slot wie sonst (`—:—` außerhalb des Realtime-Fensters).
- **Wochenende/Feiertag**: EFA berücksichtigt Kalendervarianten automatisch — wir bekommen die richtigen Werte für den jeweils nächsten Verkehrstag.
- **DST-Übergang**: `dateTime` aus EFA wird zu `time_t` (UTC) normalisiert; intern alles `time_t`, keine HH:MM-Strings.
- **Schema-Änderung in RTC-Layout**: `MAGIC` in `Esp32PersistentStore` bumpen, damit alte Strukturen nach Update nicht falsch interpretiert werden.

## 11. ÖBB-S-Bahn: Datenquelle und Request-Schema

Die Wiener-Linien-OGD-Schnittstelle (RBL-Monitor) deckt **keine ÖBB-S-Bahn ab** — sie liefert nur Tram/Bus/U-Bahn der Wiener Linien. Auch der EFA-Endpunkt (`XSLT_DM_REQUEST` auf `wienerlinien.at`) zeigt am Bahnhof Atzgersdorf empirisch nur Buslinien in der Umgebung, keine Züge. ÖBB-seitige Quellen sind also Pflicht.

### 11.1 Quelle: ÖBB Scotty (HAFAS mgate.exe)

Ein einziger Request liefert alle Abfahrten ab einer Station mit Echtzeit, Direction-Filter und Produkt-Filter (S-Bahn vs. RJ vs. Nightjet). JSON ist ESP32-tauglich (~5–8 kB pro Antwort, ArduinoJson kommt damit zurecht). Wird von der offiziellen ÖBB-Webapp verwendet, also gut über DevTools beobachtbar.

Risikoabwehr über denselben `FilterHealth`/Auth-Tripwire-Mechanismus wie für die Wiener-Linien-`towards`-Strings: ändert ÖBB Antwortstruktur oder AID, fällt das nach 3 erfolglosen Calls auf einen sichtbaren Fehlerzustand.

### 11.2 Stationen-IDs

ÖBB verwendet zwei verschiedene ID-Räume. Für `mgate.exe` werden die **internen HAFAS-Location-IDs** benötigt (nicht die 8-stelligen EVA-Nummern):

| Station | Location-ID (mgate) | `config.h` |
|---|---|---|
| Wien Atzgersdorf | `1292301` | `OEBB_EXTID_ATZG` |
| Wien Hauptbahnhof | `1290401` | `OEBB_EXTID_WIENHBF` |

Beide sind Pre-Flash-Konstanten; es gibt keine sinnvolle Laufzeit-Suche. Reproduktionsweg, falls IDs neu beschafft werden müssen: gegen `mgate.exe` einen `LocMatch`-Request mit `input.loc.name` = `Wien Atzgersdorf*` schicken; der Response enthält `match.locL[].extId`.

### 11.3 Request-Body

POST `https://fahrplan.oebb.at/bin/mgate.exe` (konkrete Auth-/Client-Werte vor Flash gegen DevTools-Mitschnitt der Webapp gegenprüfen, da HAFAS-Profile rotieren):

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

- `stbLoc.extId` → Location-ID Atzgersdorf (`1292301`)
- `dirLoc.extId` → Location-ID Wien Hbf (`1290401`). Filtert HAFAS-seitig auf Züge Richtung Hbf und schließt die Gegenrichtung (Mödling/Liesing) aus.
- `jnyFltrL` → bitmask `value="63"` = S-Bahn + Regio + REX (Bits 0–5).
- `maxJny: 6` → Puffer, im Client die ersten ≤3 Richtung Hbf nehmen.

Antwort pro Eintrag mindestens:

- `jnyL[i].stbStop.dTimeS` — Plan-Abfahrt `HHMMSS`
- `jnyL[i].stbStop.dTimeR` — Echtzeit-Abfahrt (optional, fehlt im Stör-/Ausfall-Fall)
- `jnyL[i].stbStop.dCncl` — Bool, Fahrt entfällt
- `jnyL[i].prodL[0]` → Index in `common.prodL` → Linienname (`S2`, `S3`, `S4`, `REX1` …)

Datum/Zeit-Konvertierung wie der EFA-Parser in [efa_parse.cpp](src/data/efa_parse.cpp): YYYYMMDD + HHMMSS in `time_t` mit `TZ_INFO`.

### 11.4 Datenmodell

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

Der S-Bahn-Stream ist der einzige, bei dem die **Linie pro Slot variieren kann**. `Departure` (in [Departure.h](src/data/Departure.h)) trägt dafür ein optionales Feld `line_label`. Nur der S-Bahn-Stream nutzt es; die Bus-Streams lassen es leer und der Renderer fällt auf die statische Stream-Linie zurück.

### 11.5 Polling und Heap

Bestehendes Schema bleibt: `POLL_INTERVAL_S=30` im Wachzustand, `WAKE_BEFORE_BUS_S=900` Vorlauf vor `t_ref`. `mgate.exe` ist nicht dokumentiert rate-limited; die ÖBB-Webapp pollt selbst ca. alle 30 s, 30 s sind also unauffällig. Der HTTPS-Code (für `wienerlinien.at`) funktioniert weiter, kein neues Root-Cert nötig. Die ÖBB-Antwort ist größer als die OGD-Antwort (~5–8 kB statt ~2 kB) — Heap-Spitze im wachen Zustand ist Teil des §9-Budgets in [TESTING.md](docs/TESTING.md).

## 12. Konfiguration

### Im Source-Code (committed, im Repo)

RBLs, Stations-IDs, Pins, Schwellwerte in [config.h](src/config.h): u. a. `RBL_TULL_ATZGERSDORF`, `RBL_TULL_HIETZING`, `RBL_ENDEMANN`, `FILTER_TOWARDS_58B`, `OEBB_EXTID_ATZG`, `OEBB_EXTID_WIENHBF`, `OEBB_HAFAS_AID`, sowie die e-Paper-GPIOs und die Verhaltens-Schwellwerte (siehe Tabelle unten).

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

Die WiFi-Regulierungsdomäne ist auf Österreich gepinnt (2,4 GHz Kanäle 1–13), sonst geht die Karte an Netzen auf Kanal 12/13 blind.

## 13. Beim ersten Flashen auszufüllen

- WiFi-Daten in `secrets.h` (Vorlage kopieren)
- 3 RBL-Nummern + `FILTER_TOWARDS_58B` in `config.h`
- Exakte OGD-`towards`-Strings aus realer API-Antwort verifizieren (z. B. „Hietzing" vs. „Hietzing S+U")
- DIVA-Stop-IDs für die EFA-Plan-Hints (Tullnertalgasse `60201395`, Endemanngasse `60200278`) und die EFA-Direction-Strings gegen einen echten Call verifizieren
- ÖBB: `OEBB_HAFAS_AID`/Client gegen die laufende Webapp prüfen, `jnyFltrL`-Bitmask für S-Bahn+Regio+REX bestätigen, `dirLoc`-Verhalten an mehreren Tageszeiten gegenchecken

## Schwellwert-Defaults im Überblick

| Variable | Wert | Bedeutung |
|---|---|---|
| `STALE_THRESHOLD_V2_S` | 600 | letzter Erfolg älter → `Stale`-Screen (`??:??`) |
| `OFFLINE_THRESHOLD_S` | 300 | WiFi down + Schwelle → `Offline`-Screen |
| `QUIET_HORIZON_S` | 1200 | alle Abfahrten weiter entfernt → `Quiet`-Screen |
| `NIGHT_FIRST_DEP_MIN_AHEAD_S` | 1800 | erste Abfahrt weiter (Nachtfenster) → `Night`-Screen |
| `WAKE_BEFORE_BUS_S` | 900 | Wake-Zeitpunkt vor Abfahrt |
| `BOOT_MARGIN_S` | 30 | Boot+WiFi+Render-Reserve |
| `POLL_INTERVAL_S` | 30 | API-Poll im Wachzustand |
| `ACTIVE_THRESHOLD_S` | 120 | unter dieser delta kein Deep Sleep mehr |
| `NO_DATA_SLEEP_S` | 1800 | Sleep, wenn API keine Abfahrten liefert (aber `api_ok`) |
| `API_FAILURE_RETRY_S` | 60 | Kurz-Retry, wenn API/Netz fehlschlug |
| `MAX_WAKE_OVERSHOOT_S` | 1800 | Wake über `expected_wake_at` hinaus → Drift-Guard-Resync |
| `PARTIAL_HARDCAP` | 15 | Ghost-Clearing Light Full nach N Partials |
| `LIGHT_FULL_INTERVAL_S` | 3600 | Light Full spätestens nach dieser Zeit |
| `NTP_INTERVAL_S` | 86400 | Sekunden zwischen NTP-Syncs |
| `COLD_BOOT_RETRY_S` | 60 | Retry-Intervall WiFi/NTP beim Cold Boot |
| `COLD_BOOT_MAX_RETRIES` | 5 | Cap des Cold-Boot-Versuchszählers (Boot-Check-Anzeige); danach hält der Zähler, Retry-Kadenz bleibt `COLD_BOOT_RETRY_S` |
| `NO_WIFI_REPAINT_EVERY` | 5 | KEIN-EMPFANG nur jeden N-ten No-WiFi-Cycle neu zeichnen (60 s × 5 ≈ 5 min) |
| `FILTER_HEALTH_DEAD_AFTER` | 3 | erfolglose Filter-Matches bis Fehlerzustand |
| `RESCUE_WINDOW_START_S` / `_END_S` | 20 / 40 | Rescue-Fetch-Fenster nach Display-Update |
| `RESCUE_MAX_ATTEMPTS` | 3 | max. Komplett-Fetches im Rescue-Fenster |
| `BTN_LONG_PRESS_MS` | 3000 | Halten bis Long-Press → S/W-Reset |
| `BTN_DOUBLE_CLICK_MS` | 400 | Fenster für den Doppelklick → Diagnose-Modus |
| `DIAG_MAX_S` | 600 | Sicherheits-Timeout aus dem Diagnose-Modus |
| `BOOT_INFO_SHOW_S` | 15 | Dauer des Boot-Check-Screens (0 = aus) |

Hinweis: `STALE_THRESHOLD_S` (180 s) existiert weiterhin in `config.h` als
Legacy-Wert in `CycleConfig`, steuert aber **nicht** den Stale-Screen — das
tut der State-Selector über `STALE_THRESHOLD_V2_S` (600 s).

## 14. Bedienung: BOOT-Knopf und Diagnose-Modus

Der BOOT-Taster (GPIO 0) ist das einzige Bedienelement. `button_classifier`
klassifiziert jeden Druck in **Short / Long / Double**:

- **Short** → sofortiger Update-Zyklus (weckt auch aus dem Tiefschlaf); der
  Stempel „upd HH:MM" springt immer, auch bei unveränderten Daten.
- **Long** (> `BTN_LONG_PRESS_MS`) → S/W-Reset (Deep Clean + Redraw).
- **Double** (zwei Short innerhalb `BTN_DOUBLE_CLICK_MS`) → Diagnose-Modus.

Preis der gestenfreien Doppelklick-Erkennung auf einem Taster: ein einzelner
Short-Druck löst erst nach Ablauf des `BTN_DOUBLE_CLICK_MS`-Fensters aus
(~0,4 s später) — nicht wahrnehmbar.

**Diagnose-Modus** (`runDiagMode`): Das Gerät schreibt keine seriellen Logs,
darum führt jeder Warm-Zyklus einen persistenten **CycleTrace** in zwei
RTC-Ringpuffern mit (Zyklus- und Fehler-Historie, je 16 Einträge, überstehen
den Tiefschlaf, nicht den Stromverlust). Ein Doppelklick holt einmal frische
Daten und blättert dann durch vier schlichte Text-Seiten:

1. **STATUS** — WLAN (SSID/IP/RSSI), Uhr + NTP, Stream-Selbsttest, Streaks, Heap, Uptime
2. **ZYKLEN** — jüngste Zyklen (Auslöser, Stream-OK, fehlgeschlagene Batches, Rescue/Stale, Schlaf)
3. **FEHLER** — jüngste Anomalien im Klartext
4. **DATEN-DETAILS** — Slot-Quellen (E/P/H), Fahrplan-Ladezeit, Panel-Zustand

Navigation: Kurzdruck = eine Seite weiter (mit Umlauf), Langdruck = zurück zum
Normalbetrieb, Auto-Exit nach `DIAG_MAX_S` (10 min). Beim Verlassen rendert der
nächste Warm-Zyklus wieder das gewohnte Board.

## Quellen (ÖBB-S-Bahn-Recherche)

- [ÖBB Open Data — GTFS / API-Galerie](https://data.oebb.at/de/api-galerie)
- [Wien Atzgersdorf Bahnhof (offizielle Seite)](https://bahnhof.oebb.at/de/wien/wien-atzgersdorf)
- [Wien Atzgersdorf railway station — Wikipedia (Liniennetz S2/S3/S4)](https://en.wikipedia.org/wiki/Wien_Atzgersdorf_railway_station)
- [public-transport/hafas-client — ÖBB-Profil (mgate.exe, AID, client)](https://github.com/public-transport/hafas-client)
- [juliuste/oebb-hafas — ÖBB HAFAS JavaScript-Client](https://github.com/juliuste/oebb-hafas)
- [List of HAFAS API Endpoints — derhuerst Gist](https://gist.github.com/derhuerst/2b7ed83bfa5f115125a5)
- [Schnellbahn Wien — Fahrplan & Streckennetz (S2/S3/S4 über Atzgersdorf)](https://www.schnellbahn-wien.at/netz/)
