# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**AstroPixelsPlus** is ESP32 firmware for controlling an R2-D2 robot's visual subsystems: logic displays (front/rear LED arrays), holo projectors (servo-positioned RGB lights), and dome panels (servo-actuated). It implements the Marcduino command protocol and exposes a WiFi web interface for remote control.

Primary dependencies: **ReelTwo** library (core framework), Adafruit_NeoPixel, FastLED, DFRobotDFPlayerMini.

## Build & Upload (PlatformIO)

Important hardware rule: when the full droid is connected together, USB upload
is not feasible and must not be attempted. In the normal integrated setup, any
USB cable may be connected to another controller such as protoArtoo, not the
dome AstroPixelsPlus ESP32. Use HTTP OTA for dome firmware/SPIFFS updates.
Only use PlatformIO USB upload or serial monitor in explicit "bench mode", where
the dome controller is isolated and the user confirms the USB cable is plugged
directly into that dome ESP32.

```bash
# Compile
pio run -e astropixelsplus

# Normal integrated-droid workflow: HTTP OTA to the dome controller
make ota OTA_IP=astropixelsplus.local
make uploadfs OTA_IP=astropixelsplus.local

# AP fallback address
make ota OTA_IP=192.168.4.1
make uploadfs OTA_IP=192.168.4.1

# Bench mode only, after explicit user confirmation that USB is connected
# directly to the isolated dome controller:
pio run -e astropixelsplus -t upload
pio run -e astropixelsplus -t uploadfs
pio device monitor -p /dev/ttyUSB0 -b 115200
```

## Repository Hygiene

Keep all text files LF-only. This repo has `.gitattributes` and `.editorconfig`
to prevent CRLF churn from edit tools; do not reintroduce CRLF or mixed line
endings when touching legacy firmware files.

### Post-build compatibility smoke test (fork workflow)

After firmware/library changes and a successful build, run the command compatibility matrix runner before final sign-off:

```bash
# Dry-run (show matrix groups/commands)
python3 tools/command_compat_matrix.py --dry-run

# Full smoke run against default AP target
python3 tools/command_compat_matrix.py

# Example against station-mode IP with focused groups
python3 tools/command_compat_matrix.py --host 10.0.0.21 --group holos --group logics
```

The runner sends commands through `/api/cmd` and verifies `/api/state` + `/api/health` after each step. Treat failures as regressions that must be investigated before release/upload sign-off.

The Makefile wraps PlatformIO for local builds and uses `tools/http_ota_upload.py` for OTA through the controller's HTTP upload endpoints. `make gate` runs the firmware build, the source-level tests in `tools/test_*.py` (`make test`), and the compatibility dry-run, `make ota` posts `.pio/build/astropixelsplus/firmware.bin` to `/upload/firmware`, and `make uploadfs` builds/posts `.pio/build/astropixelsplus/spiffs.bin` to `/upload/filesystem`. Firmware and SPIFFS uploads remain separate.

Use these verification labels when recording validation status: `software-verified`, `controller-upload-verified`, `full-hardware-verified`, `partial`, `full-hardware-required`.

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
5. Iterate until stable, then upload SPIFFS over HTTP OTA (`make uploadfs OTA_IP=astropixelsplus.local`, or the active dome IP)

## Fork Documentation Maintenance

When this fork adds or changes behavior (async web routes, UI flows, safety/hardening logic, command mappings, validation rules, etc.), update `FORK_IMPROVEMENTS.md` in the same work cycle.

Keep that file as the fork-specific change log and rationale document so contributors can quickly understand what differs from upstream.

## Planning & Approval Gate (Fork Workflow)

For any non-trivial request (multi-file, behavior change, API change, upload, or commit), use this mandatory flow before implementation:

1. Restate the user request in plain language.
2. List assumptions, ambiguities, and targeted clarifying questions (if any).
3. Propose a concrete implementation plan with explicit files to change.
4. Present a short TODO checklist with verification steps.
5. Stop and get explicit user approval before editing files.

Rules:
- Do not implement code changes before user approves the plan.
- If scope changes mid-task, pause, re-plan, and get approval again.
- Keep plans concise and practical; avoid over-design.
- For trivial one-file typo/text edits only, a lightweight one-line plan is enough.

## Architecture

- **One translation unit.** `AstroPixelsPlus.ino` includes the root headers mid-file, after the macros, globals and ReelTwo types they use; a header is not valid on its own.
- **Web UI** is static files in `data/` (SPIFFS), served with REST/WebSocket routes by `AsyncWebInterface.h`. The old ReelTwo `WebPages.h`/`WButton` UI is gone: it allocated heap during static init and crashed boot past ~44 buttons.
- **Command dispatch:** every transport calls `marcduinoIngressAdmit()` (`MarcduinoIngress.h`), which queues for `Marcduino::processCommand()` and the `MARCDUINO_ACTION` handlers in `Marcduino*.h` / `DomeSequences.h`. The legacy `MarcduinoSerial` path bypasses ingress. Read `docs/MARCDUINO_INGRESS.md` before changing dispatch: `processCommand()` only schedules a handler, the shared `player` holds one animation, `DM:*` sequences are handed off via `dome_pendingAnim`.
- **ReelTwo source** is at `.pio/libdeps/astropixelsplus/Reeltwo/src` after `make build`. Read it for library behavior (dispatch, `CommandEvent`, gadget command formats).
- Commands over 63 characters are silently truncated on the legacy serial, WebSocket and body-link WiFi paths (64-byte buffers).
- References: commands `docs/COMMANDS.md`, REST/WS `docs/REST_API.md`, pins and wiring `docs/HARDWARE_WIRING.md`, decisions `docs/adr/`, review rules `CODING_STANDARDS.md`.

### Adding a New Marcduino Command

1. Add a `MARCDUINO_ACTION(Name, PREFIX, ({ /* body */ }))` block in the `Marcduino*.h` file for that prefix domain, calling the hardware driver (LogicEngineRenderer, ServoDispatch, DFPlayerMini, ...).
2. For a web control, add it to the relevant `data/*.html` page and send it with `sendCmd()` from `data/app.js` (goes through `/api/cmd`).
3. `make test` runs `tools/test_ui_command_routing.py`, which fails on any UI or REST command string that matches no handler: `Marcduino::processCommand()` drops those silently.

## Hardware

- **MCU**: ESP32 (esp32dev), 4MB flash, dual OTA app slots, SPIFFS partition
- **Servos**: PCA9685 I2C servo controller (SDA GPIO21, SCL GPIO22) — 13 dome panel channels
- **LEDs**: NeoPixel strips on configurable GPIO pins (Front Logic, Rear Logic, Holos)
- **Audio**: DFPlayer Mini on Serial (MP3, 9 banks, 225+ sounds)
- **Marcduino input**: Serial2 GPIO16/17 @ 2400 baud
- **WiFi AP**: SSID `AstroPixels`, password `Astromech`, web UI at `http://192.168.4.1`
- **OTA**: HTTP multipart upload via `/upload/firmware` and `/upload/filesystem`; default host is `astropixelsplus.local`
