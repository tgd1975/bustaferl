# Session E — HW-Sichtkontrolle (§11.1–11.8)

Stand: 2026-05-20 (18:30 UTC, Host-Vergleich) · Branch: `v2/sbahn-atzgersdorf` · Plan: [docs/v2-sbahn-migration-plan.md](../v2-sbahn-migration-plan.md) §4.3 + §11

**Vorgehen umgestellt auf Mock-View-Firmwares**: statt die States in der Wildbahn zu provozieren (NTP-Override, WiFi-AP aus, AID kaputt machen), gibt es pro State eine eigene Mini-Firmware unter `tools/mockview/` mit hartkodierten Mock-Daten. `make mockview-N` flasht, das Gerät rendert einmal und geht in Deep-Sleep — Display zeigt den State persistent.

**Ablauf pro State**: `make mockview-N` flashen → fotografieren ([docs/screenshots/device/mockview-N.jpeg](../screenshots/device/)) → Host-Vergleich gegen [docs/screenshots/host/mockview-N.png](../screenshots/host/) → Befund/Drift unten eintragen → bei Drift Patch von Claude einfordern → re-flash → re-foto.

**Reihenfolge egal** — jeder Slot ist unabhängig. Empfehlung: 7 → 1 → 2 → 3 → 4 → 5 → 6 (Boot zuerst, weil er der einfachste Smoke-Test ist).

**Gate D → E**: `make ci` grün (✓ am 2026-05-20). Mock-View-Envs kompilieren alle grün.

**Vergleichs-Quellen** (2026-05-20):

- Host-Renders (Pixel-genau, 400×300, 1-bit): `docs/screenshots/host/mockview-N.png`
- Geräte-Fotos (JPEG vom realen UC8176): `docs/screenshots/device/mockview-N.jpeg`
- Design-Soll (Browser-Mock, 1920×1440 @ 4.8×): `docs/design_handoff_display/screen-N-*.png`

Primärvergleich Host ↔ Design. Geräte-Foto nur als Sanity-Check bei Glyph-/Kontrast-Zweifeln.

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
- [x] TG-Block: Badges = weiße Rechtecke + schwarzer Text, Direction "Atzgers." / "Hietzing", Plan-Marker `□` hinter 2. Slot 58A-Atz und 2. Slot 58A-Hie
- [x] 2-px Trennlinie unter TG
- [x] EG-Block: kleinere Badges, kompaktere Reihe, Header "ENDEMANNGASSE - NACH SCHLEIFE", beide 58B-Slots gefüllt
- [x] 1-px Trennlinie unter EG
- [x] Atzg-Block: 3 S-Bahn-Slots horizontal (3. Slot leer — **gewollt**, §3.3 + V14), Slot 1 = S2, Slot 2 = S3
- [x] Netzplan: 5 Spalten, Diamond/Dot/Big-Marker an erwarteten Positionen, vertikale Linie zwischen Atzg-Diamonds, ▼ über Tull

**Mock-Daten** (Anchor 18:30 UTC, 1:1 nach screen-1-normal.png): 58A-Atz = +2 RT / +18 PL · 58A-Hie = +5 RT / +20 PL · 58B-Atz = +11 RT / +31 RT · S2-Hbf = +7 RT (S2) / +21 RT (S3). Atzg-Slot 3 leer (§3.3).

**Vergleichs-PNG**: [screen-1-normal.png](../design_handoff_display/screen-1-normal.png)

**Drift / Notizen**: 2026-05-20 — erste Sichtung (Anchor 04:30 UTC) ergab folgende Drift gegen Design:

1. EG-Header-Separator `·` → muss `-` sein (Design). **Fix**: [src/render/layout.cpp:246](../../src/render/layout.cpp#L246) — `ENDEMANNGASSE - NACH SCHLEIFE`.
2. Mock-Anchor `04:30 UTC` → muss `18:30 UTC` sein (Design zeigt 18:32 ff.). **Fix**: [tools/mockview/mock_data.h](../../tools/mockview/mock_data.h) — `kMockNow = 1779647400`, neuer `kMockNowNight = 1779681600` für mockview-3.
3. Mock-Daten wichen vom Design ab (58B Slot 1 leer, S-Bahn Slot 2 Plan/S2 statt RT/S3, Offsets +4/+14 statt +2/+18 etc.). **Fix**: [tools/mockview/mock_data.cpp](../../tools/mockview/mock_data.cpp) — `buildNormalSnapshot` + `buildNightSnapshot` 1:1 nach screen-1/-3.
4. ATZGERSDORF-Header: Render `->` (ASCII) statt `→` (Design). Font `helvB10_te` hat U+2192 nicht. **Fix**: 7×5 Custom-Sprite `ARROW_RIGHT_SPRITE` + neuer `drawAtzgHeader`-Helper in [src/render/layout.cpp](../../src/render/layout.cpp).
5. Plan-Marker (`□`): Code zeichnet 1-px hollow square (siehe [src/render/plan_marker.cpp](../../src/render/plan_marker.cpp)). Aus dem ersten Foto nicht eindeutig zu beurteilen (Kameraauflösung) — Close-up im Re-Foto prüfen.

Re-Foto nach Re-Flash erwartet.

**Host-Vergleich 2026-05-20 (18:30 UTC, nach Commit `6fb5dce`)**: Alle 5 oben gelisteten Fixes wirksam. Host-Render zeigt:

- TG `58A Atzgers. 18:32 18:48□` / `58A Hietzing 18:35 18:50□` — 1:1 Design ✓
- EG `ENDEMANNGASSE - NACH SCHLEIFE` (Bindestrich) / `58B Atzgers. 18:41 19:01` — 1:1 Design ✓
- Atzg `ATZGERSDORF → WIEN HBF` (Custom-`→`-Sprite) / `S2 18:37` / `S3 18:51` / Slot 3 leer (`--:--` ohne Badge) — §3.3+V14 akzeptiert ✓
- Netzplan: HBF-Diamond (oben) ↔ ATZG-Diamond (unten) via L-Linie, ENDE-Dot, TULL-Big-Marker mit ▼, HIETZ-Dot — 1:1 Design ✓
- Plan-Marker `□` an 18:48 / 18:50 erkennbar (Sub-Pixel, aber zählbar) ✓

**Keine offene Drift mehr für Normal-State.**

---

## 11.3 → mockview-2 · Veraltet (Stale)

- [x] `make mockview-2`
- [x] Foto: [docs/screenshots/device/mockview-2.jpeg](../screenshots/device/mockview-2.jpeg)
- [x] Alle Slots `--:--` (oder `??:??` je nach Renderer)
- [x] **Keine** Plan-Marker
- [x] Layout-Struktur bleibt (TG/EG/Atzg-Blöcke + Netzplan)

**Vergleichs-PNG**: [screen-2-veraltet.png](../design_handoff_display/screen-2-veraltet.png)

**Drift / Notizen**:

- 2026-05-20 (Host-Vergleich 18:30 UTC) — Render 1:1 mit Design (modulo Atzg-Slot-3-Designer-Wunsch).
  - TG: `58A Atzgers. --:-- --:--` / `58A Hietzing --:-- --:--` ✓
  - EG: `58B Atzgers. --:-- --:--` ✓
  - Atzg: `S2 --:--` / `S3 --:--` / Slot 3 leer (kein Badge). Design zeigt dort `S7 --:--` — per §3.3 + V14 leerer Slot 3 akzeptiert; Konsistenz mit Normal-State.
  - Netzplan unverändert ✓
- **Keine offene Drift mehr für Stale-State.**

---

## 11.4 → mockview-3 · Nachtbetrieb

- [x] `make mockview-3`
- [x] Foto: [docs/screenshots/device/mockview-3.jpeg](../screenshots/device/mockview-3.jpeg)
- [x] Alle Zeiten sind Plan-Zeiten (alle mit `□`)
- [x] Morgen-Erst-Abfahrten sichtbar

**Mock-Daten**: alle Departures Plan-fallback, ca. 20 h nach Anchor angesiedelt (Frühmorgen).

**Vergleichs-PNG**: [screen-3-nachtbetrieb.png](../design_handoff_display/screen-3-nachtbetrieb.png)

**Drift / Notizen**:

- 2026-05-20 (Host-Vergleich 18:30 UTC) — Render 1:1 mit Design (modulo Atzg-Slot 3).
  - TG: `58A Atzgers. 04:23□ 04:38□` / `58A Hietzing 04:31□ 04:46□` ✓
  - EG: `58B Atzgers. 04:38□ 04:53□` ✓
  - Atzg: `S2 04:43□` / `S3 04:58□` / Slot 3 leer. Design `S7 05:13□` — per §3.3 + V14 akzeptiert.
  - Netzplan unverändert ✓
  - Plan-Marker `□` an jeder Zeit korrekt gesetzt ✓
- **Keine offene Drift mehr für Nacht-State.**

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
| 1 Normal | Alle Daten + Layout | 1:1 nach screen-1 | 1:1 | 0 | Nein (Commit `6fb5dce`) |
| 1 Normal | Atzg-Slot 3 | `S7 19:05□` | leer | Slot fehlt | Nein — §3.3 + V14 akzeptiert |
| 2 Stale | Alle Daten + Layout | `--:--` überall | gleich | 0 | Nein |
| 2 Stale | Atzg-Slot 3 | `S7 --:--` | leer | Slot fehlt | Nein — §3.3 |
| 3 Nacht | Alle Daten + Layout | Plan-Zeiten mit `□` | gleich | 0 | Nein |
| 3 Nacht | Atzg-Slot 3 | `S7 05:13□` | leer | Slot fehlt | Nein — §3.3 |
| 4 Quiet | `—`-Bar + Title + Sub | 1:1 nach screen-4 | 1:1 | 0 | Nein |
| 5 Offline | Sub-Text Zeit | `Letzte Aktualisierung 17:48` | `… 18:22` | Mock-Anchor | Optional (kosmetisch) |
| 5 Offline | Foot-Casing | `WLAN · RETRY IN 30S` | `WLAN · Retry in 23s` | Casing | **Ja** — P1 |
| 6 Auth | Foot-Präfix | `AID 0X8F · ERR 401` | `OWDL4fE4 · ERR 200` | Präfix `"AID "` fehlt | **Ja** — P2 |
| 6 Auth | Foot-Casing | ALL CAPS | mixed case | Casing | **Ja** — P3 (mit P1 gleichziehbar) |
| 6 Auth | AID-Wert + HTTP-Code | `0X8F` / `401` | `OWDL4fE4` / `200` | Designer-Annahme vs. Plan §11.7 | Nein — Mock korrekt per Plan |
| 7 Boot | Sub-Ellipsis | `lädt Fahrplan…` (U+2026) | `lädt Fahrplan...` | Glyph (3 Punkte vs. `…`) | Optional (P4) |
| 7 Boot | Foot-Casing + × | `V2.0 · UC8176 · 400×300` | `v2.0 · UC8176 · 400x300` | Casing + `×` (U+00D7) vs. `x` | **Ja** — P5 |

**Bewertung Option-A-Drift**: Pure Glyph-Drift (Helvetica statt VT323/Silkscreen) ist messbar, aber bei 400×300 / 1-bit nicht störend. Die offenen Patches (P1–P5) sind ausschließlich Text-Literal-Drift (Casing, Format-String, Sonderzeichen), keine Renderer-Verschiebung. **Wechsel auf Option B (Custom-Bitmap-Fonts) nicht erforderlich.**

---

## Cold-Boot-Doppel-Refresh (§11.1 — **separat**, nicht via mockview)

§11.1 verifiziert den Pre-Render-Splash *in der echten Boot-Sequenz*: nach `make flash` (= Produktions-Firmware, nicht mockview) sollen **zwei Refreshes hintereinander** sichtbar sein (Boot-State → Normal). Das geht nur mit `make flash` + WiFi/HAFAS funktionsfähig, nicht über Mock.

- [ ] `make flash` (Produktions-Firmware, nicht mockview)
- [ ] **Refresh 1** (~3 s): Boot-State (vgl. screen-7)
- [ ] **Refresh 2** (~5–10 s): Normal-State (vgl. screen-1)
- [ ] Wenn nur 1 Refresh sichtbar: Pre-Render aus Schritt 5.4 nicht aktiv. Serial-Log auf `[cold] pre-render boot` / `[cold] fetch start` / `[cold] post-render normal` prüfen.

**Drift / Notizen**:

---

## Patch-Block (Squash-Ziel: "Render: §11 Sichtkontroll-Anpassungen")

Liste der Render-Korrektur-Commits, die während E entstehen — werden am Ende der Session zu **einem** Commit gesquasht (§4.3 Branch-Disziplin). Die mockview-Sources unter `tools/mockview/` gehören dazu, wenn nicht bereits separat committed.

Stand 2026-05-20 — fünf identifizierte Patches aus Host-Vergleich (siehe Umsetzungsplan unten):

- [ ] **P1** — Offline-Foot ALL CAPS + Sekunden-Suffix (`WLAN · RETRY IN %dS`) — [src/render/display_state.cpp:79](../../src/render/display_state.cpp#L79)
- [ ] **P2** — Auth-Foot `"AID "`-Präfix einbauen (`AID %s · ERR %d`) — [src/render/display_state.cpp:98](../../src/render/display_state.cpp#L98)
- [ ] **P3** — Auth-Foot ALL CAPS (AID-Wert hex-uppercase, `ERR` schon CAPS) — [src/render/display_state.cpp:98](../../src/render/display_state.cpp#L98) + Aufrufer
- [ ] **P4** — Boot-Sub Ellipsis `…` (U+2026) statt drei `...` — [src/render/display_state.cpp:57](../../src/render/display_state.cpp#L57) (Font-Coverage prüfen!)
- [ ] **P5** — Boot-Foot ALL CAPS + `×` (U+00D7) statt `x` — [src/config.h:88](../../src/config.h#L88) `DISPLAY_VERSION_STR`
- [ ] **P6** (optional) — Offline-Sub-Mock-Anchor an Design 17:48 angleichen — [tools/mockview/main_5_kein_empfang.cpp](../../tools/mockview/main_5_kein_empfang.cpp)
- [ ] **P7** (Folge-Verifikation) — Re-Flash mockview-5/-6/-7, neue Host-PNGs + Geräte-Fotos, Re-Vergleich gegen Design
- [ ] **P8** — §4.1-Annahme im Migration-Plan aktualisieren (Glyph-Drift "akzeptiert", Option B nicht nötig)

---

## Abnahme

- [ ] Alle 7 States vom Auftraggeber abgenommen (mockview-1 bis -7) — **Stand 2026-05-20: 4/7 driftfrei (1/2/3/4), 3/7 mit P1–P5-Patches offen (5/6/7)**
- [ ] §11.1 Cold-Boot-Doppel-Refresh gesehen (Produktions-Firmware)
- [x] Alle Fotos abgelegt unter [docs/screenshots/device/](../screenshots/device/) (Pfad gegenüber Plan-Vorgabe geändert, siehe Header)
- [ ] Patch-Block (P1–P8) abgearbeitet + gesquasht zu einem Commit auf `v2/sbahn-atzgersdorf`
- [ ] **Gate E → F**: Display final, Tests können in F umgestellt werden

11.9 (Linien-Längen-Stress mit REX1) und 11.10 (24h-Soak) gehören laut §4.3 zu **Session G**, nicht E.

---

## Anpassungs-Umsetzungsplan (Stand 2026-05-20, Host-Vergleich)

Reihenfolge optimiert für minimale Re-Flash-Kosten: alle Code-Patches zuerst, dann **eine** Re-Flash-Runde über mockview-5/-6/-7.

### Übersicht

Fünf Code-Drifts (P1–P5) auf drei States (Offline, Auth, Boot). Alle in der **Sonderscreen-Renderlogik** (`display_state.cpp` + `config.h`), keine Anfassung an Board-Layout/Netzplan/Mock-Daten. Die Sonderscreens 5/6/7 nutzen denselben `drawCentered(..., Fullscreen_Foot, ...)`-Pfad — der Casing-Drift ist konsistent über alle drei States. Ein Patch-Cluster reicht.

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

### P7 — Re-Flash + Re-Foto + Re-Vergleich (15 min)

**Reihenfolge**:

1. Patches P1–P5 als ein lokaler Commit auf `v2/sbahn-atzgersdorf` (Squash-Ziel "Render: §11 Sichtkontroll-Anpassungen").
2. `make mockview-5` → fotografieren → `docs/screenshots/device/mockview-5.jpeg` überschreiben.
3. `make mockview-6` → fotografieren.
4. `make mockview-7` → fotografieren.
5. Host-Renders neu erzeugen: `make host-mockview-{5,6,7}` (oder via `pio test -e native …`, je nach Build-Target — siehe [docs/screenshots/host/README.md](../screenshots/host/) falls vorhanden) → `docs/screenshots/host/mockview-{5,6,7}.png` überschreiben.
6. Drift-Tabelle in §11.8 aktualisieren (Δ → 0 für P1/P2/P3/P5; P4 abhängig vom Coverage-Check).

**Tooling**: PGM → PNG nicht selbst basteln; `scripts/pgm-to-png.py` existiert bereits (siehe Memory `pgm-png-script.md`).

### P8 — §4.1-Annahme im Migration-Plan finalisieren (10 min)

**Was**: In [docs/v2-sbahn-migration-plan.md](../v2-sbahn-migration-plan.md) §4.1 (Annahmen) ergänzen: "**Glyph-Drift Option A (U8g2 Helvetica/Logisoso) ist akzeptabel.** Beobachtet 2026-05-20 im Host-Vergleich: keine Y-Drift, keine Baseline-Verschiebung, Glyph-Stil weicht vom VT323/Silkscreen-Mock ab, aber 400×300 / 1-bit-Rendering gleicht das aus. **Option B (Custom-Bitmap-Fonts) bleibt unbenutzter Folge-PR.**"

**Wo**: §4.1 + ggf. Anhang C8 (C9-Entscheidung).

### Reihenfolge + Aufwand-Schätzung

| Schritt | Aufwand | Blocker? |
|---|---|---|
| P1 (Offline-Foot) | 5 min | nein |
| P2 (Auth `"AID "`) | 5 min | nein |
| P3 (Auth ALL CAPS via `toupper`-Loop) | 10 min | nein |
| P4 (Boot `…`) | 5 min + Coverage-Check | **U+2026 muss in Font sein** |
| P5 (Boot Version-Str) | 5 min | nein |
| P6 (Mock-Anchor) | 5 min | optional, nicht empfohlen |
| P7 (Re-Flash 3× + Re-Foto + Re-Render) | 15 min | benötigt Gerät |
| P8 (Plan-Doku) | 10 min | nein |
| **Summe** | **~50–60 min** | P4 evtl. skippen |

### Abnahme nach Umsetzung

1. Drift-Tabelle in §11.8 zeigt für P1/P2/P3/P5 "Δ = 0".
2. Foto + Host-PNG + Design-PNG nebeneinander legen (3× Side-by-Side) — visueller Auftraggeber-Approval.
3. Patch-Block-Items P1–P8 abgehakt.
4. Squash auf einen Commit "Render: §11 Sichtkontroll-Anpassungen".
5. Gate E → F freigeben.
