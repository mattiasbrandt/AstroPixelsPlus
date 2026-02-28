# AstroPixelsPlus Fork Improvements

This file tracks fork-specific behavior and feature changes that differ from upstream usage or are important for builders/operators.

---

## Firmware Core

Changes to the underlying platform, runtime, and stability foundation.

### Dependency Upgrade & Pinning
Updated and pinned dependency set for reproducible builds:
- `Reeltwo#23.5.3`
- `Adafruit_NeoPixel#1.15.4`
- `FastLED@3.10.3`
- `DFRobotDFPlayerMini#V1.0.6`
- `ESPAsyncWebServer@3.5.1`
- `AsyncTCP@3.3.2`

Build verification passes with this set (`pio run` and `buildfs`). Runtime validation on hardware confirms web server stability and light control path functionality.

### Async Web Migration (Transport Layer Only)
Migrated UI transport from static ReelTwo `WebPages.h` patterns to async REST/WS + SPIFFS pages. Core control path remains unchanged: all command sources still route into `Marcduino::processCommand(...)`. Added/maintained API endpoints for state/health/logs, command posting, preferences, reboot, and OTA upload.

### Async Startup Ordering Hardening
Fixed startup ordering for async web startup to avoid early lwIP calls before WiFi connectivity is established. `initAsyncWeb()` is now started from WiFi-connected callback flow instead of unconditional early setup startup. This addresses observed `tcpip_api_call ... Invalid mbox` crashloop behavior seen during dependency test iterations.

### Runtime Health & Diagnostics
Added threshold-colored system status indicators on Home for:
- Free heap
- Min free heap since boot
- Serial2 activity age
- WiFi quality band
- I2C probe error count

Health model is intentionally split into two layers:
- **Quick health probes** for expected servo-controller roles (`0x40` panels, `0x41` holos) used by `/api/health` and periodic UI status.
- **Deep diagnostics scan** for full-bus discovery over `0x01-0x7E` used by `/api/diag/i2c?force=1`.

Deep diagnostics JSON reports scan context and operator-facing details: `scan_mode`, `devices`, `device_count`, `scan_duration_us`, `scan_age_ms`, per-controller codes/streaks, and `operator.faults` / `operator.hints`.

### Soft Sleep / Wake Runtime Control
Added runtime soft sleep state tracking in firmware (`sleepMode`, `sleepSinceMs`) while keeping ESP32, WiFi, and async web services online. Added new authenticated API endpoints:
- `POST /api/sleep` to enter quiet low-activity profile
- `POST /api/wake` to restore active profile

Sleep entry uses existing Marcduino/ReelTwo command patterns: `:SE10` (quiet reset), `*ST00` (disable holo twitch), `@0T15` (logic lights-out). Wake exit restores active baseline using `:SE14` (awake+ profile). While in sleep mode, incoming Marcduino commands are gated (blocked) except wake-profile commands.

### Artoo Serial Telemetry Profile
Added preference keys:
- `artoo` (enable telemetry profile)
- `artoobaud` (expected upstream baud)

Added API/health visibility fields: `artoo`, `artoo_enabled`, `artoo_baud`, `artoo_last_seen_ms`, `artoo_signal_bursts`. Added Serial setup UI section for Artoo communication context. Implemented lock-protected telemetry reads/writes for shared Artoo counters.

### Local Sound Execution Feature Toggle
Added `msoundlocal` preference. When disabled, sound commands may still be accepted but local playback/startup/random local sound behavior is not actuated.

### Code Remediation & Static-Analysis Hardening
Comprehensive review of correctness, robustness, and static-analysis issues completed. All findings addressed — see `CRITICAL_FINDINGS.md` for detailed technical notes.

| Fix | Description | Commit |
|-----|-------------|--------|
| C1 | String reservation in JSON builders | `3762d8e` |
| C2 | Verified same-core I2C access (non-issue) | — |
| C3 | stopRandom() boolean logic fix | `b28a1a1` |
| C4 | Copy command buffer before parsing | `9b72140` |
| C5 | Remove dead return in BitmapEffect | `b28a1a1` |
| C6-C9 | Various correctness fixes | `b28a1a1` |
| H1 | Null-check heap allocations | `d302208` |
| L3 | Makefile documentation | `2145da1` |
| L5 | F() macro for DEBUG_PRINT | `054893e`, `cd48053` |

---

## Marcduino Protocol Extensions

New and altered Marcduino commands for broader controller compatibility.

### Command Traceability
Added source-tagged command logging for ingress paths:
- `astropixel-web-api`
- `astropixel-web-ws`
- `wifi-marcduino`
- `usb-serial`
- `i2c-slave`

### Holo Command Corrections
Corrected holo OFF semantics to ReelTwo clear command form:
- `*OF00 -> HPA096`
- `*OF01 -> HPF096`
- `*OF02 -> HPR096`
- `*OF03 -> HPT096`
- `*OF04 -> HPD096`

### Expanded Holo Command Compatibility (Phase 1)
Added compatibility handlers in `MarcduinoHolo.h` for:
- `*COxxaaabbbcccddd` (compatibility parser retained in firmware)
- `*CHxx` (center command)
- `*RCxx` (RC-center compatibility behavior)
- `*TExx` (mechanical test choreography)

Added matching operator controls in `data/holos.html` under **Expanded Holo Commands** for center/test/RC operations.

### Sequence/Mode Coverage for Compatibility and Demos
Added sequence handlers: `:SE10`, `:SE11`, `:SE12`, `:SE13`, `:SE14`, `:SE15`, `:SE16`, `:SE58`. Expanded web sequence/home quick-action controls and mood preset buttons.

### Additive Compatibility Overlays (Legacy-Safe)
Kept existing commands; added aliases/wrappers for broader controller compatibility.

Holo overlays added:
- `*HD07`, `*HD08`, `*HD09`
- `*HW00`, `*HN00`

Logic quick-effect overlays added:
- `@0T12/@1T12/@2T12` (Rainbow)
- `@0T15/@1T15/@2T15` (Lights Out)
- `@0T22/@1T22/@2T22` (Fire)
- `@0T24/@1T24/@2T24` (Pulse)

### Panel Calibration Command Coverage
Added firmware handlers for panel calibration commands in `MarcduinoPanel.h`:
- `:MVxxdddd` move target panel/group to a temporary position (no persistence)
- `#SOxxdddd` store open position
- `#SCxxdddd` store closed position
- `#SWxx` swap open/closed positions

Added compatibility target mapping for varied builds:
- `00` all panels, `01`-`10` panel groups, `11` pie group, `12` lower dome group, `13` top center panel, `14` top-panels alias, `15` bottom-panels alias.

Added persisted calibration loading at boot in `AstroPixelsPlus.ino` (`loadPersistedPanelCalibration`) so saved open/closed positions survive reboot.

Calibration values accept either:
- `0000`-`0180` as degrees (scaled per-servo), or
- `0544`-`2500` as direct pulse width values.

---

## Web Interface & API

Changes to the operator-facing web UI and HTTP/WebSocket API.

### API Endpoints
- `/api/state` — runtime state payload for UI + WS
- `/api/health` — health summary backed by I2C probes/cache
- `/api/diag/i2c` — deep diagnostic scan with `?force=1`
- `/api/cmd` — command posting
- `/api/sleep`, `/api/wake` — soft sleep control
- OTA upload endpoint for firmware updates

### OTA Scope and UX
OTA firmware page updates app firmware only. SPIFFS web assets (`data/*`) still require filesystem upload (`uploadfs`). OTA UX improved to handle reconnect/reboot transition cleanly.

### Holo UX Alignment and Naming
Updated holoprojector naming in the web UI to builder-standard identifiers:
- `HP1 - Holoprojector 1 Front (FHP)`
- `HP2 - Holoprojector 2 Rear (RHP)`
- `HP3 - Holoprojector 3 Top (THP)`

Default holo `*ONxx` behavior now starts in static mode instead of cycle animation. Holos page now exposes stable per-projector effect controls (`Static`, `Cycle`, `Pulse`, `Rainbow`) and removes unsupported color-selection controls to avoid broken UX.

Holos position direction pads are centered for all three projectors to keep control alignment consistent across desktop/mobile layouts.

Holo servo status card visibility is now conditional: hidden when holo I2C health is green and shown only for warning/error states.

### Panel Calibration UI
Updated Panels calibration UI (`data/panels.html`) to expose extended compatibility targets (13/14/15). Added direct servo easing control via `:SFxx$easing` command builder and live preview.

Panel servo status card visibility is now conditional: hidden when panel I2C health is green and shown only for warning/error states.

### Logic Effects UI Coverage
Added a new **Custom effect** builder card on Logics page to construct full `@APLE` commands from UI controls instead of relying on shortcut-only coverage. Builder supports explicit target selection (both/front/rear), complete effect list, color chips, speed/sensitivity, and duration controls with live command preview.

Added shared command description parsing in `data/app.js` for `@APLE...` payloads and `:SFxx$easing` commands so tooltips and UI hints remain human-readable.

Added front/rear logic scroll-speed sliders on Logics page using existing Marcduino speed commands.

### Home Health and Status Signals
System status indicators added to Home page showing threshold-colored metrics. Sound playback module status shows single line: `Sound Playback Module: Enabled/Disabled` with disabled reason (local preference or module setting).

### Droid Naming
Configurable droid naming across firmware boot text, API state, setup page, and page branding. Build-time default via `AP_DROID_NAME` in `platformio.ini`. Runtime pref key `dname` with validation and max length enforcement.

### R2 Touch vs Droid Remote Clarity
Clarified that `remote.html` is for **Droid Remote ESPNOW pairing** (host/secret), not R2 Touch IP/port setup. Added explicit R2 Touch connection guidance under Serial setup (controller IP + port `2000`).

### Build-Time Droid Remote Toggle
Added build flag default: `-DAP_ENABLE_DROID_REMOTE=0`. `USE_DROID_REMOTE` now depends on this build flag. When compiled out:
- Remote runtime state is forced disabled
- API reports `remoteSupported=false`
- Setup/Remote pages show build-disabled status and disable/hide remote controls accordingly

---

## Build System & Tooling

Changes to build configuration, development workflow, and validation tools.

### PlatformIO Configuration
- Default env: `astropixelsplus`
- `ESP32_ARDUINO_NO_RGB_BUILTIN` defined to prevent FastLED RMT conflicts
- Optimized for size (`-Os`)
- Platform: `espressif32@5.2.0`

### Makefile Documentation
Added comments explaining that `../Arduino.mk` is a relative path dependency — the Makefile only works if Arduino.mk is checked out adjacent to this repo. PlatformIO is the canonical build system.

### Command Compatibility Matrix Smoke Runner
Added `tools/command_compat_matrix.py` to validate core Marcduino command families over HTTP API. Runner sends command matrix entries through `/api/cmd` and checks `/api/state` + `/api/health` after each command. Supports grouped runs (`--group panels|holos|logics|sequences|sound`), target host override, optional API token, and dry-run listing. Bench validation completed on this fork: matrix runner executes successfully and reports expected PASS behavior.

---

## Coverage Snapshot (BetterDuino V4 vs This Fork)

| Family | Firmware coverage | Web coverage | Current status |
|---|---|---|---|
| Panels (`:OP/:CL/:OF/:SE`) | Strong (`:OP00-12`, `:CL00-12`, `:OF00-12`, multiple `:SExx`) | Strong (All/Pie/Lower controls + sequences) | Supported in daily use |
| Panel calibration (`:MV/#SO/#SC/#SW`) | Implemented with persistence + extended targets (`00-15`) | Implemented in Panels calibration UI | Covered |
| Holo core (`*ON/*OF/*RD/*HP/*HN/*HW/*HD/*ST`) | Strong | Strong (Holos page) | Covered |
| Holo expanded family (`*CO/*RC/*TE/*MO/*MF/*H1/*F1/*CH/...`) | Partial (`*CH/*RC/*TE` + compatibility parser for `*CO`) | Partial (center/test/RC exposed) | Partial coverage |
| Logic/PSI (`@...`) | Strong | Strong (Logics page) | Covered |
| Sound (`$...`) | Handler present (core sound command family) | Partial direct controls in UI | Partial |
| BetterDuino setup suite (`#SP/#SS/...`) | Not implemented (fork uses `#AP...` + calibration `#SO/#SC/#SW`) | Not exposed | Gap by design |
| I2C/EO extras (`&`, `:EO`, `*EO`) | Not implemented as BetterDuino-style handlers | Not exposed | Gap |

---

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

---

## Notes

- This document intentionally focuses on concrete fork behavior/features.
- Local bench-specific diagnostics and personal wiring notes should stay outside this file (for example local `tasks/*` notes) unless they represent a reusable firmware behavior change.
