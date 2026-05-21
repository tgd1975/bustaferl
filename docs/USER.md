# Benutzer-Doku

## Erstinbetriebnahme

Drei Dinge musst du eintragen, bevor du flashst:

### 1. WiFi-Daten

```bash
make secrets       # legt src/secrets.h aus secrets.h.example an
$EDITOR src/secrets.h
```

Trage SSID und Passwort ein. Wenn du ein zweites Netz als Fallback willst,
entkommentiere die `SECONDARY`-Zeilen.

### 2. RBL-Nummern (Wiener-Linien-OGD)

In `src/config.h`:

```c
#define RBL_TULL_ATZGERSDORF  0   // <- ersetzen
#define RBL_TULL_HIETZING     0   // <- ersetzen
#define RBL_ENDEMANN          0   // <- ersetzen
```

(Diese Steig-IDs gelten nur für die Bus-Streams 58A/58B. Die S-Bahn-Spalte
wird über `OEBB_EXTID_*` adressiert, siehe Punkt 4.)

**Wie finde ich meine RBL?**

Die Wiener Linien veröffentlichen pro Steig eine eindeutige Nummer (RBL).
Zwei einfache Wege:

1. **OGD-Haltestellen-CSV:**
   <https://www.wienerlinien.at/ogd_realtime/doku/ogd/wienerlinien-csv-haltepunkte.csv>
   - In Spreadsheet-Programm öffnen
   - Nach „Tullnertalgasse" bzw. „Endemanngasse" filtern
   - Spalte `StopID` ist die RBL
   - Pro Linie und Richtung gibt es eine eigene RBL — du brauchst drei

2. **Live-Test in der Shell:**

   ```bash
   curl -s "https://www.wienerlinien.at/ogd_realtime/monitor?rbl=12345" | jq .
   ```

   Wenn `data.monitors[].locationStop.properties.title` deine Haltestelle
   ist und `lines[].name` die richtige Linie, hast du die RBL gefunden.

### 3. `towards`-Strings verifizieren

Die OGD-API liefert pro Departure ein `towards`-Feld (Richtungstext). Wir
filtern darauf. Bevor du flashst, prüfe an einem echten API-Call:

```bash
curl -s "https://www.wienerlinien.at/ogd_realtime/monitor?rbl=$DEINE_RBL" \
  | jq '.data.monitors[].lines[] | {name, towards}'
```

Trage die exakten Strings (Präfix reicht) in `src/config.h` ein:

```c
#define TOWARDS_58A_ATZ       "Atzgersdorf"
#define TOWARDS_58A_HIETZING  "Hietzing"
#define FILTER_TOWARDS_58B    "Atzgersdorf"
```

Achtung: die Wiener Linien hängen manchmal Suffixe an (z. B. `Hietzing S+U`).
Prefix-Match heißt: `"Hietzing"` matcht auch `"Hietzing S+U"`. Du brauchst
also nur den eindeutigen Anfang.

### 4. HAFAS-Werte für die S-Bahn-Spalte

Die S-Bahn-Spalte spricht direkt das ÖBB-HAFAS-`mgate.exe`-Endpoint an. In
`src/config.h`:

```c
#define OEBB_EXTID_ATZG     "1292301"    // Wien Atzgersdorf (HAFAS extId)
#define OEBB_EXTID_WIENHBF  "1290401"    // Wien Hbf (HAFAS extId)
#define OEBB_HAFAS_AID      "OWDL4fE4ixNiPBBm"
```

Die Werte sind **bewusst nicht** die 8-stelligen EVA-Nummern (die gelten
nur für die alte HTML-Schnittstelle). HAFAS-`mgate.exe` braucht die
internen Location-IDs. Wenn das Display später den State **Auth** zeigt,
hat ÖBB den `AID` rotiert — siehe Abschnitt „AID erneuern" unten.

## Flashen

```bash
make flash    # build + upload + serial monitor
```

Beim ersten Boot durchläuft das Gerät die Cold-Boot-Sequenz:
WiFi → NTP-Sync → API → Deep Clean → erster Render. Dauert ca. 10–20 s.

## Was die Anzeigen bedeuten

Pro Slot:

| Anzeige     | Bedeutung                                              |
|-------------|--------------------------------------------------------|
| `HH:MM`     | nächste Abfahrt aus Echtzeit                           |
| `□ HH:MM`   | nächste Abfahrt aus **Plan**-Daten (siehe HANDBUCH §4) |
| `--:--`     | Stream antwortet, hat aber keine Abfahrt im Horizont   |
| `??:??`     | Veraltet — letzte Echtzeit-Antwort über `STALE_THRESHOLD_S` alt |

Das Display nimmt einen von **sieben Zuständen** an. Vollständige
Erklärung mit Screenshots in [HANDBUCH §3](HANDBUCH.md#3-die-sieben-display-states).

### Symbol-Cheatsheet

Die abstrakten Glyphen sind ähnlich genug, dass eine Tabelle hilft:

| Anzeige          | Wo es auftaucht        | Was es heißt                                                                 |
|------------------|------------------------|------------------------------------------------------------------------------|
| `--:--`          | Slot innerhalb des Boards | Stream lebt, hat aber nichts zu sagen (Lücke / Wendezeit)                |
| `??:??`          | Slot innerhalb des Boards | Letzte erfolgreiche Antwort ist älter als `STALE_THRESHOLD_S`            |
| `—` (groß)       | Fullscreen `Quiet`     | Service-Zeit, aber **kein** Stream hat eine Fahrt im Horizont                |
| `!` (groß)       | Fullscreen `Offline`   | Keine Verbindung / Endpunkte schweigen länger als `OFFLINE_THRESHOLD_S`      |
| `§9`             | Fullscreen `Auth`      | HAFAS- oder OGD-Auth-Drift: Firmware-Update nötig                            |
| `◌`              | Fullscreen `Boot`      | Cold-Boot-Splash, verschwindet beim ersten erfolgreichen Render              |
| `□` (vor `HH:MM`) | Slot innerhalb des Boards | Plan-Daten statt Echtzeit (nur S2/S3/S4-Wechsel-Spalte zeigt die Linie zusätzlich) |

### AID erneuern

Wenn das Display den State **Auth** zeigt, hat ÖBB den HAFAS-`AID`
gewechselt. Erneuern so:

1. ÖBB-Webapp im Browser öffnen, DevTools → Network mitlaufen lassen.
2. Beliebige Station suchen → in einem Request gegen `mgate.exe` den
   `auth.aid`-Wert ablesen.
3. In `src/config.h` `OEBB_HAFAS_AID` ersetzen, neu flashen.

Das passiert empirisch ein- bis zweimal pro Jahr.

## Stromversorgung

Das Gerät arbeitet hauptsächlich im Deep Sleep (< 50 µA Stromaufnahme).
Empfohlen:

- **USB-Netzteil:** Dauerbetrieb, problemlos
- **Akku (18650 + LDO):** mehrere Wochen pro Ladung realistisch, abhängig
  von Render-Häufigkeit und WiFi-Verbindungszeit

## Troubleshooting

### Display bleibt leer

- Verkabelung prüfen, insbesondere BUSY und RST
- Modul-Revision: UC8176 vs. SSD1683 — bei letzterem die GxEPD2-Treiber-
  Klasse anpassen (siehe `docs/HARDWARE.md`)
- BS-Jumper auf der Rückseite des e-Paper-Moduls steht auf 0?

### Display zeigt nur Striche `--:--`

- API erreichbar? `curl` mit deinen RBLs vom selben WiFi probieren
- Serial-Monitor öffnen (`make monitor`) und nach `[warm]`-Logs schauen
- `towards`-Strings könnten nicht passen — siehe Punkt unten

### State „Offline" oder verdächtig viele `??:??`

- Wiener Linien haben evtl. den `towards`-Richtungstext geändert
- Aktuelle Werte abfragen: `curl https://www.wienerlinien.at/ogd_realtime/monitor?rbl=$RBL_ENDEMANN`
- Neuen Prefix in `FILTER_TOWARDS_58B` (bzw. `TOWARDS_58A_*`) eintragen, neu flashen

### State „Auth"

- HAFAS-`AID` rotiert — siehe Abschnitt „AID erneuern" oben
- Tritt typisch ein- bis zweimal pro Jahr auf, kein Hardware-Defekt

### Cold Boot bleibt hängen (keine Anzeige nach mehreren Minuten)

- Cold Boot hat 5× hintereinander WiFi oder NTP nicht hochbekommen
- WiFi-Passwort in `src/secrets.h` korrekt?
- Reichweite zum Router OK?
- Nach 5 min versucht das Gerät automatisch neu

### Display friert ein / Ghosting wird sichtbar

- Light Full Refresh alle 2 h sollte das auffangen
- Falls nicht: einmal kurz vom Strom trennen → Cold Boot mit Deep Clean
- Nach 24 h sollte der nächtliche Deep Clean alle Reste entfernen

### Geräte-Uhr läuft falsch

- NTP-Sync alle 24 h, gemeinsam mit dem nächtlichen Deep Clean
- Falls dauerhaft falsch: Zeitzone in `src/config.h` (`TZ_INFO`) prüfen
