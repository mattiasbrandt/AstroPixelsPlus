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

### Expanded holo command compatibility (phase 1)
- Added compatibility handlers in `MarcduinoHolo.h` for:
  - `*COxxaaabbbcccddd` (compatibility parser retained in firmware)
  - `*CHxx` (center command)
  - `*RCxx` (RC-center compatibility behavior)
  - `*TExx` (mechanical test choreography)
- Added matching operator controls in `data/holos.html` under **Expanded Holo Commands** for center/test/RC operations.

### Holo UX alignment and naming
- Updated holoprojector naming in the web UI to builder-standard identifiers:
  - `HP1 - Holoprojector 1 Front (FHP)`
  - `HP2 - Holoprojector 2 Rear (RHP)`
  - `HP3 - Holoprojector 3 Top (THP)`
- Default holo `*ONxx` behavior now starts in static mode instead of cycle animation.
- Holos page now exposes stable per-projector effect controls (`Static`, `Cycle`, `Pulse`, `Rainbow`) and removes unsupported color-selection controls to avoid broken UX.
- Holos position direction pads are centered for all three projectors to keep control alignment consistent across desktop/mobile layouts.
- Holo servo status card visibility is now conditional: hidden when holo I2C health is green and shown only for warning/error states.

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

## 2026-02 I2C Diagnostics Clarity and Operator UX

- Health model is intentionally split into two layers:
  - **Quick health probes** for expected servo-controller roles (`0x40` panels, `0x41` holos) used by `/api/health` and periodic UI status.
  - **Deep diagnostics scan** for full-bus discovery over `0x01-0x7E` used by `/api/diag/i2c?force=1`.
- Deep diagnostics JSON now reports scan context and operator-facing details used in troubleshooting:
  - `scan_mode` (`quick` or `deep`)
  - `devices` and `device_count`
  - `scan_duration_us` and `scan_age_ms`
  - per-controller codes/streaks and `operator.faults` / `operator.hints`
- Panels and Holos pages now expose explicit operator feedback for deep scans:
  - running state text while deep scan is in progress,
  - completion summary with timestamp, full-bus wording (`0x01-0x7E`), discovered addresses, duration, and faults,
  - failure messaging when diagnostics endpoint is unreachable.
- This wording update is deliberate to avoid implying deep scan is limited to expected addresses only.
- Upstream context considered: `reeltwo/AstroPixelsPlus#10` reports wiring-doc confusion around holo address-bridge guidance; fork diagnostics copy now emphasizes what was actually scanned versus what is expected for role mapping.

## 2026-02 Validation Tooling

### Command compatibility matrix smoke runner
- Added `tools/command_compat_matrix.py` to validate core Marcduino command families over HTTP API.
- Runner sends command matrix entries through `/api/cmd` and checks `/api/state` + `/api/health` after each command.
- Supports grouped runs (`--group panels|holos|logics|sequences|sound`), target host override, optional API token, and dry-run listing.
- Bench validation completed on this fork: matrix runner executes successfully and reports expected PASS behavior.

## 2026-02 Soft Sleep / Wake Runtime Control

- Added runtime soft sleep state tracking in firmware (`sleepMode`, `sleepSinceMs`) while keeping ESP32, WiFi, and async web services online.
- Added new authenticated API endpoints:
  - `POST /api/sleep` to enter quiet low-activity profile
  - `POST /api/wake` to restore active profile
- Sleep entry uses existing Marcduino/ReelTwo command patterns already present in this fork:
  - `:SE10` (quiet reset), `*ST00` (disable holo twitch), `@0T15` (logic lights-out)
- Wake exit restores active baseline using `:SE14` (awake+ profile).
- While in sleep mode, incoming Marcduino commands are gated (blocked) except wake-profile commands to prevent unintended movement/noise.
- Sleep state is exposed through `/api/state` and `/api/health` for UI/API clients.

## 2026-02 Panel Calibration Command Coverage

- Added firmware handlers for panel calibration commands in `MarcduinoPanel.h`:
  - `:MVxxdddd` move target panel/group to a temporary position (no persistence)
  - `#SOxxdddd` store open position
  - `#SCxxdddd` store closed position
  - `#SWxx` swap open/closed positions
- Added compatibility target mapping for varied builds:
  - `00` all panels, `01`-`10` panel groups, `11` pie group, `12` lower dome group,
    `13` top center panel, `14` top-panels alias, `15` bottom-panels alias.
- Added persisted calibration loading at boot in `AstroPixelsPlus.ino` (`loadPersistedPanelCalibration`) so saved open/closed positions survive reboot.
- Calibration values accept either:
  - `0000`-`0180` as degrees (scaled per-servo), or
  - `0544`-`2500` as direct pulse width values.
- Updated Panels calibration UI (`data/panels.html`) to expose extended compatibility targets (13/14/15).
- Added direct servo easing control in Panels calibration UI via `:SFxx$easing` command builder and live preview.
- Panel servo status card visibility is now conditional: hidden when panel I2C health is green and shown only for warning/error states.

## 2026-02 Logic Effects UI Coverage

- Added a new **Custom effect** builder card on Logics page to construct full `@APLE` commands from UI controls instead of relying on shortcut-only coverage.
- Builder supports explicit target selection (both/front/rear), complete effect list, color chips, speed/sensitivity, and duration controls with live command preview.
- Added shared command description parsing in `data/app.js` for `@APLE...` payloads and `:SFxx$easing` commands so tooltips and UI hints remain human-readable.

### Today's linked commits (2026-02-26)

- `4570ee4` — Panels UI now uses human-readable panel naming and completed flutter support for PP5/PP6 (`:OF11`, `:OF12`).
- `4aeda27` — Repo hygiene/docs cleanup: ignore local agent artifacts and trim fork notes in README.
- `b78bb07` — Added soft sleep/wake control flow in firmware + API + Home overlay toggle.
- `c115c52` — Added configurable droid naming across firmware boot text, API state, setup page, and page branding.
- `17c905c` — Added front/rear logic scroll-speed sliders on Logics page.

### 2026-02-26 detail snapshot

- **Sleep/Wake stack**
  - Firmware state machine and timed transition enforcement in `AstroPixelsPlus.ino`.
  - Command gating while sleeping in both serial/web command paths.
  - API endpoints in `AsyncWebInterface.h`: `POST /api/sleep` and `POST /api/wake`.
  - Home UX in `data/index.html` and `data/style.css`: top-right power toggle + dimmed sleep overlay with wake action.
- **Droid naming stack**
  - Build-time default via `AP_DROID_NAME` in `platformio.ini`.
  - Runtime pref key `dname` with validation and max length enforcement in `AsyncWebInterface.h`.
  - Setup page controls in `data/setup.html` and dynamic title/brand binding in `data/app.js`.
- **Logics UX**
  - Added scroll-speed sliders (front and rear) in `data/logics.html` using existing Marcduino speed commands.

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
