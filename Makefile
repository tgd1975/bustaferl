# bustaferl — convenience wrapper around PlatformIO
#
# Three test buckets:
#   test-native    host-only unit tests, ~5 s
#   test-device    on-device ESP32 tests, ~5-10 min with hardware
#   test-longterm* opt-in long-runners, 3 min to 24 h
#
# `make help` lists every target.

.DEFAULT_GOAL := help
.PHONY: help build build-all upload monitor flash \
        test test-native test-native-png test-device test-all test-device-trace \
        test-longterm-smoke test-longterm-jitter test-longterm-horizon-mock \
        test-longterm-wake test-longterm-soak-5min test-longterm-soak-15min \
        test-longterm-soak-1h test-longterm-horizon-scan \
        test-longterm-horizon-evening test-longterm-day-full \
        test-longterm-reset-watch \
        native-runtime-build native-runtime-smoke native-runtime-day \
        native-runtime-massif native-runtime-https-smoke \
        mockview-1 mockview-5 mockview-6 mockview-7 mockview-8 \
        clean format format-check lint tidy size secrets ci ci-heavy

PIO        := pio
TMP        := .tmp
RESULTS    := $(TMP)/test-results.json
TRACE_DIR  := $(TMP)/traces
NR_DIR     := $(TMP)/native-runtime
# The driver is built by PlatformIO (env:native-runtime), NOT a hand-rolled
# g++ recipe — so PIO's dependency resolution is the single source of truth for
# which src/ files compile, and the list can't silently drift out of sync (it
# did once: a render-layer split left the old g++ recipe unbuildable and no CI
# job caught it). NR_BIN is PIO's output artifact; NR_DIR still holds the run
# outputs (PGMs, run.log) the driver writes via env vars.
NR_ENV     := native-runtime
NR_BIN     := .pio/build/$(NR_ENV)/program

DEVICE_ENVS := -e device-fetch -e device-persistent -e device-render \
               -e device-sleep -e device-schedule

# The native-runtime driver's source list lives in platformio.ini
# (env:native-runtime's build_src_filter) — deliberately NOT duplicated here.
# A hand-maintained list is what drifted and broke the build silently before;
# PlatformIO's glob is now the one source of truth.
#
# NR_LDLIBS is only for the standalone https_smoke below (a single self-
# contained .cpp, not part of the driver's dependency graph).
NR_LDLIBS := -lcurl

# write-meta TARGET JSON
# Drops a sidecar `.tmp/<target>.meta.json` with commit SHA, epoch
# timestamp, and pass-boolean derived from the JSON results. The
# /release skill reads these to decide whether the recorded test run
# matches HEAD and was actually green.
define write-meta
	sha=$$(git rev-parse HEAD 2>/dev/null || echo unknown); \
	ts=$$(date +%s); \
	pass=false; \
	if [ -f "$(2)" ] && jq -e '[.. | objects | select(has("status")) | .status] | length > 0 and all(. == "PASSED" or . == "SKIPPED")' < "$(2)" >/dev/null 2>&1; then \
	  pass=true; \
	fi; \
	jq -n --arg sha "$$sha" --argjson ts "$$ts" --argjson pass "$$pass" \
	  '{commit:$$sha, timestamp:$$ts, pass:$$pass}' \
	  > $(TMP)/$(1).meta.json
endef

help:                  ## list available targets
	@python3 tools/make-help.py $(MAKEFILE_LIST)

# --- Firmware ---

build:                 ## compile firmware for ESP32
	$(PIO) run -e esp32dev

build-all: native-runtime-build  ## compile every `pio run`-buildable env (firmware + native-runtime + mockviews)
	@# The honest "does it all still compile" gate. `make ci`/`build` only cover
	@# esp32dev + the host envs; the alternate-main mockview-* firmwares each have
	@# their own build_src_filter and can rot independently — exactly how the
	@# native-runtime driver silently broke. Building them here means a drifted
	@# env fails loudly instead of lurking until someone runs its one-off target.
	@# Slow (ESP32 cross-builds dominate); not in the pre-commit path — run before
	@# a release or after touching platformio.ini / the render or logic layers.
	@#
	@# Two classes of env are deliberately EXCLUDED because they are not
	@# `pio run`-buildable by design (each fails standalone, on purpose):
	@#   * env:native and env:longterm-* — Unity *test* envs; their main() comes
	@#     from the test runner. Covered by `make test-native` / `test-longterm-*`.
	@#   * longterm-horizon-mock-firmware — #errors unless MOCK_API_BASE is
	@#     injected by test/test_longterm_horizon_mock/runner.py.
	@# device-* envs share esp32dev's src compile (they differ only by
	@# test_filter), so esp32dev already covers their compile surface.
	@# native-runtime is built via its make target (release profile, -lcurl);
	@# the rest go in one pio invocation so a failure names the culprit env.
	$(PIO) run \
	  -e esp32dev \
	  -e mockview-1-normal -e mockview-5-kein-empfang -e mockview-6-auth-fehler \
	  -e mockview-7-boot -e mockview-8-geometry

upload:                ## flash firmware to attached ESP32
	$(PIO) run -e esp32dev -t upload

monitor:               ## open serial monitor (115200)
	$(PIO) device monitor -b 115200

flash: upload monitor  ## upload + open monitor

# --- Mock-view firmwares (Session E — §11 HW visual inspection) ---
# Flashes one DisplayState with hard-coded mock data, runs deepClean
# (3× B/W flash, ~6 s) for ghost-free rendering, then deep-sleeps.
# Photograph, compare against docs/design_handoff_display/screen-N-*.png.
# No WiFi/HAFAS needed; each flash is self-contained.
mockview-1:            ## flash Normal board (mixed live + schedule)
	$(PIO) run -e mockview-1-normal -t upload

mockview-5:            ## flash Offline (Kein Empfang) — vgl. screen-5-kein-empfang.png
	$(PIO) run -e mockview-5-kein-empfang -t upload

mockview-6:            ## flash Auth (Auth-Fehler) — vgl. screen-6-auth-fehler.png
	$(PIO) run -e mockview-6-auth-fehler -t upload

mockview-7:            ## flash Boot — vgl. screen-7-boot.png
	$(PIO) run -e mockview-7-boot -t upload

mockview-8:            ## flash geometry probe (border, diagonals, font samples)
	$(PIO) run -e mockview-8-geometry -t upload

# --- Routine tests ---

test:                  test-native            ## alias: fast host tests

test-native:                                  ## all test_native_* (~5 s)
	@# The native render TUs include Adafruit_GFX.h / U8g2 headers via the
	@# -isystem paths into .pio/libdeps/esp32dev (see env:native comments in
	@# platformio.ini). On a cold checkout those don't exist yet and every
	@# render-dependent test env dies with "Adafruit_GFX.h: No such file".
	@# Materialise them once; no-op when already present.
	@test -d ".pio/libdeps/esp32dev/Adafruit GFX Library" \
	  || $(PIO) pkg install -e esp32dev
	ASAN_OPTIONS=detect_leaks=0 $(PIO) test -e native

test-native-png:       test-native            ## render screens → PNGs in .tmp/v2-pgm/
	@# test-native writes the PGM dumps (render-all-states + mockview) into
	@# .tmp/v2-pgm/; convert them to PNGs next to the PGMs for eyeballing.
	@# No path arg → the script batch-converts its default dir (.tmp/v2-pgm/);
	@# a path arg is treated as a single file, not a directory.
	python3 scripts/pgm-to-png.py
	@echo "[png] screens written to $(TMP)/v2-pgm/  (open the *.png files)"

test-device:                                  ## all test_device_*, skip if no device
	@mkdir -p $(TMP)
	@ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | grep -q . \
	  || { echo "[skip] no ESP32 on /dev/ttyUSB*/ttyACM*"; exit 0; }
	$(PIO) test $(DEVICE_ENVS) --json-output-path $(RESULTS)

test-all:              test-native test-device  ## host + device (smart skip)
	@mkdir -p $(TMP)
	@$(call write-meta,test-all,$(RESULTS))

# --- Forensic ---

test-device-trace:                            ## full serial stream into .tmp/traces/$(ENV).log
	@mkdir -p $(TRACE_DIR)
	@[ -n "$(ENV)" ] || { echo "usage: make test-device-trace ENV=device-fetch"; exit 1; }
	$(PIO) test -e $(ENV) -v 2>&1 | tee $(TRACE_DIR)/$(ENV).log

# --- CI / quality ---

ci: format-check lint tidy test-native build native-runtime-build  ## host only — fast, pre-commit-tauglich (~30-45 s)

# native-runtime-build is in `ci` on purpose: the soak driver is a *program*
# that links the full render stack, and it once rotted unnoticed for several
# commits because nothing in CI compiled it (the workflow runs `make ci`, and
# ci-heavy — which did build it — was invoked by nothing). Compiling it here is
# fast, deterministic, and network-free, so a broken driver now fails CI at the
# same gate as the firmware. The live valgrind smoke stays opt-in below.

ci-heavy: ci native-runtime-smoke  ## ci + live valgrind smoke (~5-6 min, opt-in; needs network to wienerlinien.at)

# --- Long-term (opt-in) ---

test-longterm-smoke:                          ## ~3 min  HW sanity after every flash
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-smoke -v --json-output-path $(TMP)/longterm-smoke.json

test-longterm-jitter:                         ## ~10 min retry/reconnect under real network
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-jitter -v --json-output-path $(TMP)/longterm-jitter.json

test-longterm-horizon-mock:                   ## ~10 min EFA fallback logic (PC-driven mock API)
	@mkdir -p $(TMP)
	python3 test/test_longterm_horizon_mock/runner.py

test-longterm-wake:                           ## ~30 min deep-sleep + persistent store
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-wake -v --json-output-path $(TMP)/longterm-wake.json

test-longterm-soak-5min:                      ## ~5 min quick heap-check, early bail-out
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-soak-5min -v --json-output-path $(TMP)/longterm-soak-5min.json
	@$(call write-meta,longterm-soak-5min,$(TMP)/longterm-soak-5min.json)

test-longterm-soak-15min:                     ## ~15 min heap-confidence pre-commit
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-soak-15min -v --json-output-path $(TMP)/longterm-soak-15min.json
	@$(call write-meta,longterm-soak-15min,$(TMP)/longterm-soak-15min.json)

test-longterm-soak-1h:                        ## ~1 h canonical heap-leak detector
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-soak-1h -v --json-output-path $(TMP)/longterm-soak-1h.json
	@$(call write-meta,longterm-soak-1h,$(TMP)/longterm-soak-1h.json)

test-longterm-horizon-scan:                   ## ~90 min rolling-window cliff (daytime, full data)
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-horizon-scan -v --json-output-path $(TMP)/longterm-horizon-scan.json

test-longterm-horizon-evening:                ## ~5 h evening dry-up + sleep into night (start ~21:30)
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-horizon-evening -v --json-output-path $(TMP)/longterm-horizon-evening.json

test-longterm-day-full:                       ## ~24 h pre-release, unattended overnight (start evening)
	@mkdir -p $(TMP)
	$(PIO) test -e longterm-day-full -v --json-output-path $(TMP)/longterm-day-full.json
	@$(call write-meta,longterm-day-full,$(TMP)/longterm-day-full.json)

# Flashes the PRODUCTION firmware (not a Unity test env) and watches its
# serial output for unplanned resets (brownout/watchdog/panic) over a real
# deep-sleep cycle — the field symptom none of the test_longterm_* envs can
# see, because a chip reset kills their Unity process along with the rest of
# RAM. See scripts/soak_reset_watch.py.
# RESET_WATCH_HOURS overrides the default 8h watch, e.g.
#   make test-longterm-reset-watch RESET_WATCH_HOURS=12
RESET_WATCH_HOURS ?= 8
test-longterm-reset-watch:                    ## ~8 h (RESET_WATCH_HOURS=n) production firmware, watches for spontaneous resets
	@mkdir -p $(TMP)
	@PORT=$$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -n1); \
	[ -n "$$PORT" ] || { echo "[skip] no ESP32 on /dev/ttyUSB*/ttyACM*"; exit 0; }; \
	$(PIO) run -e esp32dev -t upload --upload-port "$$PORT"; \
	python3 scripts/soak_reset_watch.py --port "$$PORT" --hours $(RESET_WATCH_HOURS)

# --- Native runtime (Schritt 9 — host loop) ---

native-runtime-build:  ## build host loop binary (via PlatformIO env:native-runtime)
	$(PIO) run -e $(NR_ENV)

native-runtime-smoke: native-runtime-build  ## 10 cycles unter valgrind, ~5 min
	@mkdir -p $(NR_DIR)
	BUSTAFERL_MAX_CYCLES=10 BUSTAFERL_TIME_SCALE=0.1 BUSTAFERL_FRESH_BOOT=1 \
	  valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=definite \
	           --errors-for-leak-kinds=definite \
	           --suppressions=test/test_native_runtime/valgrind.supp \
	           --log-file=$(NR_DIR)/valgrind.log \
	           $(NR_BIN)

native-runtime-massif: native-runtime-build  ## 50 cycles unter massif, schreibt $(NR_DIR)/massif-v2.{out,txt}
	@mkdir -p $(NR_DIR)
	BUSTAFERL_MAX_CYCLES=50 BUSTAFERL_TIME_SCALE=0.05 BUSTAFERL_FRESH_BOOT=1 \
	  valgrind --tool=massif \
	           --pages-as-heap=no \
	           --time-unit=ms \
	           --detailed-freq=10 \
	           --massif-out-file=$(NR_DIR)/massif-v2.out \
	           $(NR_BIN)
	@ms_print $(NR_DIR)/massif-v2.out > $(NR_DIR)/massif-v2.txt
	@echo "[massif] peak snapshot summary:"
	@grep -E "peak|^[ ]+[0-9]+ " $(NR_DIR)/massif-v2.txt | head -n 20 || true

native-runtime-day: native-runtime-build  ## 24 h Soak, schreibt PGM-Sammlung
	@mkdir -p $(NR_DIR)
	@echo "[runtime] 24h soak — interrupt with Ctrl-C; output in $(NR_DIR)/"
	BUSTAFERL_TIME_SCALE=1.0 BUSTAFERL_FRESH_BOOT=1 \
	  timeout 86400 $(NR_BIN) || true

native-runtime-https-smoke:  ## live-call check gegen Wiener-Linien-Endpoints
	@mkdir -p $(NR_DIR)
	g++ -std=gnu++17 -Wall -Wextra -O0 -g -DNATIVE_BUILD -I src \
	    test/test_native_runtime/https_smoke.cpp $(NR_LDLIBS) \
	    -o $(NR_DIR)/https_smoke
	$(NR_DIR)/https_smoke

# --- Code quality ---

format:                ## run clang-format in place
	@find src test -type f \( -name '*.h' -o -name '*.cpp' \) \
	  -not -path '*/fixtures/*' \
	  -not -name 'secrets.h' \
	  -print0 | xargs -0 clang-format -i

format-check:          ## verify formatting without writing
	@# secrets.h is gitignored/generated (copied from secrets.h.example in CI
	@# and locally via `make secrets`) — not linted source, so skip it.
	@find src test -type f \( -name '*.h' -o -name '*.cpp' \) \
	  -not -path '*/fixtures/*' \
	  -not -name 'secrets.h' \
	  -print0 | xargs -0 clang-format --dry-run --Werror

lint:                  ## run cppcheck
	cppcheck --enable=warning,style,performance,portability \
	  --inconclusive \
	  --error-exitcode=1 \
	  --suppress=missingIncludeSystem --inline-suppr \
	  --std=c++17 \
	  -q src/

tidy:                  ## run clang-tidy on host-compilable src/ TUs
	@# Generate compile_commands.json from the native env.
	@$(PIO) run -e native -t compiledb >/dev/null
	@# Rewrite -I paths into vendored libs to -isystem so clang-tidy
	@# does not analyse third-party headers (would produce tens of
	@# thousands of findings inside ArduinoJson etc.).
	@sed -i -E 's@-I(\.pio/libdeps/[^ ]+)@-isystem \1@g' compile_commands.json
	@# Tidy only the platform-neutral business logic (data/ + logic/).
	@# Skipped, matching the intent below:
	@#   * .pio/ vendor TUs (ArduinoFake, Adafruit_GFX, U8g2) — pulled into
	@#     the native build via library.json srcDir tricks; would flood the
	@#     report with non-actionable third-party findings.
	@#   * src/render/ TUs — they transitively include Adafruit_GFX.h /
	@#     U8g2 via layout.h/canvas.h. On a cold CI runner without a warm
	@#     ~/.platformio package cache, clang-tidy can't resolve those
	@#     headers and errors out ("Adafruit_GFX.h file not found"). These
	@#     files are covered by cppcheck (`make lint`) and the on-device
	@#     render tests, so tidy skips them for a deterministic, env-
	@#     independent result.
	@python3 -c "import json; \
	  print('\n'.join(e['file'] for e in json.load(open('compile_commands.json')) \
	    if not e['file'].startswith('.pio/') \
	    and 'src/render/' not in e['file']))" \
	  | xargs clang-tidy -p . --quiet

# --- Housekeeping ---

size:                  ## show firmware size breakdown
	$(PIO) run -e esp32dev -t size

secrets:               ## create secrets.h from template if missing
	@if [ ! -f src/secrets.h ]; then \
	  cp secrets.h.example src/secrets.h; \
	  echo "src/secrets.h created — edit it before building."; \
	else \
	  echo "src/secrets.h already exists, not touching it."; \
	fi

clean:                 ## remove build artifacts + .tmp/
	$(PIO) run -t clean
	rm -rf .pio build $(TMP)
