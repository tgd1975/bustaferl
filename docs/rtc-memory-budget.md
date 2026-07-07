# RTC-Slow-Memory-Budget

Der ESP32 hat **8192 B** RTC-Slow-Memory (`RTC_DATA_ATTR`). Dort liegen die
Daten, die Deep Sleep überleben müssen: der komprimierte Framebuffer
(Row-Delta-RLE), der letzte `StreamSnapshot`, die `ScheduleHint`-Bilanz und
`PersistedMeta`. Es gibt keine Laufzeitmessung — die Bilanz wird durch Addieren
der Struct-Größen geführt. Drift zwischen Code und dieser Tabelle wird von Hand
nachgezogen; die `check-budgets`-Skill erinnert daran.

## Bilanz

| Posten | Bytes |
|---|---|
| Framebuffer (Row-Delta-RLE, typ. ~1–3 kB, Hardcap gedeckelt) | ~3000 |
| `g_snap` (`StreamSnapshot`, 4 Streams inkl. `Departure::line_label`) | — |
| `g_sched.hint[0..3]` (`ScheduleHint`, 4 Streams) | — |
| `PersistedMeta` (`has_any_data`, `last_success_at`, `auth_error_seen`, Auth-/Filter-Streaks, Wake-/Clean-Timestamps + Alignment-Padding) | — |
| **Reserve** | ~800 / 8192 |

Die Reserve liegt bei ~800 B. Das ist komfortabel, aber nicht üppig: eine
künftige Erweiterung (mehr Slots pro Stream, weitere Meta-Felder) muss entweder
den RLE-Hardcap senken oder ein anderes Feld kürzen.

## Pflege

Bei jeder Änderung an `Departure`, `StreamSnapshot`, `ScheduleHint` oder
`PersistedMeta`:

1. Diese Tabelle aktualisieren.
2. `MAGIC` in [Esp32PersistentStore.cpp](../src/hal/Esp32PersistentStore.cpp)
   hochzählen — sonst liest ein OTA-Upgrade (ohne Power-Loss) die alte
   RTC-Struktur als Garbage.
3. Die tatsächliche RLE-Framegröße misst der §9-Heap-Profiling-Lauf
   (siehe [TESTING.md](TESTING.md)); die Struct-Anteile ergeben sich aus
   `sizeof`.

## Flash (separat, kein RTC)

ESP32-Flash ist 4 MB, die Firmware liegt aktuell bei ~1,08 MB (~83 % der
1,31-MB-App-Partition). Der größte variable Posten sind die U8g2-Font-Daten
(7 Font-Roles). `check-budgets` gated Flash hart bei < 95 % der Partition
(Soft-Warnung ab 90 %).
