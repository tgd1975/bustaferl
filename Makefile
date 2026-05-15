# bustaferl — convenience wrapper around PlatformIO
#
# `make help` lists everything. Default target is help.

.DEFAULT_GOAL := help
.PHONY: help build upload monitor flash test test-verbose test-esp32 clean \
        format format-check lint size secrets ci

PIO := pio

help:                  ## list available targets
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) \
	  | awk 'BEGIN{FS=":.*?## "}{printf "  %-16s %s\n", $$1, $$2}'

build:                 ## compile firmware for ESP32
	$(PIO) run -e esp32dev

upload:                ## flash firmware to attached ESP32
	$(PIO) run -e esp32dev -t upload

monitor:               ## open serial monitor (115200)
	$(PIO) device monitor -b 115200

flash: upload monitor  ## upload + open monitor

test:                  ## run unit tests on host
	$(PIO) test -e native

test-verbose:          ## run unit tests with verbose output
	$(PIO) test -e native -v

test-esp32:            ## on-device smoke test: WiFi + HTTPS + parse vs live API
	$(PIO) test -e esp32-test-fetch -v

clean:                 ## remove build artifacts
	$(PIO) run -t clean
	rm -rf .pio build

format:                ## run clang-format in place
	@find src test -type f \( -name '*.h' -o -name '*.cpp' \) \
	  -not -path '*/fixtures/*' \
	  -print0 | xargs -0 clang-format -i

format-check:          ## verify formatting without writing
	@find src test -type f \( -name '*.h' -o -name '*.cpp' \) \
	  -not -path '*/fixtures/*' \
	  -print0 | xargs -0 clang-format --dry-run --Werror

lint:                  ## run cppcheck
	cppcheck --enable=warning,style --error-exitcode=1 \
	  --suppress=missingIncludeSystem --inline-suppr -q src/

size:                  ## show firmware size breakdown
	$(PIO) run -e esp32dev -t size

secrets:               ## create secrets.h from template if missing
	@if [ ! -f src/secrets.h ]; then \
	  cp secrets.h.example src/secrets.h; \
	  echo "src/secrets.h created — edit it before building."; \
	else \
	  echo "src/secrets.h already exists, not touching it."; \
	fi

ci: format-check lint test build  ## what CI runs
