BUILD_ENV ?= astropixelsplus
OTA_IP ?= astropixelsplus.local
OTA_ENV := $(BUILD_ENV)_ota

-include user.mk

.PHONY: build gate ota uploadfs smoke

build:
	pio run -e $(BUILD_ENV)

smoke:
	python3 tools/command_compat_matrix.py --dry-run

gate: build smoke

ota: gate
	pio run -e $(OTA_ENV) -t upload --upload-port $(OTA_IP)

uploadfs: gate
	pio run -e $(OTA_ENV) -t uploadfs --upload-port $(OTA_IP)
