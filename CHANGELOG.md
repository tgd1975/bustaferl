# Changelog

## Unreleased

- Erste Implementierung gemäß `CONCEPT.md`:
  - HAL-Abstraktion (Clock/Network/Display/Sleep/PersistentStore)
  - Wiener-Linien-OGD-Parser mit `towards`-Filter
  - Logik-Module: stale, sleep planning, refresh planning,
    filter health, cold-boot sequencer
  - Renderer mit Adafruit GFX, RLE-komprimierter Framebuffer im RTC-RAM
  - Unit-Test-Suite (Unity, host-only, alle Logik-Module)
  - PlatformIO + Makefile + GitHub-Actions CI
  - Entwickler- und Benutzer-Dokumentation
