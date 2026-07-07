# bustaferl — Umsetzungsplan

Stand: 2026-07-07. Die v1-Basis (Phasen 1–8) ist umgesetzt und dokumentiert.
Aktueller Arbeitsstand ist **v2 — S-Bahn Atzgersdorf** (Branch
`v2/sbahn-atzgersdorf`): S-Bahn-Stream statt U1, HAFAS-Parser, kompletter
Display-Rewrite mit 7 Fullscreen-States. Was für v2 noch offen ist, steht in
[§ v2-Rollout](#v2--rollout-offen).

Historische v1-Planung: Phasen 1–4 (Setup/Architektur/Tests/Makefile) und 7–8
(Doku) sind abgeschlossen — der Code existiert und ist testabgedeckt. Phasen 5,
6 und 9 (Hardware-Bring-up, Langzeittest, Release) sind teils in den v2-Rollout
übergegangen; siehe unten.

---

## v2 — Rollout (offen)

Ableitung aus dem v2-Migrationsplan, Session G. Code und Doku-Sync (Schritte
0–10) sind erledigt; es fehlt die geräteseitige Verifikation und der Release.
Alles hier braucht das physische Gerät bzw. eine Release-Entscheidung — nichts
davon ist reine Codearbeit.

### Schritt 9 — Profiling (Native + Device)

- [ ] Heap-Spitze beim HAFAS-HTTPS-POST messen (`mgate.exe`, ~20.5–21.2 KB
      Antwort) — eigener Wächter, nicht der EFA-zugeschnittene aus
      [schedule_fetcher.cpp](src/logic/schedule_fetcher.cpp). Gate via
      `check-budgets`-Skill (§9 Heap-Profiling).
- [ ] Peak-Host-Heap unter v2-Ceiling bestätigen; Valgrind leak-frei
- [ ] ESP32 Flash- + RAM-Headroom ≥ 0 nach v2 (`make size`)
- [ ] Sleep-Budget / Deep-Sleep-Strom erneut prüfen (v2 hat einen Stream mehr)

### Schritt 11 — Manuelle HW-Verifikation

Pixel-Match hat die Device-Suite nicht; visuelle Abnahme muss ein Mensch machen.
Referenz ist `docs/screenshots/host/` (Host-Renderer = Geräte-Renderer, siehe
Memory `host-renderer-parity`).

- [ ] Linien-Längen-Stress mit Live-REX-Antwort: `S2` / `REX1`-Badges brechen
      das Layout nicht (Risiko V-Layout)
- [ ] Netzplan-Geometrie on-device gegen `screen-1-normal.png` (Diamond-Marker,
      1-px-Linien treffen Marker-Mitte — Risiko V9)
- [ ] Plan-Marker `□` (5×5 px) sauber von der HH:MM-Glyphe abgegrenzt, kein
      Verschmelzen bei Ghosting — sonst 6×6 px o. ä. (Risiko V12)
- [ ] Leere 3. S-Bahn-Spalte als **gewollt** abnehmen (2-Slot-Realität vs.
      3-Slot-Mockup — Risiko V14, ist kein Render-Bug)
- [ ] Seltene States manuell durchtesten: `Veraltet` (AP aus), `Kein Empfang`,
      `Auth-Fehler` (AID-Hack) — Schwellwerte `QUIET_HORIZON_S`,
      `OFFLINE_THRESHOLD_S` beobachten (Risiko V11)
- [ ] Boot-Button: kurzer Druck aktualisiert sichtbar (upd-Stamp tickt auch bei
      unveränderten Daten), langer Druck (≥ 3 s) löst B/W-Flash aus
- [ ] Optionaler 24-h-Soak (Light-Full alle 2 h, Nachtbetrieb-Deep-Clean,
      NTP-Sync, Ghosting-Beurteilung)

### Schritt 12 — Release v2.0.0

Major-Release wegen RTC-`MAGIC`-Bump (persistenter Zustand wird beim ersten
Wake nach Update verworfen) und sichtbarem Display-Rewrite. Folgt dem
`release`-Skill.

- [ ] Merge nach `main` (Squash oder Merge-Commit nach Auftraggeber-Wahl)
- [ ] Tag `v2.0.0` (annotiert) — Message fasst v2 zusammen
- [ ] `release`-Skill mit `--major` (24-h-Soak-Gate) ausführen
- [ ] Branch **nicht** sofort löschen — mind. eine Woche Geräte-Beobachtung
- [ ] Plan-Datei-Frontmatter archivieren („Umgesetzt in v2.0.0")
- [ ] Pre-built `.bin` als GitHub Release anhängen (optional)
- [ ] Repo-Description und Topics auf GitHub setzen

---

## v1-Historie (abgeschlossen)

Reihenfolge war die empfohlene Bearbeitungsabfolge. Die Sub-Bullets sind
kondensiert — die Details stehen im Git-Verlauf und in `docs/`.

### Phase 1 — Projekt-Setup ✅

PlatformIO + `make`-Wrapper; `platformio.ini` mit `env:esp32dev` und
`env:native`; Library-Deps (GxEPD2, Adafruit GFX, ArduinoJson) gepinnt;
`.gitignore`, `secrets.h.example`, `LICENSE`, CI-Workflow.

### Phase 2 — Software-Architektur ✅

Testbarer Kern, dünne HAL-Adapter (`IClock`/`INetwork`/`IDisplay`/`ISleep`/
`IPersistentStore` je ESP32-Impl + Mock); `data/` (Parser, `StreamSnapshot`);
`logic/` (stale/sleep/refresh/filter-health/boot-sequencer, hardware-frei);
`render/` (layout, framebuffer, RLE, overlays); `config.h`. Konstruktor-
Injection, `time_t` überall, `std::string`/`const char*` statt Arduino-`String`.

### Phase 3 — Unit-Tests ✅

Unity im `env:native`, ein Testfile pro Logik-Modul (parse, stale, sleep,
refresh, RLE, filter-health, boot-sequencer). Coverage-Ziel Logik ≥ 90 %.

### Phase 4 — Makefile ✅

`make build/upload/monitor/flash/test/clean/format/lint/size/secrets/help`,
`make ci` (`format-check` + `lint` + `test` + `build`).

### Phase 5 — Hardware-Bring-up ✅

Verkabelung, Hello-World-Full-Refresh, WiFi, erster API-Call, erster Render,
Partial-Refresh, Deep-Sleep + Wake mit RTC-RAM, Strommessung < 50 µA.

### Phase 6 — Integrations- / Langzeittest ✅

24-h-Lauf, Light-Full alle 2 h, Nacht-Deep-Clean, NTP-Sync, Ghosting-Check,
Stale-Verhalten. (v1-„58B-Filter-Drift → Fehlertext"-Screen ist in v2 durch das
Stale/Quiet/Auth-State-Modell abgelöst und nicht mehr eigenständig getestet.)

### Phase 7 — Entwickler-Dokumentation ✅

`docs/DEVELOPMENT.md`, `docs/ARCHITECTURE.md` (Modulkarte, Dataflow, State-
Machine, Konstanten), `docs/HARDWARE.md`, `docs/TESTING.md`.

### Phase 8 — Benutzer-Dokumentation ✅

`README.md`, `docs/USER.md` (Erst-Inbetriebnahme, RBL/extId, Anzeigen,
Troubleshooting), `CHANGELOG.md`.
