# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**AstroPixelsPlus** is ESP32 firmware for controlling an R2-D2 robot's visual subsystems: logic displays (front/rear LED arrays), holo projectors (servo-positioned RGB lights), and dome panels (servo-actuated). It implements the Marcduino command protocol and exposes a WiFi web interface for remote control.

Primary dependencies: **ReelTwo** library (core framework), Adafruit_NeoPixel, FastLED, DFRobotDFPlayerMini.

## Build & Upload (PlatformIO)

```bash
# Compile
pio run -e astropixelsplus

# Upload firmware
pio run -e astropixelsplus -t upload

# Upload SPIFFS filesystem
pio run -e astropixelsplus -t uploadfs

# Monitor serial (115200 baud)
pio device monitor -p /dev/ttyUSB0 -b 115200

# Build + upload in one step
pio run -e astropixelsplus -t upload && pio device monitor -p /dev/ttyUSB0 -b 115200
```

The Makefile (`make TARGET=ESP32`) is an alternative build path using a custom `../Arduino.mk` shim — prefer PlatformIO.

## Web UI Local Testing Workflow (Async Web Server)

Use this workflow when iterating on files in `data/` (HTML/CSS/JS) before uploading to SPIFFS.

```bash
# Start local static server from repo root
python3 -m http.server 8080 --directory data > /tmp/astropixels-http.log 2>&1 &

# Open in browser
http://localhost:8080/

# Tail request/errors log while testing
tail -f /tmp/astropixels-http.log

# Stop the local server when done
pkill -f "http.server 8080 --directory data"
```

Recommended collaboration loop:
1. Implement UI changes in `data/*`
2. Start local server and test in browser
3. User validates UX/behavior manually
4. Agent tails `/tmp/astropixels-http.log` to catch missing files/404s/runtime serving issues
5. Iterate until stable, then upload SPIFFS (`pio run -e astropixelsplus -t uploadfs`)

## Architecture

### Command Flow

All control paths converge on the same Marcduino command dispatcher:

```
Serial2 (2400 baud, Marcduino hardware)  ─┐
WiFi socket (port 2000)                   ├─→ MARCDUINO_ACTION macros → hardware drivers
Web UI button press                       ─┘
```

Commands are parsed from a **64-byte buffer** (hardcoded in ReelTwo). Commands longer than 63 chars are silently truncated — this is a known architectural constraint.

### Subsystem Files

| File | Responsibility |
|------|---------------|
| `AstroPixelsPlus.ino` | Setup, event loop, hardware pin/object initialization |
| `MarcduinoLogics.h` | `@1T`, `@2T`, `@0T` commands → LogicEngineRenderer sequences |
| `MarcduinoHolo.h` | `*ON`, `*OF`, `*HP`, `*RD`, `*HN` → holo projector servo/LED |
| `MarcduinoPanel.h` | `:OP`, `:CL`, `:OF` → PCA9685 servo panel choreography |
| `MarcduinoSound.h` | `@4Sn` → DFPlayer Mini MP3 playback |
| `MarcduinoSequence.h` | Compound choreography (panels + holos + LEDs coordinated) |
| `WebPages.h` | Web UI layout — WButton/WSelect/WLabel widget declarations |
| `logic-sequences.h` | 26 named LogicEngine animation sequences (LOGICENGINE_SEQ macros) |
| `effects/*.h` | Custom LED rendering effects (Plasma, Fractal, MetaBalls, FadeAndScroll, Bitmap) |
| `menus/*.h` | Optional TFT LCD screen layouts (physical display, if connected) |

### Marcduino Command Prefix Reference

| Prefix | Domain |
|--------|--------|
| `@1T`, `@2T`, `@0T` | Logic sequences (Front / Rear / All) |
| `@APLE` | AstroPixel direct LED effect |
| `:OP`, `:CL`, `:OF` | Panel open / close / off |
| `*ON`, `*OF` | Holo LED on / off |
| `*HP` | Holo position (3-digit: projector + position) |
| `*RD`, `*HN` | Holo random move / nod |
| `@4Sn` | Sound playback (bank/track) |
| `D###` | Device settings (e.g., D198 = disable auto-twitch) |
| `#AP...` | Configuration persistence (e.g., `#APWIFI1`) |

### Adding a New Marcduino Command

1. Choose the correct `Marcduino*.h` file by prefix domain.
2. Add a `MARCDUINO_ACTION(Name, "prefix", ({ /* body */ }))` block.
3. Call the appropriate hardware driver (LogicEngineRenderer, ServoDispatch, DFPlayerMini, etc.).
4. If exposing via web UI, add the corresponding widget to `WebPages.h` (see memory constraint below).

## Critical Constraints

### ESP32 Static Initialization Memory Limit

`WButton` in ReelTwo calls `malloc()` during global object construction (before `setup()`). The ESP32 heap is not fully available at this point. **Exceeding ~44 WButton instances + 7 String arrays causes a boot crash** with no useful error message.

Rules for `WebPages.h`:
- Use `WSelect` (dropdown) for grouped options instead of multiple `WButton`s.
- Use `const char*` arrays, not `String` arrays, for option lists.
- Do not increase WButton count without testing boot stability.

### Known Bugs (CRITICAL_FINDINGS.md)

- **MetaBalls effect**: incorrect stride math (`i * w + x` should be `i * mb_number + x`) causes buffer overwrite.
- **MetaBalls init**: 5 `Serial.println()` calls create timing jitter on startup.
- **FadeAndScrollEffect**: `random(1)` always returns 0 — RGB and Half palettes are never selected.
- **Remote prefs key mismatch**: `#APRNAME` and `#APRSECRET` use wrong NVS preference key strings.
- **Blocking delays**: `delay()` calls in hot paths (`AstroPixelsPlus.ino`) stall the event loop.

## Hardware

- **MCU**: ESP32 (esp32dev), 4MB flash, SPIFFS partition
- **Servos**: PCA9685 I2C servo controller (SDA GPIO21, SCL GPIO22) — 13 dome panel channels
- **LEDs**: NeoPixel strips on configurable GPIO pins (Front Logic, Rear Logic, Holos)
- **Audio**: DFPlayer Mini on Serial (MP3, 9 banks, 225+ sounds)
- **Marcduino input**: Serial2 GPIO16/17 @ 2400 baud
- **WiFi AP**: SSID `AstroPixels`, password `Astromech`, web UI at `http://192.168.4.1`
- **OTA**: Supported via ReelTwo WiFi OTA mechanism
