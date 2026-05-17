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

### 2. RBL-Nummern

In `src/config.h`:

```c
#define RBL_TULL_ATZGERSDORF  0   // <- ersetzen
#define RBL_TULL_HIETZING     0   // <- ersetzen
#define RBL_ENDEMANN          0   // <- ersetzen
```

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

## Flashen

```bash
make flash    # build + upload + serial monitor
```

Beim ersten Boot durchläuft das Gerät die Cold-Boot-Sequenz:
WiFi → NTP-Sync → API → Deep Clean → erster Render. Dauert ca. 10–20 s.

## Was die Anzeigen bedeuten

| Anzeige                     | Bedeutung                                          |
|-----------------------------|----------------------------------------------------|
| `HH:MM`                     | nächste Abfahrt (Echtzeit oder Plan-Fallback)      |
| `--:--`                     | API hat für diesen Slot nichts geliefert           |
| Banner `VERALTET`           | Daten älter als 3 min — WiFi oder API tot          |
| Banner `58B Filter ungueltig` | `towards`-String passt nicht mehr, RBL prüfen    |
| Banner `Start fehlgeschlagen` | Cold Boot konnte WiFi/NTP nicht hochbekommen     |

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

### Banner „58B Filter ungueltig"

- Wiener Linien haben den Richtungstext geändert
- Aktuelle Werte abfragen: `curl https://www.wienerlinien.at/ogd_realtime/monitor?rbl=$RBL_ENDEMANN`
- Neuen Prefix in `FILTER_TOWARDS_58B` eintragen, neu flashen

### Banner „Start fehlgeschlagen"

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
