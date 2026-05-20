# Session E — HW-Sichtkontrolle (§11.1–11.8)

Stand: 2026-05-20 (18:30 UTC, Host-Vergleich) · Branch: `v2/sbahn-atzgersdorf` · Plan: [docs/v2-sbahn-migration-plan.md](../v2-sbahn-migration-plan.md) §4.3 + §11

**Vorgehen umgestellt auf Mock-View-Firmwares**: statt die States in der Wildbahn zu provozieren (NTP-Override, WiFi-AP aus, AID kaputt machen), gibt es pro State eine eigene Mini-Firmware unter `tools/mockview/` mit hartkodierten Mock-Daten. `make mockview-N` flasht, das Gerät rendert einmal und geht in Deep-Sleep — Display zeigt den State persistent.

**Ablauf pro State** (Host-only, kein Hardware-Test): Host-Render via `pio test -e native -f test_native_mockview_dump` → [docs/screenshots/host/mockview-N.png](../screenshots/host/) → Vergleich gegen [docs/design_handoff_display/screen-N-*.png](../design_handoff_display/) → Befund/Drift unten eintragen → Patch → Host-Render neu generieren → Re-Vergleich. **Kein Re-Flash / Re-Foto nötig**, weil HostCanvas pixel-identisch zum AdafruitGfxCanvas rendert (Commit `b47ca14`).

**Reihenfolge egal** — jeder Slot ist unabhängig. Empfehlung: 7 → 1 → 2 → 3 → 4 → 5 → 6 (Boot zuerst, weil er der einfachste Smoke-Test ist).

**Gate D → E**: `make ci` grün (✓ am 2026-05-20). Mock-View-Envs kompilieren alle grün.

**Vergleichs-Quellen** (2026-05-20):

- Host-Renders (Pixel-genau, 400×300, 1-bit): `docs/screenshots/host/mockview-N.png` — **autoritativ**, weil HostCanvas = AdafruitGfxCanvas
- Design-Soll (Browser-Mock, 1920×1440 @ 4.8×): `docs/design_handoff_display/screen-N-*.png`
- Geräte-Fotos (JPEG vom realen UC8176): `docs/screenshots/device/mockview-N.jpeg` — **nur historisch**, nicht für Validation

Vergleich nur Host ↔ Design. Geräte-Tests sind hier nicht nötig — die ganze Mockview-Umstellung hatte genau das Ziel, Renderer-Validation host-only zu machen.

**Numerierungs-Korrektur** (Stand 2026-05-20): Die Boot-Section unten ist als „§11.7" getaggt, gehört aber konzeptuell zu §11.1 (Pre-Render-Splash); die Auth-Fehler-Section ist korrekt §11.7. Heading nicht verschoben, weil Slot-IDs (mockview-7 / mockview-6) eindeutig sind und ein Rename die Git-History verwirrt.

---

## 11.7 → mockview-7 · Boot

- [x] `make mockview-7`
- [x] Foto: [docs/screenshots/device/mockview-7.jpeg](../screenshots/device/mockview-7.jpeg)
- [x] `◌`-Glyph + "BUSTAFERL" + "lädt Fahrplan…" + Version-Foot (`v2.0 · UC8176 · 400×300`)

**Vergleichs-PNG**: [screen-7-boot.png](../design_handoff_display/screen-7-boot.png)

**Drift / Notizen**:

- 2026-05-20 — Render 1:1 mit Design, keine Glyph-/Layout-Drift sichtbar.
- 2026-05-20 (Host-Vergleich 18:30 UTC) — Layout + Glyph-Position passen. **Zwei Mini-Drifts identifiziert**:
  1. **Sub-Ellipsis**: Design `lädt Fahrplan…` (U+2026), Host `lädt Fahrplan...` (3 × `.`). Kosmetisch — Helvetica-Font hat U+2026 in der `_te`-Variante; lohnt sich nur falls Side-by-Side stört. Patch: [src/render/display_state.cpp:57](../../src/render/display_state.cpp#L57).
  2. **Foot-Casing**: Design `V2.0 · UC8176 · 400×300` (ALL CAPS + `×` U+00D7), Host `v2.0 · UC8176 · 400x300` (mixed case + `x`). Source ist [src/config.h:88](../../src/config.h#L88) `DISPLAY_VERSION_STR`. Patch: String dort umstellen.

Beide Drifts sind nicht layoutkritisch (gleiche Glyph-Breite ± 1 px), aber fallen im Side-by-Side auf.

---

## 11.2 → mockview-1 · Normal

- [x] `make mockview-1`
- [x] Foto: [docs/screenshots/device/mockview-1.jpeg](../screenshots/device/mockview-1.jpeg)
- [ ] TG-Block: Badges = weiße Rechtecke + schwarzer Text, Direction "Atzgers." / "Hietzing", Plan-Marker `□` hinter 2. Slot 58A-Atz und 2. Slot 58A-Hie — **Drift L1/L2/L8** (Time-Spalte fehlt, Time-Spacing zu eng, Header-Size)
- [ ] 2-px Trennlinie unter TG — **Drift L9** (im Host ~1 px)
- [ ] EG-Block: kleinere Badges, kompaktere Reihe, Header "ENDEMANNGASSE · NACH SCHLEIFE" (Mittelpunkt `·`!), beide 58B-Slots gefüllt — **Drift L1/L2/L3/L6/L8** (Time-Spalte, Spacing, Badge-Width, Separator `-` statt `·`, Header-Size)
- [x] 1-px Trennlinie unter EG
- [x] Atzg-Block: 3 S-Bahn-Slots horizontal (3. Slot leer — **gewollt**, §3.3 + V14), Slot 1 = S2, Slot 2 = S3
- [ ] Netzplan: 5 Spalten, Diamond/Dot/Big-Marker an erwarteten Positionen, vertikale Linie zwischen Atzg-Diamonds, ▼ über Tull — **Drift L7** (Tull-Big-Marker zu klein, Atzg-Marker unten unsauber, Vertikallinie zu dünn)

**Mock-Daten** (Anchor 18:30 UTC, 1:1 nach screen-1-normal.png): 58A-Atz = +2 RT / +18 PL · 58A-Hie = +5 RT / +20 PL · 58B-Atz = +11 RT / +31 RT · S2-Hbf = +7 RT (S2) / +21 RT (S3). Atzg-Slot 3 leer (§3.3).

**Vergleichs-PNG**: [screen-1-normal.png](../design_handoff_display/screen-1-normal.png)

**Drift / Notizen**: 2026-05-20 — erste Sichtung (Anchor 04:30 UTC) ergab folgende Drift gegen Design:

1. EG-Header-Separator `·` → muss `-` sein (Design). **Fix**: [src/render/layout.cpp:246](../../src/render/layout.cpp#L246) — `ENDEMANNGASSE - NACH SCHLEIFE`.
2. Mock-Anchor `04:30 UTC` → muss `18:30 UTC` sein (Design zeigt 18:32 ff.). **Fix**: [tools/mockview/mock_data.h](../../tools/mockview/mock_data.h) — `kMockNow = 1779647400`, neuer `kMockNowNight = 1779681600` für mockview-3.
3. Mock-Daten wichen vom Design ab (58B Slot 1 leer, S-Bahn Slot 2 Plan/S2 statt RT/S3, Offsets +4/+14 statt +2/+18 etc.). **Fix**: [tools/mockview/mock_data.cpp](../../tools/mockview/mock_data.cpp) — `buildNormalSnapshot` + `buildNightSnapshot` 1:1 nach screen-1/-3.
4. ATZGERSDORF-Header: Render `->` (ASCII) statt `→` (Design). Font `helvB10_te` hat U+2192 nicht. **Fix**: 7×5 Custom-Sprite `ARROW_RIGHT_SPRITE` + neuer `drawAtzgHeader`-Helper in [src/render/layout.cpp](../../src/render/layout.cpp).
5. Plan-Marker (`□`): Code zeichnet 1-px hollow square (siehe [src/render/plan_marker.cpp](../../src/render/plan_marker.cpp)). Aus dem ersten Foto nicht eindeutig zu beurteilen (Kameraauflösung) — Close-up im Re-Foto prüfen.

Re-Foto nach Re-Flash erwartet. **NOTA BENE Punkt 1**: Zweite Sichtung 2026-05-20 (Claude-Design-Review) hat den Separator als `·` (Middle-Dot, U+00B7) verifiziert — die obige Korrektur auf `-` war ein Fehl-Fix. **Rollback** in [src/render/layout.cpp:286](../../src/render/layout.cpp#L286) erforderlich — siehe P0 unten.

**Host-Vergleich 2026-05-20 (18:30 UTC) — Iteration 1 (oberflächlich, korrigiert)**: Initial als "kein Drift" markiert. Re-Review durch Claude-Design (typografisch-präzise) hat **sieben zusätzliche Layout-Drifts** identifiziert, die in der ersten Iteration nicht gesehen wurden:

| Tag | Beschreibung | Patch |
|---|---|---|
| **L1** | **Zeiten nicht rechtsbündig** — Design: `auto \| 1fr \| auto`-Spaltenlayout, Zeiten kleben am rechten Display-Rand (~18 px Margin). Host: Zeiten hängen direkt am Richtungstext, keine eigenständige Time-Spalte. Gilt für TG, EG, Atzg. | P9 |
| **L2** | **Kein Abstand zwischen den zwei Zeit-Slots** — Design: ~20 px Lücke (TG), ~14 px (EG) zwischen 1./2. Slot. Host: ~3 px — `18:32 18:48` wirkt wie eine Einheit. | P10 |
| **L3** | **58B-Badge zu schmal / Polarität-Drift** — Im Host läuft das `B` aus dem Badge raus, Polarität wirkt inkonsistent gegenüber 58A oben. Badge-Padding/Min-Width prüfen ([src/render/badge.cpp](../../src/render/badge.cpp)). | P11 |
| **L6** | **Header-Separator `·` statt `-`** — Design verwendet U+00B7 (Middle Dot). Aktueller Code (Commit `6fb5dce`) hat `-`. **Rollback**. | P0 |
| **L7** | **Netzplan-Marker-Größen + Linien-Stärken** — (a) Tull als Big-Marker (8×8) im Design, im Host gleich groß wie Ende/Hietz (4×4). (b) Atzg unten als gefüllte Raute (◆), im Host eher offen/unsauber. (c) Vertikale Verbindungslinie zwischen den beiden Atzg-Markern im Host kürzer/dünner als Design. | P12 |
| **L8** | **Header-Hierarchie 12/10/10 px** — Design: TG-Header 12 px, EG/Atzg 10 px. Host: alle drei wirken gleich groß. Font-Role-Mapping in [src/render/canvas_*.cpp](../../src/render/canvas_host.cpp#L19-L22) (`Section_Header_TG` = `helvB12_te`, `Section_Header_EG_Atzg` = `helvB10_te`) sollte das korrekt liefern — verifizieren ob beide Header tatsächlich die richtige FontRole bekommen. | P13 |
| **L9** | **Separator-Stärken 2 px (TG-Bottom) vs. 1 px (EG-Bottom)** — Design markiert TG bewusst kräftiger als wichtigste Sektion. Host: beide ~1 px. [src/render/layout.cpp](../../src/render/layout.cpp) `drawLine` vs. `fillRect(h=2)` prüfen. | P14 |

**Punkt 5 (Plan-Marker S4/S7) entfällt mit §3.3 + V14**: Atzg-Slot 3 bleibt leer, also keine Plan-Marker dort nötig. Sobald Slot 3 in einer Folge-PR mit S4/S7 belegt wird, muss `□` mit.

**Lessons learned**: Erste Iteration hat nur Inhalte (Texte, Zahlen) verglichen, nicht Geometrie/Typografie. Layout-Drifts brauchen Pixel-Lineal + Spaltensicht, nicht "sieht ähnlich aus". Für §11.8 zukünftig: Side-by-Side mit Grid-Overlay statt Frei-Vergleich.

---

## 11.3 → mockview-2 · Veraltet (Stale)

- [x] `make mockview-2`
- [x] Foto: [docs/screenshots/device/mockview-2.jpeg](../screenshots/device/mockview-2.jpeg)
- [x] Alle Slots `--:--` (oder `??:??` je nach Renderer)
- [x] **Keine** Plan-Marker
- [ ] Layout-Struktur bleibt (TG/EG/Atzg-Blöcke + Netzplan) — **erbt L1/L2/L3/L6/L7/L8/L9 von mockview-1** (selbe Board-Layout-Pipeline)

**Vergleichs-PNG**: [screen-2-veraltet.png](../design_handoff_display/screen-2-veraltet.png)

**Drift / Notizen**:

- 2026-05-20 (Host-Vergleich 18:30 UTC, Iteration 1): Texte 1:1, "kein Drift" eingetragen — oberflächlich, nur Inhalte verglichen.
- 2026-05-20 (Iteration 2, Claude-Design-Review): Stale rendert dieselbe `drawBoard`-Pipeline wie Normal — alle Layout-Drifts L1/L2/L3/L6/L7/L8/L9 aus mockview-1 gelten **identisch hier**. Nach Fix von P0/P9–P14 ist Stale automatisch mit-gefixt.
- Stale-spezifisch: `--:--` als Time-Glyphen, ohne `□`-Marker — der `--:--`-String wird (sobald Time-Spalte rechts liegt, P9) automatisch rechtsbündig sitzen. Kein eigener Patch für Stale nötig.
- Atzg-Slot 3 bleibt leer — Design `S7 --:--` per §3.3 akzeptiert.

---

## 11.4 → mockview-3 · Nachtbetrieb

- [x] `make mockview-3`
- [x] Foto: [docs/screenshots/device/mockview-3.jpeg](../screenshots/device/mockview-3.jpeg)
- [x] Alle Zeiten sind Plan-Zeiten (alle mit `□`)
- [x] Morgen-Erst-Abfahrten sichtbar

**Mock-Daten**: alle Departures Plan-fallback, ca. 20 h nach Anchor angesiedelt (Frühmorgen).

**Vergleichs-PNG**: [screen-3-nachtbetrieb.png](../design_handoff_display/screen-3-nachtbetrieb.png)

**Drift / Notizen**:

- 2026-05-20 (Host-Vergleich 18:30 UTC, Iteration 1): Texte + Plan-Marker korrekt — "kein Drift" eingetragen, aber Layout nicht geprüft.
- 2026-05-20 (Iteration 2, Claude-Design-Review): Nacht rendert dieselbe `drawBoard`-Pipeline → **L1/L2/L3/L6/L7/L8/L9 gelten identisch**. Nach Fix von P0/P9–P14 ist Nacht mit-gefixt. Bonus: weil alle Zeiten `□`-Marker tragen, ist L2 (Time-Spacing) hier besonders störend — der `□` klemmt zwischen den Slots.
- Atzg-Slot 3 bleibt leer — Design `S7 05:13□` per §3.3 akzeptiert.

---

## 11.5 → mockview-4 · Keine Abfahrten

- [x] `make mockview-4`
- [x] Foto: [docs/screenshots/device/mockview-4.jpeg](../screenshots/device/mockview-4.jpeg)
- [x] Zentraler `—` (72 px) + "Keine Abfahrten" + "in den nächsten 20 min"
- [x] **Kein** Netzplan

**Vergleichs-PNG**: [screen-4-keine-abfahrten.png](../design_handoff_display/screen-4-keine-abfahrten.png)

**Drift / Notizen**:

- 2026-05-20 (Host-Vergleich 18:30 UTC) — Render 1:1 mit Design.
  - 72-px `—`-Bar an der erwarteten Position (vertikal zentriert im 90-px Glyph-Slot) ✓
  - `KEINE ABFAHRTEN` als Title (Helvetica Bold 18) ✓
  - `in den nächsten 20 min` als Sub (Helvetica Regular 14, Umlaut korrekt) ✓
  - Kein Netzplan ✓
- **Keine offene Drift für Quiet-State.**

---

## 11.6 → mockview-5 · Kein Empfang (Offline)

- [x] `make mockview-5`
- [x] Foto: [docs/screenshots/device/mockview-5.jpeg](../screenshots/device/mockview-5.jpeg)
- [x] Fullscreen `!` (90 px) + "Kein Empfang"
- [x] Sub mit Last-Fetch (Mock: ~7 min ago)
- [x] Foot mit Retry-Hinweis (Mock: 23 s)

**Vergleichs-PNG**: [screen-5-kein-empfang.png](../design_handoff_display/screen-5-kein-empfang.png)

**Drift / Notizen**:

- 2026-05-20 (Host-Vergleich 18:30 UTC) — Glyph + Title 1:1, **Foot-Casing weicht ab**.
  - `!`-Glyph (90 px Custom-Sprite) ✓
  - `KEIN EMPFANG` Title ✓
  - Sub-Text: Design `Letzte Aktualisierung 17:48`, Host `Letzte Aktualisierung 18:22`. **Mock-Drift**, kein Layout-Issue — Design-Anchor und Mock-`kMockNow - 7 min` divergieren um ~34 min. Cosmetic; lohnt nur falls 1:1-Side-by-Side gewünscht.
  - **Foot-Casing-Drift**: Design `WLAN · RETRY IN 30S` (ALL CAPS), Host `WLAN · Retry in 23s` (mixed case). Source: [src/render/display_state.cpp:79](../../src/render/display_state.cpp#L79) `"WLAN · Retry in %ds"`. Patch nötig — siehe Umsetzungsplan unten.
  - Foot-Wert 30S vs. 23s ist zusätzlich Mock-Drift (akzeptabel, da Foot ohnehin neu formatiert).

---

## 11.7 → mockview-6 · Auth-Fehler

- [x] `make mockview-6`
- [x] Foto: [docs/screenshots/device/mockview-6.jpeg](../screenshots/device/mockview-6.jpeg)
- [x] Fullscreen `§9` (90 px) + "Auth-Fehler"
- [x] AID-Short im Foot (Mock: `OWDL4fE4`)
- [x] HTTP-Code im Foot (Mock: `200` — HAFAS-Pfad, kein 401/403)

**Vergleichs-PNG**: [screen-6-auth-fehler.png](../design_handoff_display/screen-6-auth-fehler.png)

**Drift / Notizen**:

- 2026-05-20 (Host-Vergleich 18:30 UTC) — Glyph + Title + Sub 1:1, **Foot weicht in Format + Casing ab**.
  - `§9`-Glyph (90 px) ✓
  - `AUTH-FEHLER` Title ✓
  - Sub `Client-ID veraltet · bitte neu registrieren` ✓
  - **Foot-Format-Drift**: Design `AID 0X8F · ERR 401`, Host `OWDL4fE4 · ERR 200`.
    - Literal `"AID "`-Präfix (4 Zeichen) fehlt im Host (Format-String `"%s · ERR %d"` an [src/render/display_state.cpp:98](../../src/render/display_state.cpp#L98) übergibt `aid_short` ohne führendes `"AID "`).
    - Casing: Design ALL CAPS, Host mixed case (gleicher Renderer-Pfad wie mockview-5).
    - Mock-Werte `OWDL4fE4`/`200` sind per Plan §11.7 korrekt (HAFAS-Pfad zeigt HTTP 200 + `err: "AID"`); Designer hat klassischen `401` gezeichnet. Mock-Werte bleiben — sie sind realistischer als die Design-Annahme.
  - Patch: Format-String auf `"AID %s · ERR %d"` umstellen + Casing ALL CAPS — siehe Umsetzungsplan unten.

---

## 11.8 · Font-/Pixel-Kalibrierung

- [x] Pro State je 1 Foto direkt neben dem Design-PNG (Side-by-Side via `docs/screenshots/host/` und `docs/screenshots/device/`)
- [x] Glyph-Drift dokumentieren (Y-Offset, Höhen, Baseline) — siehe Tabelle unten
- [ ] §4.1-Annahme im Plan aktualisieren mit beobachteter Drift — TODO im Anschluss an Patch-Block
- [ ] **Wenn Drift unzumutbar** → Decision auf Custom-Bitmap-Fonts (Option B aus Schritt 0.6) als Follow-up-PR — **Stand 2026-05-20: zumutbar, Option A bleibt**

**Drift-Inventar** (State · Element · Soll · Ist · Δ · Patch nötig?):

| State | Element | Soll (Design) | Ist (Host) | Δ | Patch nötig? |
|---|---|---|---|---|---|
| 1/2/3 Board | **Time-Spalte rechtsbündig** | `auto \| 1fr \| auto`, Zeiten am rechten Rand | Zeiten am Text geklebt, keine Spalte | Layout-Bug | **Ja — L1 → P9** |
| 1/2/3 Board | **Time-Slot-Spacing** | ~20 px TG / ~14 px EG zwischen den 2 Slots | ~3 px | Layout-Bug | **Ja — L2 → P10** |
| 1 Normal | **58B-Badge** | sauberes weißes Rechteck, schwarzes `58B` | `B` läuft raus / Polarität inkonsistent | Badge-Width-Bug | **Ja — L3 → P11** |
| 1/2/3 Board | **EG-Header-Separator** | `·` (U+00B7) | `-` (Bindestrich) | Falscher Glyph nach Commit `6fb5dce` | **Ja — L6 → P0** (Rollback) |
| 1/2/3 Board | **Netzplan-Marker** | Tull = Big-Marker 8×8; Atzg unten = ◆ gefüllt; Vertikallinie sichtbar zwischen beiden Atzg | Tull = 4×4 wie Ende/Hietz; Atzg unten offen/unsauber; Vertikallinie zu dünn | Geometrie-Drift | **Ja — L7 → P12** |
| 1/2/3 Board | **Header-Hierarchie** | TG 12 px / EG 10 px / Atzg 10 px | alle ~gleich groß | Font-Role-Mapping verifizieren | **Ja — L8 → P13** |
| 1/2/3 Board | **Separator-Stärken** | TG-Bottom 2 px / EG-Bottom 1 px | beide ~1 px | Linien-Bug | **Ja — L9 → P14** |
| 1/2/3 | Atzg-Slot 3 | `S7 19:05□` / `S7 --:--` / `S7 05:13□` | leer (kein Badge, kein `--:--`) | Slot fehlt | Nein — §3.3 + V14 akzeptiert |
| 4 Quiet | `—`-Bar + Title + Sub | 1:1 nach screen-4 | 1:1 | 0 | Nein |
| 5 Offline | Sub-Text Zeit | `Letzte Aktualisierung 17:48` | `… 18:22` | Mock-Anchor | Optional (P6, nicht empfohlen) |
| 5 Offline | Foot-Casing | `WLAN · RETRY IN 30S` | `WLAN · Retry in 23s` | Casing | **Ja — P1** |
| 6 Auth | Foot-Präfix | `AID 0X8F · ERR 401` | `OWDL4fE4 · ERR 200` | Präfix `"AID "` fehlt | **Ja — P2** |
| 6 Auth | Foot-Casing | ALL CAPS | mixed case | Casing | **Ja — P3** |
| 6 Auth | AID-Wert + HTTP-Code | `0X8F` / `401` | `OWDL4fE4` / `200` | Designer-Annahme vs. Plan §11.7 | Nein — Mock korrekt per Plan |
| 7 Boot | Sub-Ellipsis | `lädt Fahrplan…` (U+2026) | `lädt Fahrplan...` | Glyph (3 Punkte vs. `…`) | Optional (P4) |
| 7 Boot | Foot-Casing + × | `V2.0 · UC8176 · 400×300` | `v2.0 · UC8176 · 400x300` | Casing + `×` (U+00D7) vs. `x` | **Ja — P5** |

**Bewertung Option-A-Drift (revidiert)**: Iteration 1 hat nur Inhalts-Drifts erfasst (P1–P5, alle Text-Literal). Iteration 2 (Claude-Design-Review) hat **strukturelle Layout-Drifts** (L1/L2/L3/L7/L8/L9) und einen Rollback-Punkt (L6/P0) hinzugefügt. Die Layout-Drifts sind kritisch — Time-Spalte und Header-Hierarchie sind keine Kosmetik, sondern Lesbarkeits-Determinanten. Option B (Custom-Bitmap-Fonts) bleibt trotzdem unbenutzter Folge-PR: alle L-Patches sind in der bestehenden Layout-Geometrie umsetzbar, kein Font-Stack-Wechsel nötig.

---

## Cold-Boot-Doppel-Refresh (§11.1 — **deferred zu Session F/G**)

§11.1 verifiziert den Pre-Render-Splash *in der echten Boot-Sequenz* — der einzige verbleibende Hardware-Test in §11, weil er Wake-Cycle-Verhalten testet, nicht Renderer-Output. Da die Mockview-Umstellung Renderer-Validation host-only macht, ist §11.1 nicht mehr Teil von Session E (Display-Sichtkontrolle), sondern zieht zu Session F/G (Integration-Tests gegen echtes Gerät).

**Hier nur Erinnerungs-Eintrag**, kein Hardware-Schritt in E:

- [ ] §11.1 → deferred (Session F/G): Cold-Boot-Doppel-Refresh-Verifikation via `make flash` + Serial-Log-Check auf `[cold] pre-render boot` / `[cold] fetch start` / `[cold] post-render normal`

---

## Patch-Block (Squash-Ziel: "Render: §11 Sichtkontroll-Anpassungen")

Liste der Render-Korrektur-Commits, die während E entstehen — werden am Ende der Session zu **einem** Commit gesquasht (§4.3 Branch-Disziplin). Die mockview-Sources unter `tools/mockview/` gehören dazu, wenn nicht bereits separat committed.

Stand 2026-05-20 (Iteration 2 nach Claude-Design-Review) — **dreizehn Patches** (P0/P1–P5 Inhalts-Drift, P9–P14 Layout-Drift, P6/P7/P8 Folge-Arbeit):

**Sonderscreens (Iteration 1, Inhalts-Drift)**:

- [ ] **P1** — Offline-Foot ALL CAPS + Sekunden-Suffix (`WLAN · RETRY IN %dS`) — [src/render/display_state.cpp:79](../../src/render/display_state.cpp#L79)
- [ ] **P2** — Auth-Foot `"AID "`-Präfix einbauen (`AID %s · ERR %d`) — [src/render/display_state.cpp:98](../../src/render/display_state.cpp#L98)
- [ ] **P3** — Auth-Foot ALL CAPS (AID-Wert hex-uppercase, `ERR` schon CAPS) — [src/render/display_state.cpp:98](../../src/render/display_state.cpp#L98) + Aufrufer
- [ ] **P4** — Boot-Sub Ellipsis `…` (U+2026) statt drei `...` — [src/render/display_state.cpp:57](../../src/render/display_state.cpp#L57) (Font-Coverage prüfen!)
- [ ] **P5** — Boot-Foot ALL CAPS + `×` (U+00D7) statt `x` — [src/config.h:88](../../src/config.h#L88) `DISPLAY_VERSION_STR`

**Board-States (Iteration 2, Layout-Drift)**:

- [ ] **P0** — **Rollback EG-Header-Separator** `-` zurück auf `·` (U+00B7) — [src/render/layout.cpp:286](../../src/render/layout.cpp#L286). Klärt L6.
- [ ] **P9** — **Time-Spalte rechtsbündig** im TG/EG/Atzg-Block — `auto | 1fr | auto`-Layout in [src/render/layout.cpp](../../src/render/layout.cpp) `drawSectionTG`/`drawSectionEG`/`drawSectionAtzg`. Time-Block-Right-Edge = `FB_W - 18`. Klärt L1.
- [ ] **P10** — **Time-Slot-Spacing** ~20 px (TG) / ~14 px (EG) zwischen 1. und 2. Slot — [src/render/layout.cpp](../../src/render/layout.cpp), Konstanten `TIME_SLOT_GAP_TG` / `TIME_SLOT_GAP_EG`. Klärt L2.
- [ ] **P11** — **58B-Badge-Width** korrigieren — [src/render/badge.cpp](../../src/render/badge.cpp) `drawBadge(..., BadgeSize::md)`. `B` muss vollständig im Rechteck sitzen; Min-Width prüfen gegen Glyph-Breite + Padding (Werte aus [docs/design_handoff_display/display.jsx](../design_handoff_display/display.jsx) Z. 158-160). Klärt L3.
- [ ] **P12** — **Netzplan-Marker + Linien** — [src/render/network_plan.cpp](../../src/render/network_plan.cpp): (a) Tull als Big-Marker 8×8 (statt 4×4-Dot), (b) Atzg unten als gefüllte Raute (◆ 7×7 rotated), (c) Vertikallinie zwischen den beiden Atzg-Markern auf volle Höhe + ggf. 2 px. Klärt L7.
- [ ] **P13** — **Header-FontRole** — verifizieren ob `Section_Header_TG` (helvB12_te) tatsächlich nur für TG, `Section_Header_EG_Atzg` (helvB10_te) für EG/Atzg verwendet wird. Falls falsche Zuordnung in [src/render/layout.cpp](../../src/render/layout.cpp), korrigieren. Klärt L8.
- [ ] **P14** — **Separator-Stärken** — TG-Bottom-Separator von `drawLine` (1 px) auf `fillRect(0, y, FB_W, 2, INK)` ändern, EG-Bottom bleibt 1 px — [src/render/layout.cpp](../../src/render/layout.cpp). Klärt L9.

**Folge-Arbeit**:

- [ ] **P6** (optional, **nicht empfohlen**) — Offline-Sub-Mock-Anchor an Design 17:48 angleichen — [tools/mockview/main_5_kein_empfang.cpp](../../tools/mockview/main_5_kein_empfang.cpp)
- [ ] **P7** (Folge-Verifikation) — Host-PNGs neu generieren via `pio test -e native -f test_native_mockview_dump` für mockview-1/-2/-3/-5/-6/-7, Re-Vergleich gegen Design. **Kein Re-Flash, kein Re-Foto** — HostCanvas ist pixel-identisch zum Gerät (Commit `b47ca14`).
- [ ] **P8** — §4.1-Annahme im Migration-Plan aktualisieren (Glyph-Drift akzeptiert, Option B nicht nötig — **aber Layout-Drift war erwartungsfehler** in §4.1, nachjustieren)

---

## Abnahme

- [ ] Alle 7 States Host-PNG ↔ Design-PNG vergleichbar, Drift = 0 — **Stand 2026-05-20 (Iteration 2): 1/7 driftfrei (Quiet/4), 6/7 mit Patches offen (1/2/3 mit L-Patches, 5/6/7 mit P-Patches)**
- [ ] §11.1 Cold-Boot-Doppel-Refresh — **deferred zu Session F/G** (Hardware-Test, nicht Display-Sichtkontrolle)
- [x] Host-Renders in [docs/screenshots/host/](../screenshots/host/) — autoritativ (HostCanvas = AdafruitGfxCanvas)
- [ ] Patch-Block (P0/P1–P5/P9–P14 + Folge P7/P8) abgearbeitet + gesquasht zu einem Commit auf `v2/sbahn-atzgersdorf`
- [ ] **Gate E → F**: Display final, Tests können in F umgestellt werden

11.9 (Linien-Längen-Stress mit REX1) und 11.10 (24h-Soak) gehören laut §4.3 zu **Session G**, nicht E.

---

## Anpassungs-Umsetzungsplan (Stand 2026-05-20, Iteration 2 nach Claude-Design-Review)

Reihenfolge optimiert für minimale Re-Flash-Kosten: alle Code-Patches zuerst (in zwei Clustern — Sonderscreens und Board-Layout — getrennt PR-fähig), dann **eine** Re-Flash-Runde über mockview-1/-2/-3/-5/-6/-7.

### Übersicht

**Cluster A — Sonderscreen-Inhalte (P1–P5, ~30 min)**: alle in [src/render/display_state.cpp](../../src/render/display_state.cpp) + [src/config.h](../../src/config.h), reine String-Literal- und Format-Drift. Keine Geometrie-Änderung.

**Cluster B — Board-Layout-Geometrie (P0/P9–P14, ~2–3 h)**: in [src/render/layout.cpp](../../src/render/layout.cpp), [src/render/badge.cpp](../../src/render/badge.cpp), [src/render/network_plan.cpp](../../src/render/network_plan.cpp). Tatsächliche Renderer-Geometrie-Patches — Spaltenlayout, Spacing, Marker-Größen, Linien-Stärken. **Kritischer Cluster**, weil mockview-1/2/3 davon abhängen und §11.8-Bewertung dort hängt.

**P6/P7/P8**: Folge-Arbeit (optionale Mock-Angleichung, Re-Foto-Runde, Plan-Doku).

Cluster A und B sind unabhängig — können parallel oder hintereinander entwickelt werden. Squash am Ende auf einen Commit.

### P1 — Offline-Foot ALL CAPS (5 min)

**Was**: Format-String von `"WLAN · Retry in %ds"` auf `"WLAN · RETRY IN %dS"` + Fallback `"WLAN · retrying..."` auf `"WLAN · RETRYING..."`.

**Wo**: [src/render/display_state.cpp:79](../../src/render/display_state.cpp#L79), [src/render/display_state.cpp:82](../../src/render/display_state.cpp#L82).

**Wie**:

```cpp
if (retry_in_s > 0) {
  std::snprintf(foot_buf, sizeof(foot_buf), "WLAN · RETRY IN %dS", retry_in_s);
} else {
  std::snprintf(foot_buf, sizeof(foot_buf), "WLAN · RETRYING...");
}
```

**Test**: `make test-native-host-render` (sollte schon Snapshot-Frame-Hash haben — aktualisieren mit `--update-baseline`).

**Risiko**: Keiner — pure String-Literal-Änderung, FOOT_BUF_CAP (48 B) reicht.

### P2 — Auth-Foot `"AID "`-Präfix (5 min)

**Was**: Format-String von `"%s · ERR %d"` auf `"AID %s · ERR %d"`, Fallback-AID von `"AID ---"` auf `"---"` (damit nicht `"AID AID ---"`).

**Wo**: [src/render/display_state.cpp:94-101](../../src/render/display_state.cpp#L94-L101).

**Wie**:

```cpp
char foot_buf[FOOT_BUF_CAP];
const char *aid =
    (aid_short != nullptr && aid_short[0] != '\0') ? aid_short : "---";
if (http_code > 0) {
  std::snprintf(foot_buf, sizeof(foot_buf), "AID %s · ERR %d", aid, http_code);
} else {
  std::snprintf(foot_buf, sizeof(foot_buf), "AID %s · ERR ---", aid);
}
```

**Test**: gleicher Snapshot-Hash-Pfad wie P1. Bestehender Unit-Test in `test_native_drawAuth` evtl. anpassen (sucht ggf. nach `"AID "`-Prefix oder dessen Abwesenheit).

**Risiko**: FOOT_BUF_CAP (48 B) noch ausreichend: `"AID OWDL4fE4 · ERR 200"` = 22 B, plus Reserve für längere AIDs (cap = 10 von `AUTH_AID_SHORT_CAP`).

### P3 — Auth-Foot ALL CAPS (10 min)

**Was**: AID-Wert hex-uppercase rendern. `OWDL4fE4` hat bereits gemischte Hex-Stellen — entweder im Aufrufer (`mockview/main_6_auth_fehler.cpp`) auf `OWDL4FE4` umstellen oder im Renderer per `std::toupper` während des `snprintf`.

**Wo**: [tools/mockview/main_6_auth_fehler.cpp:15](../../tools/mockview/main_6_auth_fehler.cpp#L15) (Mock-Wert) **und** Produktions-Aufrufer (Suche nach `auth_aid_short`-Zuweisung).

**Empfehlung**: Im Renderer per `for (char *p = foot_buf; *p; ++p) *p = std::toupper(*p);` nach `snprintf`. Damit ist die Quelle (HAFAS-Response) unverändert — die AID bleibt im Log-/Debug-Pfad korrekt-case, nur das Display-Render verwendet ALL CAPS.

**Wie** (an display_state.cpp angedockt nach Z. 101):

```cpp
for (char *p = foot_buf; *p != '\0'; ++p) {
  *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
}
```

**Achtung**: `std::toupper` kennt kein UTF-8 — `·` (U+00B7, 2 B in UTF-8) wird byte-weise behandelt. Bei `0xC2 0xB7` ergibt `toupper(0xC2) = 0xC2` und `toupper(0xB7) = 0xB7` (kein A–Z). Also safe.

**Risiko**: Auch der `·`-Trenner und Hex-Digits laufen durch `toupper` — Hex-Digits werden ALL CAPS (`f` → `F`), `·` bleibt. Genau gewünscht.

### P4 — Boot-Sub `…` statt `...` (5 min, optional)

**Was**: String-Literal `"lädt Fahrplan..."` auf `"lädt Fahrplan…"` (echtes U+2026).

**Wo**: [src/render/display_state.cpp:57](../../src/render/display_state.cpp#L57).

**Coverage-Check**: `Fullscreen_Sub` = `u8g2_font_helvR14_te`. Die `_te`-Variante (German Extended) enthält Umlaute, prüfen ob U+2026 dabei ist. Wenn nicht, bleibt der String `...` (drei Punkte) — kein Patch.

**Wie**: Vor dem Edit kurz `grep "0x2026" $(find ~/.platformio -name "u8g2_font_helvR14_te.c")` o.ä. — falls Glyph vorhanden, edit; sonst Drift akzeptieren und in §4.1 dokumentieren.

**Risiko**: Wenn Glyph fehlt, rendert U8g2 ein `?` oder leeres Rechteck. **Vorher prüfen.**

### P5 — Boot-Foot ALL CAPS + `×` (5 min)

**Was**: `DISPLAY_VERSION_STR` von `"v2.0 · UC8176 · 400x300"` auf `"V2.0 · UC8176 · 400×300"`.

**Wo**: [src/config.h:88](../../src/config.h#L88).

**Wie**:

```c
#define DISPLAY_VERSION_STR "V2.0 · UC8176 · 400×300"
```

**Coverage-Check**: `×` (U+00D7) ist in `helvR08_te` definitiv enthalten (Latin-1 Multiplication Sign). `V` und `UC` sind Standard-ASCII. Kein Risiko.

**Risiko**: `DISPLAY_VERSION_STR` wird ggf. an anderen Stellen (Serial-Log, Build-Info) verwendet — kurz `grep DISPLAY_VERSION_STR src/` prüfen, ob ein Aufrufer Lowercase erwartet. (Voraussichtlich nicht — es ist ein Anzeige-String.)

### P6 — Offline-Mock-Anchor angleichen (optional, 5 min)

**Was**: Mock-`last_fetch_at` im Offline-Mockview so wählen, dass das HH:MM dem Design (`17:48`) entspricht. Aktuell zeigt der Host `18:22` (Mock = `kMockNow - 7 min`, Anchor 18:30 UTC → 18:22 lokal, mit `Europe/Vienna` evtl. +1h).

**Wo**: [tools/mockview/main_5_kein_empfang.cpp](../../tools/mockview/main_5_kein_empfang.cpp).

**Wie**: `last_fetch_at = kMockNow - 42*60;` (42 min vor 18:30 UTC = 17:48 UTC; Renderer formatiert per `localtime_r` — wenn Test-Env `TZ=UTC`, dann 17:48, sonst 19:48 in MESZ). Kalibration mit ersten Re-Run abgleichen.

**Verzichtbar**: Wenn man den semantischen Sinn "Last-Fetch ~7 min ago" beibehalten will, gibt es keinen sachlichen Grund, den Wert dem Design anzupassen — der Designer hat einen anderen Zeitpunkt gewählt. **Empfehlung: nicht patchen**, sondern in der Drift-Tabelle als "Designer-Variante" markieren.

### P7 — Host-PNG-Regenerierung + Re-Vergleich (10 min)

**Reihenfolge**:

1. Patches als Commits auf `v2/sbahn-atzgersdorf` (Cluster A: P1-P5; Cluster B: P0/P9-P14).
2. Host-Renders neu erzeugen: `pio test -e native -f test_native_mockview_dump` → `docs/screenshots/host/mockview-N.png` werden überschrieben (siehe [test/test_native_mockview_dump/test_main.cpp](../../test/test_native_mockview_dump/test_main.cpp)).
3. Pro betroffenem State (1/2/3/5/6/7): Host-PNG ↔ Design-PNG visuell vergleichen.
4. Drift-Tabelle in §11.8 aktualisieren (Δ → 0 für jeden geklärten Patch; offene Punkte als "BLOCKED — needs human" markieren wenn Patch nicht erfolgreich).

**Tooling**: PGM → PNG nicht selbst basteln; `scripts/pgm-to-png.py` existiert bereits (siehe Memory `pgm-png-script.md`).

**Kein Re-Flash, kein Re-Foto**: HostCanvas rendert pixel-identisch zum AdafruitGfxCanvas (Memory `host-renderer-parity.md`). Geräte-Tests entfallen vollständig — die ganze Mockview-Umstellung hatte genau dieses Ziel.

### P0 — Rollback EG-Header-Separator (2 min, **Cluster B**)

**Was**: String-Literal `"ENDEMANNGASSE - NACH SCHLEIFE"` zurück auf `"ENDEMANNGASSE · NACH SCHLEIFE"` (U+00B7 Middle Dot). Iteration 1 hatte fälschlich auf `-` umgestellt; Claude-Design hat als `·` verifiziert.

**Wo**: [src/render/layout.cpp:286](../../src/render/layout.cpp#L286).

**Wie**:

```cpp
LAYOUT_EG_HEADER_Y, "ENDEMANNGASSE · NACH SCHLEIFE");
```

**Coverage-Check**: `·` (U+00B7) ist in `helvB10_te` enthalten (Latin-1). Bereits im "ATZGERSDORF →"-Aufruf via Custom-Sprite umgangen, aber für `·` keine Sonderbehandlung nötig — direkt im String.

**Risiko**: Keiner — wir hatten den `·` bis Commit `6fb5dce` und es funktionierte.

### P9 — Time-Spalte rechtsbündig (45 min, **Cluster B**)

**Was**: TG/EG/Atzg-Block bekommen ein `auto | 1fr | auto`-Layout: Badge links (auto), Direction flexibel (1fr, dehnt sich), Times rechts (auto, rechtsbündig). Rechte Display-Margin: ~18 px (aus [docs/design_handoff_display/display.jsx](../design_handoff_display/display.jsx) verifizieren).

**Wo**: [src/render/layout.cpp](../../src/render/layout.cpp) — `drawSectionTG`, `drawSectionEG`, `drawSectionAtzg`.

**Wie** (für TG, analog für EG; Atzg ist 3 Slots ohne Direction):

```cpp
// Vor dem Patch: Cursor wandert von Badge → Direction → Times links-nach-rechts
// Nach dem Patch: zwei getrennte Cursor:
//   - Cursor_L startet links, zeichnet Badge + Direction (1fr-flexibel)
//   - Cursor_R startet rechts (FB_W - 18 - text_width(Time2)), zeichnet Times rechtsbündig
constexpr int RIGHT_MARGIN = 18;
int time2_x = FB_W - RIGHT_MARGIN - canvas.textWidth(time2_str);
int time1_x = time2_x - TIME_SLOT_GAP_TG - canvas.textWidth(time1_str);
canvas.setCursor(time1_x, row_y); canvas.print(time1_str);
canvas.setCursor(time2_x, row_y); canvas.print(time2_str);
```

**Test**: `test_native_render_input` — Frame-Hash bricht, mit `--update-baseline` neu setzen. Visuell: Time1+Time2 müssen Pixel-exakt am rechten Rand ankleben (FB_W=400, RIGHT_MARGIN=18 → Time2-Right-Edge = 382).

**Risiko**: `□`-Plan-Marker werden direkt hinter Time geklebt — muss in die `time_x`-Rechnung einbezogen werden. Sonst läuft `□` über den Rand. Patch: `time2_x -= PLAN_MARKER_WIDTH` wenn Time2 plan ist.

### P10 — Time-Slot-Spacing (10 min, **Cluster B**)

**Was**: Konstanten `TIME_SLOT_GAP_TG = 20` und `TIME_SLOT_GAP_EG = 14` einführen (Werte aus display.jsx verifizieren). Verwenden im `time1_x = time2_x - TIME_SLOT_GAP - …`-Ausdruck aus P9.

**Wo**: [src/render/layout.cpp](../../src/render/layout.cpp) (Konstanten + Verwendung in `drawSection*`).

**Wie**: Bereits in P9-Patch enthalten — beide zusammen ein einziger Edit.

**Risiko**: Wenn Glyphen-Breite (`textWidth`) bei Helvetica anders als bei VT323 berechnet wird, kann die nominale 20-px-Lücke optisch zu klein wirken. Visuell justieren auf 22/24 px falls nötig.

### P11 — 58B-Badge-Width (15 min, **Cluster B**)

**Was**: `drawBadge(..., BadgeSize::md)` muss für `"58B"` (3 Glyphen) eine Min-Width liefern, die das `B` vollständig umschließt. Aktuell rendert das `B` raus.

**Wo**: [src/render/badge.cpp](../../src/render/badge.cpp).

**Wie**: Min-Width = `text_width + 2 × padding_x`. Verifizieren ob `padding_x` für `BadgeSize::md` mit display.jsx (Z. 158-160) übereinstimmt. Wenn aktuelle Berechnung `rect_w = text_width + pad_l + pad_r`, dann ggf. `pad_l/pad_r` zu klein.

**Test**: `test_native_drawBadge` — Pixel-Mask gegen erwartete Bounding-Box prüfen. Wenn der Test fehlt, neu schreiben.

**Risiko**: Wenn ich `BadgeSize::md` breiter mache, wandert das `Atzgers.` einen Pixel nach rechts — kollidiert mit P9 (rechtsbündige Times) nicht, weil 1fr-Spalte das auffängt.

### P12 — Netzplan-Marker + Linien (30 min, **Cluster B**)

**Was**: Drei Sub-Patches in [src/render/network_plan.cpp](../../src/render/network_plan.cpp):

1. **Tull als Big-Marker** — `drawNetworkPlan` zeichnet aktuell Tull mit `drawDot(4×4)`; muss `drawBigMarker(8×8)` werden (gefülltes Quadrat, "you-are-here").
2. **Atzg unten als gefüllte Raute** — aktuell evtl. nur 4×4-Hollow oder unsauber gerendert. Sollte `drawDiamond(7×7, filled)` sein, identisch zur oberen Atzg-Raute.
3. **Vertikallinie Atzg-oben ↔ Atzg-unten** — aktuell zu dünn/kurz. Volle Höhe zwischen beiden Marker-Zentren, Stroke 1 px (oder 2 px wenn 1 px zu zart wirkt — visuell justieren).

**Wo**: [src/render/network_plan.cpp](../../src/render/network_plan.cpp).

**Test**: `test_native_network_plan` — Pixel-genaue Asserts auf Marker-Mitten + Linien-Endpunkte. Bestehende Tests anpassen, neue für Tull-Big-Marker hinzufügen.

**Risiko**: Tull-Big-Marker mit 8 px Höhe könnte das `▼`-Glyph (5×5 Custom-Sprite) überlappen. Y-Position des `▼` ggf. 2 px höher rücken, damit Spalte nicht zusammenklebt.

### P13 — Header-FontRole-Mapping (10 min, **Cluster B**)

**Was**: Verifizieren in [src/render/layout.cpp](../../src/render/layout.cpp) (Aufrufer von `setRoleFont`):

- TG-Header → `FontRole::Section_Header_TG` (= `helvB12_te` per [canvas_host.cpp:19-22](../../src/render/canvas_host.cpp#L19-L22))
- EG-Header → `FontRole::Section_Header_EG_Atzg` (= `helvB10_te`)
- Atzg-Header → `FontRole::Section_Header_EG_Atzg` (= `helvB10_te`)

**Wie**: `grep -n "setRoleFont.*Section_Header" src/render/layout.cpp` und prüfen, ob die richtigen Rollen pro Header. Falls falsch, korrigieren.

**Risiko**: Wenn `helvB12_te` größer wirkt als erwartet, kann das die TG-Y-Werte verschieben. Pixel-Tests in `test_native_render_input` müssen angepasst werden.

### P14 — Separator-Stärken (5 min, **Cluster B**)

**Was**: TG-Bottom-Separator von `canvas.drawLine(0, y, FB_W-1, y, 1)` (1 px) auf `canvas.fillRect(0, y, FB_W, 2, 1)` (2 px) ändern. EG-Bottom bleibt 1 px (`drawLine`).

**Wo**: [src/render/layout.cpp](../../src/render/layout.cpp), Suche nach `drawLine.*FB_W` bei den Section-Übergängen.

**Wie**:

```cpp
// TG → EG separator (2 px, kräftiger weil TG = wichtigste Sektion)
canvas.fillRect(0, LAYOUT_SEP_TG_Y, FB_W, 2, 1);

// EG → Atzg separator (1 px, normal)
canvas.drawLine(0, LAYOUT_SEP_EG_Y, FB_W - 1, LAYOUT_SEP_EG_Y, 1);
```

**Risiko**: 2-px-Separator schluckt 1 px mehr vertikalen Platz — TG-Block-Höhe ggf. ein Pixel weniger. Visuell prüfen, ob Y-Maße noch passen.

### P8 — §4.1-Annahme im Migration-Plan finalisieren (10 min, revidiert)

**Was**: In [docs/v2-rollout/v2-sbahn-migration-plan.md](v2-sbahn-migration-plan.md) §4.1 (Annahmen) ergänzen:

1. **Glyph-Drift Option A (U8g2 Helvetica/Logisoso) ist akzeptabel** — wie ursprünglich geplant. Keine Y-/Baseline-Drift, Glyph-Stil weicht vom VT323/Silkscreen-Mock ab, aber 400×300 / 1-bit-Rendering gleicht das aus.
2. **Layout-Drift wurde initial unterschätzt** — Iteration 1 der §11.8-Sichtkontrolle hat nur Inhalts-Drift erfasst, nicht Geometrie. Iteration 2 (Claude-Design-Review) hat sieben Layout-Drifts identifiziert (L1/L2/L3/L6/L7/L8/L9), die in v2 noch zu fixen sind (P0/P9–P14). Lehre: §11.8 zukünftig mit Grid-Overlay statt Frei-Vergleich.
3. **Option B (Custom-Bitmap-Fonts) bleibt unbenutzter Folge-PR** — Layout-Drifts sind in bestehender Geometrie umsetzbar, kein Font-Stack-Wechsel nötig.

**Wo**: §4.1 + ggf. Anhang C8 (C9-Entscheidung) + V8 (Risiko-Tabelle, "Font-Metriken passen nicht zum geplanten Layout" — war eigentlich genau das, was passiert ist).

### Reihenfolge + Aufwand-Schätzung (revidiert)

| Schritt | Cluster | Aufwand | Blocker? |
|---|---|---|---|
| **P0** (EG-Separator `·` Rollback) | B | 2 min | nein |
| **P1** (Offline-Foot ALL CAPS) | A | 5 min | nein |
| **P2** (Auth `"AID "`-Präfix) | A | 5 min | nein |
| **P3** (Auth ALL CAPS via `toupper`) | A | 10 min | nein |
| **P4** (Boot `…` Ellipsis) | A | 5 min + Coverage | **U+2026 muss in Font sein** |
| **P5** (Boot Version-Str) | A | 5 min | nein |
| **P9** (Time-Spalte rechtsbündig) | B | 45 min | Test-Anpassung |
| **P10** (Time-Slot-Spacing) | B | 10 min | mit P9 koppeln |
| **P11** (58B-Badge-Width) | B | 15 min | Test-Anpassung |
| **P12** (Netzplan-Marker + Linien) | B | 30 min | Test-Anpassung |
| **P13** (Header-FontRole-Mapping) | B | 10 min | Pixel-Test anpassen |
| **P14** (Separator-Stärken) | B | 5 min | Pixel-Test anpassen |
| **P6** (Mock-Anchor Offline, opt.) | — | 5 min | nicht empfohlen |
| **P7** (Host-PNG-Regenerierung 6×, kein Re-Flash) | — | 10 min | host-only |
| **P8** (Plan-Doku revidiert) | — | 10 min | nein |
| **Summe Cluster A** | A | **30 min** | P4 evtl. skippen |
| **Summe Cluster B** | B | **~2 h** | inkl. Test-Anpassung |
| **Folge-Arbeit** | — | **~40 min** | P7 erst nach A+B |
| **Gesamt** | — | **~3 h** | mit Tests + Re-Foto |

### Abnahme nach Umsetzung

1. Drift-Tabelle in §11.8 zeigt für P0/P1/P2/P3/P5/P9/P10/P11/P12/P13/P14 "Δ = 0".
2. Foto + Host-PNG + Design-PNG nebeneinander legen (**6× Side-by-Side** für mockview-1/2/3/5/6/7) — visueller Auftraggeber-Approval.
3. Patch-Block-Items abgehakt.
4. Squash auf einen Commit "Render: §11 Sichtkontroll-Anpassungen (Layout + Sonderscreens)".
5. Gate E → F freigeben.
