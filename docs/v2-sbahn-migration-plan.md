# v2 — S-Bahn Atzgersdorf · Migrationsplan

Stand: 2026-05-19 · Vor-Umsetzungs-Stand. Setzt das in [CONCEPT.md §v2](../CONCEPT.md#v2--s-bahn-atzgersdorf) beschriebene Zielbild um **und** das Display-Redesign aus [docs/design_handoff_display/README.md](design_handoff_display/README.md).

Vorlage und Stil: [docs/main-refactor-plan.md](main-refactor-plan.md). Dieses Dokument plant nur die v2-Migration; Schichten-Refactor von `main.cpp` und Engine-Extraktion sind im main-refactor abgeschlossen und bleiben unberührt.

v2 umfasst zwei orthogonale Änderungs-Schichten:

- **Daten-Topologie** (CONCEPT §v2): 5 Streams → 4, U1 raus, S-Bahn rein, HAFAS-Parser, Layout-Block 3 thematisch neu.
- **Display-Redesign** ([docs/design_handoff_display/](design_handoff_display/)): kompletter Renderer-Rewrite — Bitmap-Fonts (VT323 + Silkscreen), Line-Badges, Plan-Marker `□`, Netzplan-Komponente, 7 Display-States (Boot/Normal/Veraltet/Nachtbetrieb/Keine Abfahrten/Kein Empfang/Auth-Fehler) als Fullscreen-Selektor statt heutigem Overlay-Modell.

Beide Schichten werden in **einem** Plan abgehandelt, weil sie sich Touch-Sites teilen (`render/layout.cpp`, `data/Departure.h`, `data/stream_labels.h`, `config.h`) und ein Roll-out in zwei Schritten den Aufwand verdoppeln würde.

## Inhaltsverzeichnis

- [1. Status quo](#1-status-quo)
- [2. Zielbild](#2-zielbild)
- [3. Pros / Cons / Out-of-Scope](#3-pros--cons--out-of-scope)
- [4. Schritt-für-Schritt-Plan](#4-schritt-für-schritt-plan)
  - [Pre-Phase — PoC isoliert](#pre-phase--poc-öbb-fetch-isoliert-host-skript)
  - [Schritt 0–11 — Umsetzung](#schritt-0--pre-flash-verifikation-der-hafas-parameter--render-stack-entscheidung)
  - [Schritt 12 — Release](#schritt-12--release-v200)
  - [4.3 Session-Gruppierung A-G](#43-session-gruppierung-a-g)
- [5. Risiken](#5-risiken)
- [6. Tests](#6-tests)
- [Anhang A — HAFAS-Request-/Response-Vertrag](#anhang-a--hafas-request-response-vertrag)
- [Anhang B — RTC-Bilanz nach v2](#anhang-b--rtc-bilanz-nach-v2)
- [Anhang C — Open Questions vor Schritt 0](#anhang-c--open-questions-vor-schritt-0)

---

## 1. Status quo

### 1.1 Was heute steht

| Komponente | Stand | Quelle |
|---|---|---|
| Stream-Topologie | **5 Streams** (`STREAM_58A_ATZ`, `…_HIETZING`, `STREAM_58B_ATZ`, `STREAM_U1_LEOPOLDAU`, `STREAM_U1_OBERLAA`) | [src/data/StreamSnapshot.h:11-18](../src/data/StreamSnapshot.h#L11-L18) |
| Realtime-Backend | Wiener-Linien-OGD (`monitor?stopId=…`), **nur GET** | [src/data/wienerlinien_parse.cpp](../src/data/wienerlinien_parse.cpp), [src/hal/Esp32Network.h:17](../src/hal/Esp32Network.h#L17) |
| Hint-Backend | Wiener-Linien-EFA (`XSLT_DM_REQUEST`), per DIVA-Stop | [src/logic/schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp), [src/data/efa_parse.h](../src/data/efa_parse.h) |
| Filter-Quelle | Single source of truth: `buildStreamFilters` + `buildScheduleFilters` | [src/logic/filter_builder.cpp](../src/logic/filter_builder.cpp) |
| Stream-Labels | Einzige Stelle: `streamLabel()` | [src/data/stream_labels.h](../src/data/stream_labels.h) |
| Layout (gesamt) | drei `drawHeader` + fünf `drawStreamLine`, Adafruit-GFX-Default-Font × size 2, Overlay-Banner unten am Stale-/Filter-/Boot-Fehler | [src/render/layout.cpp](../src/render/layout.cpp) |
| Display-Polarität | invertiert: schwarzer Hintergrund, weiße Schrift (`fb.clear(false)` + ink=1) | [src/render/layout.cpp:113](../src/render/layout.cpp#L113) |
| Font-Stack | Adafruit GFX integrierter 5×7-Font, `setTextSize(2)` → 10×14 px | [src/render/layout.cpp:49-55](../src/render/layout.cpp#L49-L55) |
| State-Modell | `OverlayKind ∈ {None, Stale, FilterDead, StartFailed}` als *zusätzlicher* Overlay über Snapshot-Render | [src/render/layout.h:15-25](../src/render/layout.h#L15-L25) |
| Departure-Struktur | `when`, `valid`, `source ∈ {Unknown, Realtime, Plan, Hint}` — Linie/Richtung implizit über Stream-Index, Plan/Realtime visuell nicht differenziert | [src/data/Departure.h](../src/data/Departure.h) |
| RTC-Bilanz | ~7412 / 8192 Byte belegt, ~780 B Reserve nach Schritt 2.3 des main-refactors | [docs/main-refactor-plan.md Anhang A](main-refactor-plan.md) |
| Test-Buckets | 23 `test_native_*`, 5 `test_device_*`, 9 `test_longterm_*` | [test/](../test/) |

### 1.2 Was sich für v2 ändert

**Daten-Schicht** (laut [CONCEPT.md §v2-1](../CONCEPT.md#1-motivation-und-geltungsbereich)):

- Streams 3 + 4 (beide U1) entfallen vollständig. Ein neuer Stream `STREAM_SBAHN_HBF` ersetzt sie → `STREAM_COUNT = 4`.
- Neue Datenquelle **ÖBB Scotty mgate.exe (HAFAS, POST/JSON)** für den S-Bahn-Stream; Bus-Streams bleiben auf OGD.
- `Departure` braucht ein optionales `line_label`, weil im S-Bahn-Stream pro Slot die Linie variiert (S2/S3/S4/REX).

**Display-Schicht** (laut [docs/design_handoff_display/README.md](design_handoff_display/README.md)):

- **Renderer-Rewrite**, kein inkrementeller Patch. Neuer Font-Stack (Bitmap-Fonts VT323 + Silkscreen), neue geometrische Primitive (Line-Badges als invertierte Rechtecke, Plan-Marker `□`, Netzplan unten).
- **State-Modell wird zu Fullscreen-Selektor.** Heute Overlay über Snapshot, morgen exklusive Auswahl aus 7 States: Boot, Normal, Veraltet, Nachtbetrieb, Keine Abfahrten, Kein Empfang, Auth-Fehler. Drei davon sind komplett-fullscreen (Boot/Offline/Auth) mit Glyph + Titel + Sub + Foot; die anderen vier nutzen das Board-Layout mit unterschiedlichen Daten.
- **Pre-Render-Splash bei leerem Snapshot.** Wenn `cycle_runner` mit leerem Meta startet (Power-On, Firmware-Update mit MAGIC-Bump, oder persistierter Auth-Fehler), zeichnet er **vor** dem WiFi-Connect einen Splash- oder Auth-Screen — User sieht sofort, dass das Gerät arbeitet, statt 5 s vermeintlich-toten Display. Erst danach: Fetch + Content-Render. Zwei E-Paper-Refreshes in diesem Pfad; normale Deep-Sleep-Wakes (Snapshot in RTC vorhanden, kein Auth-Fehler) bleiben bei einem Refresh. Siehe Schritt 5.4.
- **Direction-Texte** statisch pro Stream (`"Atzgers."`, `"Hietzing"`) — abgekürzt, max 8 Zeichen, kommen *nicht* mehr aus `towards` (zu lang).
- **Plan/Live-Differenzierung sichtbar.** Heute wird `DepartureSource` nur intern verfolgt; neu erscheint ein hohles 5×5-px-Quadrat hinter jeder Plan-Zeit. Live-Zeiten bleiben unmarkiert.
- **Netzplan** als geometrische Karte unten: Hbf · Atzg · Ende · Tull · Hietz mit „you-are-here"-Marker und L-förmiger Linie über den Transferknoten Atzg.
- **Stale-Banner entfällt.** `--:--` in allen Slots *ist* das Signal (Design-Entscheidung [docs/design_handoff_display/README.md §2 Veraltet](design_handoff_display/README.md)).

### 1.3 Was unverändert bleibt (harte Constraints)

- **Sleep-Planner, Refresh-Planner, Stale-Policy, Cold-Boot-Sequencer** kennen den S-Bahn-Stream nur über `t_ref = min(stream[*].slot[*].when)`. Kein Eingriff.
- **HAL-Interfaces** `IClock`, `IDisplay`, `IPersistentStore`, `ISleep`, `IRenderer`, `IButton` bleiben strukturell. Einzige Erweiterung: `INetwork::httpPost` (Schritt 1).
- **Schichten-Regel** aus [docs/ARCHITECTURE.md](ARCHITECTURE.md). HAFAS-Parser landet in `data/`, kein HAL-Touch außer Esp32Network.
- **`main.cpp`** (~80 LOC nach main-refactor Schritt 8) wird nicht angefasst — `cycle_runner` iteriert über `STREAM_COUNT` und ist von der Reduktion 5→4 selbst nicht betroffen.
- **Display-Hardware** (400×300, UC8176, B/W, GxEPD2). Polarität bleibt invertiert wie heute — das Design-Template baut darauf auf. Frame-Diff + RLE-Persistierung bleiben Frame-format-agnostisch.

### 1.4 Touch-Site-Inventar (heute → v2)

| Wo | Heute | v2 |
|---|---|---|
| [src/data/StreamSnapshot.h:11-18](../src/data/StreamSnapshot.h#L11-L18) | `STREAM_U1_LEOPOLDAU=3`, `STREAM_U1_OBERLAA=4`, `STREAM_COUNT=5` | `STREAM_SBAHN_HBF=3`, `STREAM_COUNT=4` |
| [src/data/Departure.h](../src/data/Departure.h) | `{when, source, valid}` | + `char line_label[6] = ""` (optional, S-Bahn füllt) |
| [src/data/stream_labels.h](../src/data/stream_labels.h) | switch über 5 Indices | switch über 4 Indices, neuer Eintrag `"SBahn-Hbf"` |
| [src/config.h:13-14](../src/config.h#L13-L14) | `RBL_SUEDTIROLER_LEOPOLDAU`, `RBL_SUEDTIROLER_OBERLAA` | entfernen |
| [src/config.h:39-40](../src/config.h#L39-L40) | `TOWARDS_U1_LEOPOLDAU`, `TOWARDS_U1_OBERLAA` | entfernen |
| [src/config.h:47](../src/config.h#L47) | `DIVA_SUEDTIROLER_PLATZ` | entfernen |
| [src/config.h:55-56](../src/config.h#L55-L56) | `EFA_TOWARDS_U1_LEOPOLDAU`, `EFA_TOWARDS_U1_OBERLAA` | entfernen |
| [src/config.h](../src/config.h) (neu) | — | `OEBB_EXTID_ATZG`, `OEBB_EXTID_WIENHBF`, `OEBB_STBLOC_EXTID`, `OEBB_DIRLOC_EXTID`, `OEBB_MGATE_URL`, `OEBB_HAFAS_AID`, `OEBB_HAFAS_CLIENT_JSON`, `OEBB_HAFAS_VER`, `OEBB_JNYFLTR_PRODUCTS`, `OEBB_MAX_JNY` |
| [src/logic/filter_builder.cpp:11-13](../src/logic/filter_builder.cpp#L11-L13) | zwei U1-Zeilen | eine S-Bahn-Zeile (neuer Filter-Typ — siehe Schritt 4) |
| [src/logic/filter_builder.cpp:21-24](../src/logic/filter_builder.cpp#L21-L24) | zwei U1-Schedule-Zeilen | weggelassen (S-Bahn-Stream hat keinen Schedule-Filter) |
| [src/logic/snapshot_fetcher.cpp:16-19](../src/logic/snapshot_fetcher.cpp#L16-L19) | `FETCH_ORDER` über 5 Streams | über 4 Streams; S-Bahn-Stream getrennt gefetcht (anderer Endpoint, POST) |
| [src/logic/snapshot_fetcher.cpp](../src/logic/snapshot_fetcher.cpp) | nur OGD-Batch-Loop | + ÖBB-Single-Call (Sub-Funktion `fetchOebbStream`) |
| [src/render/layout.cpp](../src/render/layout.cpp) (komplett) | drei Block-Renders mit `drawStreamLine`, Adafruit-GFX-Default-Font, `OverlayKind`-Overlay | **kompletter Rewrite** nach [docs/design_handoff_display/](design_handoff_display/): Bitmap-Fonts, Line-Badges, Plan-Marker, Trennlinien, Section-Header neuer Stil, neuer S-Bahn-Block mit Linienkennung pro Slot, Netzplan unten |
| [src/render/](../src/render/) (neu) | — | `bitmap_fonts.{h,cpp}` (oder U8g2-Anbindung, abh. von §4 Schritt 0.6), `badge.{h,cpp}` (Line-Badge-Primitive), `network_plan.{h,cpp}` (Netzplan-Sub-Renderer), `display_state.{h,cpp}` (State-Selector + Fullscreen-Renderer für Boot/Offline/Auth/Quiet) |
| [src/render/layout.h:15-25](../src/render/layout.h#L15-L25) | `OverlayKind {None, Stale, FilterDead, StartFailed}` | ersetzt durch `DisplayState {Boot, Normal, Stale, Night, Quiet, Offline, Auth}`; `RenderInput` bekommt zusätzliche Felder für `last_fetch_at`, `auth_err_code`, `firmware_version` |
| [src/logic/render_input.cpp](../src/logic/render_input.cpp) | mapped `overlay` aus Stale-/FilterHealth-/Boot-Signalen | ersetzt durch State-Selector aus [docs/design_handoff_display/README.md „State Selection Logic"](design_handoff_display/README.md): Priority Boot > Auth > Offline > Stale > Quiet > Night > Normal |
| [src/hal/INetwork.h](../src/hal/INetwork.h) | `httpGet`, `httpGetStream` | + `httpPost(url, body, content_type, out)` (mind. nicht-streaming) |
| [src/hal/Esp32Network.{h,cpp}](../src/hal/Esp32Network.h) | nur GET | + POST-Pfad, gleicher TLS-Kontext |
| [src/hal/Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp) | `MAGIC`, `SCHED_MAGIC` | beide bumpen (STREAM_COUNT-Änderung invalidiert RLE-Frame *und* Schedule-Layout) |
| Tests: `test_native_filter_builder`, `…_filter_health`, `…_slot_merger`, `…_wienerlinien_parse`, `…_efa_parse`, `…_schedule_fetcher`, `…_snapshot_fetcher`, `…_snapshot_logger`, `…_render_input`, `…_cycle_runner_*`, `test_device_fetch`, `test_device_schedule`, `test_longterm_*` | jeweils `STREAM_COUNT=5`-implizit (Index 3/4 erwartet U1-Daten) | auf 4 Streams umstellen; ggf. Fixture-Anpassung |

Zähle ich pro Datei einmal: ~14 Quellfiles und ~16 Testfiles werden angefasst.

### 1.5 Was offen ist (vor Code-Eingriff zu klären)

**Daten-Schicht** — CONCEPT §v2-10 nennt sechs Pre-Flash-Verifikationspunkte. Drei davon **müssen** vor Schritt 1 beantwortet sein, weil sie das Datenmodell oder den Request-Body bestimmen:

1. **AID/Client-Werte aus aktueller ÖBB-Webapp** ([CONCEPT §v2-10 Punkt 1](../CONCEPT.md#10-pre-flash-verifikation--open-questions)). Festlegung 2026-05-19: **Weg A — PoC zuerst** (Pre-Phase P.1, Default-AID `OWDL4fE4ixNiPBBm` aus CONCEPT §v2-4 hartgekapselt). Liefert der PoC dreimal `err: "OK"`, ist der Wert empirisch bestätigt.

   Fallbacks für den Fall, dass der PoC mit `err: "AID"` oder HTTP 4xx scheitert:
   - **Weg B — DevTools-Mitschnitt** (`https://fahrplan.oebb.at/webapp/` mit offenem Network-Tab, Filter „Fetch/XHR", eine beliebige Abfahrtsanfrage → JSON-Body eines `mgate.exe`-POST kopieren). Werte 1:1 nach `config.h` mit Beleg-Kommentar `// confirmed against webapp on 2026-MM-DD`.
   - **Weg C — Open-Source-Wrapper** als Sekundärquelle: [public-transport/hafas-client](https://github.com/public-transport/hafas-client/blob/main/p/oebb/profile.js), [juliuste/oebb-hafas](https://github.com/juliuste/oebb-hafas). Git-Log dort gibt Anhaltspunkt, wann ÖBB zuletzt rotiert hat.

2. **`jnyFltrL`-Bitmask für S-Bahn+Regio+REX** (Punkt 2).

   *Was ist `jnyFltrL` überhaupt?* HAFAS nennt eine Zugfahrt eine *journey* (kurz `jny`); `jnyFltrL` ist die **journey filter list** — ein Array von Filter-Bedingungen, die der Server vor dem Antworten auf die Anfrage anwendet, damit nur „passende" Departures zurückkommen. Pro Filter steht im Feld `type` der Filter-Typ (`PROD` = Produkt-Klasse, `OP` = Operator, `META` = Vorgegebenes Filter-Set) und in `value` der Selektor-Wert. Bei `type=PROD` ist `value` ein **dezimal-kodiertes Bitfeld**, in dem jedes Bit für eine Produkt-Klasse steht (Bit 0 = IC, Bit 1 = RJ, Bit 2 = D-Zug, Bit 3 = Regio/REX, Bit 4 = S-Bahn, Bit 5 = Bus, weitere Bits je Profil verschieden). Beispiel: `value="48"` = `0b110000` = nur S-Bahn + Bus. Wir wollen für den Bustaferl S-Bahn + REX-Regio, also Bit 3 + Bit 4 = `0b011000 = 24`; der CONCEPT-Default `63` ist die tolerante Variante „alles außer Bus und seltene Sonderzüge" und liefert in Atzgersdorf praktisch dasselbe Ergebnis (IC/RJ/D-Züge halten dort nicht). Ohne `jnyFltrL` würden wir auch SEV-Busse und U-Bahn-ähnliche Klassen mitbekommen.

   Wie kommen wir an den richtigen Wert?
   - **Weg A — PoC-Skript.** Default `"63"` (Bits 0–5) aus CONCEPT §v2-4 hartgekapselt. Sichten der drei PoC-Antworten zeigt, welche Produkt-Klassen (`prodL[…].cls`, `name`) tatsächlich zurückkommen. Wenn ausschließlich S-Bahn-Züge (`name` matched `^S\d`) + gelegentlich REX-Züge auftauchen und kein Bus/Tram/U-Bahn, ist die Bitmask passend.
   - **Weg B — DevTools mit Toggle.** Webapp-DevTools wie in 1, dann den Produktfilter in der Webapp manuell umschalten (S-Bahn aus → S-Bahn an). Vor jedem Toggle einen Request mitschneiden. Das `jnyFltrL[0].value`-Feld unterscheidet sich zwischen den beiden Captures — der Bit-Differenz-Wert ist die S-Bahn-Bitstelle. Für die Plan-Soll-Maske (S-Bahn + Regio + REX) den Filter so setzen, dass alle drei Häkchen aktiv sind, dann den Wert ablesen.
   - **Weg C — HAFAS-Profile-Doku.** [HAFAS-Profile-Tabelle (derhuerst Gist)](https://gist.github.com/derhuerst/2b7ed83bfa5f115125a5) listet die Produkt-Bits pro Profil. Weg B ist genauer (live aus der Webapp), Weg C ist robuster gegen einzelne Bit-Verschiebungen pro Profil-Version.

3. **`dirLoc`-Verhalten** (Punkt 3).

   *Was ist `dirLoc`?* Im HAFAS-Request-Body ist `stbLoc` die **station** (StationBoard-Location — der Bahnhof, dessen Abfahrtstafel wir anfragen). `dirLoc` ist eine optionale **direction location** — ein zweiter Bahnhof, der **downstream** der Fahrt liegen muss. Setzt man `dirLoc.extId = OEBB_DIRLOC_EXTID` (= Wien Hbf), antwortet der Server nur mit Zügen, deren Laufweg ab Atzgersdorf den Wiener Hauptbahnhof als (kommenden) Halt enthält. Das filtert die Gegenrichtung (Mödling, Wr. Neustadt, Liesing) automatisch heraus, ohne dass wir clientseitig Endhaltestellen-Strings vergleichen müssen. Funktional ist `dirLoc` eine *Pfad-Bedingung* („Fahrt enthält Haltepunkt X"), nicht ein *Zielfilter* („Endstation = X") — Züge mit Endstation Floridsdorf oder Praterstern sind also erlaubt, solange sie Hbf auf dem Weg streifen.

   Die offene Frage: Reicht das in *allen* Fahrplan-Lagen, oder gibt es Edge-Cases, in denen HAFAS `dirLoc` ignoriert oder weniger streng auslegt? Bekannte Risiken:
   - **Triebwagen-Wende kurz nach Atzgersdorf**: Falls eine S-Bahn formal Richtung Hbf läuft, aber tatsächlich auf einem benachbarten Gleis als Leerfahrt wendet — Verhalten von HAFAS unklar, in der Praxis selten.
   - **Bauarbeiten/Umleitungen**: Wenn die Stammstrecke gesperrt ist und ein Zug formal noch als „Richtung Hbf" gefahren wird, aber gar nicht über Atzgersdorf fährt — vermutlich filtert HAFAS richtig, aber unbestätigt.
   - **`maxJny=6` zu klein**: Wenn `dirLoc` *nicht* serverseitig filtert (z. B. wegen API-Version-Drift), kommt eine 6-Departure-Antwort zur Hälfte Gegenrichtung zurück und unser Display zeigt 3 statt 6 Hbf-Departures. Mitigation: `maxJny=6` so gewählt, dass selbst worst-case (alle Departures Gegenrichtung) nicht alle Display-Slots leer lässt; PoC-Phase prüft empirisch.

   Wenn die Pre-Phase P.3 oder der DevTools-Sweep aus Schritt 0.3 in einer der drei Tageszeiten Gegenrichtungs-Departures aufdeckt, kommt clientseitig ein Direction-Healthcheck dazu: pro `jny` das Feld `jnyL[i].dirTxt` lesen (das ist der vom Server gerenderte Richtungsstring, z. B. `"Wien Floridsdorf"` oder `"Mödling"`). Akzept-Liste: alles was mit `"Wien Hbf"`, `"Wien Floridsdorf"`, `"Praterstern"`, `"Mistelbach"`, `"Laa/Thaya"` beginnt; alles andere verwerfen. Liste empirisch aus den PoC-Snapshots ermitteln.

   Festlegung: erst nach Pre-Phase entscheidbar, ob clientseitiger Filter überhaupt nötig wird.

**Display-Schicht** — vier Designentscheidungen, die das Rendering-Konzept festlegen und vor Schritt 7 beantwortet sein müssen (Font-Stack, Glyph-Substitution, Direction-Text-Quelle, ▼-Netzplan-Glyph — letztere als C12 in [Anhang C](#anhang-c--open-questions-vor-schritt-0)):

1. **Font-Stack** — drei Optionen, Trade-off Aufwand vs. Pixel-Treue:

   | Option | Was | Aufwand | Pixel-Treue zum Design | Flash | Risiko |
   |---|---|---|---|---|---|
   | **A — U8g2_for_Adafruit_GFX** (empfohlen) | Library liefert ~100 Bitmap-Fonts. Wir wählen pro Rolle den ähnlichsten: `u8g2_font_logisoso28_tr` (TG-Daten), `…16_tr` (EG/Atzg), `u8g2_font_5x8_tr` (Section-Header), `u8g2_font_helvR08_tr` (Network-Labels). 90-px FullscreenError-Glyphen sind **nicht** aus dem u8g2-Stack, sondern Custom-PROGMEM-Sprites — siehe C9. Bridge wird via [GxEPD2-Beispiel `GxEPD2_U8G2_Fonts_Example`](https://github.com/ZinggJM/GxEPD2_4G/blob/main/examples/GxEPD2_U8G2_Fonts_Example/GxEPD2_U8G2_Fonts_Example.ino) angedockt. | 1 Library in `platformio.ini`, ~0.5d Integration | ~80%. Glyphen sind ähnlich, nicht identisch. VT323-Charakter (terminal-glitch-Optik) fehlt — Logisoso ist sauberer und runder. | ~10–15 KB (Lib lädt Glyphen on-demand pro Größe) | gering |
   | **B — Custom Bitmap-Fonts** | TTF von Google Fonts laden (VT323, Silkscreen), via `fontconvert` (aus Adafruit-GFX-Source bauen) oder via [Online-Tool fontconvert.glcdfont](https://oleddisplay.squix.ch/) je Größe in `GFXfont`-PROGMEM-Struktur umwandeln. Eine `bitmap_fonts_data.h` pro Font+Größe. | ~1d (fontconvert bauen + 7 Größen rasterisieren + Spritesheet-Asserts) — plus Tooling-Risiko (Memory `feedback-no-tooling-rabbit-holes`) | 100% (originalgetreu) | ~30–60 KB | mittel: fontconvert ist C-Tool, baut nicht out-of-the-box; Pflege-Overhead bei jeder Font-Größen-Änderung |
   | **C — Hybrid** | U8g2 für die häufigen Größen + Custom-PROGMEM-Glyphen für die seltenen (z. B. nur die 90-px FullscreenError-Glyphen `!`, `§9`, `◌` als 1-bit-Sprites). | ~0.7d | ~85% | ~12–20 KB | gering, aber zwei Render-Pfade nebeneinander |

   **Empfehlung Option A** für die erste v2-Iteration. Wenn Side-by-Side-Vergleich in Schritt 11.8 Option A unzumutbar findet, kommt Option B als Follow-up-PR. Festlegung in [Anhang C C8](#anhang-c--open-questions-vor-schritt-0).
2. **Direction-Text-Quelle** — statisch vs. on-the-fly aus API. Trade-off im Detail:
   - **Statisch in `data/stream_labels.h::display_dir()`** (siehe [§2.2](#22-neue-module-im-detail)). Werte sind hartkodiert: `STREAM_58A_ATZ → "Atzgers."`, `STREAM_58A_HIETZING → "Hietzing"`, `STREAM_58B_ATZ → "Atzgers."`, `STREAM_SBAHN_HBF → ""` (Richtung steht im Section-Header).
     - **Pro**: deterministisch, kein Layout-Bruch durch unerwartete API-Strings, max 8 Zeichen garantiert, Tests fixieren das Display-Verhalten ohne API-Live-Fixture-Drift.
     - **Pro**: Lesbarkeit der Abkürzungen liegt beim Entwickler („Atzgers." ist gut, „Bhf.A.S" wäre schlecht — die Wahl hat keinen Algorithmus, das ist Design-Disziplin).
     - **Con**: wenn die Wiener Linien jemals eine neue Endhaltestelle ansagen (z. B. „Atzgersdorf S Bf" wegen Umbau), bleibt das Display still falsch, bis jemand `display_dir()` editiert.
   - **On-the-fly aus API `towards` mit Truncate**: `truncate(stream.slot[0].towards_text, 8)` (Feld müsste neu durch den OGD-Parser propagiert werden — heute landet `towards` nicht in `Departure`).
     - **Pro**: auto-aktualisierend bei String-Drift, kein Code-Edit nötig.
     - **Con**: Truncate-Heuristik ist brüchig — „Bhf. Atzgersdorf S (üb. Atzgersdorfer Str.)" → „Bhf. Atz" liest sich anders als die manuell gewählte Abkürzung. Plus: jede API-Antwort wird Pixel-relevant; ein Tippfehler bei Wiener Linien bricht das Layout.
     - **Con**: bricht den heutigen `Departure`-Footprint (`towards`-String-Feld → +20-30 B pro Slot → RTC-Bilanz wird eng).
     - **Con**: pro Stream gleicher Wert in `slot[0]` und `slot[1]` — Redundanz, oder ein Hoist-Aufwand im Render-Pfad.
   - **Empfehlung statisch**. Konkrete Disziplin: bei jedem v2-Refresh wird `display_dir()` einmal an einer Live-Antwort gegengeprüft (1 Min Aufwand). Wenn die Wiener Linien je den Endhaltestellen-Namen ändern, ist das ein Anlass-Commit, kein Wartungs-Problem.

3. **Glyph-Substitution für FullscreenError**: `◌` (dotted circle) und `§9` sind in keinem Bitmap-Font Standard. Zwei Optionen:
   - **ASCII-Substitut**: `o` für `◌`, `S9` für `§9`. Aufwand 0, Design-Treue gering — die Boot-/Auth-Screens wirken behelfsmäßig.
   - **Eigene 1-bit-Glyph-Assets in PROGMEM**: drei custom Sprites (90 px `◌`, 90 px `!` ist meist im Font, 90 px `§9` als Composite-Glyph). Aufwand ~0.3d, Design-Treue 100%, Pflege minimal (drei statische Pixel-Arrays in `bitmap_fonts.cpp`).
   - **Festlegung (2026-05-19): eigene Glyph-Assets.** Konsistent mit dem Anspruch des Design-Handoffs „pixel-perfect", und Aufwand ist verschwindend gering gegen den Renderer-Rewrite drum herum. Das macht den Auth-/Boot-Screen erkennbar zu *unserem* Screen, nicht zu einem Fallback-Behelf.

Die restlichen drei aus Daten-Schicht (Slot-Anzahl 2 vs. 3, Hint-Variante, Heap-Spitze) sind nach Schritt 3 entscheidbar und werden in §4.1 (Annahmen) festgehalten — siehe [Anhang C](#anhang-c--open-questions-vor-schritt-0).

---

## 2. Zielbild

### 2.1 Struktur nach v2

```text
src/
├── config.h                       U1-Konstanten weg; ÖBB- + Display-Direction-Konstanten dazu
├── data/
│   ├── Departure.h                + char line_label[6]; bool live() {return source==Realtime;}
│   ├── StreamSnapshot.h           Stream-Enum: 4 Werte, neuer STREAM_SBAHN_HBF
│   ├── stream_labels.h            4 Einträge; + display_dir(stream) statisch
│   ├── oebb_hafas_parse.{h,cpp}   NEU — Parser für mgate.exe-Antwort
│   ├── wienerlinien_parse.*       unverändert (außer STREAM_COUNT-Größe)
│   └── efa_parse.*                unverändert (außer STREAM_COUNT-Größe)
├── logic/
│   ├── filter_builder.{h,cpp}     OGD-Filter: 3 Einträge; Schedule-Filter: 3 Einträge
│   ├── snapshot_fetcher.{h,cpp}   + fetchOebbStream() für STREAM_SBAHN_HBF;
│   │                              FETCH_ORDER nur noch über die 3 OGD-Streams
│   ├── schedule_fetcher.*         unverändert (S-Bahn-Stream wird übersprungen)
│   ├── slot_merger.cpp            keine Änderung (mergt über STREAM_COUNT)
│   ├── render_input.{h,cpp}       neuer State-Selector (Boot/Auth/Offline/Stale/Quiet/Night/Normal)
│   └── cycle_runner.*             reicht zusätzliche Signale durch (last_fetch_at, auth_err_code)
├── hal/
│   ├── INetwork.h                 + httpPost()
│   └── Esp32Network.{h,cpp}       + httpPost-Impl
└── render/
    ├── layout.{h,cpp}             KOMPLETTER REWRITE nach docs/design_handoff_display/
    ├── bitmap_fonts.{h,cpp}       NEU — VT323- + Silkscreen-Bitmap-Daten
    │                              (oder Bridge auf U8g2_for_Adafruit_GFX, abh. von §0.6)
    ├── badge.{h,cpp}              NEU — Line-Badge-Primitive (sm/md/lg)
    ├── plan_marker.{h,cpp}        NEU — 5×5-px-Hohlquadrat
    ├── network_plan.{h,cpp}       NEU — Geometrischer Netzplan unten
    ├── display_state.{h,cpp}      NEU — Fullscreen-Renderer (Boot/Offline/Auth/Quiet)
    ├── frame_buffer.h             unverändert
    └── rle.{h,cpp}                unverändert
```

`main.cpp` bleibt — alle Stream-Konstanten kommen aus `config.h`, alle Stream-Iterationen aus `STREAM_COUNT`.

### 2.2 Neue Module im Detail

**`data/oebb_hafas_parse.{h,cpp}`**

```cpp
namespace bustaferl {

struct OebbStreamFilter {
  std::string stbloc_extid;  // HAFAS extId, Departure-Board station (Atzgersdorf)
  std::string dirloc_extid;  // HAFAS extId, dirLoc → must touch this station downstream (Wien Hbf)
  std::string products;      // jnyFltrL "value", e.g. "63" (= S-Bahn+Regio+REX)
  int max_jny = OEBB_MAX_JNY;
};

// Builds the mgate.exe POST body for one StationBoard request. Pure function,
// host-testable. AID/Client come from config.h.
std::string buildOebbRequest(const OebbStreamFilter &f);

struct OebbParseResult {
  bool endpoint_responded = false;  // true ⇔ valid JSON with err=="OK" and svcResL[0].res non-null
  bool filter_matched = false;      // true ⇔ ≥1 surviving departure (dirLoc reached, product accepted, not cancelled)
  bool auth_error_seen = false;     // true ⇔ err ∈ {"AID","AUTH"} (siehe State-Selector §2.2)
};

// Parses an mgate.exe StationBoard response into `slot[0..N-1]`. Each slot gets
// `source = Realtime` (when `dTimeR` present) or `Plan` (only `dTimeS`); the
// line-name (`prodL[…].name`) is copied into `line_label`. Return value is
// `true` iff the JSON parsed (regardless of API-level err); inspect the result
// struct for semantic outcome.
bool parseOebbStationBoard(const std::string &json, StreamData &out_stream,
                           OebbParseResult &result);

} // namespace bustaferl
```

Begründung gegen `OebbStreamFilter` als `struct`-Familie pro Stream (wie bei OGD): es gibt nur **einen** S-Bahn-Stream. Eine `StreamFilter`-Tabelle der Größe `STREAM_COUNT` für genau einen aktiven Eintrag wäre Boilerplate, der unbenutzte Einträge mitführt. Stattdessen: `OebbStreamFilter` als Single-Value-Filter, vom `filter_builder` direkt ausgegeben.

**`logic/snapshot_fetcher.cpp` — erweitert um Sub-Funktion `fetchOebbStream`**

```cpp
namespace bustaferl {

// Internal: one mgate.exe POST → parseOebbStationBoard → write into
// out.stream[STREAM_SBAHN_HBF]. Updates summary like the OGD batch loop
// (counts as one batch). Logs analogously via SNAP_LOG.
static bool fetchOebbStream(INetwork &net, const std::string &mgate_url,
                            const OebbStreamFilter &f, StreamSnapshot &out,
                            FetchSummary &summary);

} // namespace bustaferl
```

`fetchSnapshot` ruft beide Pfade hintereinander: erst die bestehende OGD-Batch-Schleife (über die 3 verbleibenden OGD-Streams), dann `fetchOebbStream` für `STREAM_SBAHN_HBF`. Reihenfolge bewusst OGD-zuerst, weil der mgate-Call größer und teurer ist und ein OGD-Fehler den S-Bahn-Stream nicht verbergen soll.

`api_ok` bleibt definiert als „mindestens ein Batch hat geparst" — wird durch den S-Bahn-Call automatisch toleranter, weil ein OGD-Totalausfall noch eine S-Bahn-Antwort haben kann (und umgekehrt). Das ist erwünschtes Verhalten.

**`logic/filter_builder.{h,cpp}` — erweitert um S-Bahn-Filter-Getter**

```cpp
namespace bustaferl {

void buildStreamFilters(StreamFilter (&f)[STREAM_COUNT]);            // OGD
void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]);  // EFA
OebbStreamFilter buildOebbFilter();                                   // NEU
} // namespace bustaferl
```

Die ersten drei Indices der `StreamFilter`-Tabelle werden mit den OGD-Streams belegt; **`f[STREAM_SBAHN_HBF]` bleibt default-leer** (`rbl = 0`). Das ist absichtlich: `wienerlinien_parse::findFilterForRbl` matcht nie auf `rbl=0`, weil die OGD-Antwort kein RBL 0 enthält. Der S-Bahn-Slot wird ausschließlich von `fetchOebbStream` befüllt.

Analog `ScheduleStreamFilter[STREAM_SBAHN_HBF]` mit `diva = 0` — `schedule_fetcher` iteriert über die distinct DIVAs (`fetchSchedule` sieht heute schon eine DIVA als „kein Schedule" wenn niemand sie nennt). Test in `test_native_schedule_fetcher` bestätigt.

**`hal/INetwork.h` + `Esp32Network` — `httpPost` + HTTP-Status**

```cpp
// in INetwork.h
struct HttpResult {
  bool ok;            // request lief (kein Transport-Error); Body in `out`
  int  http_status;   // 200, 4xx, 5xx; 0 bei Transport-Error
};
virtual HttpResult httpPost(const std::string &url, const std::string &body,
                            const std::string &content_type, std::string &out) = 0;
```

Der HTTP-Status wird im Result-Struct mitgegeben (nicht via Out-Parameter, nicht via separate `lastHttpStatus()`-Methode). Begründung: `lastHttpStatus()` wäre ein stateful Side-Channel auf einer ansonsten zustandslosen HAL-API und für `FakeNet` in Native-Tests umständlich. `HttpResult` koppelt Body + Status atomar.

`httpGet`/`httpGetStream` bekommen die gleiche `HttpResult`-Rückgabe in derselben Edit-Session (Schritt 1.2) — sonst hätten wir zwei Konventionen nebeneinander. Caller (`api_fetcher`) zählt `http_status ∈ {401,403}` über drei aufeinanderfolgende OGD-Calls und setzt damit den OGD-seitigen Auth-Tripwire (siehe State-Selector §2.2).

Streaming-POST (analog zu `httpGetStream`) wird **nicht** in der ersten Iteration hinzugefügt. Begründung: HAFAS-Antworten sind ~20.5–21.2 KB roh (Pre-Phase 2026-05-19), das ist ~½ der EFA-Antworten (~38 KB), die heute Streaming brauchen. Damit rückt der HAFAS-Pfad näher an die Streaming-Schwelle als ursprünglich aus [CONCEPT §v2-6](../CONCEPT.md#6-api-polling-quoten-und-wake-verhalten) (5–8 KB) angenommen — Schritt 9 (Heap-Profiling) misst, ob die `std::string`-Variante die Heap-Reserve verletzt. Wenn ja, kommt `httpPostStream` als Follow-up. YAGNI bis dahin, aber das Schwellen-Monitoring ist enger.

`Esp32Network::httpPost` reused das bestehende `WiFiClientSecure` und die `setInsecure()`-Policy aus dem GET-Pfad — Let's-Encrypt-Cert für `fahrplan.oebb.at` liegt im Bundle, kein neues Root-Cert nötig (verifiziert via `openssl s_client -connect fahrplan.oebb.at:443` vor Schritt 1).

**`data/Departure.h` — `line_label`**

```cpp
struct Departure {
  time_t when = 0;
  DepartureSource source = DepartureSource::Unknown;
  bool valid = false;
  char line_label[6] = "";   // NEU: "S2", "S3", "S4", "REX1", "" (Bus-Streams)

  bool operator==(const Departure &o) const {
    return valid == o.valid && when == o.when && source == o.source
        && std::strcmp(line_label, o.line_label) == 0;
  }
};
```

`char[6]` statt `std::string`: `Departure` lebt in `StreamData::slot[]`, das wiederum heute in `StreamSnapshot` und nach Frame-Render kompakt in RLE persistiert wird. Heap-Allokation pro Slot wäre vermeidbarer Heap-Druck (4 × 4 × pointer + len + cap = 64 B Overhead), `char[6]` ist 6 B. Maximal-Länge 5 + null-terminator deckt `REX1` (4), `RJX1` (4), `NJ123` (5) ab; alles Längere wird abgekürzt zu `"xx"` (siehe [CONCEPT §v2-7](../CONCEPT.md#7-layout-block-3-display)).

Touch-Sites: `Departure`-Vergleich, `slot_merger::insertSorted` (kopiert via aggregate-assign — funktioniert), `frame_buffer`-Diff vergleicht Bytes (line_label wandert mit). Bus-Streams setzen `line_label[0] = '\0'` per Default-Init; der Renderer wertet `line_label` nur im S-Bahn-Block aus.

**`render/layout.cpp` — Vollständiger Rewrite nach Design-Handoff**

Der heutige Renderer ist Block-für-Block mit Adafruit-GFX-Default-Font. Das neue Layout aus [docs/design_handoff_display/README.md](design_handoff_display/README.md) ist deutlich strukturierter und nutzt zwei Bitmap-Fonts in fünf Größen, drei Badge-Größen, einen Plan-Marker und einen Netzplan. Ein inkrementeller Patch der bestehenden Datei ist nicht sinnvoll — die alte Funktion `renderFrame` wird durch ein neues Modul ersetzt.

Layout-Geometrie (alle Werte in echten Display-Pixeln, [docs/design_handoff_display/README.md „Region boundaries"](design_handoff_display/README.md)):

```text
y ∈ [  0,  98 ]  TG-Block  · 10 px top-pad, 12 px Silkscreen-Header, 2× 32-px-Reihe
y ∈ [ 98, 100 ]  2-px Trennlinie edge-to-edge
y ∈ [100, 150 ]  EG-Block  ·  8 px top-pad, 10 px Silkscreen-Header, 1× 24-px-Reihe
y ∈ [150, 151 ]  1-px Trennlinie
y ∈ [151, 300 ]  Atzg-Block + Netzplan (Netzplan margin-top: auto → bottom-anchor)
x-Padding: 18 px links/rechts für alle Blöcke.
```

Zerlegung in fünf neue Sub-Module:

- **`render/bitmap_fonts.{h,cpp}`** — Daten-Tabellen für VT323 (Größen 14/16/18/20/22/28/72/90 px) und Silkscreen (7/8/10/11/12/14/18 px). Implementierungs-Entscheidung Custom vs. U8g2 in [§4 Schritt 0.6](#schritt-0--pre-flash-verifikation-der-hafas-parameter--render-stack-entscheidung).
- **`render/badge.{h,cpp}`** — `drawBadge(canvas, x, y, text, BadgeSize)` zeichnet ein paper-gefülltes Rechteck mit ink-Text. Drei Größen `sm` (S-Bahn), `md` (EG), `lg` (TG); Padding-/Min-Width-Werte aus [display.jsx Z. 158-160](design_handoff_display/display.jsx).
- **`render/plan_marker.{h,cpp}`** — `drawPlanMark(canvas, x, y)` zeichnet ein 5×5-px Hohlquadrat mit 1-px-Stroke. Nicht gerendert wenn die Zeit selbst leer (`--:--`).
- **`render/network_plan.{h,cpp}`** — `drawNetworkPlan(canvas, x, y, width)` zeichnet das fünfspaltige Schema `Hbf · Atzg · Ende · Tull · Hietz` mit Punkten (4×4), Diamanten (7×7 rotated), Big-Marker (8×8 — „you are here" über Tull), 1-px-Linien und L-förmiger Vertikalverbindung am Transferknoten Atzg. Labels 7-px Silkscreen, Tull + Atzg fett.
- **`render/display_state.{h,cpp}`** — Fullscreen-Renderer für die vier Sonderscreens:
  - `drawBoot(canvas, version_str)` → `◌` (90 px) + „bustaferl" (18 px) + „lädt Fahrplan…" (16 px) + Foot
  - `drawOffline(canvas, last_fetch_at, retry_in_s)` → `!` (90 px) + „Kein Empfang" + Sub mit Last-Fetch-HH:MM + Foot
  - `drawAuth(canvas, aid, http_code)` → `§9` (oder Substitut, siehe §1.5 Q3) + „Auth-Fehler" + diagnostisches Foot
  - `drawQuiet(canvas)` → `—` (72 px) + „Keine Abfahrten" + „in den nächsten 20 min"

**`render/layout.cpp` — neuer `renderFrame`**

```cpp
void renderFrame(const RenderInput &in, Frame &fb) {
  fb.clear(false);                                    // ink-Hintergrund
  ExternalCanvas c(FB_W, FB_H, fb.data());

  switch (in.state) {
  case DisplayState::Boot:    drawBoot(c, in.firmware_version); return;
  case DisplayState::Offline: drawOffline(c, in.last_fetch_at, in.retry_in_s); return;
  case DisplayState::Auth:    drawAuth(c, in.auth_aid, in.auth_http_code); return;
  case DisplayState::Quiet:   drawQuiet(c); return;
  case DisplayState::Stale:
  case DisplayState::Night:
  case DisplayState::Normal:  drawBoard(c, in); return;
  }
}
```

`drawBoard` rendert die drei Datenblöcke + Netzplan; ob Plan-Marker oder `--:--` erscheint, ergibt sich aus `Departure::source` und `Departure::valid` — kein extra Stale-Pfad nötig, weil der State-Selector Stale-Daten zu Slots mit `valid=false` umsetzt (analog zur heutigen `OverlayKind::Stale`-Logik, aber an einer Stelle).

**`logic/render_input.{h,cpp}` — neuer State-Selector**

Heute baut `composeRenderInput` einen `RenderInput` mit `OverlayKind`. Nach v2 wird daraus ein State-Selector. Die Entscheidungs-Reihenfolge aus [docs/design_handoff_display/README.md „State Selection Logic"](design_handoff_display/README.md):

```cpp
struct SelectorSignals {
  bool first_render_ever;        // PersistedMeta::has_any_data == false (post-MAGIC-bump too)
  bool auth_error_seen;          // parseOebbStationBoard hat err ∈ {"AID","AUTH"} gesehen
                                 //   ODER mehr als N OGD-Retries mit HTTP 401/403 gescheitert
  bool wifi_up;
  time_t now;                    // Europe/Vienna local time
  time_t last_success;           // last successful end-to-end fetch
};

DisplayState selectDisplayState(const StreamSnapshot &snap,
                                const ScheduleSnapshot &schedule,
                                const PersistedMeta &meta,
                                const SelectorSignals &sig) {
  // Auth vor Boot: ein Cold-Boot mit kaputter AID würde sonst „lädt Fahrplan…" zeigen
  // statt den Auth-Screen — der wirklich relevante User-Hinweis bliebe verborgen.
  if (sig.auth_error_seen)                                            return DisplayState::Auth;
  if (sig.first_render_ever && !meta.has_any_data)                    return DisplayState::Boot;
  if (!sig.wifi_up && (sig.now - sig.last_success) > OFFLINE_THRESHOLD_S) return DisplayState::Offline;
  if ((sig.now - sig.last_success) > STALE_THRESHOLD_S)               return DisplayState::Stale;
  if (allDeparturesBeyond(snap, sig.now + QUIET_HORIZON_S))           return DisplayState::Quiet;
  if (outsideServiceWindow(sig.now)
      && nextDepartureFarAway(snap, sig.now))                         return DisplayState::Night;
  return DisplayState::Normal;
}
```

**Wichtig zu Auth-Detection**: HAFAS antwortet bei rotierter AID typischerweise mit **HTTP 200 + `err: "AID"` (oder `"AUTH"`)** im JSON-Body, nicht mit HTTP 401/403. Der Auth-State wird daher *aus dem Parser-Ergebnis* getriggert (`parseOebbStationBoard` setzt `auth_error_seen=true` bei `err ∈ {"AID","AUTH"}`), nicht aus dem HTTP-Status. Als zusätzliche Tripwire werden OGD-401/403 mitgezählt — drei aufeinanderfolgende Auth-HTTP-Errors am OGD-Pfad führen ebenfalls zu `auth_error_seen=true`. Reset: ein erfolgreicher Parse (`err == "OK"`) löscht das Flag wieder. Der Status wird in `PersistedMeta::auth_error_seen` persistiert, damit der Auth-Screen Wake-Cycles übersteht.

**Helper-Spezifikation** (alle pure functions in `logic/render_input.cpp`, ein Native-Test pro Helper):

```cpp
// True, wenn jeder valid Slot aller Streams nach `horizon` liegt.
// Leere Snapshots (kein valid Slot) → true (Quiet).
bool allDeparturesBeyond(const StreamSnapshot &snap, time_t horizon);

// True, wenn `now` (Europe/Vienna) außerhalb des Bus-/S-Bahn-Service-Fensters
// liegt. Standard: [SERVICE_WINDOW_START_HOUR, SERVICE_WINDOW_END_HOUR), Wrap
// über Mitternacht wenn END < START (z.B. [5:00, 1:00) ⇒ Nacht ist [1:00, 5:00)).
bool outsideServiceWindow(time_t now);

// True, wenn die nächste valid Plan-Abfahrt mehr als NIGHT_FIRST_DEP_MIN_AHEAD_S
// in der Zukunft liegt. Schützt vor Night-Trigger bei aktivem Spätbetrieb.
bool nextDepartureFarAway(const StreamSnapshot &snap, time_t now);
```

`first_render_ever` ergibt sich aus `PersistedMeta::has_any_data == false`. **Nach jedem MAGIC-Bump** (Schritt 2.7) ist die Meta-Struktur invalidiert → `has_any_data` ist `false` → das erste Wake nach Firmware-Update zeigt den Boot-Screen. Das ist gewollt: visuelle Bestätigung, dass das Update gelaufen ist. Erst der erste erfolgreiche Fetch setzt `has_any_data = true`.

**`composeRenderInput`-Signatur (v2)** — die bestehende Helper-Funktion wird auf das neue State-Modell umgestellt:

```cpp
RenderInput composeRenderInput(DisplayState state,
                               const StreamSnapshot &snap,
                               const ScheduleSnapshot &sched,
                               const PersistedMeta &meta);
```

Befüll-Regeln je `state`:

- `Boot`: `firmware_version = DISPLAY_VERSION_STR`; restliche Daten-Felder leer.
- `Auth`: `auth_aid_short` aus `meta` (gekürzte gespeicherte AID), `auth_http_code` aus `meta` (zuletzt gesehener Status); Daten-Felder leer.
- `Offline`: `last_fetch_at = meta.last_success_at`, `retry_in_s = OFFLINE_THRESHOLD_S - (now - last_success)` (geclamped auf ≥0).
- `Stale`/`Night`/`Quiet`/`Normal`: `snap` und `sched` durchreichen; `state` steuert Render-Variante in `drawBoard`.

Pure function — keine Seiteneffekte, host-testbar in `test_native_render_input`.

Pure Funktion, host-testbar. Schwellwerte (`OFFLINE_THRESHOLD_S = 300`, `STALE_THRESHOLD_S = 600`, `QUIET_HORIZON_S = 1200`, `NIGHT_FIRST_DEP_MIN_AHEAD_S = 1800`, Service-Window) werden in `config.h` als `DEFAULT_*` konstanten geführt — analog zum `cycle_runner`-Konfig-Pattern aus dem main-refactor.

**`data/Departure.h` — `live()`-Helper**

Heute trägt `Departure` die Information schon (`source == Realtime`), aber kein expliziter Accessor. Schritt 2 fügt einen `inline bool live() const { return source == DepartureSource::Realtime; }` hinzu — der Renderer braucht das genau einmal pro Slot, ein Accessor macht die Render-Site lesbar.

**`data/stream_labels.h` — `display_dir()`**

```cpp
inline const char *display_dir(int idx) {
  switch (idx) {
  case STREAM_58A_ATZ:      return "Atzgers.";
  case STREAM_58A_HIETZING: return "Hietzing";
  case STREAM_58B_ATZ:      return "Atzgers.";
  case STREAM_SBAHN_HBF:    return "";        // S-Bahn: Richtung in Header
  default: return "";
  }
}
```

Statisch, max 8 Zeichen (Layout-Constraint aus [README.md TG/EG row anatomy](design_handoff_display/README.md)). Bewusst nicht aus `towards` abgeleitet — der OGD-String `"Bhf. Atzgersdorf S (üb. Atzgersdorfer Str.)"` ist viel zu lang und ändert sich gelegentlich. Annahme [Schritt 7]: wenn das Display-Label später konfigurierbar werden soll, kommt das in eine eigene PR; jetzt Hartkodierung statisch parallel zum `streamLabel()`.

### 2.3 Stream-Layout im Detail

```text
STREAM_58A_ATZ       (0) — Tullnertalgasse 58A → Atzgersdorf       (OGD)
STREAM_58A_HIETZING  (1) — Tullnertalgasse 58A → Hietzing          (OGD)
STREAM_58B_ATZ       (2) — Endemanngasse   58B → Atzgersdorf       (OGD)
STREAM_SBAHN_HBF     (3) — Bhf. Atzgersdorf S-Bahn → Wien Hbf      (ÖBB HAFAS)
STREAM_COUNT = 4
```

`SLOTS_PER_STREAM = 2` bleibt für alle Streams — Variante A aus [CONCEPT §v2-5.2](../CONCEPT.md#52-slot-anzahl-pro-stream). Eine Erweiterung auf 3 für den S-Bahn-Stream allein wäre eine eigene PR nach v2-Roll-out, sobald Beobachtungsdaten vorliegen.

### 2.4 Konfiguration

Neue Konstanten in `config.h`:

```cpp
// ÖBB Wien Atzgersdorf → Wien Hauptbahnhof
// Daten-Layer: HAFAS-interne Location-IDs pro Locality (NICHT die DB-EVAs;
// 81xxxxxx resolvt zwar zum Namen, hat aber keine Fahrplanlegs gebunden —
// siehe §4.1 Pre-Phase-Annahmen).
#define OEBB_EXTID_ATZG           "1292301"   // Wien Atzgersdorf Bahnhst
#define OEBB_EXTID_WIENHBF        "1290401"   // Wien Hbf (U)

// Rollen-Layer: welche extId füllt welchen Request-Slot
#define OEBB_STBLOC_EXTID         OEBB_EXTID_ATZG
#define OEBB_DIRLOC_EXTID         OEBB_EXTID_WIENHBF

#define OEBB_MGATE_URL            "https://fahrplan.oebb.at/bin/mgate.exe"
#define OEBB_HAFAS_AID            "OWDL4fE4ixNiPBBm"   // §4.1 Pre-Phase bestätigt
#define OEBB_HAFAS_CLIENT_JSON \
  "{\"id\":\"OEBB\",\"type\":\"WEB\",\"name\":\"webapp\",\"l\":\"vs_webapp\"}"
#define OEBB_HAFAS_VER            "1.67"
#define OEBB_JNYFLTR_PRODUCTS     "63"                 // §4.1 Pre-Phase bestätigt
#define OEBB_MAX_JNY              6
```

`OEBB_HAFAS_AID`, `OEBB_HAFAS_CLIENT_JSON`, `OEBB_HAFAS_VER`, `OEBB_JNYFLTR_PRODUCTS` sind in der Pre-Phase 2026-05-19 empirisch bestätigt (siehe §4.1). Bei künftiger AID-Rotation (Tripwire feuert) hier mit neuem Wert + Kommentar `// re-confirmed against webapp on 2026-MM-DD` aktualisieren.

Display-State-Schwellwerte:

```cpp
// State-Selector (logic/render_input)
#define OFFLINE_THRESHOLD_S         300    //  5 min ohne erfolgreichen Fetch + WiFi unten
#define STALE_THRESHOLD_S           600    // 10 min ohne erfolgreichen Fetch (WiFi-Status egal)
#define QUIET_HORIZON_S            1200    // 20 min — keine Abfahrten in diesem Fenster → Quiet
#define NIGHT_FIRST_DEP_MIN_AHEAD_S 1800   // 30 min — wenn nächste Plan-Abfahrt weiter → Night

// Service-Window: Spätbetrieb 58A/58B/S-Bahn endet ~0:30, Frühbetrieb ab ~5:00.
// END_HOUR < START_HOUR ⇒ Fenster wrappt um Mitternacht. outsideServiceWindow()
// gibt true zurück für [SERVICE_WINDOW_END_HOUR, SERVICE_WINDOW_START_HOUR), also 01:00–04:59.
#define SERVICE_WINDOW_START_HOUR  5
#define SERVICE_WINDOW_END_HOUR    1

// Display-Versionsstring (Foot-Zeile Boot-Screen)
#define DISPLAY_VERSION_STR "v2.0 · UC8176 · 400x300"
```

Entfernt (Schritt 2.4 löscht **alle** U1-Konstanten, kein Reststand):

```cpp
#define RBL_SUEDTIROLER_LEOPOLDAU 4105
#define RBL_SUEDTIROLER_OBERLAA   4124
#define TOWARDS_U1_LEOPOLDAU     "Leopoldau"
#define TOWARDS_U1_OBERLAA       "Oberlaa"
#define DIVA_SUEDTIROLER_PLATZ    60201349
#define EFA_TOWARDS_U1_LEOPOLDAU "Leopoldau"
#define EFA_TOWARDS_U1_OBERLAA   "Oberlaa"
#define LINE_U1                  "U1"
```

---

## 3. Pros / Cons / Out-of-Scope

### 3.1 Pros

- **Konzept-Versprechen aus CONCEPT §v2 wird eingelöst.** S-Bahn statt zwei U1-Streams ist der vorzimmer-praktische Use-Case (zum Bahnhof, nicht in die Stadt mit dem Umweg).
- **Display-Versprechen aus Design-Handoff wird eingelöst.** Stationstafel-Optik, klare Plan/Live-Differenzierung, sichtbarer Netzplan. Der heutige Render ist funktional, der neue lesbar.
- **Schichtenregel wird nicht verletzt.** HAFAS-Parser landet sauber in `data/`, ein neuer HAL-Methodenstub (`httpPost`) ist die einzige Cross-Layer-Bewegung. Renderer-Rewrite bleibt komplett in `render/`.
- **Single-source-of-truth-Architektur des main-refactors trägt:** Stream-Topologie-Änderung berührt genau `stream_labels.h`, `filter_builder.cpp`, `config.h`, `snapshot_fetcher.cpp` und `layout.cpp`. Kein Suchen-und-Ersetzen quer durchs Repo.
- **State-Selector wird host-testbar** (heute lief die Overlay-Auswahl im warmCycle als verstreute `if`s). `selectDisplayState` ist pure function, jedes der 7 States bekommt einen Native-Test.
- **Bestehende Test-Infrastruktur skaliert mit:** Pro-Stream-Tests werden Index-3-Erwartung ergänzt, ein neuer `test_native_oebb_hafas_parse` analog zu `test_native_wienerlinien_parse` mit Fixture-Pattern.
- **`DepartureSource` bleibt protokoll-agnostisch** ([docs/main-refactor-plan.md §2.1, „bewusst protokoll-agnostische Benennung"](main-refactor-plan.md)) — exakt der Punkt, für den der Refactor die Benennung neutral gewählt hat. v2 nutzt dieselben drei Werte ohne Erweiterung; `Departure::live()` ist ein Accessor, kein neuer Zustand.

### 3.2 Cons

- **AID/Client/jnyFltrL-Brüchigkeit.** ÖBB rotiert diese Werte ohne Vorankündigung. Mitigation: Pre-Flash-Verifikation in Schritt 0 + `FilterHealth` für S-Bahn-Stream + Fullscreen-Auth-Fehler-Screen ([docs/design_handoff_display/README.md §6](design_handoff_display/README.md)).
- **Heap-Spitze beim HTTPS-POST** noch nicht profiled. Bestehende Heap-Wächter in [src/logic/schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp) sind auf den EFA-Fall zugeschnitten; HAFAS-Pfad braucht eigene Messung (Schritt 10). Risiko mittel-hoch — die Antwortgröße ist ~20.5–21.2 KB statt 38 KB (Pre-Phase 2026-05-19); siehe V3.
- **Renderer-Rewrite ist substantiell.** [src/render/layout.cpp](../src/render/layout.cpp) wird nicht inkrementell patcht, sondern ersetzt durch fünf neue Sub-Module + neues `layout.cpp`. Ohne Pixel-Match-Tests (es gibt keine) ist die Validation visuell — Risiko, dass kleine Geometrie-Drift erst beim Roll-out auffällt. Mitigation: `RecordingRenderer` der Native-Runtime kann PGM-Dumps schreiben, A/B-Vergleich gegen Design-Screenshots aus `docs/design_handoff_display/screen-*.png` möglich (pixel-vergleichend, aber Glyph-Spritedaten in Bitmap-Fonts müssen 1:1 zum Design-VT323/Silkscreen passen).
- **Font-Daten in Flash.** Zwei Bitmap-Fonts in vier bzw. fünf Größen kosten je nach Stack ~20–80 KB Flash (U8g2 packt ~1 KB pro Font-Size; Custom-PROGMEM ist enger). Bestehende Firmware ist ~600 KB, ESP32-Flash hat 4 MB — Headroom ausreichend, aber sichtbarer Anstieg in `make size`.
- **RTC-Frame-MAGIC-Bump** verwirft beim Update den letzten gerenderten Frame → erstes Wake nach Update macht Light Full statt Partial. Einmalig, akzeptabel.
- **RTC-Footprint-Zuwachs.** `Departure::line_label` (4 Streams × 2 Slots × 6 B = 48 B) plus `PersistedMeta`-Erweiterung aus Schritt 2.6 (~12 B inkl. Padding) = +60 B. Eingespart werden ~84 B durch die entfallenden U1-Hints. Netto steigt der Reserve-Wert leicht von ~780 B auf ~804 B — siehe [Anhang B](#anhang-b--rtc-bilanz-nach-v2).
- **Layout-Kalibrierung** für Linien-Längen `S2` / `REX1` ist visuell zu prüfen — `test_device_render`-Suite hat heute keinen Pixel-Match, nur Pass/Fail auf API-Pfade. Schritt 7-Validation muss per Mensch erfolgen (Schritt 11).
- **Doku-Sync notwendig in fünf Files** (CONCEPT.md, README.md, ARCHITECTURE.md, HANDBUCH.md, USER.md — letztere zwei wegen Display-Änderung). Schritt 11 dafür eingeplant.

### 3.3 Ausdrücklich nicht in Scope

**Dauerhaft out of scope** (bewusste Begrenzung von v2, keine geplante Nachfolge):

- **Keine HAFAS-Hint-Quelle** (Variante 2 aus [CONCEPT §v2-8](../CONCEPT.md#8-plan-hints-analogie-zu-v1-12)). Default Variante 1 — kein Hint-Pfad für S-Bahn. Begründung: Stammstrecke fährt 5:00–0:30, das 70-Minuten-Realtime-Fenster reicht praktisch immer.
- **Kein clientseitiger Richtungs-Healthcheck.** `dirLoc` wird vertraut, solange Schritt 0 Punkt 3 das bestätigt. Fallback-Heuristik via `jnyL[i].dirTxt` nur reaktiv (siehe [Anhang C](#anhang-c--open-questions-vor-schritt-0)).
- **Keine Vorbereitung weiterer Stationen** (Tullnerfeld o.ä.). v2 ist exakt eine S-Bahn-Strecke.
- **Kein Bahnsteig-Display.** `dPltfS.txt` wird geparst und liegt ungenutzt im Snapshot herum, um spätere Erweiterung billig zu halten — Renderer ignoriert das Feld.
- **Keine Verspätungsanzeige als Zahl.** `dTimeR` ersetzt `dTimeS` still — derselbe Modus wie bei der OGD-Realtime/Plan-Fallback.
- **Keine i18n.** Display-Strings (`"lädt Fahrplan…"`, `"Auth-Fehler"`, `"Kein Empfang"`, `"Keine Abfahrten"`) sind hart deutsch — passend zum Wiener-Use-Case mit einem einzigen Aufstellort. Mehrsprachigkeit erforderte Font-Erweiterung (Umlaute heute über VT323/Silkscreen-Standard-Bitmap-Range abgedeckt; andere Sprachen evtl. nicht).
- **Keine DST-Manuell-Behandlung.** HAFAS liefert Zeiten in Europe/Vienna lokal mit serverseitig aufgelöstem DST — wir interpretieren mit `TZ_INFO` ohne Eigenlogik. Mitigation-Risiko: kein Code-Pfad bei DST-Wechsel speziell. Auswirkung wäre maximal ±60 min Anzeigedrift an zwei Tagen im Jahr.
- **Kein automatischer HAFAS-Fallback.** Wenn `fahrplan.oebb.at` dauerhaft wegbricht (z.B. ÖBB schaltet die API ab), bleibt der S-Bahn-Stream auf `--:--`. Eine EFA-Fallback-Quelle wurde geprüft und verworfen: EFA-Wiener-Linien kennt die ÖBB-S-Bahn-Halte nicht in derselben Granularität (keine DIVA für Atzgersdorf-Bahnhof in der Bus-Topology); ein „weicher Rollback" auf U1-Streams wäre möglich, ist aber so groß wie diese Migration in Rückwärts. Im realen Bruch-Fall: neue PR oder Re-Flash älterer Firmware.
- **Kein Pixel-Match-Test gegen Design-PNGs.** Die Screenshots aus `docs/design_handoff_display/screen-*.png` sind 2×-CSS-gerenderte Browser-Mocks, kein 1:1-bitmap-Vergleich möglich (VT323 als TTF vs. on-device Bitmap-Font können einzelne Pixel verschoben sein). Validation bleibt visuell.

**Vertagt** (akzeptierte v2-Einschränkungen mit Aussicht auf Follow-up-PR):

- **Kein dritter Slot** für den S-Bahn-Stream — Design-Handoff zeigt drei Slots im S-Bahn-Block ([design_handoff_display/README.md „Atzgersdorf S-Bahn row anatomy"](design_handoff_display/README.md)). Daten-seitig bleibt `SLOTS_PER_STREAM = 2` (Variante A aus CONCEPT §v2-5.2); der Renderer zeichnet nur die zwei verfügbaren Slots und lässt die dritte Spalte leer. **Visueller Mismatch zum Mockup wird in 11.2 explizit gegen Auftraggeber abgenommen.** Migration auf 3 Slots → eigene PR nach v2-Roll-out, sobald Beobachtungsdaten zeigen, ob der dritte Slot Mehrwert hat.
- **Kein `httpPostStream`** (siehe §2.2 Begründung). Erst bei Bedarf nach Schritt 9 (Heap-Profiling).
- **Kein Custom-VT323/Silkscreen-Font-Stack.** Option A (U8g2) für v2, Option B (Custom-Bitmap) nur falls Schritt 11.8 die Drift unzumutbar findet.

**Bewusst nicht betroffen** (Persistierung, kein Touch):

- **WLAN-Credentials in NVS bleiben erhalten.** RTC-MAGIC-Bumps in Schritt 2.7 invalidieren ausschließlich RTC-Slow-Memory (Snapshot, Frame, Schedule). WLAN-Daten liegen in NVS (separate Partition) und überleben Firmware-Update + RTC-Reset. Erstes Boot nach Update geht direkt in den Connect-Pfad, ohne Re-Konfiguration durch den User.

---

## 4. Schritt-für-Schritt-Plan

### 4.0 Vorgehensmodell

Analog [main-refactor-plan §4.0](main-refactor-plan.md). Zusammenhängende Umsetzung in einem Schwung; Auftraggeber wird **am Anfang** (Schritt 0 — AID/Client/jnyFltrL + Font-Stack-Entscheidung klären) und **am Ende** (visuelle Layout-Inspektion + Roll-out + Release-Tag) involviert.

- **Branch**: neuer Branch `v2/sbahn-atzgersdorf` aus `main` abgezweigt (erst nach Abschluss des main-refactors auf `main` gemergt).
- **Commits**: Konvention wie main-refactor — `<Kategorie>: <kurze Beschreibung>`, Kategorien `Data`, `HAL`, `Engine`, `Render`, `Doku`, `Test`, `Tooling`. Kein Co-Authored-By-Trailer.
- **Granularität**: ein Commit pro Schritt-Checkbox (Sub-Steps dürfen gebündelt sein).
- **Push: nein.** Lokal bleiben bis Auftraggeber explizit grünes Licht gibt.
- **Annahmen während der Umsetzung** werden in §4.1 mit Schritt-Referenz festgehalten.
- **Merge-Strategie bei Drift**: Falls während der 2–3 Wochen Branch-Lebenszeit andere PRs auf `main` landen, wird `v2/sbahn-atzgersdorf` per `git merge main` (kein Rebase) periodisch aktualisiert, um saubere Konflikt-Auflösungs-Commits zu behalten. Konflikt-Erwartung in `render/layout.cpp` (komplett-Rewrite — Theirs-bevorzugen) und `data/StreamSnapshot.h` (Index-Verschiebung — manuell auflösen). Schedule-, Filter-, Cycle-Module sollten konfliktfrei mergen.
- **Final-Merge nach `main`**: Squash-Merge oder Merge-Commit nach Auftraggeber-Wahl in Schritt 12. Branch wird **nicht** sofort gelöscht (mind. eine Woche Beobachtung am Gerät).

**Gesamt-Aufwandsschätzung** (zur Erwartungs-Steuerung — Plan ist substantieller als ein üblicher Feature-Branch):

| Phase | Aufwand |
|---|---|
| Pre-Phase (PoC) | 0.25 d |
| Schritt 0 (Pre-Flash + Render-Stack) | 1–1.5 d |
| Schritt 1 (`httpPost` + `HttpResult`) | 0.5–0.75 d |
| Schritt 2 (Datenmodell + MAGICs) | 0.5 d |
| Schritt 3 (`oebb_hafas_parse` + Tests) | 1–1.5 d |
| Schritt 4 (filter_builder) | 0.5 d |
| Schritt 5 (snapshot_fetcher + OEBB-Pfad) | 0.5–1 d |
| Schritt 6 (schedule_fetcher skip) | 0.25 d |
| Schritt 7 (Display-Rewrite) | 3–4 d |
| Schritt 8 (Tests umstellen) | 2.5–3 d |
| Schritt 9 (Heap-Profiling) | 0.5 d |
| Schritt 10 (Doku-Sync) | 1 d |
| Schritt 11 (HW-Verifikation) | 1 d + ggf. 24h Soak |
| Schritt 12 (Release) | 0.5 d |
| **Summe** | **≈ 13–17 Tage** netto, realistisch ≈ 3 Kalenderwochen bei Vollzeit-Verfügbarkeit |

Konkrete Session-Gruppierung in [§4.3 — Session-Gruppierung A-G](#43-session-gruppierung-a-g).

### 4.1 Annahmen während der Umsetzung

Plan-Reifungs-Stand: **2026-05-19** (alle untenstehenden Festlegungen tragen dieses Datum, sofern nicht anders vermerkt). Neue Annahmen während der Umsetzung als Tabellen-Zeile mit eigenem Datum nachtragen.

| # | Schritt | Annahme | Begründung |
|---|---|---|---|
| C1 | 0.1 | AID-Beschaffung: **Weg A** (PoC-Skript mit CONCEPT-Default-AID `OWDL4fE4ixNiPBBm`); DevTools nur als Fallback. | Bei `err: "OK"` ist der Wert empirisch validiert — kein zusätzlicher Aufwand. |
| C8 | 0.6 | Font-Stack: **Option A** — `U8g2_for_Adafruit_GFX` mit Logisoso (Daten) + Helv/5x8 (Headers). | Kein Tooling-Aufwand (Memory `feedback-no-tooling-rabbit-holes`); ~80% Pixel-Treue ausreichend für v2. Option B als Follow-up-PR, falls 11.8 die Drift unzumutbar findet. |
| C9 | 0.7 | Glyph-Substitution: **Custom-1-bit-Bitmap-Assets** in PROGMEM (`◌`, `!`, `§9`, je 90 px). | Aufwand verschwindend gegen Renderer-Rewrite; Design-Treue 100%. Boot- und Auth-Screen werden *unsere* Screens, nicht ASCII-Fallback. |
| C10 | 0.8 | Direction-Text-Quelle: **statisch** in `data/stream_labels.h::display_dir()` (`"Atzgers."`, `"Hietzing"`, leer für S-Bahn). | Layout-Determinismus + RTC-Footprint klein halten; Wartung über Anlass-Commit bei String-Drift seitens Wiener Linien. |
| C12 | 7.6 | ▼-Glyph im Netzplan als **5×5-Custom-Triangle-Sprite**, nicht aus dem Font. | U8g2-Glyph-Verfügbarkeit auf 10-px-Höhe ist font-versionsabhängig; deterministischer 25-Pixel-Sprite ist robuster und 30 B groß. |
| Plan-Reifung | §2.2 | Auth-State wird über **Parser-Flag** `auth_error_seen` getriggert (HAFAS `err: "AID"`/`"AUTH"`), nicht über HTTP-401/403. OGD-401/403 als sekundäre Tripwire. | HAFAS sendet auth-Failure als HTTP 200 + Body-err-Code; reine HTTP-Status-Logik liefe in den Auth-Pfad nicht hinein. |
| Plan-Reifung | 2.7 | `first_render_ever ⇒ Boot-Screen` greift **auch nach jedem Firmware-Update** (MAGIC-Bumps invalidieren Meta). | Gewollt: visuelle Bestätigung, dass das Update gelaufen ist; einmaliges Boot-Wake nach Update ist akzeptabel. |
| Pre-Phase 2026-05-19 | §2.4 / 0.1 | **Station-extIds sind HAFAS-interne Location-IDs, nicht die DB-EVAs.** Atzgersdorf = `1292301` („Wien Atzgersdorf Bahnhst"), Wien Hbf = `1290401` („Wien Hbf (U)"). Die 8-stelligen `81xxxxxx`-EVAs aus CONCEPT §v2 resolven zum Stationsnamen, aber HAFAS hängt keine Fahrplanlegs daran → leere `jnyL`. | LocMatch-Befund 2026-05-19 (`.tmp/poc-oebb/locmatch-atzgersdorf.json`); Variant mit neuen IDs liefert 6 jnyL, alte IDs 0. |
| Pre-Phase 2026-05-19 | 0.1 | **AID `OWDL4fE4ixNiPBBm` empirisch bestätigt** über vier PoC-Aufrufe in 30 min: HTTP 200, `err=OK`, 6 reale Departures pro Antwort. DevTools-Mitschnitt nicht erforderlich. | C1-Weg-A erfolgreich; AID muss erst nachverifiziert werden, wenn `auth_error_seen`-Tripwire feuert. |
| Pre-Phase 2026-05-19 | 0.2 | **`jnyFltrL.value = "63"` empirisch korrekt.** Ohne Filter liefert HAFAS 9 Treffer inkl. Bus (`cls=64`); mit „63" (Bits 0–5) bleiben 6 Bahn-Treffer (`cls=16,32`). Empirisch identisch zu „1023" → engere Maske wäre möglich, aber „63" ist robust. | C2-Weg-A erfolgreich; DevTools-Toggle entfällt. |
| Pre-Phase 2026-05-19 | 0.3 | **`dirLoc` filtert Gegenrichtung serverseitig wie erwartet.** Variante ohne dirLoc liefert `S 2 → Mödling Bahnhof` (Gegenrichtung); mit `dirLoc.extId=1290401` listet HAFAS ausschließlich Züge Richtung Marchegg/Wolkersdorf/Absdorf/Gänserndorf/Mistelbach/Hollabrunn (via Wien Hbf). Kein clientseitiger `dirTxt`-Healthcheck nötig. | Risiko V2 entschärft; Akzept-Liste in §1.5 Daten-Schicht Item 3 wird nicht implementiert. Spätabend-Drift wird im Live-Betrieb am Display sichtbar. |
| Pre-Phase 2026-05-19 | 0.4 | **TLS-Issuer = DigiCert Global G2 TLS RSA SHA256 2020 CA1** (im `WiFiClientSecure`-Standard-Bundle). Cert gültig bis 2026-12-08. | `setInsecure()` reicht; expliziter Root-Cert-Mitgabe nicht erforderlich. |
| Pre-Phase 2026-05-19 | §2.4 / Schritt 3.5 | **`cfg`-Block im Request weglassen.** `{"polyEnc":"GPA","rtMode":"HYBRID"}` produziert `err=PARSE` (HTTP 200, 254 B Antwort). Der Request bleibt ohne `cfg`. | E_with_cfg-Variant 2026-05-19. |
| Pre-Phase 2026-05-19 | Schritt 3.5 | **`line_label = whitespace_strip(prodL[i].nameS)`**, nicht `name`. `nameS` ist die kanonische Kurzform (`"S 1"`, `"REX 1"`); `name` enthält die Zugnummer als Anhang (`"S 1 (Zug-Nr. 28842)"`) und ist nicht für `line_label[6]` geeignet. Whitespace strippen → `"S1"`, `"REX1"` (passt in 5 ASCII + `\0`). | Schema-Inspektion 2026-05-19. |
| Pre-Phase 2026-05-19 | Schritt 3.5 | **Zeit-Felder `dTimeS`/`dTimeR` sind `HHMMSS` (6-stellig)**, nicht HHMM. Parser muss die Sekunden trimmen oder als Teil der Zeit interpretieren. | Smoke-Daten zeigen `142900` = 14:29:00. |
| Pre-Phase 2026-05-19 | V3 / §5 | **Antwortgröße liegt bei ~20.5–21.2 KB roh** (drei Samples 14:31/14:46/15:01: 20506/21133/21188 B), nicht 5–8 KB wie in V3 angenommen. Treiber: `prodL[].himIdL` enthält pro Linie 17–55 FREETEXT-Referenzen. | Risiko V3 muss nachgeschärft werden (Mitigation: Parser ignoriert `himIdL` komplett — reduziert Parse-Zeit, vermeidet unnötige ArduinoJson-DOM-Knoten). |
| Pre-Phase 2026-05-19 | §2.4 / 2.5 | **Konstanten-Schema: Daten-Layer + Rollen-Layer.** `OEBB_EXTID_ATZG = "1292301"` und `OEBB_EXTID_WIENHBF = "1290401"` sind die physischen Tatsachen (Locality → HAFAS-extId); `OEBB_STBLOC_EXTID = OEBB_EXTID_ATZG` und `OEBB_DIRLOC_EXTID = OEBB_EXTID_WIENHBF` binden Request-Rolle an Locality. Beide Achsen unabhängig editierbar. | Auftraggeber-Festlegung 2026-05-19; Memory [`feedback-locality-names`](../.claude/projects/.../memory/feedback-locality-names.md). |

Weitere Annahmen werden hier nachgetragen, sobald die Umsetzung beginnt.

### 4.2 Schritte

#### Pre-Phase — PoC: ÖBB-Fetch isoliert (Host-Skript)

- [ ] erledigt

**Zweck**: empirisch klären, ob ein `mgate.exe`-Aufruf gegen Atzgersdorf → Wien Hbf reproduzierbar funktioniert, bevor wir das Gesamtprojekt anfassen. Bewusst minimal, bewusst Wegwerf-Code, **kein Touch an `src/`** und kein neuer Test-Bucket — alles bleibt in `scripts/` und `.tmp/`, das `make ci` ignoriert. Wenn die Pre-Phase scheitert (API antwortet nicht wie erwartet, AID rotiert anders als angenommen, dirLoc unbrauchbar), ist das Wissen geschaffen *ohne* Refactor-Rollback.

- **P.1** **`scripts/poc-oebb-fetch.py` anlegen.** Python 3, nur stdlib (`urllib.request` + `json`) — keine venv, kein `pip install`, kein Tooling-Rabbit-Hole. Hartgekapselt:
  - Body wie in [Anhang A](#anhang-a--hafas-request-response-vertrag), Werte aus CONCEPT §v2 als Defaults.
  - POST gegen `https://fahrplan.oebb.at/bin/mgate.exe` mit `Content-Type: application/json`.
  - Antwort als formatiertes JSON nach stdout.
  - Zusätzlich auf stderr eine 5-Zeilen-Summary: HTTP-Status, `err`-Feld, Anzahl `jnyL`-Einträge, erste 3 Departures mit Linie (`prodL[…].name`) + Plan-Zeit + Echtzeit + Cancelled-Flag.
  - CLI-Args optional: `--aid …`, `--products …`, `--max-jny …` — zum schnellen Experimentieren ohne Code-Edit.
- **P.2** **Drei Aufrufe abfeuern** (Morgen ~7:30, Mittag ~13:00, Spätabend ~22:00), Antworten unter `.tmp/poc-oebb/` ablegen:

  ```text
  mkdir -p .tmp/poc-oebb
  python3 scripts/poc-oebb-fetch.py > .tmp/poc-oebb/morning.json  2> .tmp/poc-oebb/morning.summary
  python3 scripts/poc-oebb-fetch.py > .tmp/poc-oebb/noon.json     2> .tmp/poc-oebb/noon.summary
  python3 scripts/poc-oebb-fetch.py > .tmp/poc-oebb/evening.json  2> .tmp/poc-oebb/evening.summary
  ```

- **P.3** **Antworten sichten** (5 Min pro Antwort, keine Doku-Pflicht):
  - Funktioniert `dirLoc` als Gegenrichtungs-Filter zuverlässig? (Alle Einträge Richtung Hbf, keine Mödling/Wr. Neustadt?)
  - Welche `prodL[…].name`-Werte tatsächlich (S2/S3/S4/REX1/…)?
  - HTTP 200 + `err: "OK"` in allen drei Antworten?
  - Antwortgröße in KB notieren (für Heap-Risiko V3 ein erster Anker).
- **P.4** **Erkenntnisse als §4.1-Annahme einchecken.** Format-Beispiel:

  ```text
  **[Pre-Phase, 2026-05-19]** AID "OWDL4fE4ixNiPBBm" funktioniert,
  jnyFltrL "63" liefert S-Bahn-Linien S1/S2/S3/S4, dirLoc filtert Gegenrichtung,
  Antwortgröße ~20.5–21.2 KB roh (himIdL-Treiber). HAFAS-extIds 1292301/1290401,
  NICHT die DB-EVAs (siehe §4.1).
  ```

  Damit ist Schritt 0.1/0.2/0.3 entweder **bestätigt** (DevTools-Mitschnitt entfällt) oder **rotwarnend** (DevTools-Sweep wird Pflicht).
- **P.5** **Antworten aufheben.** `.tmp/poc-oebb/*.json` sind potentielle Fixtures für Schritt 3.4 (`test/test_native_oebb_hafas_parse/fixtures/oebb_live_*.h`). Nicht löschen bis Schritt 3 durch.
- **P.6** **Cleanup-Pointer.** `scripts/poc-oebb-fetch.py` wird **nicht** ins finale v2-Release übernommen. Lösch-Zeitpunkt: nach Schritt 3.4, sobald die Fixtures im Repo sind und das Skript keinen Zweck mehr hat. Eigener kleiner Commit, Subject: `Tooling: PoC-Skript poc-oebb-fetch.py entfernen (Fixtures in Schritt 3 übernommen)`.

**Aufwand**: 2–4 h gesamt — Skript ~1 h, drei Aufrufe + Sichtung ~1 h, Annahmen ~30 min, Cleanup-Pointer notiert (kein Aufwand jetzt).

**Validation**: drei JSON-Antworten unter `.tmp/poc-oebb/`; mindestens eine enthält drei verschiedene S-Bahn-Linien mit `dTimeR`-Echtzeitwerten; §4.1-Eintrag steht.

**Reversibel**: trivial — `rm scripts/poc-oebb-fetch.py && rm -rf .tmp/poc-oebb/`. `.tmp/` ist gitignored, einziges Repo-Artefakt ist das Skript selbst, das nie auf `main` kommt (bleibt auf `v2/sbahn-atzgersdorf` und wird bei P.6 entfernt).

**Was diese Phase nicht macht:**

- **Keine HAL-Erweiterung** (kein `httpPost` auf `INetwork`).
- **Kein Touch an `src/data/`** (kein `oebb_hafas_parse`-Modul-Stub).
- **Kein neuer Test-Bucket** (das Skript ist nicht Teil von `make test`).
- **Kein C++-Code.** Auch nicht „nur kurz mal ArduinoJson-Snippet bauen". Python isoliert die Frage „spricht der Server überhaupt mit uns?" sauber von „können wir das mit unserem Stack auch?". Die zweite Frage beantworten Schritte 1 + 3.

#### Schritt 0 — Pre-Flash-Verifikation der HAFAS-Parameter + Render-Stack-Entscheidung

- [ ] erledigt

Blocker für alle nachfolgenden Schritte. Ergebnis sind verifizierte HAFAS-Konstanten in `config.h` **und** eine festgeschriebene Render-Stack-Entscheidung mit Build-System-Hooks.

**HAFAS-Parameter (0.1–0.4):**

- **0.1** **AID/Client-Werte abfangen.** DevTools öffnen auf `https://fahrplan.oebb.at/webapp`, eine beliebige Abfahrtsabfrage Atzgersdorf → Wien Hbf machen, im Network-Tab den `mgate.exe`-Request finden, Request-Body als JSON parsen. Felder `auth.aid`, `client.id`, `client.type`, `client.name`, `client.l`, `ver` notieren. Werte als HEREDOC-Block in `docs/v2-sbahn-migration-plan.md` Anhang A einchecken (Beleg für künftige Updates), und in `config.h` als `OEBB_HAFAS_AID`, `OEBB_HAFAS_CLIENT_JSON`, `OEBB_HAFAS_VER` setzen.
- **0.2** **`jnyFltrL`-Bitmask bestimmen.** In derselben DevTools-Session den Produktfilter der Webapp einmal toggeln (S-Bahn aus, S-Bahn an). Vergleichen, welches `jnyFltrL[].value`-Feld wechselt — das ist der Bitvektor für „nur S-Bahn", umgekehrt für die Maske inkl. Regio+REX. Wert in `config.h` als `OEBB_JNYFLTR_PRODUCTS` setzen.
- **0.3** **`dirLoc` über zwei Werktage gestaffelt gegenchecken.** Sechs mgate-Requests (drei Tageszeiten × zwei Werktage) mit `stbLoc.extId = OEBB_STBLOC_EXTID`, `dirLoc.extId = OEBB_DIRLOC_EXTID`, `maxJny = 10`: Hauptverkehr (~7:30), Mittag (~13:00) und Spätabend (~22:00). Zwei aufeinanderfolgende Werktage absichern Werktagsfahrplan-Varianz; Wochenende ist hier nicht relevant (Atzgersdorf-S-Bahn fährt Mo-Fr dichter, am Wochenende ändert sich nur die Frequenz, nicht die Richtung). Antworten aus dem Network-Tab (oder PoC-Skript-Output) in `.tmp/hafas-fixtures/` ablegen. Pro Antwort: enthält die Liste nur Züge Richtung Hauptbahnhof, oder mischen sich Gegenrichtungs-Departures (Mödling/Wr. Neustadt) durch? Wenn ja → Schritt 3 erweitert um clientseitigen Direction-Healthcheck via `jnyL[i].dirTxt`.

  > **Festlegung 2026-05-19**: dirLoc-Wirksamkeit ist durch das Vergleichspaar „mit dirLoc" vs „ohne dirLoc" (Pre-Phase Debug-Varianten) belegt — Variante ohne dirLoc liefert `S 2 → Mödling`, Variante mit dirLoc filtert sie serverseitig aus. Die ursprüngliche Sechs-Request-Schleife (3 Tageszeiten × 2 Werktage) entfällt. Sparse-Mode-Drift (Spätabend) wird im echten Live-Betrieb am Display erkannt — falsche Gegenrichtungs-Departures sind visuell offensichtlich; bei Auffälligkeit wird der clientseitige `dirTxt`-Healthcheck nachgerüstet.
- **0.4** **Cert-Check.** `openssl s_client -connect fahrplan.oebb.at:443 -servername fahrplan.oebb.at < /dev/null 2>/dev/null | openssl x509 -noout -issuer` → Issuer notieren. Wenn nicht in `WiFiClientSecure`-Bundle (Let's Encrypt, DigiCert, ISRG) → in Schritt 1 explizit ein Root-Cert mitgeben statt `setInsecure()`.

**Render-Stack (0.6–0.8):**

- **0.6** **Font-Stack festlegen.** Drei Optionen:
  - **Option A — U8g2_for_Adafruit_GFX** (Empfehlung). Library liefert ~100 Bitmap-Fonts, darunter `u8g2_font_logisoso16_tr` / `u8g2_font_logisoso28_tr` (VT323-ähnliche Optik, fettere Glyphen) und `u8g2_font_5x8_tr` / `u8g2_font_helvR08_tr` (Silkscreen-ähnlich für Headers). Pro: kein Font-Tooling, GxEPD2-offiziell unterstützte Bridge. Contra: kein exakter VT323/Silkscreen-Match — die Glyph-Sprites werden ähnlich aussehen, nicht identisch. Aufwand: +1 Library in `platformio.ini`, ~0.5d zur Integration.
  - **Option B — Custom Bitmap-Fonts** via Adafruit-`fontconvert` aus VT323.ttf + Silkscreen.ttf in `GFXfont`-Strukturen. Pro: Pixel-exakt zum Design-Handoff. Contra: `fontconvert` muss aus Adafruit-GFX-Source gebaut werden (kleiner Tooling-Aufwand); pro Font + Größe ein PROGMEM-Block, ~20–40 KB Flash insgesamt. Aufwand: ~1d (Tool-Setup + 7 Größen rasterisieren).
  - **Option C — Hybrid**: U8g2 für Datenzeilen (Größenvielfalt vorhanden), Custom für FullscreenError-Glyphen (90 px existiert in keiner Standard-Lib).
  - **Empfehlung Option A** für die erste v2-Iteration, weil sie die Memory-Regel `feedback-no-tooling-rabbit-holes` respektiert. Pixel-Treue ist Cons in §3.2; wenn Sichtkontrolle (Schritt 12) den Unterschied unzumutbar findet, kommt Option B als Follow-up.
  - Entscheidung in [Anhang C C9](#anhang-c--open-questions-vor-schritt-0) festhalten.
- **0.7** **Glyph-Substitution (Festlegung 2026-05-19: Custom-Glyph-Assets).** `◌` (90 px, Boot-Screen) und `§9` (90 px, Auth-Screen) als eigene 1-bit-Sprites in `render/bitmap_fonts.cpp` (~1 KB pro Glyph: 90×90 px = 8100 Bits ≈ 1013 B; drei Glyphen ≈ 3 KB PROGMEM). Format: `static const uint8_t GLYPH_DOTTED_CIRCLE_90[]` etc.

  **Sprite-Generierung in zwei Stufen** (nicht händisch pixeln — 8100 Bits sind zu fehleranfällig; bewusst keine Python-Bibliothek-Dep, konsistent mit P.1 „stdlib-only"):

  1. **PNG → PBM** mit ImageMagick (üblicherweise systemweit verfügbar; falls nicht, vom User via Paket-Manager installieren — `check-tool`-Skill prüft):

     ```text
     convert docs/design_handoff_display/glyph-source/dotted-circle-90.png \
             -threshold 50% -depth 1 -compress none .tmp/glyph/dc.pbm
     ```

  2. **PBM → PROGMEM-Array** mit `scripts/pbm-to-progmem.py` (Python-stdlib-only — PBM-P1-Format ist ASCII, `open(...).read()` + split reicht):

     ```text
     scripts/pbm-to-progmem.py .tmp/glyph/dc.pbm --name GLYPH_DOTTED_CIRCLE_90 \
                               > src/render/glyph_dotted_circle_90.h
     ```

  Source-PNGs unter `docs/design_handoff_display/glyph-source/` einchecken (kleine Files, < 5 KB pro 90×90-Bitmap). Generierte `.h`-Files mit-committen — Build-Reproduzierbarkeit ohne ImageMagick/Python-Toolchain auf der Build-Maschine. Skript bleibt nach v2 im Repo als Pflege-Tool. Wenn ImageMagick auf dem Auftraggeber-System fehlt, ist GIMP-Export als PBM (`Datei → Exportieren als → .pbm`) eine ein-Klick-Alternative — die zweite Stufe (Python) bleibt gleich.

  Render-API: `drawCustomGlyph(canvas, x, y, GLYPH_*, 90)`. `o`/`S9`-Fallback ist nicht mehr Plan, kann aber als Default in `display_state.cpp` für unbekannte States bleiben.
- **0.8** **Direction-Text-Quelle festschreiben.** Statisch in `data/stream_labels.h::display_dir()` (siehe [§2.2](#22-neue-module-im-detail)). Werte: 58A→Atz = `"Atzgers."`, 58A→Hie = `"Hietzing"`, 58B→Atz = `"Atzgers."`. S-Bahn-Stream hat keinen Direction-Text in der Zeile (Richtung im Header). Annahme [Schritt 0.8]: nicht aus `towards`, weil OGD-Strings zu lang. Wenn der Auftraggeber das später konfigurierbar haben möchte → separate PR.

**Aufwand**: 1–1.5 d, abhängig davon wie schnell die ÖBB-Webapp im DevTools-Mitschnitt kooperiert und wie viel Font-Konfiguration in Option A nötig ist.

**Validation**: Drei Antwort-Fixtures unter `.tmp/hafas-fixtures/`, `docs/v2-sbahn-migration-plan.md` Anhang A mit konkreten Werten überschrieben, Render-Stack-Entscheidung in §4.1 [Schritt 0.6] festgehalten.

**Reversibel**: trivial — Pre-Flash, kein Code-Eingriff (außer ggf. `platformio.ini` Library-Listing für Option A; das ist 1 Zeile).

#### Schritt 1 — `INetwork::httpPost` + `Esp32Network::httpPost`

- [ ] erledigt

- **1.1** [src/hal/INetwork.h](../src/hal/INetwork.h):
  - `HttpResult`-Struct einführen (`{bool ok; int http_status;}`).
  - `httpPost(url, body, content_type, out) -> HttpResult` hinzufügen (rein virtuell).
  - `httpGet`/`httpGetStream` von `bool`-Rückgabe auf `HttpResult` umstellen — in derselben Edit-Session, damit keine zwei Konventionen nebeneinander leben. Caller (`api_fetcher`, `schedule_fetcher`) entsprechend angleichen.
  - Native-Tests müssen ab jetzt `HttpResult` zurückgeben — `FakeNet`-Klassen in `test/test_native_runtime/` und in den jeweiligen Test-Buckets ergänzen (Standardwert für Tests, die den Status nicht prüfen: `{true, 200}`).
- **1.2** [src/hal/Esp32Network.{h,cpp}](../src/hal/Esp32Network.h): `httpPost`-Impl analog zum bestehenden `httpGet`. Wieder `WiFiClientSecure` mit `setInsecure()` (oder Cert aus Schritt 0.4); `HTTPClient::POST(body)` statt `GET()`. Response-Body wie heute in `out` einsammeln; `HttpResult.http_status = http.getCode()`. Content-Type Default `application/json`. `httpGet`/`httpGetStream` parallel auf `HttpResult` umstellen.
- **1.3** Host-Test `test_native_api_fetcher`: existiert schon für GET → eine zweite Test-Funktion für POST mit `FakeNet`, der canned Response liefert. Verifiziert Retry-Logik (`fetchWithRetry`-Wrapper kommt in Schritt 5 dazu, hier nur die Direct-Call-Variante). Zusätzlich: ein Test, der `FakeNet` mit `HttpResult{true, 401}` antworten lässt und prüft, dass der Caller den Status sieht (Vorbereitung für OGD-Auth-Tripwire in Schritt 5).

**Aufwand**: 0.5 d.

**Validation**: `make test` grün; `make build` für `env:esp32dev` grün; ein temporärer Smoke-Test gegen die ÖBB-API kann manuell mit `curl --data-binary @body.json https://fahrplan.oebb.at/bin/mgate.exe` cross-validiert werden (Smoke-Test nicht ins Repo).

**Reversibel**: trivial — POST-Methode entfernen, Native-Mocks zurückbauen.

#### Schritt 2 — Datenmodell-Änderungen + RTC-MAGIC-Bumps

- [ ] erledigt

Atomarer Schritt: Stream-Enum + `Departure::line_label` + `Departure::live()` + `display_dir()` + beide MAGICs zusammen, damit zwischen den Sub-Steps kein inkonsistenter Zustand committet wird.

- **2.1** [src/data/StreamSnapshot.h:11-18](../src/data/StreamSnapshot.h#L11-L18): U1-Werte raus, `STREAM_SBAHN_HBF = 3` rein, `STREAM_COUNT = 4`. Kein Aliasing der alten Werte (würde Test-Drift maskieren).
- **2.2** [src/data/Departure.h](../src/data/Departure.h): `char line_label[6] = ""` ergänzen; `inline bool live() const { return source == DepartureSource::Realtime; }` als Accessor (Renderer-Convenience, Plan und Hint sind beide „nicht-live" für den Plan-Marker); `operator==` um `strcmp(line_label, …) == 0` erweitern. `<cstring>`-Include hinzu.
- **2.3** [src/data/stream_labels.h](../src/data/stream_labels.h): die zwei `U1-…`-cases entfernen, ein neuer case `STREAM_SBAHN_HBF: return "SBahn-Hbf";`. Zusätzlich `display_dir(int idx)` nach [§2.2](#22-neue-module-im-detail) einsetzen (statische Tabelle, max 8 Zeichen). Entscheidung aus Schritt 0.8.
- **2.4** [src/config.h:13-14, 29, 39-40, 47, 55-56](../src/config.h#L13): **alle** U1-bezogenen Konstanten entfernen, inklusive `LINE_U1`. Wenn Test-Fixtures noch `LINE_U1` referenzieren, wandern die in Schritt 8 mit; Build wird zwischenzeitlich rot, ist nach Schritt 8 wieder grün. Begründung: wir migrieren in einem Rutsch — kein separater Cleanup-Commit für eine einzelne Konstante.
- **2.5** Neue Konstanten aus [§2.4](#24-konfiguration) in `config.h` einsetzen. Werte aus Schritt 0.1/0.2 (HAFAS) und §2.4 (Display-State-Schwellwerte + `DISPLAY_VERSION_STR`).
- **2.6** **`PersistedMeta`-Struktur erweitern** ([src/logic/persisted_meta.h](../src/logic/persisted_meta.h) bzw. wo die Struct heute definiert ist; vor Edit zu prüfen: `grep -rn "PersistedMeta" src/`). Neue Felder, die der State-Selector aus Schritt 7.2 verlangt:

  ```cpp
  struct PersistedMeta {
    // … bestehende Felder …
    bool   has_any_data     = false;  // false bis zum ersten erfolgreichen Fetch (→ Boot-State)
    time_t last_success_at  = 0;      // letzter erfolgreicher End-to-End-Fetch (→ Stale/Offline)
    bool   auth_error_seen  = false;  // HAFAS err="AID"/"AUTH" gesehen, bis nächster OK-Parse (→ Auth)
    uint8_t ogd_auth_streak = 0;      // aufeinanderfolgende OGD-HTTP-401/403 (≥3 → auth_error_seen=true)
  };
  ```

  RTC-Footprint: +12–16 B (bool je 1 B, time_t je 4 B auf ESP32, uint8_t 1 B; je nach Alignment-Padding). Bilanz in [Anhang B](#anhang-b--rtc-bilanz-nach-v2) entsprechend anpassen.

  Schritt 2.7 bumpt die MAGIC ohnehin → kein eigenes `META_MAGIC` nötig, sofern `PersistedMeta` heute denselben MAGIC-Block teilt. Falls separat: viertes MAGIC zur Liste in 2.7 hinzufügen (vor Edit prüfen).

  Reset-Pflichten:
  - `auth_error_seen` wird gelöscht durch jeden erfolgreichen `parseOebbStationBoard` mit `result.endpoint_responded == true`.
  - `ogd_auth_streak` wird auf 0 zurückgesetzt durch jeden OGD-Call mit `http_status == 200`.
- **2.7** [src/hal/Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp): **alle drei** RTC-MAGICs bumpen, weil drei unabhängige Persistierungs-Strukturen alle inkompatibel werden:
  - **`MAGIC` (RLE-Frame)** — `Departure::line_label` ändert das gerenderte Frame-Layout *und* der Display-Rewrite produziert grundlegend andere Pixel. Reuse würde beim ersten Wake einen Müll-Frame zeigen, bis Light-Full den Schirm überschreibt.
  - **`SCHED_MAGIC` (Schedule-Hints)** — `ScheduleHint` ist strukturell identisch, aber Index 3/4 enthielt vorher U1-Schedules; Reuse würde falsche Hint-Werte für den S-Bahn-Index injizieren.
  - **`SNAP_MAGIC` (Snapshot)** — `Departure::line_label` ändert die Bytegröße von `StreamData::slot[]`; alter Snapshot würde mit `memcpy` falsch decodiert.

  Falls heute nur `MAGIC` und `SCHED_MAGIC` existieren und der Snapshot kein eigenes Magic hat (gleiche MAGIC-Quelle wie das Frame), reicht ein einzelner Bump — aber dann muss verifiziert werden, dass beide Strukturen ein gemeinsames MAGIC tragen. Vor Edit zu prüfen: `grep -n "MAGIC" src/hal/Esp32PersistentStore.cpp` + `grep -rn "STREAM_SNAP_MAGIC\|SNAP_MAGIC" src/`.

  `test_device_persistent` muss alle drei Strukturen nach Bump als „invalid" zurückweisen und Frischbefüllung erzwingen.

**Aufwand**: 0.5 d.

**Validation**: `make ci` grün (vermutlich rot — viele Tests erwarten `STREAM_COUNT == 5`). Die Roten sind in Schritt 8 zu fixen; vor Schritt 3 muss `make build` für `env:esp32dev` und der reine Compile-Pfad von `env:native` grün sein. Wenn Touch-Sites in `test/`-Files identifiziert werden, in Schritt 8 abarbeiten.

**Reversibel**: ja, aber MAGIC-Bumps haben Hardware-Konsequenzen (gespeicherte Frames + Hints werden verworfen). Wenn rollback, dann zusätzlich `MAGIC` nochmal bumpen, damit das neuere Layout nicht von der alten Firmware fehlinterpretiert wird.

#### Schritt 3 — `data/oebb_hafas_parse.{h,cpp}` + Tests

- [ ] erledigt

- **3.1** [src/data/oebb_hafas_parse.h](../src/data/oebb_hafas_parse.h) anlegen mit Signaturen aus [§2.2](#22-neue-module-im-detail) (`OebbStreamFilter`, `buildOebbRequest`, `parseOebbStationBoard`).
- **3.2** [src/data/oebb_hafas_parse.cpp](../src/data/oebb_hafas_parse.cpp): `buildOebbRequest` als String-Bau mit `ArduinoJson::serializeJson` über einen `JsonDocument`. Body-Schema aus [Anhang A](#anhang-a--hafas-request-response-vertrag).
- **3.3** `parseOebbStationBoard`: `deserializeJson` mit `NestingLimit(20)` (HAFAS verschachtelt `prodL`/`opL`/`himL` tief). Reihenfolge:
  1. `doc["err"]` lesen.
     - `"OK"` → weiter mit Schritt 2.
     - `"AID"` oder `"AUTH"` → `result.endpoint_responded = false`, `result.auth_error_seen = true`, return true (Auth-Pfad triggert Display-State, [§2.2 State-Selector](#22-neue-module-im-detail)).
     - andere err-Codes (`"FAIL"`, `"PROBLEMS"`, …) → `result.endpoint_responded = false`, return true.
  2. `doc["svcResL"][0]["res"]["jnyL"]` als Array — wenn null → `result.endpoint_responded = true`, `result.filter_matched = false`, return true.
  3. Pro `jny`: `jny["stbStop"]["dCncl"]` → Cancelled skippen; `jny["stbStop"]["dTimeS"]` + `["dDateS"]` lesen, optional `dTimeR` + `dDateR`. Konvertieren via Howard-Hinnant + `TZ_INFO` (analog [src/data/wienerlinien_parse.cpp:32-79](../src/data/wienerlinien_parse.cpp#L32)). HAFAS liefert Zeiten in **Europe/Vienna lokal ohne TZ-Suffix**; die DST-Brüche werden serverseitig bereits aufgelöst, lokale Zeit → epoch-Konvertierung erfolgt über `TZ_INFO` ohne weitere Anpassung.
  4. `jny["prodL"][0]` als Index in `doc["svcResL"][0]["res"]["common"]["prodL"]`, daraus `nameS` lesen, **alle Whitespaces strippen**, in `line_label` schreiben (mit `strncpy`, null-terminieren, abkürzen zu `"xx"` wenn länger als 5). Pre-Phase 2026-05-19 zeigt: `nameS = "S 1"` → `line_label = "S1"`; `nameS = "REX 1"` → `"REX1"`. Das Feld `name` enthält die Webapp-Display-Form mit angehängter Zugnummer (`"S 1 (Zug-Nr. 28842)"`) und wird nicht ausgewertet.
  5. Slot füllen, bis `SLOTS_PER_STREAM` erreicht.
  6. `result.endpoint_responded = true`, `result.filter_matched = (matched_count > 0)`, `result.auth_error_seen = false`.

  SEV-Busse, die HAFAS bei Schienenersatzverkehr anzeigt, werden **nicht** clientseitig gefiltert — sie sind Plan-Ersatz für die ausgefallene S-Bahn auf derselben Strecke (Atzgersdorf → Hbf) und damit gewollte Anzeige. Der `line_label` zeigt dann z. B. `"SEV1"` oder ähnliches statt `"S2"`.
- **3.4** Fixtures: `.tmp/hafas-fixtures/*.json` aus Schritt 0.3 nach `test/test_native_oebb_hafas_parse/fixtures/oebb_live_{morning,noon,evening}.h` konvertieren (raw-string-literal-Embed analog zu [test/test_native_wienerlinien_parse/](../test/test_native_wienerlinien_parse/)).
- **3.5** Tests in `test/test_native_oebb_hafas_parse/test_main.cpp`:
  - `test_buildRequest_includes_aid_and_client`
  - `test_buildRequest_includes_stbLoc_dirLoc`
  - `test_buildRequest_includes_products_filter`
  - `test_parse_morning_fixture_three_slots_realtime`
  - `test_parse_noon_fixture_line_labels_S2_S3`
  - `test_parse_evening_fixture_includes_REX`
  - `test_parse_cancelled_skipped`
  - `test_parse_err_aid_sets_auth_error_seen`           // err="AID"  → State-Selector → Auth-Screen
  - `test_parse_err_auth_sets_auth_error_seen`          // err="AUTH" → State-Selector → Auth-Screen
  - `test_parse_err_fail_sets_endpoint_not_responded`   // err="FAIL" → Stale/Offline-Pfad, KEIN Auth
  - `test_parse_empty_jnyL_sets_filter_unmatched`
  - `test_parse_long_line_label_abbreviates_xx`

**Aufwand**: 1–1.5 d (Parser plus Tests).

**Validation**: `make test` (Native) grün — neuer Test-Bucket ohne Device-Abhängigkeit, läuft im `env:native`.

**Reversibel**: ja, isoliert.

#### Schritt 4 — `filter_builder.{h,cpp}` umstellen + S-Bahn-Filter-Getter

- [ ] erledigt

- **4.1** [src/logic/filter_builder.cpp](../src/logic/filter_builder.cpp): U1-Zeilen in `buildStreamFilters` und `buildScheduleFilters` entfernen. `f[STREAM_SBAHN_HBF]` bleibt default-konstruiert (`rbl = 0` bzw. `diva = 0`).
- **4.2** [src/logic/filter_builder.h](../src/logic/filter_builder.h): `OebbStreamFilter buildOebbFilter();` hinzufügen.
- **4.3** Implementierung in `filter_builder.cpp`:

  ```cpp
  OebbStreamFilter buildOebbFilter() {
    OebbStreamFilter f;
    f.stbloc_extid = OEBB_STBLOC_EXTID;
    f.dirloc_extid = OEBB_DIRLOC_EXTID;
    f.products = OEBB_JNYFLTR_PRODUCTS;
    f.max_jny = OEBB_MAX_JNY;
    return f;
  }
  ```

- **4.4** Test `test_native_filter_builder` erweitern:
  - alte U1-Erwartungen entfernen
  - Assert: `f[STREAM_SBAHN_HBF].rbl == 0` für OGD-Filter (default)
  - Assert: `f[STREAM_SBAHN_HBF].diva == 0` für Schedule-Filter (default)
  - Neuer Test: `buildOebbFilter()` liefert die ÖBB-Konstanten

**Aufwand**: 0.5 d.

**Validation**: `make test` grün (zumindest dieser Bucket).

**Reversibel**: ja.

#### Schritt 5 — `snapshot_fetcher.cpp` um `fetchOebbStream` erweitern

- [ ] erledigt

- **5.1** [src/logic/snapshot_fetcher.cpp:16-19](../src/logic/snapshot_fetcher.cpp#L16-L19): `FETCH_ORDER` reduzieren auf die 3 OGD-Streams. Index `STREAM_SBAHN_HBF = 3` taucht hier nicht auf.
- **5.2** Neue interne Funktion `fetchOebbStream(net, mgate_url, filter, out, summary, meta)`:
  - `buildOebbRequest(filter)` → POST-Body.
  - `fetchWithRetry`-ähnlicher Wrapper mit POST (Erweiterung in `api_fetcher` als `fetchPostWithRetry`, siehe 5.6 für Retry-Policy gegen `HttpResult.http_status`).
  - Erfolgreiche Antwort → `parseOebbStationBoard(body, out.stream[STREAM_SBAHN_HBF], parse_result)`.
  - **Result-Propagation aus dem Parser** (kritisch — sonst sieht der Snapshot-Logger / State-Selector die Werte nicht):
    1. `out.stream[STREAM_SBAHN_HBF].endpoint_responded = parse_result.endpoint_responded;`
    2. `out.stream[STREAM_SBAHN_HBF].filter_matched     = parse_result.filter_matched;`
    3. `meta.auth_error_seen = meta.auth_error_seen || parse_result.auth_error_seen;` (Sticky bis nächster OK-Parse)
    4. Bei `parse_result.endpoint_responded == true`: `meta.auth_error_seen = false;` (Reset — wenn HAFAS wieder normal antwortet, ist der Auth-Drift weg).
  - `summary.total_batches++`; bei Fail `summary.failed_batches++`.
  - Logging-Konvention analog OGD-Batch: `[api] oebb httpPost failed after %d attempts`, `[api] oebb succeeded on attempt %d/%d`, zusätzlich `[api] oebb auth_error_seen=1` wenn der Parser das Flag setzt.

  `StreamData::{endpoint_responded, filter_matched}` bleiben die kanonische Quelle für Snapshot-Logger und Filter-Health-Tracking; `OebbParseResult` ist nur das Transport-Vehikel von Parser nach Caller. Daher die explizite Kopie.
- **5.3** Public `fetchSnapshot` ruft nach der OGD-Schleife einmal `fetchOebbStream(net, oebb_mgate_url, …, meta)`. Signatur erweitern: `fetchSnapshot(INetwork&, const std::string &ogd_base, const std::string &mgate_url, const StreamFilter (&)[STREAM_COUNT], const OebbStreamFilter &oebb_filter, StreamSnapshot &out, FetchSummary &summary, PersistedMeta &meta)`. Innerhalb der OGD-Schleife wird `meta.ogd_auth_streak` gepflegt: HTTP-401/403 → `++`, HTTP-200 → `= 0`. Bei `streak ≥ 3` setzt der Caller `meta.auth_error_seen = true`.
- **5.4** Caller `cycle_runner` anpassen:
  - **`CycleConfig`** bekommt ein neues Feld `std::string mgate_url`, in [main.cpp::makeCycleConfig](../src/main.cpp#L31) auf `OEBB_MGATE_URL` gesetzt.
  - **`PersistedMeta &meta`-Argument** für `fetchSnapshot` kommt aus dem `cycle_runner`-State — `PersistedMeta` lebt heute schon im warmen Cycle-State (geladen aus RTC am Wake-Anfang, zurückgeschrieben vor Sleep). `cycle_runner` reicht eine Referenz an `fetchSnapshot` durch, ohne dass `CycleConfig` selbst die Struktur halten muss. Vor Edit zu prüfen: `grep -n "PersistedMeta" src/logic/cycle_runner.cpp` — wenn `meta` heute schon in der Warm-Cycle-Funktion sichtbar ist, ist der Eingriff minimal.
  - **`last_success_at`-Update**: nach erfolgreichem `fetchSnapshot` (mindestens ein Stream mit `endpoint_responded && filter_matched`) setzt `cycle_runner` `meta.last_success_at = clock.now()`. Das ist die einzige Zustands-Mutation, die `cycle_runner` an `PersistedMeta` macht — alle anderen Felder werden im Fetch-Pfad (5.2) gesetzt.
  - **Cold-Boot-Pre-Render (Splash vor Fetch).** Bei leerem Snapshot (Power-On, Firmware-Update mit MAGIC-Bump, oder persistierter Auth-Fehler) zeichnet `cycle_runner` einen Pre-Fetch-Screen **vor** dem WiFi-Connect — der Display-Pfad braucht nur SPI, keine Netzwerk-Verbindung. Neuer Cold-Boot-Flow:

    ```cpp
    void runColdCycle(…) {
      PersistedMeta meta = loadMeta();           // RTC oder Default nach MAGIC-Bump
      StreamSnapshot emptySnap{};
      ScheduleSnapshot emptySched{};
      SelectorSignals preSig{ .first_render_ever = !meta.has_any_data,
                              .auth_error_seen   = meta.auth_error_seen,
                              .wifi_up           = false,
                              .now               = clock.now(),
                              .last_success      = meta.last_success_at };
      DisplayState pre = selectDisplayState(emptySnap, emptySched, meta, preSig);
      if (pre == DisplayState::Boot || pre == DisplayState::Auth) {
        RenderInput in = composeRenderInput(pre, emptySnap, emptySched, meta);
        renderFrame(in, fb);
        display.updateFull(fb);                  // ~1.5–3 s — Splash/Auth-Screen sichtbar.
                                                 // Light-Full setzt zugleich die Baseline,
                                                 // an die das Content-Update darunter
                                                 // partial-diffen kann (RTC-Frame nach
                                                 // MAGIC-Bump leer → Full nötig).
      }
      wifiConnect();
      fetchSnapshot(net, …, meta);
      SelectorSignals postSig{ .first_render_ever = !meta.has_any_data,
                               .auth_error_seen   = meta.auth_error_seen,
                               .wifi_up           = wifi.isConnected(),
                               .now               = clock.now(),
                               .last_success      = meta.last_success_at };
      DisplayState s = selectDisplayState(snap, sched, meta, postSig);
      renderFrame(composeRenderInput(s, snap, sched, meta), fb);
      display.updatePartial(fb);                 // zweiter Refresh: Content
      saveMeta(meta);                            // has_any_data ggf. jetzt true
    }
    ```

    Warm-Cycles (normales Wake, RTC voll, `has_any_data && !auth_error_seen`) überspringen den Pre-Render — der Selector liefert vor dem Fetch `Normal`/`Stale`/`Quiet` mit leerem Snapshot, was beide nicht zur Anzeige eignet, also bleibt der Display-Update auf das Post-Fetch-Render beschränkt. Der `if`-Wächter `pre ∈ {Boot, Auth}` filtert das sauber.

    `composeRenderInput` ist dieselbe Helper-Funktion, die schon vor v2 den `RenderInput` baut — sie wird in Schritt 7.1 ohnehin erweitert (zusätzliche Felder), kein neuer Helper nötig. Bei `state == Boot` füllt sie aus den Defaults (`DISPLAY_VERSION_STR` für Foot), bei `state == Auth` aus `meta.auth_error_seen` + persistiertem AID-Short / HTTP-Code.
- **5.5** Test `test_native_snapshot_fetcher` erweitern: `FakeNet` mit zwei kanonischen Antworten (OGD-JSON für die Bus-Streams, HAFAS-JSON für den S-Bahn-Stream). Assertion: nach `fetchSnapshot` ist `out.stream[0..2]` aus OGD befüllt, `out.stream[3]` aus HAFAS, `summary.total_batches == 2` (1 OGD-Batch bei 3 Streams + 1 OEBB-Call).
- **5.6** [src/logic/api_fetcher.{h,cpp}](../src/logic/api_fetcher.h): `fetchPostWithRetry(INetwork &, url, body, content_type, out, FetchConfig) → HttpResult` als zweite Funktion. `fetchWithRetry` (GET) bleibt parallel, beide auf `HttpResult`-Rückgabe umgestellt (siehe Schritt 1).

  **Retry-Policy gegen `HttpResult.http_status`** (gilt für beide Methoden, damit GET/POST konsistent sind):

  | `http_status` | Retry? | Begründung |
  |---|---|---|
  | `0` (Transport-Error / Timeout) | ja, bis `max_attempts` | echte Netzwerk-Pannen sind oft transient |
  | `200`–`299` | nein (Erfolg) | Body weiterreichen |
  | `401`, `403` | **nein** (Auth-Drift) | Retry würde nur den Streak-Zähler verfälschen; sofort an Caller, der den Tripwire pflegt |
  | `408`, `429`, `5xx` | ja, exponentielles Backoff | Server-Last / Rate-Limit |
  | `4xx` (übrige) | nein | Client-Fehler — eigener Bug, kein Retry-Use-Case |

  Auth-relevante Codes (`401`/`403`) gehen also **nach dem ersten Treffer** mit `HttpResult{true, 401}` zurück; der Caller (in 5.3) erhöht `meta.ogd_auth_streak`. Drei Wake-Cycles mit 401-in-Folge ergeben dann den Auth-State. Tests in `test_native_api_fetcher` decken alle Tabellen-Zeilen ab.

  **Return-Vertrag nach erschöpften Retries** (gilt für GET und POST gleich):

  - Reine Transport-Fails (`http_status == 0`) bis `max_attempts` → Rückgabe `HttpResult{false, 0}`. `out` ist leer.
  - Server-Errors (`429`/`5xx`) bis `max_attempts` → Rückgabe `HttpResult{true, last_seen_status}` (z.B. `{true, 503}`). `out` enthält den letzten Body, auch wenn er ein Error-Body ist.
  - Sofort-Returns (Auth-Codes, übrige `4xx`, Erfolge) liefern `HttpResult{true, http_status}` mit dem entsprechenden Body in `out`.

  Caller-Konvention: `ok == false` ist *immer* Transport-Fail, `ok == true` heißt „HTTP-Pipeline ging durch, semantischer Erfolg muss via `http_status` geprüft werden".

**Aufwand**: 0.5–1 d.

**Validation**: `make test` grün; `make build` für `env:esp32dev` grün; `test_device_fetch` muss erweitert werden, um die zwei-Endpunkt-Welt zu testen (kommt in Schritt 8).

**Reversibel**: ja, `fetchOebbStream` ist ein zusätzlicher Aufruf am Ende; entfernen → Stream 3 bleibt leer, Display zeigt `--:--`.

#### Schritt 6 — `schedule_fetcher` skippt den S-Bahn-Stream

- [ ] erledigt

Trivial — `schedule_fetcher::fetchSchedule` iteriert heute schon über distinct DIVAs aus `filters[]`. Mit `filters[STREAM_SBAHN_HBF].diva == 0` (Default aus Schritt 4) wird kein Call für diesen Stream gemacht.

- **6.1** Bestätigen durch Lesen: [src/logic/schedule_fetcher.cpp](../src/logic/schedule_fetcher.cpp) — der DIVA-Distinct-Loop muss `diva == 0` als „skip" behandeln. Wenn nicht (heute wird das geprüft, aber double-check): Guard hinzufügen.
- **6.2** Test `test_native_schedule_fetcher` erweitern: Filter mit `diva == 0` an Index 3 — Erwartung: `result.calls_attempted == 1` (nur einer von beiden DIVAs aus den Bus-Streams, da Tullnertalgasse 58A→Atz und 58A→Hie sich die DIVA teilen, bleibt es bei 2 distinct DIVAs für Tullnertalgasse + Endemann).

**Aufwand**: 0.25 d.

**Validation**: `make test` grün.

#### Schritt 7 — Display-Redesign + State-Selector

- [ ] erledigt

Substantieller Schritt — fünf neue Sub-Module in `render/`, ersetzte `render/layout.cpp`, neuer State-Selector in `logic/render_input.cpp`. Reihenfolge der Sub-Steps bewusst hierarchisch (Fonts vor Primitiven vor Komposition), damit jeder Sub-Schritt eigenständig grün lassbar ist.

- **7.1** **State-Modell umstellen.** [src/render/layout.h](../src/render/layout.h): `OverlayKind`-Enum wird zu `DisplayState {Boot, Normal, Stale, Night, Quiet, Offline, Auth}`. `RenderInput` erweitert um:
  - `time_t last_fetch_at` (für Offline-Foot „Letzte Aktualisierung HH:MM")
  - `int retry_in_s` (für Offline-Foot „Retry in Xs")
  - `char auth_aid_short[10]` (erste 8 Zeichen der AID + `\0`, für Auth-Foot „AID 0x…")
  - `int auth_http_code` (zuletzt gesehener Auth-relevanter HTTP-Status — 200 wenn Auth über Parser-Flag kam, 401/403 wenn HTTP-Tripwire)
  - `const char *firmware_version` (Pointer auf `DISPLAY_VERSION_STR`-Konstante)

  **Noch kein Renderer-Touch** — diese Sub-Schritt-Datei kompiliert, alte `renderFrame`-Signatur wird kurz `RenderInput`-incompatible, deshalb Sub-Step 7.2 parallel.

- **7.2** **State-Selector in `logic/render_input.cpp`.** Heute leitet `composeRenderInput` `OverlayKind` aus drei Bedingungen ab. Neue Funktion `selectDisplayState(snap, schedule, meta, sig) → DisplayState` mit `SelectorSignals` nach [§2.2](#22-neue-module-im-detail). Pure function. Aufrufer (`cycle_runner`) baut `SelectorSignals` zusammen aus:
  - `sig.first_render_ever = !meta.has_any_data`
  - `sig.auth_error_seen` aus `OebbParseResult.auth_error_seen` der letzten Antwort (persistiert in `PersistedMeta`); ODER drei aufeinanderfolgende OGD-Calls mit `HttpResult.http_status ∈ {401,403}` (Counter in `PersistedMeta::ogd_auth_streak`)
  - `sig.wifi_up` aus aktueller WiFi-Connection
  - `sig.now` aus `IClock::now()`
  - `sig.last_success` aus `PersistedMeta::last_success_at`

  Helper `allDeparturesBeyond`, `outsideServiceWindow`, `nextDepartureFarAway` werden als kleine pure Functions in derselben Datei definiert (Signatur in [§2.2](#22-neue-module-im-detail)). Host-Test: alle 7 States durch Konstruktion erreichen + jeden Helper isoliert testen.

- **7.3** **`render/bitmap_fonts.{h,cpp}`.** Je nach Entscheidung in Schritt 0.6:
  - Option A: `#include <U8g2_for_Adafruit_GFX.h>` + Mapping-Tabelle `fontFor(FontRole)` mit den 7 logischen Rollen (TG-Row, EG-Row, Atzg-Row, Network-Label, Section-Header-TG, Section-Header-EG-Atzg, FullscreenError-Glyph etc.).
  - Option B: `PROGMEM`-Arrays mit `GFXfont`-Strukturen, generiert via `fontconvert` aus VT323.ttf + Silkscreen.ttf. Source-Generator-Skript unter `scripts/build-bitmap-fonts.sh` einchecken, aber Output `bitmap_fonts_data.h` committen (Build-Reproduzierbarkeit ohne TTF-Files im Repo).
  - Public API: `setRoleFont(canvas, FontRole)` — abstrahiert über beide Optionen.

- **7.4** **`render/badge.{h,cpp}`.** `drawBadge(canvas, x, y, text, BadgeSize)`. Drei Größen mit fixen Paddings aus [display.jsx Z. 158-160](design_handoff_display/display.jsx). Renderlogik: `fillRect` mit ink-Farbe (Hintergrund ist paper im invertierten Display, Badge muss visuell ein „Inset" sein → Rechteck mit *paper* gefüllt, Text in *ink*), dann Text mittels `setRoleFont(BADGE_sm/md/lg)` darauf. Return-Wert: rechte X-Kante des gezeichneten Badges, damit der Caller den nächsten Element-X-Offset kennt. Host-testbar über `FrameBuffer<>`-Inspektion (Pixel-genaue Asserts auf Badge-Bounding-Box).

- **7.5** **`render/plan_marker.{h,cpp}`.** `drawPlanMark(canvas, x, y)` zeichnet ein 5×5-px Hohlquadrat mit 1-px-Stroke (vier `drawLine`-Aufrufe oder ein `drawRect`). Nicht aufgerufen wenn `Departure::valid == false`. Host-test: 5×5-Pixel-Mask gegen Erwartung.

- **7.6** **`render/network_plan.{h,cpp}`.** `drawNetworkPlan(canvas, x, y, width)`. Implementiert das fünfspaltige Schema:
  1. Top-Row: 4×4-Dot (Hbf) + 1-px-Linie + 7×7-Diamond (Atzg).
  2. Vertikale 1-px-Linie zwischen den beiden Atzg-Markern (am Center der Atzg-Spalte).
  3. ▼-Glyph über der Tull-Spalte als **5×5-Custom-Triangle-Sprite** (Festlegung C12, siehe Anhang C). Daten in `network_plan.cpp` als `static const uint8_t TRIANGLE_DOWN_5[]`, gerendert via `drawCustomGlyph`-Helper aus Schritt 0.7.
  4. Bottom-Row: 7×7-Diamond (Atzg) + Linie + 4×4-Dot (Ende) + 8×8-Big (Tull, „you are here") + 4×4-Dot (Hietz).
  5. Labels-Row mit 7-px Silkscreen, Tull + Atzg fett.
  Bilder/Maße strikt aus [docs/design_handoff_display/README.md „Network plan"](design_handoff_display/README.md). Host-test: zentral pixel-stamp asserts an drei Stellen (Diamond-Mitte, Big-Mitte, vertikale Linie).

- **7.7** **`render/display_state.{h,cpp}` (Fullscreen-Renderer).** Vier Funktionen:
  - `drawBoot(canvas, version_str)` — `◌` (90 px, oder Substitut aus Schritt 0.7) + „bustaferl" (18 px) + „lädt Fahrplan…" (16 px) + Foot (8 px) mit `DISPLAY_VERSION_STR`.
  - `drawOffline(canvas, last_fetch_at, retry_in_s)` — `!` (90 px) + „Kein Empfang" + Sub mit „Letzte Aktualisierung HH:MM" (formatiert) + Foot mit „WLAN · Retry in Xs".
  - `drawAuth(canvas, aid_short, http_code)` — `§9` als Custom-Glyph (aus Schritt 0.7, 90 px) + „Auth-Fehler" + „Client-ID veraltet · bitte neu registrieren" + Foot „AID 0x… · ERR XXX".
  - `drawQuiet(canvas)` — `—` (72 px) + „Keine Abfahrten" (14 px Silkscreen) + „in den nächsten 20 min" (18 px VT323).
  Jede Funktion clear()ed nicht selbst — der Caller (`renderFrame`) tut das einmalig.

- **7.8** **`render/layout.cpp` neu — `renderFrame` als State-Dispatcher + `drawBoard`.**
  - `renderFrame(in, fb)` clear()ed Frame, dispatched per `switch (in.state)`. Boot/Offline/Auth/Quiet → eine der vier `draw*`-Funktionen aus 7.7. Stale/Night/Normal → `drawBoard(canvas, in)`.
  - `drawBoard(canvas, in)` baut die drei Daten-Blöcke (TG/EG/Atzg) + Netzplan nach Geometrie aus [§2.2](#22-neue-module-im-detail):
    - TG: `drawSectionHeader("TULLNERTALGASSE", 12 px)` + zwei TG-Reihen mit `drawBadge(lg)` + Direction-Text + Plan-Marker für jede nicht-live Zeit.
    - 2-px Trennlinie y=98.
    - EG: `drawSectionHeader("ENDEMANNGASSE · NACH SCHLEIFE", 10 px)` + EG-Reihe mit `drawBadge(md)`.
    - 1-px Trennlinie y=150.
    - Atzg: `drawSectionHeader("ATZGERSDORF → WIEN HBF", 10 px)` + drei S-Bahn-Slots horizontal mit `drawBadge(sm)` + Plan-Marker. **Aktuell sind nur 2 Slots datenseitig befüllt** (Variante A); der dritte bleibt leer (`--:--` ohne Plan-Marker). Layout reserviert die Spalte, wenn nur 2 Slots vorhanden, bleibt sie sichtbar leer.
    - Netzplan unten: `drawNetworkPlan(canvas, 18, 232, 364)` (oder y-Wert nach Höhe des Atzg-Blocks anpassen).
  - `drawBoard` für State `Stale` erzwingt `valid = false` per Pre-Pass auf den Slots, damit `--:--` rendert; State `Night` macht nichts Spezielles — die Plan-Marker erscheinen schon durch `source != Realtime`.
  - Direction-Text aus `display_dir(stream_idx)` ([§2.2](#22-neue-module-im-detail)). Empty-String → Spalte überspringen (S-Bahn-Stream).

**Aufwand**: 3–4 d gesamt — 7.3 (~1d Font-Stack, abh. von Option), 7.4–7.6 (~0.5d je Primitive, host-testbar), 7.7 (~0.5d), 7.8 (~1d Layout-Komposition), 7.1+7.2 (~0.5d State-Selector). Visuelle Kalibrierung in Schritt 12.

**Validation**: `make ci` grün (alle Native-Tests, inkl. Render-Pixel-Asserts); `pio run -e device-render -t test` grün; PGM-Dumps der Native-Runtime zeigen erkennbare Strukturen (Header, Badges, Trennlinien, Netzplan).

**Reversibel**: nur als ganzer Block. Sub-Steps 7.1+7.2 (State-Modell) sind reversibel; sobald 7.3 (Fonts) committet ist, hängt 7.8 unausweichlich davon ab.

#### Schritt 8 — Tests umstellen

- [ ] erledigt

Sammelschritt für alle Test-Touch-Sites, die durch STREAM_COUNT-Änderung, neuen State-Selektor und Layout-Rewrite rot wurden.

- **8.1** `test_native_*`-Buckets, die `STREAM_COUNT == 5` annehmen:
  - `test_native_filter_builder` ([Schritt 4](#schritt-4--filter_builderhcpp-umstellen--s-bahn-filter-getter))
  - `test_native_filter_health` — Stream-Index 3 testet jetzt S-Bahn-Verhalten, nicht U1
  - `test_native_slot_merger` — Hint-Tests an Index 3/4 entfallen (S-Bahn hat keinen Hint, Variante 1)
  - `test_native_wienerlinien_parse` — Fixtures, die U1-RBLs erwartet hatten: U1-Section in Fixture entfernen, Assertion `endpoint_responded[3..4]` weg
  - `test_native_efa_parse` — Fixtures, die DIVA `60201349` (Südtirolerplatz) referenzieren: entfernen
  - `test_native_schedule_fetcher` — Erwartung: 2 statt 3 distinct DIVA-Calls
  - `test_native_schedule_refresh` — Stream-Index 3/4 entfällt
  - `test_native_snapshot_fetcher` — siehe Schritt 5.5
  - `test_native_snapshot_logger` — Summary-Format hat jetzt 4 Stream-Zeilen statt 5
  - `test_native_render_input` — **komplett überarbeitet**: alle 7 `DisplayState`-Pfade einzeln getestet (`selectDisplayState`-Pure-Function-Tests); `composeRenderInput`-Tests prüfen die line_label-Propagation auf Index 3
  - `test_native_cycle_runner_*` — alle drei Buckets: cold + warm + invariants — Fakes liefern jetzt OGD-Batch + OEBB-Call, Recording-Traces erwarten beide. **`cycle_runner_cold` zusätzlich** mit zwei Fixtures, weil Cold-Cycle bei *jedem* Deep-Sleep-Wake läuft, der Pre-Render aber nur bei leerem Meta greift:
    - `test_cold_first_boot` — Fixture: `PersistedMeta{has_any_data=false, auth_error_seen=false}`. Erwartung: Trace enthält `[render]` **zweimal** (Pre-Fetch mit `state=Boot`, Post-Fetch mit `state=Normal` o.ä.) sowie `[display.updateFull]` einmal + `[display.updatePartial]` einmal.
    - `test_cold_subsequent_wake` — Fixture: `PersistedMeta{has_any_data=true, auth_error_seen=false, last_success_at=now-300}`. Erwartung: Trace enthält `[render]` **genau einmal** (Post-Fetch) und `[display.updatePartial]` einmal.
    - `test_cold_persisted_auth` — Fixture: `PersistedMeta{has_any_data=true, auth_error_seen=true}`. Erwartung: Pre-Render zeigt **Auth** (nicht Boot), Post-Fetch entweder weiter Auth (wenn Drift bleibt) oder Normal (wenn Parser erfolgreich `auth_error_seen=false` setzt) — zwei Renders, beide deterministisch via Mock-Fetch-Antwort steuerbar.

    `cycle_runner_warm` darf das `[render]`-Event nur einmal haben (Pre-Render-Pfad greift im Warm-Cycle gar nicht).
- **8.2** **Neue `test_native_*`-Buckets für Render-Primitive:**
  - `test_native_badge` (neu) — Pixel-stamp asserts auf `drawBadge` in allen drei Größen, Rechteck-Bounds, Text-Position
  - `test_native_plan_marker` (neu) — 5×5-Pixel-Mask-Check
  - `test_native_network_plan` (neu) — Asserts auf Diamond-/Big-/Dot-Pixel an erwarteten Koordinaten + Vertikallinie zwischen den Atzg-Markern
  - `test_native_display_state` (neu) — Renderer für jeden der vier Fullscreen-States (Boot/Offline/Auth/Quiet). **Snapshot-Strategie statt hartkodierter Hashes**, weil Hartkodierung bei jedem U8g2-Library-Upgrade alle States gleichzeitig rot wirft, ohne semantische Änderung:
    - Erwarteter Frame als binärer Dump unter `test/test_native_display_state/snapshots/{boot,offline,auth,quiet}.bin` eingecheckt.
    - Test vergleicht `FrameBuffer<>::data()` Byte-für-Byte gegen die Snapshot-Datei.
    - Falls Drift: `UPDATE_SNAPSHOTS=1 make test-native-display-state` schreibt die aktuellen Frames als neue Snapshots zurück, der Diff im Git-Status zeigt die tatsächlichen Pixel-Änderungen reviewbar. Für visuelle Inspektion: `scripts/bin-to-pgm.sh <snapshot.bin> > out.pgm` (Mini-Helper, 5 Z.):

      ```bash
      #!/usr/bin/env bash
      # bin-to-pgm.sh — 400×300 1-bit framebuffer → P5 PGM (8-bit grayscale)
      printf 'P5\n400 300\n255\n'
      python3 -c "import sys; d=sys.stdin.buffer.read(); \
        sys.stdout.buffer.write(bytes(255 if (d[i//8]>>(7-i%8))&1 else 0 for i in range(400*300)))" < "$1"
      ```

      Output direkt in jedem Bildbetrachter (eog, feh, GIMP) öffenbar. Vorbild: Jest-Snapshots, aber 50 Z. C++ Test-Bucket + 5 Z. Shell-Helper.
    - Library-Upgrade-Workflow: `UPDATE_SNAPSHOTS=1`, dann PR-Diff der `.bin`-Files visuell prüfen — wenn nur Anti-Aliasing-artige Pixelverschiebungen erkennbar, ist das Update unkritisch; wenn ganze Glyphen anders aussehen, ist es ein Regression-Kandidat.
  - `test_native_auth_tripwire` (neu) — `SelectorSignals.auth_error_seen` wird gesetzt für (a) HAFAS-`err: "AID"`, (b) HAFAS-`err: "AUTH"`, (c) drei OGD-401-in-Folge; und wird durch erfolgreichen Parse wieder gelöscht.
- **8.3** `test_device_*`-Buckets:
  - `test_device_fetch`: zweite kanonische Antwort (HAFAS-Fixture) + Assertion auf `stream[3].slot[0..1].valid`
  - `test_device_schedule`: U1-Erwartung raus, ÖBB hat keinen Schedule-Call → Erwartung „2 calls instead of 3"
  - `test_device_render`: **komplett umgebaut** — heute prüft die Suite den alten Layout-Stil mit Adafruit-GFX-Font. Neu: alle 7 States einmal rendern + via `IDisplay`-Mock-Frame ausgeben (kein Pixel-Match-Assert auf Device, weil Bitmap-Font-Versions-Drift Tests instabil machen würde — stattdessen Smoke: jedes State rendert ohne Crash, Frame-Diff zwischen States ist non-zero).
- **8.4** `test_longterm_*`-Buckets:
  - `test_longterm_smoke`: Erwartung „alle 5 Streams" → „alle 4 Streams"; Stream 3 ist jetzt S-Bahn, validiert über `line_label != ""` für mindestens einen Slot
  - `test_longterm_horizon_evening`: Hint-Bridge gilt nur für Bus-Streams (3 statt 5)
  - `test_longterm_horizon_scan`: Cliff-Test gilt für Bus-Streams; S-Bahn-Stream hat keinen Hint, fällt nach 70 min auf `--:--` (nicht auf `next_today`). Plus: bestätigt, dass nach Cliff der State zu `Quiet` springt (wenn alle Bus-Streams auch leer)
  - `test_longterm_day_full`, `test_longterm_jitter`, `test_longterm_wake_cycle`: STREAM_COUNT-Touch-Sites + Display-State-Transition-Logs
- **8.5** Fixture-Update: alle `wl_live.h`-Fixtures, die U1-Daten enthielten, müssen mit-gezogen werden (das ist viel `git diff`-Lärm aber keine echte Logik).

**Aufwand**: ~2.5–3 d — die fünf neuen Native-Buckets (4 Render-Primitive + Auth-Tripwire) inklusive der händisch erzeugten Pixel-Masks ziehen mehr Zeit als die Touch-Sites der bestehenden Tests.

**Validation**: `make ci` grün (alle Buckets); `make test-device` grün; `make test-longterm-smoke` grün; `make test-longterm-soak-15min` als Baseline + Diff gegen pre-v2-Baseline (Drift erwartet — andere Streams, andere Logs, andere State-Transitions).

**Reversibel**: theoretisch — aber praktisch ist das Test-Update sehr verflochten mit Schritt 2–7. Rollback würde alle Schritte rollback bedeuten.

#### Schritt 9 — Heap-Profiling

- [ ] erledigt

- **9.1** Native-Runtime ([test/test_native_runtime/](../test/test_native_runtime/)) um den OEBB-Pfad erweitern: `HttpsNet` muss `httpPost` implementieren (libcurl `CURLOPT_POSTFIELDS`).
- **9.2** `make native-runtime-smoke` mit valgrind/massif laufen lassen — `fetchSnapshot` mit beiden Endpunkten, 50 Cycles. Erwartung: kein Leak über Cycle-Grenzen, Peak-Heap < pre-v2-Peak + 8 KB (HAFAS-Antwort).
- **9.3** Auf Device: `test_device_fetch` mit `Serial.printf("free heap before/after oebb call: %u/%u\n", …)` instrumentieren. Wenn Spitze die EFA-Heap-Wächter triggern würde (`< 90 KB free`), `httpPostStream` einplanen — als Follow-up-PR, nicht im selben Branch.
- **9.4** **Sleep-Budget-Messung** (Risiko V13). Auf Device mit `esp_timer_get_time()`-Stempeln um beide HTTPS-Pfade:

  ```cpp
  int64_t t0 = esp_timer_get_time();
  // OGD-Batch …
  int64_t t1 = esp_timer_get_time();
  // OEBB-Call …
  int64_t t2 = esp_timer_get_time();
  Serial.printf("[budget] ogd=%lldms oebb=%lldms total=%lldms\n",
                (t1-t0)/1000, (t2-t1)/1000, (t2-t0)/1000);
  ```

  Erwartung: OGD ~2.5 s (Connect + 3 GETs), OEBB ~2 s (separater TLS-Handshake + 1 POST). Gesamt-Wake-Dauer ~6–7 s inkl. WiFi-Connect + Render + Deep-Sleep-Entry. Schwellwert: wenn `total > 10 s`, ist das Sleep-Budget gefährdet → Mitigation Optionen: (a) TLS-Session-Resumption über RTC ablegen, (b) beide Calls parallelisieren mit zwei `WiFiClientSecure`-Instanzen (Heap-teuer), (c) OEBB-Call nur jeden zweiten Wake-Cycle. Entscheidung erst nach Messung — nicht jetzt vorab.

  Drei Vergleichsläufe: vor v2 (nur OGD+EFA), nach Schritt 5 (v2 mit OEBB), und ein Kontrolllauf nach Cold-Boot (frische TLS-Sitzungen, kein Cache).

**Aufwand**: 0.75 d (vorher 0.5 d) — die Sleep-Budget-Messung verlangt drei On-Device-Läufe + Auswertung.

**Validation**: massif-Report unter `.tmp/native-runtime/massif-v2.out`; Device-Log mit Heap-Werten **und** Sleep-Budget-Zeilen für alle drei Vergleichsläufe.

**Reversibel**: nur Diagnostik, kein Code-Refactor.

#### Schritt 10 — Doku-Sync

- [ ] erledigt

- **10.1** [CONCEPT.md §v2-11](../CONCEPT.md#11-migrationsschritte-zur-späteren-umsetzung-nicht-teil-dieses-konzepts): Header von „Migrationsschritte (zur späteren Umsetzung, nicht Teil dieses Konzepts)" auf „Migrationsschritte (umgesetzt, siehe [docs/v2-sbahn-migration-plan.md](docs/v2-sbahn-migration-plan.md))" ändern. Liste der elf Sub-Steps abhaken oder durch Verweis auf diesen Plan ersetzen. Zusätzlich: CONCEPT.md §v2-7 (Layout-Block 3) durch Verweis auf `docs/design_handoff_display/` ersetzen oder synchronisieren — das alte Block-3-Mockup ist nach Schritt 7 nicht mehr aktuell. **Zusätzlich (Pre-Phase 2026-05-19)**: CONCEPT.md §v2-3 (Datenquellen-Tabelle, ca. Z. 420–425) und §v2-4 (Request-Schema, ca. Z. 445–457, 493–494, 603) korrigieren bzw. ergänzen, dass die 8-stelligen EVAs (`8100634` / `8100002`) nur für die Legacy-`stboard.exe`-HTML-Schnittstelle gelten; für `mgate.exe` benötigt HAFAS die internen Location-IDs `1292301` (Atzgersdorf) und `1290401` (Wien Hbf). Belege-Block ergänzen: LocMatch-Query gegen `mgate.exe` als nachvollziehbarer Weg, die HAFAS-IDs zu beschaffen.
- **10.2** [README.md](../README.md) Zeile 4: „nächste Abfahrten der Wiener-Linien-Buslinien 58A, 58B" um „und der ÖBB-S-Bahn Atzgersdorf → Wien Hbf" ergänzen. Display-ASCII-Block (Zeile 10-18) komplett ersetzen durch das neue Layout-Schema aus [docs/design_handoff_display/README.md](design_handoff_display/README.md). Banner-Liste entfällt (Stale ist jetzt `--:--`-Signal statt Banner); stattdessen die 7 Display-States in zwei Sätzen erklären, mit Verweis auf USER.md.
- **10.3** [docs/ARCHITECTURE.md](ARCHITECTURE.md) Modulkarte: `oebb_hafas_parse.{h,cpp}` in `data/` eintragen; in `render/` die fünf neuen Sub-Module (`bitmap_fonts`, `badge`, `plan_marker`, `network_plan`, `display_state`). „Wo welche Konstante wirkt"-Tabelle ergänzen (`OEBB_*`, `OFFLINE_THRESHOLD_S`, `QUIET_HORIZON_S` …). Speicher-Layout: `Departure::line_label` als zusätzliche 6 B pro Slot + Font-Daten in Flash (~20–80 KB) vermerken. Zustandsmaschine: `OverlayKind` → `DisplayState` aktualisieren.
- **10.4** [docs/HANDBUCH.md](HANDBUCH.md): **kompletter Rewrite des Display-Abschnitts** — alle 7 States dokumentieren, Screenshots aus `docs/design_handoff_display/screen-*.png` einbetten (sind schon im Repo, müssen nicht neu erzeugt werden). Plan-Marker-Erklärung. Netzplan-Erklärung mit „you are here"-Marker.
- **10.5** [docs/USER.md](USER.md): „Was bedeuten die Anzeigen" komplett neu — die 7 States listen, jeweils 1 Satz Bedeutung + Trigger-Bedingung. Beispiel-Screenshots aus `docs/design_handoff_display/`. Plan-Marker `□` erklären (Plan vs. Live). **Symbol-Cheatsheet** als kompakte Tabelle erzwingen (Stale `--:--` in Slots vs. Quiet `—` Fullscreen vs. Boot `◌` vs. Offline `!` vs. Auth `§9` — diese fünf Glyphen sind ähnlich abstrakt und werden ohne Tabelle verwechselt).
- **10.6** [docs/HARDWARE.md](HARDWARE.md): unverändert (keine Hardware-Änderung).
- **10.7** [docs/TESTING.md](TESTING.md): neue Test-Buckets eintragen — `test_native_oebb_hafas_parse`, `test_native_badge`, `test_native_plan_marker`, `test_native_network_plan`, `test_native_display_state`.
- **10.8** [CHANGELOG.md](../CHANGELOG.md): Eintrag für Release 2.0 (v2 ist eine Major-Änderung wegen RTC-MAGIC-Bump = Reset-on-Update, **sichtbarer Display-Rewrite** + S-Bahn-Stream statt U1).
- **10.9** [docs/design_handoff_display/README.md](design_handoff_display/README.md): Status-Header oben einfügen — „Umgesetzt in v2.0 (Roll-out 2026-MM-DD)" mit Verweis auf diesen Plan. Quelldateien bleiben als Referenz.
- **10.10** Markdown-Lint über alle geänderten `.md`-Files mit `markdownlint-cli2 --fix`.
- **10.11** **`.tmp/`-Cleanup-Audit.** Vor dem Final-Merge nach `main`: `ls .tmp/` prüfen — gitignored, aber lokal noch da. Erwartet sind:
  - `.tmp/poc-oebb/` (gelöscht in P.6 zusammen mit dem PoC-Skript)
  - `.tmp/hafas-fixtures/` (verwendet als Quelle für `test_native_oebb_hafas_parse/fixtures/` in 3.4; nach 3.4 nicht mehr benötigt, kann weg)
  - `.tmp/native-runtime/massif-v2.out` (Heap-Profiling, 9.2; aufheben für Anhang B Update)
  - `.tmp/v2-rollout/` (HW-Verifikations-Fotos aus 11.x; aufheben bis Release-Tag steht)

  `.gitignore` ist bereits korrekt (`/.tmp/` als Eintrag); falls Schritt 0.3 / 9 / 11 neue Pfade nutzt, hier verifizieren. `make clean` räumt `.tmp/` heute schon vollständig.

**Aufwand**: 1 d — die Display-Doku-Anpassungen sind substantiell.

**Validation**: `markdownlint-cli2` ohne Fehler; visuelle Sichtkontrolle der eingebetteten Screenshots in HANDBUCH/USER.

#### Schritt 11 — Manuelle HW-Verifikation am Ende

- [ ] erledigt

Mit Auftraggeber am Gerät, mit allen 7 Display-States. Pro State Side-by-Side-Vergleich gegen `docs/design_handoff_display/screen-*.png`.

- **11.1** Flash `make flash` auf ESP32. Cold-Boot-Sequenz visuell verifizieren — **zwei Display-Refreshes hintereinander** sichtbar:
  1. Innerhalb ~3 s nach Reset: `Boot`-State (`◌`/`o` + „bustaferl" + „lädt Fahrplan…" + Version-Foot). Splash-Refresh läuft *vor* dem WiFi-Connect.
  2. Innerhalb ~5–10 s danach (nach Fetch): `Normal`-State (oder anderer, falls Auth/Offline). Content-Refresh.

  Wenn nur **ein** Refresh sichtbar wird (Display geht direkt zu Normal): Pre-Render aus Schritt 5.4 nicht aktiv. Mit Logic-Analyzer oder Serial-Log (`[cold] pre-render boot`, `[cold] fetch start`, `[cold] post-render normal`) verifizieren.
- **11.2** Nach erfolgreichem Cold-Boot-Fetch: `Normal`-State. Vergleich gegen `screen-1-normal.png`. Prüfen:
  - TG-Block: Badges sichtbar als weiße Rechtecke mit schwarzem Text, Direction-Text „Atzgers." / „Hietzing", Plan-Marker `□` hinter Plan-Zeiten, keine hinter Live-Zeiten.
  - 2-px Trennlinie unter TG.
  - EG-Block: kleinere Badges, kompaktere Reihe, Section-Header „ENDEMANNGASSE · NACH SCHLEIFE".
  - 1-px Trennlinie unter EG.
  - Atzg-Block: drei S-Bahn-Slots horizontal (dritter Slot leer wenn nur 2 Departures), Section-Header mit Pfeil.
  - Netzplan unten: 5 Spalten, Diamond/Dot/Big-Marker an erwarteten Positionen, vertikale Linie zwischen den Atzg-Diamonds, ▼ über Tull.
- **11.3** Stale-Test: WiFi-AP aus → nach 3 min `Veraltet`-State. Vergleich gegen `screen-2-veraltet.png`. Prüfen: alle Slots `--:--`, **keine** Plan-Marker, Layout-Struktur bleibt.
- **11.4** Nachtbetrieb-Test: Uhrzeit-Override (Test-Hack: NTP-Server temporär auf 02:00 setzen) → `Nachtbetrieb`-State. Vergleich gegen `screen-3-nachtbetrieb.png`. Prüfen: alle Zeiten sind Plan-Zeiten (alle mit `□`), Morgen-Erst-Abfahrten sichtbar.
- **11.5** Keine-Abfahrten-Test: schwierig zu provozieren — entweder spät genug warten oder Test-Hack mit dem `QUIET_HORIZON_S` auf 30 s. Vergleich gegen `screen-4-keine-abfahrten.png`. Prüfen: zentraler `—` (72 px) + „Keine Abfahrten" + „in den nächsten 20 min", **kein Netzplan**.
- **11.6** Offline-Test: WiFi-AP aus + warten bis `OFFLINE_THRESHOLD_S` (5 min). Vergleich gegen `screen-5-kein-empfang.png`. Prüfen: Fullscreen `!` (90 px) + „Kein Empfang" + Sub mit Last-Fetch + Foot mit Retry.
- **11.7** Auth-Drift-Test: `OEBB_HAFAS_AID` in `config.h` temporär auf `"INVALID"` setzen, neu flashen. HAFAS antwortet erwartungsgemäß mit **HTTP 200 + `err: "AID"`** (Parser-Pfad — *nicht* via HTTP 401/403). Verifiziert, dass der State-Selector den Auth-Screen aus `parseOebbStationBoard.auth_error_seen = true` triggert. Vergleich gegen `screen-6-auth-fehler.png`. Prüfen: nach **einem** HAFAS-Call Fullscreen `§9` + „Auth-Fehler" + AID/ERR im Foot (kein 3-Strikes-Delay wie bei OGD-401, weil das Parser-Flag direkt setzt). Reset-Verhalten: AID zurück auf gültigen Wert, neu flashen → erstes erfolgreiches Wake räumt Auth-State auf und zeigt Normal.
- **11.8** Visuelle Kalibrierung Font + Pixel-Treue: pro State 1× Foto gegen Design-PNG halten. Erkenntnisse zu Glyph-Drift in [Schritt 7] §4.1-Annahme festhalten; wenn die Drift unzumutbar ist, Decision auf Custom-Bitmap-Fonts (Option B aus Schritt 0.6) als Follow-up-PR.
- **11.9** Linien-Längen-Stress: HAFAS-Antwort mit `REX1` warten ab (~stündlich Richtung Hbf). Layout darf nicht überlappen.
- **11.10** Optional 24h-Soak (`make test-longterm-day-full`) über Nacht — Auftraggeber-Entscheidung ob nötig. Validiert State-Transitions über echten Tageslauf (Nachtbetrieb → Normal → Keine Abfahrten am Mittag o.ä.).

**Aufwand**: 1 d + ggf. 24h Warten.

**Validation**: alle 7 State-Beobachtungen passieren wie beschrieben; alle Side-by-Side-Vergleiche dokumentiert (Fotos im `.tmp/v2-rollout/`); Auftraggeber-Abnahme.

#### Schritt 12 — Release v2.0.0

- [ ] erledigt

Major-Release wegen RTC-MAGIC-Bump (alle Geräte verwerfen ihren persistenten Zustand beim ersten Wake nach Update) und sichtbarem Display-Rewrite. Folgt der Konvention des `release`-Skills.

- **12.1** **Test-Gate verifizieren.** `make ci` grün, `make test-device` grün, `make test-longterm-soak-15min` grün — alle nicht älter als 24h. Falls stale, neu laufen lassen.
- **12.2** **Branch nach `main` mergen.** Nach Auftraggeber-Wahl: Squash-Merge (komprimiert die ~15 Commits zu einem) oder Merge-Commit (Historie bleibt sichtbar). Default: Merge-Commit, weil die Schritte-Granularität für künftiges Bisect wertvoll ist.
- **12.3** **Tag setzen.** `v2.0.0` als annotiertes Tag auf den Merge-Commit. Tag-Message zusammenfasst: „v2 — S-Bahn Atzgersdorf + Display-Redesign. Major: RTC verworfen beim Update, Display-Layout neu." Verweis auf [docs/v2-sbahn-migration-plan.md](v2-sbahn-migration-plan.md) für Details.
- **12.4** **`release`-Skill ausführen.** Optional, wenn der Plan eingerichtet ist; sonst manuell. Bei Major-Release zusätzlich 24h-Soak (`--major`-Flag des Skills).
- **12.5** **Branch behalten.** `v2/sbahn-atzgersdorf` bleibt mindestens eine Woche nach Tag stehen — nicht löschen. Beobachtungsfenster für unentdeckte Drifts am Gerät.
- **12.6** **CHANGELOG-Eintrag verifizieren.** Aus 10.8 sollte der v2.0-Block bereits stehen. Hier nur Datum nachtragen.
- **12.7** **Plan-Datei archivieren.** Frontmatter dieser Datei updaten: „Stand: 2026-MM-DD · Umgesetzt in v2.0.0 (Tag: vXXX, Merge: `<sha>`)". Die Datei bleibt als Plan-of-Record stehen, analog [main-refactor-plan.md](main-refactor-plan.md) (Memory `main-refactor-plan-keep`).

**Aufwand**: 0.5 d (ohne 24h-Soak-Wartezeit).

**Validation**: Tag erscheint im `git log`, `make release` (falls vorhanden) idempotent grün, Display am Gerät zeigt unveränderten Normal-State über ≥ 48 h.

**Reversibel**: nein — sobald getaggt und gemergt, gilt das als Release. Rollback erfordert neuen Branch und neue Major-Version.

### 4.3 Session-Gruppierung A-G

Sieben Sessions, abwechselnd Auftraggeber-getrieben (HW-/Netzwerk-Zugang) und Code-getrieben (vollständig autonom durch Claude). Ein Code-Session-Commit pro Sub-Step, eine HW-Session = ein Beobachtungs-Block mit Foto-Dokumentation in `.tmp/v2-rollout/`. Reihenfolge ist gerichtet: jede Session hat ein Gate, das die nächste freischaltet.

| Session | Schritte | Driver | Aufwand | Inhalt | Gate zur nächsten |
|---|---|---|---|---|---|
| **A** | Pre-Phase + 0 | Auftraggeber | 1–2 d (Kalender; Sample-Lauf über zwei Werktage) | PoC-Skript drei Tageszeiten; HAFAS-Konstanten + Cert-Check; Render-Stack-Entscheidung (C8) bestätigt | §4.1 Annahmen mit AID/jnyFltrL/dirLoc-Befunden + Antwort-Fixtures unter `.tmp/hafas-fixtures/`; `config.h` HAFAS-Werte gesetzt |
| **B** | 1, 2, 6 | Claude | ~1.5 d | HAL: `HttpResult` + `httpPost`; Datenmodell: `Departure::line_label`, `PersistedMeta`-Erweiterung, Stream-Enum, MAGIC-Bumps; `schedule_fetcher` skip-Guard | `make ci` baut (einzelne Test-Buckets bleiben rot — werden in F gefixt); `env:esp32dev` linkt |
| **C** | 3, 4, 5 | Claude | ~2.5–3 d | `data/oebb_hafas_parse.{h,cpp}` + native Tests aus den Fixtures aus A; `filter_builder` mit `buildOebbFilter`; `snapshot_fetcher` mit `fetchOebbStream` + OGD-Auth-Streak | `make test` grün für `test_native_oebb_hafas_parse`, `_filter_builder`, `_snapshot_fetcher`, `_api_fetcher` |
| **D** | 7 | Claude | ~3–4 d | Display-Rewrite: `bitmap_fonts`, `badge`, `plan_marker`, `network_plan`, `display_state`, neuer `layout.cpp`, State-Selector in `render_input.cpp` + Cold-Boot-Pre-Render-Sequenz in `cycle_runner` | `make ci` grün inkl. Render-Pixel-Asserts; PGM-Dumps der Native-Runtime zeigen alle 7 States erkennbar |
| **E** | 11.1–11.8 | Auftraggeber + Claude (Patches) | 1 d + Iterationen | HW-Sichtkontrolle aller 7 States gegen `design_handoff_display/screen-*.png`; Glyph-/Y-Drift-Korrekturen iterativ; Plan-Marker-Sichtbarkeit; Netzplan-Geometrie | Alle 7 States vom Auftraggeber abgenommen; Fotos in `.tmp/v2-rollout/screen-*.jpg` |
| **F** | 8, 10 | Claude | ~3 d | Restliche Test-Buckets umstellen (`cycle_runner_*`, `slot_merger`, `filter_health`, `longterm_*`); neue Render-Primitive-Buckets + Snapshot-Files; vollständiger Doku-Sync (CONCEPT, README, ARCHITECTURE, HANDBUCH, USER, TESTING, CHANGELOG) | `make ci` + `make test-device` + `make test-longterm-soak-15min` alle grün; `markdownlint-cli2` clean über alle .md |
| **G** | 9, 11.9–11.10, 12 | Auftraggeber + Claude (Auswertung) | 1 d + ggf. 24 h Soak | Heap-Profiling (Native + Device); Sleep-Budget-Messung; Linien-Längen-Stress mit live REX-Antwort; optionaler 24h-Soak; Release-Merge nach `main` + Tag `v2.0.0` + Plan-Datei archivieren (12.7) | Tag gesetzt, Branch eine Woche Beobachtungsfenster |

**Branch-Disziplin pro Session**:

- Sessions B/C/D/F sind je ein logischer „Commit-Block" auf `v2/sbahn-atzgersdorf`. Innerhalb der Session ein Commit pro Sub-Step (Konvention aus §4.0).
- Session A produziert nur `.tmp/`-Artefakte + `config.h`-Werte; ein einzelner Commit für `config.h` reicht.
- Session E produziert Render-Korrektur-Patches (kleine Commits, je 1–10 Z. Pixel-Tuning); diese werden nach Abschluss von E zu einem Squash-Block „Render: §11 Sichtkontroll-Anpassungen" zusammengefasst.
- Session G: ein letzter Commit (Doku-Archivierung 12.7), dann Tag.

**Pausen zwischen Sessions sind erlaubt** — der Plan ist nicht für „in einem Schwung" gemacht. Sinnvolle Pause-Punkte: nach A (Daten in der Hand), nach C (Daten-Layer steht), nach D (Display kompiliert, vor erstem HW-Check), nach E (Display final), nach F (Tests grün).

**Kalender-Erwartung**: brutto ca. 2–3 Wochen, davon ca. 11 d Claude-Code-Arbeit + 3–4 d Auftraggeber-getrieben. Wenn A und G sich über mehrere Tage strecken (HAFAS-Sampling, 24h-Soak), kann sich die Kalenderzeit auf 3–4 Wochen ausdehnen — die Netto-Aktiv-Zeit bleibt gleich.

---

## 5. Risiken

| ID | Risiko | Wahrscheinlichkeit | Auswirkung | Mitigation |
|---|---|---|---|---|
| V1 | **AID/Client-Wert rotiert nach Release.** ÖBB ändert die Webapp-AID, unsere hartkodierte funktioniert nicht mehr. | mittel | Banner „Auth ungültig" auf dem Display, Re-Flash nötig | Schritt 0.1 dokumentiert die Werte mit Datum; FilterHealth fängt Drift; Auth-Screen ist Auftrag, AID zu aktualisieren — kein stilles Versagen |
| V2 | **`dirLoc`-Filter lässt Gegenrichtung durch.** HAFAS zeigt trotz `dirLoc.extId = Hbf` Züge Richtung Mödling/Wr. Neustadt. | gering (PoC-Antworten in der Webapp sauber) | falsche Züge im Display, Vorzimmer-User verpasst tatsächliche Abfahrt | Pre-Phase P.3 sweep über drei Tageszeiten; wenn auch nur einmal Gegenrichtung sichtbar → clientseitiger `dirTxt`-Check in Schritt 3 (Akzept-Liste in §1.5 Daten-Schicht Item 3) |
| V3 | **HTTPS-Antwort sprengt ESP32-Heap.** HAFAS-Antworten sind ~20.5–21.2 KB roh (Pre-Phase 2026-05-19: 20506/21133/21188 B über 30 min), **nicht** die ursprünglich angenommenen 5–8 KB — Treiber sind `prodL[].himIdL`-Listen mit 17–55 FREETEXT-Referenzen pro Linie. Während TLS-Handshake aktiv ist, fragmentiert der Heap; sinkt der freie Heap unter ~50 KB, crasht mbedtls. Der bestehende EFA-Pfad (38 KB) hat dafür Streaming + Heap-Wächter — der neue HAFAS-Pfad startet ohne. | mittel-hoch (Antwort ~2× kleiner als EFA, aber neue Allocation-Klasse + näher an der Streaming-Schwelle als angenommen) | OOM-Crash beim ersten HAFAS-Call, ESP32 rebootet | Schritt 9 misst Peak-Heap auf Device + Native-Runtime mit valgrind; wenn Spitze gefährlich → `httpPostStream` als Follow-up (Streaming-Pendant zu `httpGetStream`) ODER Parser ignoriert `himIdL` komplett (reduziert Parse-Zeit, vermeidet unnötige ArduinoJson-DOM-Knoten — Body bleibt aber im `std::string`-Buffer, kein RTC-/Heap-Vorteil bei der Pufferung selbst). |
| V4 | **RTC-Slow-Memory-Reserve wird eng.** ESP32 hat 8 KB RTC-Slow-Memory; nutzen heute ~7.3 KB. Nach v2 zusätzlich `Departure::line_label` (48 B über alle Slots) und etwas Display-Frame-Varianz. Bleiben ~864 B Reserve — für künftige Features (mehr Slots, mehr Meta-Felder) eng. | gering | nächste Erweiterung muss RLE-Hardcap senken oder ein anderes Feld kürzen, kein direktes v2-Problem | Bilanz in [Anhang B](#anhang-b--rtc-bilanz-nach-v2) führt die Belegung; Schritt 9 misst die tatsächliche RLE-Größe nach Display-Redesign |
| V5 | **`STREAM_COUNT == 5`-Annahmen werden übersehen.** Heute haben wir 5 Streams (3 Bus + 2 U1), nach v2 nur 4. Wenn irgendwo im Code `Departure foo[5]` statt `Departure foo[STREAM_COUNT]` steht, kompiliert es weiter, liest aber Garbage am Ende. Tests sind die häufigste Stelle. | mittel | Compile-Fehler oder stiller Garbage-Read im betroffenen Test/Modul | Schritt 8 ist Sammelschritt für Test-Touch-Sites; `make ci` enforced alle Buckets nach jeder Stream-Index-Änderung |
| V6 | **Bestehende Hint-Tests brechen, weil Stream-Index 3 die Bedeutung wechselt.** `test_longterm_horizon_evening` prüft heute, dass am Abend an Index 3 (U1-Leopoldau) EFA-Hint-Daten erscheinen. Nach v2 ist Index 3 die S-Bahn — die hat *keinen* Hint-Pfad (Variante 1). Der Test schlägt fehl. | hoch | rote Tests bis aktiv umgestellt | Schritt 8.4 listet die fünf betroffenen `test_longterm_*`-Buckets und ihre konkreten Anpassungen |
| V7 | **`line_label` macht RLE-Kompression schlechter.** Der gerenderte Frame wird RLE-komprimiert in RTC abgelegt (Hardcap 7168 B). Mehr ink-Pixel im Frame (Badges, line_label-Text, Netzplan) bedeutet kürzere Run-Längen, schlechtere Kompression. Worst-case-Frame könnte den Hardcap sprengen. | gering | Frame wird verworfen, nächster Cycle erzwingt Light-Full-Refresh statt Partial (sichtbares Blinken) | Schritt 9 misst RLE-Größe an worst-case-Frame (alle Slots voll, Netzplan, Plan-Marker); wenn Grenze knapp → Hardcap-Aufstockung oder Slot-Reduktion |
| V8 | **Font-Metriken passen nicht zum geplanten Layout.** Der gewählte U8g2-Font (Logisoso o.ä.) hat andere Höhen/Baseline-Offsets als die VT323-px-Maße, auf die das Design-Handoff-Layout kalibriert ist. Beispiel: ein als „28 px" markierter Font ist tatsächlich 32 px hoch und überschreitet den TG-Y-Bereich. (Optik-Drift ist kein Risiko — die wird in C9 akzeptiert.) | mittel | Layout-Y-Werte stimmen nicht, Text wandert in Trennlinie oder andere Blöcke | Schritt 7.3 testet pro Font-Rolle die tatsächliche `getTextBounds`-Höhe gegen die geplante Region; wenn Inkompatibilität → entweder anderen U8g2-Font wählen oder Y-Maße aus §2.2 anpassen, dann Doku-Sync |
| V9 | **Netzplan-Geometrie subtil falsch.** Diamond ist eigentlich ein 45°-rotiertes Rechteck — bei 1-bit-Rasterisierung wird daraus ein achteckiger Pixelhaufen, nicht ein sauberer Diamond. 1-px-Linien können um ein Pixel verschoben sein und treffen nicht das Marker-Zentrum. | mittel | Netzplan wirkt schief oder unausgerichtet | `test_native_network_plan` setzt Pixel-genaue Asserts auf Marker-Mitten + Linien-Endpunkte; Schritt 11 vergleicht visuell mit `screen-1-normal.png` |
| V10 | **Font-Daten sprengen Flash-Limit oder verlangsamen Boot.** U8g2-Schriften liegen in PROGMEM; 5–7 Größen + 2 Familien können kumulativ wachsen. Plus Custom-Glyph-Sprites aus C10. | gering | OOM beim Link (~3.4 MB Reserve heute, viel Headroom) / längere Power-on-Latenz | `make size` nach Schritt 7.3; bei Grenze: weniger Font-Größen, on-demand-Loading |
| V11 | **State-Selector schaltet zu früh oder zu spät.** Schwellwerte `QUIET_HORIZON_S`, `OFFLINE_THRESHOLD_S`, Service-Window-Hours sind angenommen, nicht beobachtet. Beispiel: `Quiet` triggert mittags wegen Schulferien-Lücke, obwohl Normalbetrieb gemeint ist. | mittel | Display zeigt falschen State zur falschen Zeit, irreführend für User | `test_native_render_input` deckt alle 7 Pfade pure-functional ab; Schritt 11 testet drei der seltenen States manuell durch Hacks (NTP-Override, AP-Aus, etc.) |
| V12 | **Plan-Marker `□` zu klein oder verschmilzt mit Zeit-Glyph.** 5×5 px auf 400×300 Display ist klein — auf realem e-Paper mit Ghosting evtl. nicht sauber abgrenzt von der HH:MM-Glyphe daneben. | gering | User erkennt Plan/Live-Unterschied nicht | Schritt 11.2 visuelle Inspektion; 5×5 px sind nominal deutlich erkennbar, aber Sichtkontrolle erst on-device verlässlich. Wenn unklar → 6×6 px oder anderes Symbol (z. B. `*` oder `~`) |
| V13 | **Sleep-Budget reicht nicht für zwei Endpunkte.** Pro Wake heute: ein WiFi-Connect + ein TLS-Handshake (OGD) + ein EFA-Call. v2: zusätzlich ein TLS-Handshake gegen `fahrplan.oebb.at` + POST. TLS-Handshake ≈ 1.5–3 s auf ESP32; pro Wake-Cycle steigt der Awake-Anteil entsprechend → Batterie-Lebensdauer sinkt um ~5–10 %. **Cold-Boot zusätzlich**: Pre-Fetch-Splash-Render mit Full-Refresh kostet weitere ~1.5–3 s, betrifft aber nur den allerersten Boot nach Power-On oder Firmware-Update — Akku-Impact praktisch null. | mittel | spürbar kürzere Akku-Laufzeit im Dauerbetrieb; Wake-Window ggf. zu kurz, dass beide Calls landen, bevor `cycle_runner` ins Sleep zurückgeht | Schritt 9.4 misst Warm-Cycle-Dauer mit + ohne OEBB-Call sowie einmalig die Cold-Boot-Dauer; falls Warm-Cycle > 10 s → Sleep-Budget in `cycle_runner` anheben oder die zwei Calls parallelisieren (Phase-2-PR) |
| V14 | **3-Slot-Mockup ≠ 2-Slot-Realität (visuell).** Design-Handoff zeigt drei S-Bahn-Slots, v2 zeichnet nur zwei und lässt die dritte Spalte leer. Sichtvergleich gegen `screen-1-normal.png` zeigt einen leeren Bereich, den der Auftraggeber als „Render-Bug" deuten könnte. | hoch (sicher sichtbar) | Konfusion bei Schritt 11.2; ggf. Diskussion ob die Variante-A-Entscheidung rückgängig | §3.3 explizit ausschreibt, dass der Mismatch erwartet ist; 11.2 verlangt die Abnahme als „leer ist gewollt"; eigene PR (3. Slot) optional nach v2 |

---

## 6. Tests

Tabellarisches Inventar der neuen und geänderten Test-Pairs:

| Bucket | Status | Touch |
|---|---|---|
| `test_native_oebb_hafas_parse` | **neu** | Parser-Tests aus Schritt 3.5 |
| `test_native_filter_builder` | geändert | + `buildOebbFilter`-Test, U1-Erwartungen weg |
| `test_native_filter_health` | geändert | Index 3 ist S-Bahn |
| `test_native_slot_merger` | geändert | keine Hint-Tests für Index 3 (S-Bahn ohne Hint) |
| `test_native_wienerlinien_parse` | geändert | Fixtures: U1-Section raus |
| `test_native_efa_parse` | geändert | Fixtures: DIVA Südtirolerplatz raus |
| `test_native_schedule_fetcher` | geändert | 2 statt 3 distinct DIVAs |
| `test_native_schedule_refresh` | geändert | STREAM_COUNT |
| `test_native_snapshot_fetcher` | geändert | + OEBB-Call mit FakeNet-Antwort |
| `test_native_snapshot_logger` | geändert | 4 statt 5 Stream-Zeilen |
| `test_native_render_input` | **stark geändert** | 7-State-Selector als Pure-Function-Tests + `line_label`-Propagation auf S-Bahn-Index |
| `test_native_badge` | **neu** | Pixel-stamp asserts auf `drawBadge(sm/md/lg)` |
| `test_native_plan_marker` | **neu** | 5×5-px-Hohlquadrat-Mask-Check |
| `test_native_network_plan` | **neu** | Marker-Positionen + Vertikallinie zwischen Atzg-Diamonds |
| `test_native_display_state` | **neu** | je Fullscreen-State (Boot/Offline/Auth/Quiet) ein deterministischer Frame-Hash |
| `test_native_auth_tripwire` | **neu** | `SelectorSignals.auth_error_seen` über HAFAS `err`-Codes und OGD-401-Streak |
| `test_native_cycle_runner_warm` | geändert | Recording-Trace erwartet beide Endpunkte; weiterhin genau ein `[render]`-Event |
| `test_native_cycle_runner_cold` | **stark geändert** | drei Fixtures (`first_boot`, `subsequent_wake`, `persisted_auth`); Pre-Render-Sequenz mit Full+Partial-Refresh-Trace |
| `test_native_cycle_runner_invariants` | geändert | beide Endpunkte; Invariante „warm cycle ⇒ genau ein render" hinzu |
| `test_native_api_fetcher` | geändert | + `fetchPostWithRetry`-Tests |
| `test_native_irenderer` | unverändert | Frame ist `STREAM_COUNT`-agnostisch (template) |
| `test_native_runtime_renderer` | unverändert | dito |
| `test_native_runtime_diskstore` | geändert | RTC-MAGIC-Bumps reflektieren |
| `test_device_fetch` | geändert | + HAFAS-Pfad |
| `test_device_schedule` | geändert | 2 statt 3 DIVA-Calls |
| `test_device_render` | **komplett umgebaut** | alle 7 States rendern ohne Crash, Frame-Diff zwischen States non-zero (kein Pixel-Match auf Device) |
| `test_device_persistent` | geändert | RTC-MAGIC + `Departure::line_label` |
| `test_device_sleep` | unverändert | Sleep ist Stream-agnostisch |
| `test_longterm_*` | alle geändert | STREAM_COUNT-Touch-Sites |

Coverage-Ziel bleibt: `logic/` + `data/` ≥ 90 %; neue `oebb_hafas_parse.cpp` braucht eigene Coverage-Messung in Schritt 3.

---

## Anhang A — HAFAS-Request-/Response-Vertrag

Format wird in Schritt 0.1 finalisiert. Hier der Vor-Verifikations-Stand aus [CONCEPT §v2-4](../CONCEPT.md#4-request-schema-primär-mgateexe):

Pre-Phase 2026-05-19 hat den Vertrag empirisch verifiziert (Fixtures unter `.tmp/poc-oebb/sample-{1,2,3}.json`). Die Werte unten sind die bestätigten Konstanten — bei künftiger AID-Rotation oder ID-Drift diesen Block aktualisieren mit Datum.

**Request-Body** (POST `https://fahrplan.oebb.at/bin/mgate.exe`):

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

Die `extId`-Werte sind HAFAS-interne Location-IDs für „Wien Atzgersdorf Bahnhst" (`1292301`) und „Wien Hbf (U)" (`1290401`). Sie unterscheiden sich von den 8-stelligen DB-EVAs (`8100634` / `8100002`), die nur für die Legacy-`stboard.exe`-HTML-Schnittstelle funktionieren. Quelle: `mgate.exe`-`LocMatch`-Service.

**Optional NICHT setzen:**

- `cfg`-Block (z. B. `{"polyEnc":"GPA","rtMode":"HYBRID"}`) führt zu `err=PARSE`. Pre-Phase 2026-05-19 verifiziert.

**Response-Felder, die geparst werden**:

- `err` — Wert-abhängige Auswertung:
  - `"OK"` → erfolgreicher Parse, weiter mit `svcResL`
  - `"AID"` / `"AUTH"` → `OebbParseResult.auth_error_seen = true` (triggert Auth-Screen, siehe State-Selector §2.2)
  - alle anderen (`"FAIL"`, `"PROBLEMS"`, `"PARSE"`, …) → `OebbParseResult.endpoint_responded = false`, Stale/Offline-Pfad
- `svcResL[0].res.jnyL[i]` — Liste der Departures
  - `.stbStop.dCncl` — Cancelled
  - `.stbStop.dDateS` (`YYYYMMDD`) + `.stbStop.dTimeS` (`HHMMSS`) — Plan-Abfahrt
  - `.stbStop.dDateR` + `.stbStop.dTimeR` — Echtzeit-Abfahrt (optional)
  - `.prodL[0]` — Index in `svcResL[0].res.common.prodL[]`
  - `.dirTxt` — vom Server gerenderte Ziel-Anzeige (z. B. `"Wolkersdorf im Weinviertel Bahnhof"`). Nicht in `line_label` übernehmen — zu lang; nur für optionalen Direction-Healthcheck, falls künftig dirLoc unzuverlässig wird.
- `svcResL[0].res.common.prodL[i].nameS` — Linienkennung im Format `"S 1"` / `"REX 1"` (mit Leerzeichen); Parser strippt Whitespace zu `"S1"` / `"REX1"`. Das Feld `name` ist die Webapp-Display-Form mit angehängter Zugnummer (`"S 1 (Zug-Nr. 28842)"`) und wird nicht ausgewertet.
- `svcResL[0].res.common.prodL[i].prodCtx.lineId` (optional) — stabiler Linien-Schlüssel im Format `"at:obb:vor|S1:"`, nützlich falls künftig Deduplizierung oder Linien-Cache nötig wird. Aktuell nicht ausgewertet.
- `svcResL[0].res.common.prodL[i].himIdL` — Service-Meldungs-Referenzen (FREETEXT-IDs). Aktuell **nicht ausgewertet**; Parser sollte das Array überspringen, weil es ~80 % der Antwortgröße ausmacht (siehe V3 in §5).

**HTTP-Layer** — `HttpResult.http_status` aus [Schritt 1.1](#schritt-1--inetworkhttppost--esp32networkhttppost):

- `200` ist Normalfall (auch bei `err: "AID"` — HAFAS sendet semantische Fehler im Body, nicht im HTTP-Code)
- `4xx`/`5xx` → Transport-Fehler, Retry-Pfad in `api_fetcher::fetchPostWithRetry`

`Content-Type: application/json; charset=UTF-8`. Header `User-Agent` wird vom `HTTPClient` automatisch gesetzt; HAFAS akzeptiert es ohne Anpassung.

---

## Anhang B — RTC-Bilanz nach v2

Ausgehend von der Bilanz nach main-refactor Schritt 2.3 ([main-refactor-plan Anhang A](main-refactor-plan.md)):

| Posten | Bytes |
|---|---|
| **Stand nach main-refactor** | 7412 / 8192 (~780 B Reserve) |
| – `g_sched.hint[3..4]` entfällt (2 × `sizeof(ScheduleHint) = 40 B`) | −80 |
| – `g_meta.filter_miss_streak[3..4]` entfällt (2 × 1 B falls so groß; konservativ 4 B) | −4 |
| + `Departure::line_label[6]` in `g_snap` (4 Streams × 2 Slots × 6 B = 48 B; RLE-Frame indirekt — sehr klein dank Lauflängenkompression, hier konservativ als 0 angesetzt) | +48 |
| + `PersistedMeta` neu (Schritt 2.6): `has_any_data` 1 B + `last_success_at` 4 B + `auth_error_seen` 1 B + `ogd_auth_streak` 1 B + Alignment-Padding ~4 B | +12 |
| + Display-Redesign-Frame (mehr ink-Pixel durch Badges + Netzplan → RLE-Output ggf. größer, im worst-case sehr leeres Bild aber kleiner) | ±50 (Annahme; in Schritt 9 zu messen) |
| **Stand nach v2** | ~7388 / 8192 (~804 B Reserve, ±50) |

Reserve bleibt komfortabel, aber etwas enger als die ursprüngliche Schätzung (~864 B vor PersistedMeta-Erweiterung). Genauere Bilanz wird in Schritt 9 (Heap-Profiling) gemessen.

**Flash-Bilanz** (separat, kein RTC):

| Posten | Bytes |
|---|---|
| Bestehende Firmware | ~600 KB |
| Font-Daten (Option A — U8g2 mit 7 Roles) | ~10–15 KB |
| Font-Daten (Option B — Custom Bitmaps für VT323 + Silkscreen, 7 Größen) | ~30–60 KB |
| Render-Sub-Module (`badge`/`plan_marker`/`network_plan`/`display_state`) Code | ~5–8 KB |
| HAFAS-Parser + neuer Filter-Builder | ~3–5 KB |
| **Stand nach v2 (Option A)** | ~620 KB |
| **Stand nach v2 (Option B)** | ~650 KB |

ESP32-Flash 4 MB — beide Optionen weit unter Limit.

---

## Anhang C — Open Questions vor Schritt 0

| # | Frage | Antwort-Schritt / Festlegung |
|---|---|---|
| C1 | Aktuelle AID/Client/ver-Werte der ÖBB-Webapp? | 0.1 — **bestätigt 2026-05-19** via PoC-Smoke: AID `OWDL4fE4ixNiPBBm` + Client-Werte aus CONCEPT §v2-4 liefern HTTP 200 + `err=OK` + reale Departures über vier Aufrufe in 30 min. |
| C2 | Bitmask-Wert von `jnyFltrL` für S-Bahn+Regio+REX? | 0.2 — **bestätigt 2026-05-19**: `"63"` filtert Bus (`cls=64`) zuverlässig aus, lässt Bahn (`cls=16,32`) durch; identisch zu `"1023"`-Antwort. |
| C3 | Reicht `dirLoc` als Gegenrichtungs-Filter (Hauptverkehr/Mittag/Spätabend)? | 0.3 — **bestätigt 2026-05-19** (Mittag-Sample): Variante ohne dirLoc zeigt `S 2 → Mödling`, mit `dirLoc.extId=1290401` filtert HAFAS Gegenrichtung serverseitig aus. Sparse-Mode-Drift wird im Live-Betrieb am Display sichtbar; bei Auffälligkeit `dirTxt`-Healthcheck nachrüsten. |
| C4 | TLS-Cert-Issuer von `fahrplan.oebb.at` im `WiFiClientSecure`-Bundle? | 0.4 — **bestätigt 2026-05-19**: DigiCert Global G2 TLS RSA SHA256 2020 CA1 (im Standard-Bundle); Cert gültig bis 2026-12-08. |
| C5 | Variante 1 (kein Hint) oder Variante 2 (HAFAS-Hint-Call) für S-Bahn? | bleibt bei Variante 1 (Default aus CONCEPT §v2-8); Re-Eval nach v2-Roll-out wenn Bedarf |
| C6 | 2 oder 3 Slots für den S-Bahn-Stream? | bleibt bei 2 (Variante A aus CONCEPT §v2-5.2); Re-Eval nach v2-Roll-out |
| C7 | Heap-Verhalten des HAFAS-Calls? | misst Schritt 9 |
| C8 | **Font-Stack — Option A (U8g2), B (Custom-Bitmap) oder C (Hybrid)?** | 0.6 — **Festlegung 2026-05-19: Option A** (U8g2_for_Adafruit_GFX); Wechsel auf B nur wenn Sicht-Vergleich in Schritt 11.8 unzumutbar |
| C9 | Glyph-Substitution für `◌` (Boot) und `§9` (Auth) — ASCII-Fallback oder Custom-Glyph-Asset? | 0.7 — **Festlegung 2026-05-19: Custom-1-bit-Glyph-Assets** in `bitmap_fonts.cpp` (~200 B/Glyph) |
| C10 | Direction-Text-Quelle: statisch in `stream_labels.h` oder aus API-`towards` mit Truncate? | 0.8 — **Festlegung 2026-05-19: statisch** (Werte: 58A→Atz `"Atzgers."`, 58A→Hie `"Hietzing"`, 58B→Atz `"Atzgers."`, S-Bahn leer) |
| C11 | Pixel-Match-Test gegen Design-PNGs sinnvoll, oder bleibt Validation visuell? | Schritt 7.8 Empfehlung: nur visuell + Frame-Hash, kein PNG-Diff (siehe §3.3 letzter Punkt) |
| C12 | ▼-Glyph in `drawNetworkPlan` (7.6) — Bitmap-Font-Char oder Custom-Sprite? | 7.6 — **Festlegung 2026-05-19: 5×5-Custom-Triangle-Sprite** in `network_plan.cpp`. Begründung: U8g2-Standard-Fonts haben ▼ nur in größeren Logisoso-Varianten zuverlässig, auf 10-px-Höhe wechselt die Glyph-Verfügbarkeit zwischen Font-Versionen — ein 25-Pixel-Hand-Sprite ist deterministisch und kostet ~30 B. Identische Patternwahl wie für die 90-px-Glyphs in C9. |
