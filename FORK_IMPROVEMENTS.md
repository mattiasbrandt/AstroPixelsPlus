# AstroPixelsPlus Fork Improvements

This file tracks fork-specific behavior and feature changes that differ from upstream usage or are important for builders/operators.

## 2026-02 Core Platform and Web Stack

### Dependency upgrade validation (current test baseline)
- Updated and pinned dependency set for reproducible builds:
  - `Reeltwo#23.5.3`
  - `Adafruit_NeoPixel#1.15.4`
  - `FastLED@3.10.3`
  - `DFRobotDFPlayerMini#V1.0.6`
  - `ESPAsyncWebServer@3.5.1`
  - `AsyncTCP@3.3.2`
- Build verification passes with this set (`pio run` and `buildfs`).
- Runtime validation on hardware confirms:
  - Web server starts and remains stable.
  - Light control path (logic/holo command handling) remains functional.

### Async Web migration (transport layer only)
- Migrated UI transport from static ReelTwo `WebPages.h` patterns to async REST/WS + SPIFFS pages.
- Core control path remains unchanged: all command sources still route into `Marcduino::processCommand(...)`.
- Added/maintained API endpoints for state/health/logs, command posting, preferences, reboot, and OTA upload.

### Async startup ordering hardening
- Fixed startup ordering for async web startup to avoid early lwIP calls before WiFi connectivity is established.
- `initAsyncWeb()` is now started from WiFi-connected callback flow instead of unconditional early setup startup.
- This addresses observed `tcpip_api_call ... Invalid mbox` crashloop behavior seen during dependency test iterations.

### OTA scope and UX
- OTA firmware page updates app firmware only.
- SPIFFS web assets (`data/*`) still require filesystem upload (`uploadfs`).
- OTA UX improved to handle reconnect/reboot transition cleanly.

## 2026-02 Command and Sequence Enhancements

### Command traceability
- Added source-tagged command logging for ingress paths:
  - `astropixel-web-api`
  - `astropixel-web-ws`
  - `wifi-marcduino`
  - `usb-serial`
  - `i2c-slave`

### Holo command corrections
- Corrected holo OFF semantics to ReelTwo clear command form:
  - `*OF00 -> HPA096`
  - `*OF01 -> HPF096`
  - `*OF02 -> HPR096`
  - `*OF03 -> HPT096`
  - `*OF04 -> HPD096`

### Added sequence/mode coverage for compatibility and demos
- Added sequence handlers:
  - `:SE10`, `:SE11`, `:SE12`, `:SE13`, `:SE14`, `:SE15`, `:SE16`, `:SE58`
- Expanded web sequence/home quick-action controls and mood preset buttons.

### Additive compatibility overlays (legacy-safe)
- Kept existing commands; added aliases/wrappers for broader controller compatibility.
- Holo overlays added:
  - `*HD07`, `*HD08`, `*HD09`
  - `*HW00`, `*HN00`
- Logic quick-effect overlays added:
  - `@0T12/@1T12/@2T12` (Rainbow)
  - `@0T15/@1T15/@2T15` (Lights Out)
  - `@0T22/@1T22/@2T22` (Fire)
  - `@0T24/@1T24/@2T24` (Pulse)

## 2026-02 Artoo-Oriented and Sound Controls

### Artoo serial telemetry profile
- Added preference keys:
  - `artoo` (enable telemetry profile)
  - `artoobaud` (expected upstream baud)
- Added API/health visibility fields:
  - `artoo`, `artoo_enabled`, `artoo_baud`, `artoo_last_seen_ms`, `artoo_signal_bursts`
- Added Serial setup UI section for Artoo communication context.

### Local sound execution feature toggle
- Added `msoundlocal` preference.
- When disabled, sound commands may still be accepted but local playback/startup/random local sound behavior is not actuated.

## 2026-02 Remote Clarification and Build-Time Control

### R2 Touch vs Droid Remote clarity
- Clarified that `remote.html` is for **Droid Remote ESPNOW pairing** (host/secret), not R2 Touch IP/port setup.
- Added explicit R2 Touch connection guidance under Serial setup (controller IP + port `2000`).

### Build-time Droid Remote toggle (memory-focused)
- Added build flag default:
  - `-DAP_ENABLE_DROID_REMOTE=0`
- `USE_DROID_REMOTE` now depends on this build flag.
- When compiled out:
  - Remote runtime state is forced disabled.
  - API reports `remoteSupported=false`.
  - Setup/Remote pages show build-disabled status and disable/hide remote controls accordingly.

## 2026-02 Reliability and Static-Analysis Hardening

### Correctness
- Fixed MetaBalls stride/indexing to match buffer layout and prevent OOB behavior.
- Fixed remote preference command-key mismatches for hostname/secret handling.

### Runtime safety
- Replaced blocking async reboot delays with scheduled reboot path in async loop.
- Added cached I2C health scan data to avoid repeated full scan on each health request.
- Added lock-protected telemetry reads/writes for shared Artoo counters.

### Startup/runtime pacing
- Reworked sound module initialization to deferred/retry flow (reduced setup blocking impact).
- Adjusted event loop pacing to avoid overly tight polling.

## 2026-02 Home Health and Status Signals

- Added threshold-colored system status indicators on Home for:
  - Free heap
  - Min free heap since boot
  - Serial2 activity age
  - WiFi quality band
  - I2C probe error count
- Added clearer build-disabled representation for Droid Remote in Home/Setup.
- Home System status now reports a single sound line:
  - `Sound Playback Module: Enabled/Disabled`
  - Disabled reason is shown as either local preference or module setting.

## 2026-02 Validation Tooling

### Command compatibility matrix smoke runner
- Added `tools/command_compat_matrix.py` to validate core Marcduino command families over HTTP API.
- Runner sends command matrix entries through `/api/cmd` and checks `/api/state` + `/api/health` after each command.
- Supports grouped runs (`--group panels|holos|logics|sequences|sound`), target host override, optional API token, and dry-run listing.
- Bench validation completed on this fork: matrix runner executes successfully and reports expected PASS behavior.

## Notes

- This document intentionally focuses on concrete fork behavior/features.
- Local bench-specific diagnostics and personal wiring notes should stay outside this file (for example local `tasks/*` notes) unless they represent a reusable firmware behavior change.

## Historical Context: Why Async Web Migration Happened

Before the async migration, this fork (like upstream-era patterns) relied on ReelTwo web page construction (`WebPages.h`, `WifiWebServer`, `WButton`-heavy static UI definitions).

### What was attempted in the legacy model
- Reorganize web pages and add richer controls directly in `WebPages.h`.
- Add more panel/holo/logic controls and grouping improvements inside static widget arrays.
- Tune/trim button layout to fit ESP32 constraints.

### Problems encountered repeatedly
- ESP32 stability degraded as static UI complexity increased.
- `WButton`-heavy static initialization consumed heap early (before full runtime availability), causing fragile boot behavior when control count grew.
- Practical UI growth ceiling made further feature expansion risky.
- Iteration speed was poor because transport/UI/layout logic were tightly coupled to firmware-side static widget construction.

### Decision rationale for migration
The fork moved to async web + SPIFFS UI because it solves the structural bottleneck instead of only trimming symptoms.

- Async handlers (`AsyncWebInterface.h`) keep firmware-side web control lightweight.
- UI complexity now lives in static assets (`data/*.html`, `data/app.js`, `data/style.css`) where growth is safer and easier to iterate.
- Command behavior remains consistent because all control paths still dispatch through the same Marcduino handling path.
- This preserves compatibility while unlocking maintainable feature growth.

### Outcome of that decision
- Better stability under richer UI.
- Clearer separation of concerns (firmware command execution vs web presentation).
- Faster iteration on operator UX without reintroducing static-init heap pressure.
