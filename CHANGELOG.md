# Changelog

## Unreleased

### v2 — S-Bahn Atzgersdorf

- Ersetzt die beiden U1-Streams durch einen ÖBB-S-Bahn-Stream
  (Bahnhof Atzgersdorf → Wien Hauptbahnhof) via HAFAS `mgate.exe` (POST/JSON).
  `STREAM_COUNT` 5 → 4.
- Neuer Parser `data/oebb_hafas_parse` (Request-Bau + StationBoard-Parse),
  `INetwork::httpPost`, `Departure::line_label` für die pro-Slot variable
  Linienkennung (S2/S3/S4/REX1).
- Neues Display-Layout (siehe `docs/design_handoff_v2/`): Linien-Badges,
  per-Sektion-Banner (`58B Filter ungueltig`, `OEBB-API: Auth ungueltig`),
  Fußzeile, Boot-Splash.
- RTC-`MAGIC` / `SCHED_MAGIC` gebumpt → erster Boot nach Update macht einen
  Light-Full-Refresh und holt Plan-Hints neu.
- **Vor dem ersten Flash:** `OEBB_HAFAS_AID`, `OEBB_HAFAS_CLIENT_JSON`,
  `OEBB_JNYFLTR_PRODUCTS` in `src/config.h` gegen die ÖBB-Webapp verifizieren
  (siehe `docs/v2-sbahn-migration-plan.md` §0).

### v1

- Erste Implementierung gemäß `CONCEPT.md`:
  - HAL-Abstraktion (Clock/Network/Display/Sleep/PersistentStore)
  - Wiener-Linien-OGD-Parser mit `towards`-Filter
  - Logik-Module: stale, sleep planning, refresh planning,
    filter health, cold-boot sequencer
  - Renderer mit Adafruit GFX, RLE-komprimierter Framebuffer im RTC-RAM
  - Unit-Test-Suite (Unity, host-only, alle Logik-Module)
  - PlatformIO + Makefile + GitHub-Actions CI
  - Entwickler- und Benutzer-Dokumentation
