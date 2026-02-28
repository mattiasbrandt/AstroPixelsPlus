# AstroPixelsPlus Fork Improvements

This file tracks fork-specific behavior and feature changes that differ from upstream usage or are important for builders/operators.

---

## Why This Fork Exists

The original AstroPixelsPlus firmware is designed primarily for **live performance and remote-controlled operation** — driving R2-D2 around conventions with a radio controller, triggering actions via Marcduino serial commands from a body-mounted controller.

**This fork takes a different approach.** My R2 operates mainly as a **static display piece** — positioned at exhibitions, photo opportunities, or in the workshop, where the goal is reliable, repeatable automation rather than real-time driving. This shifts the priorities:

- **WiFi client mode** takes precedence over access-point-only operation. The dome controller joins the local network and stays accessible from any device on that LAN.
- **Web-first control** is the primary interface. Instead of relying on a physical radio controller or serial-command infrastructure, actions are triggered through a proper browser-based UI — sequences, effects, panel choreography, and diagnostics are all one click away.
- **Stability over responsiveness.** Since the droid isn't being driven in real-time, rock-solid uptime, memory safety, and predictable health monitoring matter more than shaving milliseconds off command latency.

This philosophical difference explains the disproportionate effort spent on web-server hardening, async stability, health diagnostics, and the rich web UI — features that may be overkill for a convention-driving setup but are essential for a display-piece that needs to run unattended for hours and be operated by visitors or crew through a phone or tablet.

---

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
Comprehensive review of correctness, robustness, and static-analysis issues completed. All findings addressed and merged to main.

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

## Hardware Gadget Support

Optional hardware add-ons that extend the static display experience beyond the core dome visuals. These gadgets are implemented via ReelTwo library components and are gated by build flags — unused gadgets compile out entirely.

### Overview

| Gadget | ReelTwo class | Wiring (default) | Build flag | Runtime pref key | Primary commands |
|---|---|---|---|---|---|
| BadMotivator (Smoke) | `BadMotivator` | `PIN_AUX5` (GPIO 19) | `AP_ENABLE_BADMOTIVATOR` | `badmot` | `#BMSMOKE`, `#BMSTOP`, `#BMPULSE<sec>`, `#BMAUTO,<min>,<max>` |
| FireStrip (Fire) | `FireStrip` | `PIN_AUX4` (GPIO 18) | `AP_ENABLE_FIRESTRIP` | `firest` | `#FIRESPARK<ms>`, `#FIREBURN<ms>`, `#FIRESTOP` |
| ChargeBayIndicator | `ChargeBayIndicator` | AUX bus `PIN_AUX1/2/3` (GPIO 2/4/5) | `AP_ENABLE_CBI` | `cbienb` | `#CBION`, `#CBIOFF`, `#CBISET<n>` |
| DataPanel | `DataPanel` | AUX bus `PIN_AUX1/2/3` (GPIO 2/4/5) | `AP_ENABLE_DATAPANEL` | `dpenab` | `#DPON`, `#DPOFF`, `#DPSET<n>` |

Notes:
- All four gadget build flags default to `0` (disabled) in `platformio.ini`.
- In addition to the `#...` Marcduino commands, the ReelTwo gadget classes also accept raw `CommandEvent` tokens used by built-in sequences and REST helpers.

---

### BadMotivator (Smoke Generator)

**Purpose**: Controlled smoke/fog bursts for the "bad motivator" prop effect (dome smoke port), intended to create an attention-grabbing moment during static display.

**Hardware**: A smoke/fog element driven through a relay or MOSFET module from a single ESP32 GPIO. Ensure the smoke element power is fused and that the ESP32 shares ground with the smoke power supply.

| Parameter | Value |
|---|---|
| Default GPIO | `PIN_AUX5` (GPIO 19) |
| Pin override | `-DPIN_AUX5=<gpio>` build flag |
| Build flag | `-DAP_ENABLE_BADMOTIVATOR=1` |
| Runtime pref key | `badmot` (bool) |
| ReelTwo source | [`src/dome/BadMotivator.h`](https://github.com/reeltwo/Reeltwo/tree/master/src/dome/BadMotivator.h) |

**Commands** (Marcduino `#` domain; see `docs/COMMANDS.md` for full details):

| Command | Action |
|---|---|
| `#BMSMOKE` | Start smoke (continuous until stopped) |
| `#BMSTOP` | Stop smoke immediately |
| `#BMPULSE<sec>` | Timed smoke pulse (validated range `1–30`) |
| `#BMAUTO,<min>,<max>` | Automatic random smoke bursts (validated range `1–60` seconds) |

**Internal tokens** (raw ReelTwo `CommandEvent` strings used by built-in sequences/REST):

| Token | Used by | Meaning |
|---|---|---|
| `BMON` | `/api/smoke` and sequences | Smoke on |
| `BMOFF` | `/api/smoke` and sequences | Smoke off |

**Safety notes**:
- Prefer `#BMPULSE...` over continuous smoke for automation so the effect ends even if a stop command is lost.
- Provide adequate ventilation and keep the smoke outlet/tubing away from wiring looms and servo linkages.
- Add a fuse on the smoke element power line sized to your module (1A is a common starting point, but match the device spec).

---

### FireStrip (Fire Effects)

**Purpose**: WS2812B fire/spark strip animation to visually complement smoke sequences and add ambient motion during static display.

**Hardware**: WS2812B (NeoPixel-compatible) strip powered from a dedicated 5V supply. Data is a single GPIO. A firmware-side brightness cap is applied on the `#FIREBURN` command path.

| Parameter | Value |
|---|---|
| Default data GPIO | `PIN_AUX4` (GPIO 18) |
| Pin override | `-DPIN_AUX4=<gpio>` build flag |
| Build flag | `-DAP_ENABLE_FIRESTRIP=1` |
| Runtime pref key | `firest` (bool) |
| Brightness cap | `FIRE_MAX_BRIGHTNESS = 200` (firmware wrapper) |
| ReelTwo source | [`src/dome/FireStrip.h`](https://github.com/reeltwo/Reeltwo/tree/master/src/dome/FireStrip.h) |

**Commands** (Marcduino `#` domain):

| Command | Action |
|---|---|
| `#FIRESPARK<ms>` | Spark effect for `1–5000` ms |
| `#FIREBURN<ms>` | Fire/burn effect for `1–10000` ms (brightness capped) |
| `#FIRESTOP` | Stop effect immediately and turn strip off |

**Internal tokens** (raw ReelTwo `CommandEvent` strings used by built-in sequences/REST):

| Token | Meaning |
|---|---|
| `FS1<ms>` | Spark effect for `<ms>` milliseconds |
| `FS2<ms>` | Burn effect for `<ms>` milliseconds |
| `FSOFF` | Off |

---

### ChargeBayIndicator

**Purpose**: Body Charge Bay Indicator animations (MAX7219 LED chain) to keep body electronics-looking visuals alive during long static sessions.

**Hardware**: MAX7219 LED driver chain on the AstroPixels AUX bus (`PIN_AUX1/2/3`). Can share the same chain with the DataPanel.

| Parameter | Value |
|---|---|
| Bus pins | `PIN_AUX1` (GPIO 2, LOAD/CS), `PIN_AUX2` (GPIO 4, CLK), `PIN_AUX3` (GPIO 5, DIN) |
| Build flag | `-DAP_ENABLE_CBI=1` |
| Runtime pref key | `cbienb` (bool) |
| ReelTwo source | [`src/body/ChargeBayIndicator.h`](https://github.com/reeltwo/Reeltwo/tree/master/src/body/ChargeBayIndicator.h) |

**Commands** (Marcduino `#` domain):

| Command | Action |
|---|---|
| `#CBION` | Enable (default pattern) |
| `#CBIOFF` | Disable (all LEDs off) |
| `#CBISET<n>` | Set pattern index (`0–9`) |

---

### DataPanel

**Purpose**: Body Data Panel animations (MAX7219 LED chain) to present continuous "operating" activity.

**Hardware**: MAX7219 LED driver chain, typically on the same AUX bus and chain as the ChargeBayIndicator.

| Parameter | Value |
|---|---|
| Bus pins | Shared AUX bus (`PIN_AUX1/2/3`) |
| Build flag | `-DAP_ENABLE_DATAPANEL=1` |
| Runtime pref key | `dpenab` (bool) |
| ReelTwo source | [`src/body/DataPanel.h`](https://github.com/reeltwo/Reeltwo/tree/master/src/body/DataPanel.h) |

**Commands** (Marcduino `#` domain):

| Command | Action |
|---|---|
| `#DPON` | Enable (default pattern) |
| `#DPOFF` | Disable (all LEDs off) |
| `#DPSET<n>` | Set pattern index (`0–9`) |

---

### Feature Toggle System

Gadgets are controlled at two levels:

**Build-time flags** (`platformio.ini`) — when a flag is `0`, the gadget and its command handlers are compiled out.

```ini
build_flags =
    -DAP_ENABLE_BADMOTIVATOR=0
    -DAP_ENABLE_FIRESTRIP=0
    -DAP_ENABLE_CBI=0
    -DAP_ENABLE_DATAPANEL=0

    ; Optional pin overrides
    -DPIN_AUX4=18
    -DPIN_AUX5=19
```

**Runtime preferences** (ESP32 NVS, namespace `astro`) — when a gadget is compiled in, it can be enabled/disabled without reflashing via the Setup page or `GET/POST /api/pref`:

| Gadget | Preference key | Type |
|---|---|---|
| BadMotivator | `badmot` | bool |
| FireStrip | `firest` | bool |
| ChargeBayIndicator | `cbienb` | bool |
| DataPanel | `dpenab` | bool |

---

### Health Integration Notes

Gadget support and runtime enablement are reported in `/api/health` under a `gadgets` object:

- `present: false` means the gadget was compiled out (build flag `0`).
- `present: true` means the gadget code exists in this build; `enabled` is then read from NVS.
- Gadget health is presence/enablement reporting only (no physical smoke/LED feedback channel).

---

### Why These Features Were Added

This fork operates R2 primarily as a **static display piece**. For live-performance droids, effects are driven by operator timing. For unattended exhibition use, reliable ambient automation and memorable moments matter more than real-time control:

- **BadMotivator smoke** creates an interactive moment without requiring an operator.
- **FireStrip sparks and fire** provide a strong visual complement in low-light exhibition spaces.
- **ChargeBayIndicator and DataPanel** keep the body looking "alive" during long display sessions.

---

### Reference Documentation

| Document | Content |
|---|---|
| `docs/HARDWARE_WIRING.md` | Gadget wiring, pin assignments, AUX bus layout, and power notes |
| `docs/COMMANDS.md` | Full gadget command reference (`#BM*`, `#FIRE*`, `#CBI*`, `#DP*`) with examples |
| `docs/SETUP.md` | Build flag enablement, runtime toggles, and first-time verification steps |

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
