# bustaferl — convenience wrapper around PlatformIO
#
# Three test buckets:
#   test-native    host-only unit tests, ~5 s
#   test-device    on-device ESP32 tests, ~5-10 min with hardware
#   test-longterm* opt-in long-runners, 3 min to 24 h
#
# `make help` lists every target.

.DEFAULT_GOAL := help
.PHONY: help build upload monitor flash \
        test test-native test-device test-all test-device-trace \
        test-longterm-smoke test-longterm-jitter test-longterm-horizon-mock \
        test-longterm-wake test-longterm-soak-5min test-longterm-soak-15min \
        test-longterm-soak-1h test-longterm-horizon-scan \
        test-longterm-horizon-evening test-longterm-day-full \
        native-runtime-build native-runtime-smoke native-runtime-day \
        native-runtime-https-smoke \
        clean format format-check lint tidy size secrets ci ci-heavy

PIO        := pio
# Overridable so CI can pin an exact major version (formatting output
# differs across clang-format majors; see .github/workflows/ci.yml).
CLANG_FORMAT ?= clang-format
TMP        := .tmp
RESULTS    := $(TMP)/test-results.json
TRACE_DIR  := $(TMP)/traces
NR_DIR     := $(TMP)/native-runtime
NR_BIN     := $(NR_DIR)/bustaferl-native-runtime

DEVICE_ENVS := -e device-fetch -e device-persistent -e device-render \
               -e device-sleep -e device-schedule

# Sources compiled into the native-runtime driver. Anything Adafruit-GFX
# or Arduino-Core specific stays out — same constraint as env:native's
# build_src_filter in platformio.ini.
NR_SRC := \
  test/test_native_runtime/main.cpp \
  test/test_native_runtime/DiskStore.cpp \
  test/test_native_runtime/HttpsNet.cpp \
  test/test_native_runtime/RecordingRenderer.cpp \
  src/data/efa_parse.cpp \
  src/data/wienerlinien_parse.cpp \
  src/logic/api_fetcher.cpp \
  src/logic/boot_sequencer.cpp \
  src/logic/button_classifier.cpp \
  src/logic/cycle_runner.cpp \
  src/logic/display_apply.cpp \
  src/logic/filter_builder.cpp \
  src/logic/filter_health.cpp \
  src/logic/refresh_planner.cpp \
  src/logic/render_input.cpp \
  src/logic/schedule_fetcher.cpp \
  src/logic/schedule_refresh.cpp \
  src/logic/sleep_planner.cpp \
  src/logic/slot_merger.cpp \
  src/logic/snapshot_fetcher.cpp \
  src/logic/snapshot_logger.cpp \
  src/logic/stale_policy.cpp \
  src/render/rle.cpp

NR_CXXFLAGS := -std=gnu++17 -Wall -Wextra -O2 -g -DNATIVE_BUILD \
               -I src -isystem .pio/libdeps/native/ArduinoJson/src
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
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) \
	  | awk 'BEGIN{FS=":.*?## "}{printf "  %-30s %s\n", $$1, $$2}'

# --- Firmware ---

build:                 ## compile firmware for ESP32
	$(PIO) run -e esp32dev

upload:                ## flash firmware to attached ESP32
	$(PIO) run -e esp32dev -t upload

monitor:               ## open serial monitor (115200)
	$(PIO) device monitor -b 115200

flash: upload monitor  ## upload + open monitor

# --- Routine tests ---

test:                  test-native            ## alias: fast host tests

test-native:                                  ## all test_native_* (~5 s)
	$(PIO) test -e native

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

# --- CI / quality ---

ci: format-check lint tidy test-native build  ## host only — fast, pre-commit-tauglich (~30-45 s)

ci-heavy: ci native-runtime-smoke  ## ci + native-runtime-smoke (~5-6 min, CI-Pipeline)

# --- Native runtime (Schritt 9 — host loop) ---

native-runtime-build: $(NR_BIN)  ## build host loop binary

$(NR_BIN): $(NR_SRC)
	@# ArduinoJson lives under .pio/libdeps/native/; ensure it's been
	@# fetched by triggering the native env at least once.
	@[ -d .pio/libdeps/native/ArduinoJson ] || $(PIO) run -e native -t compiledb >/dev/null
	@mkdir -p $(NR_DIR)
	g++ $(NR_CXXFLAGS) $(NR_SRC) $(NR_LDLIBS) -o $(NR_BIN)

native-runtime-smoke: $(NR_BIN)  ## 10 cycles unter valgrind, ~5 min
	@mkdir -p $(NR_DIR)
	BUSTAFERL_MAX_CYCLES=10 BUSTAFERL_TIME_SCALE=0.1 BUSTAFERL_FRESH_BOOT=1 \
	  valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=definite \
	           --errors-for-leak-kinds=definite \
	           --suppressions=test/test_native_runtime/valgrind.supp \
	           --log-file=$(NR_DIR)/valgrind.log \
	           $(NR_BIN)

native-runtime-day: $(NR_BIN)  ## 24 h Soak, schreibt PGM-Sammlung
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

# secrets.h is gitignored and user-owned (CI drops in the untracked
# template) — formatting it is neither possible to enforce nor useful.
format:                ## run clang-format in place
	@find src test -type f \( -name '*.h' -o -name '*.cpp' \) \
	  -not -path '*/fixtures/*' \
	  -not -name 'secrets.h' \
	  -print0 | xargs -0 $(CLANG_FORMAT) -i

format-check:          ## verify formatting without writing
	@find src test -type f \( -name '*.h' -o -name '*.cpp' \) \
	  -not -path '*/fixtures/*' \
	  -not -name 'secrets.h' \
	  -print0 | xargs -0 $(CLANG_FORMAT) --dry-run --Werror

lint:                  ## run cppcheck
	cppcheck --enable=warning,style,performance,portability \
	  --inconclusive \
	  --error-exitcode=1 \
	  --suppress=missingIncludeSystem --inline-suppr \
	  --std=c++17 \
	  -q src/

tidy:                  ## run clang-tidy on host-compilable src/ TUs
	@# Generate compile_commands.json from the native env. Only the
	@# platform-neutral TUs (data/, logic/, render/rle) land in it;
	@# ESP32-only files (main.cpp, hal/Esp32*.cpp, render/layout.cpp,
	@# render/error_overlay.cpp) are intentionally skipped — they pull
	@# in Adafruit_GFX / WiFi etc. which clang-tidy would either
	@# misanalyse or flood with library findings. They are still
	@# covered by cppcheck (`make lint`) and the on-device test set.
	@$(PIO) run -e native -t compiledb >/dev/null
	@# Rewrite -I paths into vendored libs to -isystem so clang-tidy
	@# does not analyse third-party headers (would produce tens of
	@# thousands of findings inside ArduinoJson etc.).
	@sed -i -E 's@-I(\.pio/libdeps/[^ ]+)@-isystem \1@g' compile_commands.json
	@# Tidy exactly the TUs the database knows about.
	@python3 -c "import json; \
	  print('\n'.join(e['file'] for e in json.load(open('compile_commands.json'))))" \
	  | xargs clang-tidy -p . --quiet

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
