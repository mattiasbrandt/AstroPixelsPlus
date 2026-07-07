BUILD_ENV ?= astropixelsplus
OTA_IP ?= astropixelsplus.local
FIRMWARE_BIN ?= .pio/build/$(BUILD_ENV)/firmware.bin
SPIFFS_BIN ?= .pio/build/$(BUILD_ENV)/spiffs.bin

-include user.mk

.PHONY: build buildfs gate ota uploadfs smoke test

build:
	pio run -e $(BUILD_ENV)

buildfs:
	pio run -e $(BUILD_ENV) -t buildfs

smoke:
	python3 tools/command_compat_matrix.py --dry-run

test:
	python3 tools/test_dome_layout_validation.py
	python3 tools/test_dome_layout_preview.py
	python3 tools/test_operator_disabled_interlock.py
	python3 tools/test_wiring_commissioning_seam.py
	python3 tools/test_marcduino_ingress_echo_policy.py

gate: build test smoke

ota: gate
	python3 tools/http_ota_upload.py firmware --host "$(OTA_IP)" --file "$(FIRMWARE_BIN)"

uploadfs: gate buildfs
	python3 tools/http_ota_upload.py filesystem --host "$(OTA_IP)" --file "$(SPIFFS_BIN)"
