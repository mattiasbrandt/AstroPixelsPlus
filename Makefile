#TARGET?=Mega2560
TARGET?=ESP32
PORT?=/dev/ttyUSB0
#ESP32_FILESYSTEM=littlefs
#ESP32_PSRAM=enabled
ESP32_FILESYSTEM=spiffs
ESP32_FILESYSTEM_PART=spiffs
ESP32_PARTSCHEME=min_spiffs
ESP32_FLASHSIZE=4MB
GITHUB_REPOS= \
reeltwo/Reeltwo \
adafruit/Adafruit_NeoPixel \
FastLED/FastLED \
DFRobot/DFRobotDFPlayerMini

# NOTE: Arduino.mk is resolved relative to this repo's parent directory.
# This Makefile only works if the Arduino.mk repo is checked out adjacent to this one.
# PlatformIO (platformio.ini) is the canonical build system for this project.
include ../Arduino.mk
