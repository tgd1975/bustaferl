# bustaferl — Tests

Three buckets, three prefixes, three Make-Targets. The prefix decides where
a test runs and when it's expected to be run.

| Prefix | Runs on | Cadence | Make-Target |
| --- | --- | --- | --- |
| `test_native_*` | host (env:native) | after every edit, ~5 s | `make test-native` (alias `make test`) |
| `test_device_*` | ESP32 (env:device-*) | pre-commit with hardware, ~5–10 min | `make test-device` |
| `test_longterm_*` | ESP32 or host | opt-in, 3 min … 24 h | `make test-longterm-*` |

The Makefile is the canonical entry point — `make help` lists every target.
Direct `pio test -e …` calls are for debugging single envs, not routine work.

## test_native_* — host suite (12)

| Folder | Was es deckt |
| --- | --- |
| [test_native_api_fetcher](test_native_api_fetcher/) | `logic/api_fetcher` retry + outcome reporting |
| [test_native_boot_sequencer](test_native_boot_sequencer/) | `logic/boot_sequencer` cold-boot order |
| [test_native_display_apply](test_native_display_apply/) | `logic/display_apply` change detection |
| [test_native_efa_parse](test_native_efa_parse/) | `data/efa_parse` pure-parse (Pair: [test_device_schedule](test_device_schedule/)) |
| [test_native_filter_health](test_native_filter_health/) | `logic/filter_health` drift detection |
| [test_native_refresh_planner](test_native_refresh_planner/) | `logic/refresh_planner` cadence math |
| [test_native_rle](test_native_rle/) | `render/rle` round-trip (Pair: [test_device_persistent](test_device_persistent/)) |
| [test_native_schedule_fetcher](test_native_schedule_fetcher/) | `logic/schedule_fetcher` retry + plan-fallback |
| [test_native_sleep_planner](test_native_sleep_planner/) | `logic/sleep_planner` next-wake math |
| [test_native_slot_merger](test_native_slot_merger/) | `logic/slot_merger` live + plan merge |
| [test_native_stale_policy](test_native_stale_policy/) | `logic/stale_policy` banner thresholds |
| [test_native_wienerlinien_parse](test_native_wienerlinien_parse/) | `data/wienerlinien_parse` pure-parse (Pair: [test_device_fetch](test_device_fetch/)) |

## test_device_* — on-device suite (5)

| Folder | Was es deckt (env) |
| --- | --- |
| [test_device_fetch](test_device_fetch/) | WiFi + HTTPS + Wiener-Linien-Parse vs. Live-API (`env:device-fetch`) |
| [test_device_persistent](test_device_persistent/) | RTC slow memory + RLE round-trip (`env:device-persistent`) |
| [test_device_render](test_device_render/) | `render/layout` + `error_overlay` vs. real GFX-Stack (`env:device-render`) |
| [test_device_schedule](test_device_schedule/) | EFA-Pipeline (3 back-to-back XSLT_DM_REQUEST + Heap) (`env:device-schedule`) |
| [test_device_sleep](test_device_sleep/) | `Esp32Sleep::wakeupCause` mapping via Light-Sleep (`env:device-sleep`) |

### Test-Pair-Konvention (host ↔ device)

Drei Pure/Integration-Paare. Jeder device-Test, der eine host-Variante hat,
nennt diese im Top-of-File-Kommentar — eine Zeile.

| Host-pure | Device-Integration |
| --- | --- |
| `test_native_rle` | `test_device_persistent` |
| `test_native_wienerlinien_parse` | `test_device_fetch` |
| `test_native_efa_parse` | `test_device_schedule` |

## test_longterm_* — opt-in long-runners (8 sources / 10 targets)

Je Test eine eigene Source — **Ausnahme**: die `soak`-Familie teilt eine
Source und parametrisiert per Build-Define (`LONGTERM_SOAK_CYCLES`).

| Folder | Dauer | Klasse | Concern |
| --- | --- | --- | --- |
| [test_longterm_smoke](test_longterm_smoke/) | ~3 min | Routine | HW-Sanity nach Flash |
| [test_longterm_jitter](test_longterm_jitter/) | ~10 min | Routine | Retry/Reconnect unter injiziertem WiFi-Drop |
| [test_longterm_horizon_mock](test_longterm_horizon_mock/) | ~10 min | Routine | EFA-Fallback Logic (PC-driven Mock-API) |
| [test_longterm_wake_cycle](test_longterm_wake_cycle/) | ~30 min | Routine | Deep-Sleep + Persistent Store (echte ESP-Resets) |
| [test_longterm_soak](test_longterm_soak/) (5min) | ~5 min | Routine | Heap-Quick-Check, early bail-out |
| [test_longterm_soak](test_longterm_soak/) (15min) | ~15 min | Routine | Heap-Confidence Pre-Commit |
| [test_longterm_soak](test_longterm_soak/) (1h) | ~1 h | Routine | Heap-Leak canonical |
| [test_longterm_horizon_scan](test_longterm_horizon_scan/) | ~90 min | Iteration | Rolling-Window-Cliff bei voller Datenlage |
| [test_longterm_horizon_evening](test_longterm_horizon_evening/) | ~5 h | Iteration | Evening-Dry-Up live + Sleep in Nacht-Stille |
| [test_longterm_day_full](test_longterm_day_full/) | ~24 h | Pre-release | beide Live-Übergänge + Refresh-Budget |

## Neue Tests anlegen

Präfix bestimmt den Bucket — die Filter-Regeln in `platformio.ini` greifen
automatisch (glob-basiert):

```text
test/test_native_<x>/test_main.cpp     → läuft in env:native
test/test_device_<x>/test_main.cpp     → braucht env:device-<x> Eintrag
test/test_longterm_<x>/test_main.cpp   → braucht env:longterm-<x> Eintrag
```

Für device/longterm noch je ein Env in `platformio.ini` mit
`test_filter = test_<folder>` ergänzen. Filter-Regeln greifen sonst nichts
an — pure host-Tests sind reine `git add`.

## `.tmp/` — projektlokales Scratch-Verzeichnis

`make test-device` und alle `test-longterm-*`-Targets schreiben JSON-Results
nach `.tmp/test-results.json` bzw. `.tmp/longterm-<name>.json`. `.tmp/` ist
gitignored. Für Stream-Forensik (warum drift Heap?): `make test-device-trace
ENV=device-fetch` legt das volle Serial-Log unter `.tmp/traces/<env>.log`.
