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
Added runtime soft sleep state tracking in firmware (`sleepMode`, `sleepSinceMs`) while keeping ESP32, WiFi, and async web services online. Added new API endpoints:
- `POST /api/sleep` to enter quiet low-activity profile
- `POST /api/wake` to restore active profile

Sleep entry uses existing Marcduino/ReelTwo command patterns: `:SE10` (quiet reset), `*ST00` (disable holo twitch), `@0T15` (logic lights-out). Wake exit restores active baseline using `:SE14` (awake+ profile). While in sleep mode, incoming Marcduino commands are gated (blocked) except wake-profile commands.

### Body Controller Link (protoR2link — UART + WiFi fallback)

Bidirectional integration with the [Artoo Controller](https://www.artoo.uk/artooinventions) running the [protoArtoo firmware](https://github.com/mattiasbrandt/protoArtoo). Transport is UART (primary, over slip ring) with automatic WiFi/UDP fallback when the UART heartbeat stales.

**Protocol:**

| Command | Direction | Transport | Purpose |
|---|---|---|---|
| `#APHB\r` | Dome→Body | UART or UDP:4901 | Dome heartbeat (1 Hz) |
| `#PAHB\r` | Body→Dome | UART or UDP:4901 | Body heartbeat (1 Hz) |
| `#APSL\r` | Dome→Body | UART or UDP:4901 | Dome entered sleep |
| `#APWU\r` | Dome→Body | UART or UDP:4901 | Dome exited sleep |
| `#PASL\r` | Body→Dome | UART or UDP:4901 | Body entered sleep |
| `#PAWU\r` | Body→Dome | UART or UDP:4901 | Body exited sleep |
| `:SExx\r`, `$x\r`, `:OPxx\r` etc. | Dome→Body | UART or UDP:4901 | Sequence/sound/panel commands |

Body→dome commands (WiFi path) use HTTP POST to the dome's `/api/cmd` endpoint.

**Preference keys:**
- `mbodylink` (bool, default `true`) — Enable body controller link
- `mbodywifi` (bool, default `true`) — Enable WiFi/UDP fallback transport
- `bodypeerip` (string) — Manual body peer IP override (optional; mDNS preferred)

**Core implementation:**
- `BodyLinkWiFi.h` — UDP socket management, peer discovery (mDNS `protoartoo.local` + received-packet source learning), transport selection, WiFi RX/TX helpers
- `sendBodyCommand()` — routes to UART or UDP based on active transport
- `handleBodySerial()` — manual Serial2 read loop, intercepts heartbeats before ReelTwo dispatch
- `handleBodyLinkHeartbeat()` — sends `#APHB` at 1 Hz on active transport, logs transport transitions
- `bodyLinkConnected()` / `bodyLinkActiveTransport()` — connection state and transport selection helpers
- `bodyLinkResolvePeer()` — mDNS hostname resolution for body peer IP (runs in WiFi event task)

**Bilateral sleep/wake sync:**
- `enterSoftSleepMode()` sends `#APSL` to body on local sleep entry
- `exitSoftSleepMode()` sends `#APWU` to body on local wake
- `MARCDUINO_ACTION(BodySleepSync, #PASL)` — mirrors body sleep to dome locally
- `MARCDUINO_ACTION(BodyWakeSync, #PAWU)` — mirrors body wake to dome locally
- Echo suppression via `fromPeer` flag: peer-initiated transitions do not re-send sync back

**Integration points:**
- 13 sequences (:SE01–:SE15) call `sendBodyCommand()` to synchronize body-side sound and panel actions
- `/api/health` exposes `body_link` object: `enabled`, `connected`, `transport`, `uart_hb_age_ms`, `wifi_hb_age_ms`, `hb_rx`, `peer_ip`, `peer_source`
- Real-time WebSocket state broadcasts include body link status
- `serial.html` shows live badge: `Connected (UART)` / `Connected (WiFi)` / `Waiting` / `Disabled`
- `index.html` health indicator reflects transport in tooltip

**Safety:**
- When body link is enabled, ReelTwo's `marcduinoSerial.setStream()` is explicitly disabled to prevent Serial2 race conditions
- 65-byte UART buffer with overflow logging and null-termination safety
- `#PAWU` whitelisted in sleep gate so a body-initiated wake is never blocked by dome sleep mode

### Dome Layout Contract for Body Editor Integration

Added a dome-owned layout read model for protoArtoo/editor consumers:

- `GET /api/dome/layout` returns the bundled Mr Baddeley Complex Dome MK4 layout composed with live runtime state.
- `GET /api/dome/element-status` returns operator-maintained disabled flags for every known layout element.
- `POST /api/dome/element-status` persists advisory disabled flags and short reasons, e.g. marking `PP3` disabled while an upper pie linkage is binding.

The layout endpoint deliberately reports high-level element identity and geometry only. It does **not** expose Marcduino command strings, servo slots, PCA9685 channels, SPI chains, or other backend implementation details. Those remain owned by firmware command handlers and protoArtoo's coordinator mapping.

The bundled MK4 template is now reviewable JSON under `templates/dome-layouts/` and compiled into firmware through deterministic tooling:

| File | Purpose |
|---|---|
| `templates/dome-layouts/mr-baddeley-complex-dome-mk4.json` | Source-of-truth MK4 layout template, extracted from the existing `data/panels.html` SVG geometry |
| `templates/dome-layouts/schema-v1.json` | JSON Schema for template authors and reviewers |
| `templates/dome-layouts/README.md` | Template contribution policy, validation commands, and review checklist |
| `tools/generate_dome_layout_header.py` | Validates the template and generates the firmware table |
| `tools/check_dome_layout_generated.py` | Drift check for generated output |
| `tools/validate_dome_layout_templates.py` | Validates bundled and future contributed display templates without requiring them to be the firmware-selected MK4 template |
| `GeneratedDomeLayout.h` | Committed generated firmware table used by `/api/dome/layout` |
| `DomeElementStatus.h` | Persistent operator status storage and strict status JSON parser |

Runtime behavior:

- Commandable ring/pie panels expose `active` based on the boot-applied `servoDispatch` state, not pending NVS wiring config.
- Fixed panels, holos, logic displays, and PSI elements are present as first-class layout context but remain non-commandable in this endpoint.
- Operator `disabled` status is advisory. It is surfaced to editors/automation but does not block raw Marcduino commands in v1.
- The Panels page exposes a Dome Layout Status section for marking any layout element disabled with a short reason; disabled commandable panels are highlighted on the SVG and suppressed in individual web UI panel buttons/clicks while raw Marcduino commands remain accepted.
- Status is keyed by generated element index in NVS, with template/schema metadata stored beside it so stale flags are ignored after a layout revision.

### Servo Grind Protection — Per-Mask Post-Close PWM Release

**Problem discovered:** During the 2026-05-21 protoR2 integration test, `#PASL` (body sleep sync) fired mid-panel-wave, the sleep guard blocked `:SE00`/`:OF00`, and `SeqPanelAllClose` fought the running sequence — grinding noise, no recovery without a full power cycle.

Two underlying issues:

1. PCA9685 is a dumb PWM emitter. After a close sequence, the servo holds position indefinitely under active PWM. Any mechanical conflict (misrouted wire, wiring offset, fighting sequence) grinds the motor until power is cut.
2. `shouldBlockCommandDuringSleep()` blocked ALL commands during sleep mode, including emergency stop signals.

#### Part 1 — Stop command sleep bypass

`:SE00` (stop sequence) and `:CL00` (all-close) are now whitelisted in `shouldBlockCommandDuringSleep()`. These are safety signals, not control signals — they must always pass through regardless of sleep state.

#### Part 2 — Post-close timed servo PWM release

After a close sequence completes, the PCA9685 continues driving PWM to hold the servo in position. If the servo is already at its mechanical stop (or fighting a misconnected wire), this holds it under constant load.

Fix: a timer in `mainLoop()` fires after a configurable delay and calls `servoDispatch.setOutput(pin, false)` (writes `LED_FULL_OFF_H` to the PCA9685 register — actually cuts hardware PWM) followed by `servoDispatch.disable(i)`. This bounds the worst-case grind duration to the delay window, after which the servo is de-energized regardless of firmware state.

#### Iteration 1 — Known limitations addressed

Initial implementation used a single global timer with a fixed 1500 ms delay. Two limitations were then identified and fixed:

- **Timer not cancelled on open commands.** If `:OP01` fired after a close, the pending release timer would still cut that servo's PWM mid-hold. Fixed by calling `cancelPanelRelease()` inside every open command handler (`:OP00`–`:OP12`, `:OF00`–`:OF12`).
- **Fixed delay wrong for varspeed sequences.** Dynamic close commands (`:CL$`) accept a speed parameter — a slow close at speed 200 takes far longer than the default 1500 ms. Fixed by scaling the delay: `min(max(args[1] * 10u, 1500u), 30000u)`.

#### Iteration 2 — Upper cap added

Varspeed delay scaling had no upper bound, which could produce arbitrarily long hold times. Added a 30,000 ms cap to prevent a pathological high-speed argument from locking servos energized for minutes.

#### Iteration 3 — Gap fix: open→close sequences never rearmed the timer

ACTION-style sequences that open then close panels (e.g. SE01 ScreamSequence, SE02 WaveSequence) called `cancelPanelRelease()` at start to prevent the previous release from cutting mid-open — but never rearmed a release after the close phase finished. Result: after the sequence completed with panels closed, servos stayed energized indefinitely.

Fixed by adding `schedulePanelRelease(ALL_DOME_PANELS_MASK, N)` after every `SEQUENCE_PLAY_ONCE` call whose sequence ends in a closed position. Delay values chosen from observed sequence durations:

| Sequence type | Delay |
|---|---|
| Standard open-close (SeqPanelAllOpenClose, SeqPanelWave, SeqPanelOpenCloseWave) | 8000 ms |
| Fast wave (SeqPanelWaveFast) | 6000 ms |
| Long open-close (SeqPanelAllOpenCloseLong) | 15000 ms |

Sequences with unknown end state (SeqPanelMarchingAnts, SeqPanelAlternate, SeqPanelDance, SeqPanelLongDisco, SeqPanelLongHarlemShake) receive `cancelPanelRelease()` only — no schedule.

For ANIMATION-style sequences (SE06 ShortSequence, $815 HarlemShake, $720 YodaClearMind), the schedule is issued as `DO_ONCE({ schedulePanelRelease(...); })` immediately after the relevant `DO_SEQUENCE` step.

#### Iteration 4 (final) — Per-mask redesign: correct group isolation

**Problem with the global timer:** `:CL01` (close group 1) with a global timer would eventually cut PWM for ALL groups — including a panel in group 2 that the operator had just opened with `:OP02`. The requirement is that an open panel must stay energized regardless of what close commands fire on other groups.

**Architecture:** Single global timer replaced with per-group mask tracking.

```cpp
static uint32_t sPanelReleaseAtMs   = 0;   // when the timer fires
static uint32_t sPanelReleaseMask   = 0;   // which groups are pending release

static void schedulePanelRelease(uint32_t mask, uint32_t delayMs = 1500);
static void cancelPanelRelease(uint32_t mask = ALL_DOME_PANELS_MASK);
```

Semantics:
- `schedulePanelRelease(mask, delay)` — ORs the mask bits into `sPanelReleaseMask`, then extends the deadline only if the new deadline is later than the current one ("never shorten"). This ensures a slow-close sequence already in progress gets its full time even when another group also closes.
- `cancelPanelRelease(mask)` — clears those bits from `sPanelReleaseMask`. If the mask reaches 0, the timer is cancelled.
- `releasePanelServos()` — iterates all servo channels, cuts PWM only for channels whose group bit is set in `sPanelReleaseMask`. Then clears the mask.

**Result:** `:CL01` schedules a release for `PANEL_GROUP_1` only. `:OP02` cancels `PANEL_GROUP_2`'s pending release only. A panel open in group 2 is never disturbed by group 1's close.

**PWM cutoff implementation:** `servoDispatch.setOutput(pin, false)` writes `LED_FULL_OFF_H` to the PCA9685 output register — this is the correct path for actually cutting hardware PWM, as opposed to `disable(i)` which only updates firmware state. Guarded by `#ifndef USE_I2C_ADDRESS` since `ServoDispatchDirect` does not expose `setOutput()`.

#### Files changed

| File | Changes |
|---|---|
| `AstroPixelsPlus.ino` | `sPanelReleaseAtMs`, `sPanelReleaseMask` globals; `releasePanelServos()`, `schedulePanelRelease()`, `cancelPanelRelease()` implementations; mainLoop timer check; sleep bypass for `:SE00`/`:CL00` |
| `MarcduinoPanel.h` | All `:CL00`–`:CL12`, `:OP00`–`:OP12`, `:OF00`–`:OF12` static handlers updated with per-mask calls; all dynamic `$`-suffix handlers (`:CL$`, `:OP$`, `:OF$`, `:OC$`, `:OCL$`, `:OCR$`, `:OW$`, `:OWF$`, `:OWC$`, `:OMA$`, `:OAP$`, `:OD$`, `:OS$`, `:OF$`) updated |
| `MarcduinoSequence.h` | SE01–SE04, SE10–SE14, SE16, SE51–SE58 updated; SE06, $815, $720 animation gap fixes; SE12 narrowed to PIE_PANEL |

**Build verification:** SUCCESS — RAM 18.6%, Flash 82.9%. Hardware test pending (physical panel rewire required first).

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

### March 2026 Review Patchset (Items 1-8 Follow-up)

Implemented a targeted reliability patchset after a full fork review:

- Fixed sound bank indexing to avoid out-of-bounds array access for bank 9.
- Fixed random-sound upper-bound exclusion so the last candidate sound can be selected.
- Hardened panel calibration value parsing (`#SO/#SC/:MV`) to require exactly four digits.
- Re-enabled gadget preference keys (`badmot`, `firest`, `cbienb`, `dpenab`) in pref-key allowlist.
- Preserved build-flag defaults for gadget preference reads, including `badmot` when `AP_ENABLE_BADMOTIVATOR=1`.
- Stored Serial/Sound numeric preferences as NVS integers (`mserial2`, `msound`, `msoundser`, `mvolume`, `msoundstart`, `mrandom`, `mrandommin`, `mrandommax`) so reboot-time firmware reads use saved UI values.
- Added strict duration input validation (`1-99` seconds, digits-only) for `/api/cbi` and `/api/datapanel`.
- Fixed FadeAndScroll random enum selection to include terminal enum values.

### March 2026 Stability + Calibration Recovery Follow-up

Additional reliability and operator workflow updates after live bench validation:

- Fixed malformed async JSON assembly in health/diagnostic payload paths (removed extra closing braces) to restore stable `/api/health` parsing in the web UI.
- Added consistent boolean preference handling for setup/serial/gadget keys:
    - unified default bool mapping for reads
    - bool write handling for `mserialpass`, `mserial`, `mwifi`, `mwifipass`, `mbodylink`, and gadget toggles.
- Added runtime compatibility guard for body link mode:
    - if body link is enabled while serial ingest is disabled, firmware forces Serial2 active to prevent heartbeat link breakage.
- Added WiFi modem-sleep mitigation (`WiFi.setSleep(false)`) to reduce intermittent multi-second API/UI latency spikes.
- Added panel calibration recovery API and UI:
    - `POST /api/panelcal/reset` clears persisted `soXX/scXX` keys for a selected panel target mask (or aliases), with optional reboot.
    - Panels page includes `Reset Saved Calibration` action in calibration section.
- Confirmed in-field root cause of non-moving panels during this session was invalid persisted panel calibration values; reset workflow restored panel movement.
- Updated command dispatch in web UI to REST-first (`/api/cmd`) with WebSocket fallback for improved consistency on panel commands.
- Serial page performance/clarity updates:
    - reduced duplicate state fetches by reusing a single state request
    - body-link badge now uses lightweight state data path
    - revised labeling (`Serial Communication`) and body-link constraint messaging.

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
- `body-link-uart`
- `body-link-wifi`

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
Added mood presets and demo controls for quiet, mid-awake, full-awake, awake+, top-panel showcase, panel wiggle, and bye-bye wave. Mood changes now adjust the droid's idle personality and body sync without moving the dome panels; panel movement stays under explicit panel and sequence controls. Mood buttons highlight the active mood reported by the dome.

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
OTA is now the preferred post-initial-USB setup path for both firmware and SPIFFS while keeping the project on SPIFFS. The explicit OTA partition table keeps the 4 MB dual-slot layout (`app0`/`app1` at 0x140000 each, SPIFFS at 0x170000). `make ota OTA_IP=<host-or-ip>` uploads firmware with `tools/http_ota_upload.py` to `/upload/firmware`; `make uploadfs OTA_IP=<host-or-ip>` builds SPIFFS and uploads with the same tool to `/upload/filesystem`. Default host is `astropixelsplus.local`; AP fallback is `OTA_IP=192.168.4.1`. Firmware and SPIFFS uploads remain separate.

Browser upload endpoints now support both `/upload/firmware` and `/upload/filesystem` with JSON success/error responses. Firmware upload uses `Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)` to avoid multipart content-length rollback risk; filesystem upload uses `U_SPIFFS`. The web UI surfaces returned JSON `error` text.

The Makefile intentionally does not use PlatformIO `espota`/ArduinoOTA port 3232; deployed controllers accept OTA through the async web upload endpoints.

### Holo UX Alignment and Naming
Updated holoprojector naming in the web UI to builder-standard identifiers:
- `HP1 - Holoprojector 1 Front (FHP)`
- `HP2 - Holoprojector 2 Rear (RHP)`
- `HP3 - Holoprojector 3 Top (THP)`

Default holo `*ONxx` behavior now starts in static mode instead of cycle animation. Holos page now exposes stable per-projector effect controls (`Static`, `Cycle`, `Pulse`, `Rainbow`) and removes unsupported color-selection controls to avoid broken UX.

Holos position direction pads are centered for all three projectors to keep control alignment consistent across desktop/mobile layouts.

Holo servo status card visibility is now conditional: hidden when holo I2C health is green and shown only for warning/error states.

### Panel Calibration UI
Redesigned Panels calibration section (`data/panels.html`) for improved usability and clarity:

**Position Control:**
- Replaced numerical text input with an intuitive slider (800–2200 μs range, matching MG90S servo limits)
- Real-time microsecond value display updates as you drag
- Visual range labels (800 μs / 1500 μs / 2200 μs) provide context

**Easing Selection:**
- Fixed dropdown labels to match actual easing implementation (was incorrectly showing non-existent options like "Bounce", "Elastic")
- Each option now clearly describes its behavior (e.g., "3 — Ease In/Out Quad ⭐ (most natural)")
- Added educational text explaining what easing is and how to choose the right type
- Highlights recommendation: Ease In/Out Quad (#3) for most panel movements

**Calibration Guidance:**
- Added MG90S servo range explanation with safety notes (800–2200 μs, center at 1500 μs)
- Step-by-step calibration instructions acknowledging manufacturing variance between servos
- Practical tips: servo buzzing indicates mechanical stress—move away from extremes
- Suggested starting values: ~900 μs (closed), ~2100 μs (open)
Updated Panels calibration UI (`data/panels.html`) to expose extended compatibility targets (13/14/15). Added direct servo easing control via `:SFxx$easing` command builder and live preview.

Panel servo status card visibility is now conditional: hidden when panel I2C health is green and shown only for warning/error states.

### Panel Calibration Command Fix (dangling pointer)

Fixed a silent failure in `:MV`, `#SO`, `#SC`, `#SW` where the servo never moved despite the command being accepted.

**Root cause:** `MARCDUINO_ACTION` wraps the handler body in `DO_ONCE()` inside `animateOnce()`, deferring execution to the next `AnimatedEvent::process()` loop iteration. The incoming `cmd` string is a temporary buffer that is freed before that iteration runs. `getCommand()` inside the deferred body returned a dangling pointer, causing all argument parsing to silently fail.

**Fix:** `:MV`, `#SO`, `#SC`, `#SW` are now intercepted and executed synchronously in `processMarcduinoCommandWithSource()` (`AsyncWebInterface.h`) while `cmd` is still valid on the stack. The `MARCDUINO_ACTION` stubs in `MarcduinoPanel.h` are kept as empty registrations so the Marcduino command registry still recognises the prefixes.

**Limitation:** Commands sent over physical Serial2 (Marcduino serial path) still go through `Marcduino::processCommand()` directly and will hit the empty stubs — calibration is a web UI workflow and is not expected to be driven over serial.

Follow-up: raw `:SM` servo move commands hit the same deferred `getCommand()` trap when arriving through queued web/body/serial ingress. Dome-owned sequences worked because they call `servoDispatch.moveToPulse(...)` directly and never parse a Marcduino suffix. `:SM` is now intercepted synchronously for all ingress paths before queueing, logs `[SM-exec]` with the parsed slot/timing/pulse, and cancels pending panel-release state for the target slot group before moving.

### Panel Naming Clarity & Dome Diagram (MK4 Remap)

Resolved systematic mislabeling and incorrect servo-to-panel mapping for the **Mr. Baddeley MK4 Complex Dome** — the dominant dome design in 2026 builds.

**Root problem:** The firmware's `servoSettings[]` and web UI were based on an older ReelTwo numbering scheme that did not match actual MK4 hardware. Three naming systems existed with no documented cross-reference, causing wiring confusion.

**Three naming systems now documented and anchored to printed-droid labels:**

| System | Scope |
|--------|-------|
| Printed-Droid / Mr. Baddeley | P1–P14 (all panels incl. fixed), PP1–PP6 (pie) |
| PCA9685 channel | ch1–ch13 physical wiring positions |
| Marcduino firmware | `:OP01`–`:OP13`, maps 1:1 to `servoSettings[]` slots |

**MK4 servo panels (printed-droid labels):**
- Ring (lower): P1, P2, P3, P4, P7, P11, P13
- Pie (upper/inner): PP1, PP2, PP4, PP6
- Fixed (no servo): P5 (Magic Panel frame), P6, P8 (Rear PSI), P9 (Rear Logic), P10, P12 (FLDs), P14 (Front PSI), center top

**`servoSettings[]` remapped (`AstroPixelsPlus.ino`):**

This table was updated by the Panel Command Numbering Fix (2026-05-27) — see that section below for the full migration story. Current state:

| PCA9685 ch | Panel (printed-droid) | Marcduino | Address bit |
|---|---|---|---|
| 1 | P1 | `:OP01` | `PANEL_P1` |
| 2 | P2 | `:OP02` | `PANEL_P2` |
| 3 | P3 | `:OP03` | `PANEL_P3` |
| 4 | P4 | `:OP04` | `PANEL_P4` |
| 5 | P7 | `:OP07` | `PANEL_P7` |
| 6 | P11 | `:OP11` | `PANEL_P11` |
| 7 | P13 | `:OP13` | `PANEL_P13` |
| 8 | PP5 | `:OPP5` | `PANEL_PP5` (unserviced on standard MK4) |
| 9 | PP1 | `:OPP1` (alias `:OP08`) | `PANEL_PP1` |
| 10 | PP2 | `:OPP2` (alias `:OP09`) | `PANEL_PP2` |
| 11 | PP4 | `:OPP4` (alias `:OP10`) | `PANEL_PP4` |
| 12 | PP6 | `:OPP6` (alias `:OP12`) | `PANEL_PP6` |
| 13 | PP3 | `:OPP3` | `TOP_PIE_PANEL` (unserviced on standard MK4) |

Group shortcuts (MarcDuino V3): `:OP14` / `:CL14` / `:OF14` = all pie panels; `:OP15` / `:CL15` / `:OF15` = all ring panels.

**Changes applied:**

- `AstroPixelsPlus.ino` — `servoSettings[]` fully remapped to MK4 wiring with per-slot comments (printed-droid label, PCA9685 channel, Marcduino command, panel type).
- `DomeSequences.h` — `#define DP5`–`DPP4` updated to match new slot positions.
- `docs/HARDWARE_WIRING.md` — Panel naming section replaced with clean MK4 cross-reference table. Fixed/servo classification matches user's physical build.
- `data/panels.html` — Tables (ring panels, pie panels), calibration dropdown, and SVG element IDs/onclick handlers all updated to match corrected MK4 mapping. SVG colors: blue ring panels, blue-tinted pie panels, orange open-state, silver fixed panels and dome gaps.
- `data/panels.html` — Interactive **Dome Panel Map** SVG added: top-down dome schematic. Click any servo panel → sends `:OP`; click again → sends `:CL`. Fixed panels shown in grey with no click action. Centre top marked as fixed (not clickable).
- `data/holos.html` — Added **Holo Position Map** SVG: top-down dome view with HP1/HP2/HP3. Click → on (`*ON0x`); click again → off (`*OF0x`). SVG description trimmed to one line.

**SVG geometry (as implemented):**

- CX=CY=240, R_out=172, R_ring_in=146, R_pie_out=118, R_pie_in=54
- Ring band: r=146–172. Pie band: r=54–118. Silver gap zone r=118–146 represents the visible dome skin between pie and ring sections.
- Ring panel angular positions follow the MK4 reference: RPSI 3°–25°, P7 32°–52°, P6 55°–68°, MP 72°–92°, P4 96°–114°, silver gap 114°–122°, P3 122°–136°, P2 140°–153°, P1 157°–171°, HP1 gap 171°–185°, P13 185°–198°, FPSI 202°–215°, FLD 219°–248°, P11 252°–272°, silver gap 272°–280°, P10 280°–298°, RLD 302°–323°, HP2 gap 323°–3°.
- Pie panels (55° wide, 5° gaps): PP3 fixed 332°–27° (straddles top), PP4 servo 32°–87°, PP2 servo 92°–147° (covers right ring gap 114°–122°), PP1 servo 152°–207°, PP6 servo 212°–267°, PP5 fixed 272°–327° (covers left ring gap 272°–280°).
- Silver gaps appear only in the outer ring band — not in the pie area — because the pie panels are strategically positioned to cover the ring gap angles.

### Dynamic Servo Wiring Config — Point-and-Test Channel Assignment

**The builder pain this fixes:** you wire panel and holo servos to the PCA9685 boards, fire `:OP01`, and... the wrong panel moves. Or nothing moves. The only way to debug it physically is to lift the dome off, disconnect the slip-ring, work back through your carefully zip-tied servo bundles, and trace each suspect cable from the panel hinge all the way down to its PCA9685 header — then re-route, re-tie, re-seat, and try again. Every PCA9685-based R2 build hits this. The fork has shipped *two* separate "wrong by one channel" bugs to date — once on panels, once on holos — both because the channel numbers printed on the boards (0–15) and the channel numbers the firmware historically used in its source didn't line up.

**The fix:** a new **Servo Wiring Config** section on the Panels page and a matching **Holo Wiring Config** section on the Holos page. Both are collapsed by default — open them once at build/rewire time, leave them alone afterwards.

#### What the operator sees

A table, one row per servo slot:

| Panel | Active | Channel | Command | Test |
|---|---|---|---|---|
| P1 | ☑ | `1 ▾` | `:OP01` | ▶ Test |
| P11 | ☑ | `6 ▾` | `:OP06` | ▶ Test |
| PP5 (no servo on standard MK4) | ☐ | — | — | — |
| … | | | | |

- **Channel column** = the silkscreen number printed on the PCA9685 board (0–15). What the builder reads when they look at the board. The firmware's internal channel counting is no longer surfaced anywhere — it's hidden.
- **Active checkbox** — uncheck for any slot where no servo is actually wired. Inactive slots are silently skipped by every panel/holo command — no crash, no error, no broken sequence.
- **Test button** — pulses that specific physical channel:
  - For **panels**: opens the servo and *holds it open* until the operator clicks Close. Walk to the dome, see which panel moved, walk back, click Close.
  - For **holos**: starts a sweep — one extreme, 1 s pause, other extreme, 1 s pause, repeat — until the operator clicks Stop. Lets the builder see which projector axis is being driven and in which direction.
  - Only one test runs per board at a time. Clicking Test on a second row automatically stops the first.

#### What the operator gets behaviourally

- **Out of the box (no saved config)** — the dome works without touching the new sections if your physical wiring matches the firmware's shipped defaults: ring panels on silkscreen CH0–CH6 (P1–P4, P7, P11, P13), pie panels on CH8–CH11 (PP1, PP2, PP4, PP6), holo axes on the holo board's CH0–CH5. The Mr Baddeley MK4 Complex Dome standard defines which panels exist and which have servo mounts (he also makes a Simple Dome with few or no moving panels) — but it does NOT define controller choice or channel wiring, so these defaults are a firmware convention, not a community one.
- **Wired differently than the defaults** — set the per-slot mapping entirely from the web UI: change the channel dropdowns to match your physical wiring, click Save, reboot. Every Marcduino command (`:OP01`–`:OP11`, `:OP00`, `:CL00`, wave/flutter/marching-ants sequences, the panel SVG diagram clicks) routes through whatever mapping you save.
- **P11 not wired yet?** — uncheck Active for P11, save, reboot. `:OP06` and group commands skip it cleanly. No grinding, no errors. Wire it later, re-check, save, reboot.
- **Two slots assigned the same channel** — both rows turn amber, the Save button greys out, with a message naming the conflicting channel. Even if the UI is bypassed entirely, the dome refuses to save a conflicting config (returns an error and leaves the saved settings untouched).
- **Wiring stuck open?** — reboot. Test pulses don't survive a power cycle. The dome comes back with whatever the saved config says.
- **Saved config survives reboot** — config lives in the dome's persistent storage, separate from anything else (no risk of accidentally clobbering it via factory reset of other prefs).

#### What this also fixed in passing

- **Holo board off-by-one (real chip-boundary bug).** The holo defaults used to silently address the panel board's last channel instead of the holo board's first channel — the firmware-pin → board math meant that pin 16 stayed on the panel board (`0x40` CH15) instead of crossing to the holo board (`0x41` CH0). The original pins `{16,17,…,21}` were one short across all six holo axes. Corrected to `{17,…,22}` so the front/top/rear horizontal+vertical axes route to `0x41` CH0–CH5 as the chip-boundary math actually requires. See [ADR 0004](docs/adr/0004-holo-servosettings-starts-at-pin-17.md) for the math.
- **Docs vs firmware alignment.** The panel section of `docs/HARDWARE_WIRING.md` previously claimed `CH1 = P1` (a 1-indexed silkscreen layout), while the firmware actually drove `CH0` for P1. Rewritten so the channel tables match what the firmware ships with (`CH0 = P1`) and with an explicit callout that the table is just the default — not a wiring standard. Builders whose physical wiring differs no longer have to reverse-engineer the firmware to figure out what `:OP01` will drive; they look at the Wiring Config UI.

#### What this does NOT change

- **Which command drives which slot.** `:OP01` still always drives panel slot 0, `:OP02` slot 1, and so on. The operator picks which **physical channel** each slot reaches — not which command triggers which panel. Remapping the commands themselves is a much larger change deferred for now.
- **Sequence timing.** Wave / flutter / marching-ants choreography uses fixed per-step delays. If a slot in the middle of a sequence is marked inactive, that step is silently skipped but the timing still ticks through — so the wave gets a brief pause where the missing panel would have been. Cosmetic, not breaking.

#### Files changed

| File | Changes |
|---|---|
| `AstroPixelsPlus.ino` | Corrected default channel mappings for both PCA9685 boards. Added boot-time loaders that apply the saved wiring config before the first hardware update, plus a state machine in the main loop driving the holo sweep test. New operator-facing warnings if a slot is enabled but has no working command routing. |
| `AsyncWebInterface.h` | Four new endpoints: read/save panel config, read/save holo config, raw channel test, stop test. Server-side validation rejects bad payloads before saving. Auto-stops any previous test before starting a new one. All test/save/reject events log to the web log viewer. |
| `data/panels.html` | New collapsed *Servo Wiring Config* section under the existing calibration card. Conflict highlighting, test buttons, save flow with reboot prompt. Collapse state is remembered per browser. |
| `data/holos.html` | New collapsed *Holo Wiring Config* section grouped by projector (FHP / RHP / THP) with horizontal/vertical axis rows and sweep-direction indicators. |
| `docs/HARDWARE_WIRING.md` | Servo channel mapping tables rewritten in silkscreen-first numbering. Holo table corrected. Callout explaining the (now-hidden) internal numbering convention. |
| `docs/adr/0001`…`0004` | Design records for: UI uses silkscreen channel numbers; the runtime hook used to apply saved config; why command-to-slot routing stays fixed for now; why the holo board starts at firmware pin 17, not 16. Background reading for anyone modifying this code later. |

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

## Ported from thePunderWoman Fork

Feature additions ported from [Jessica Janiuk's AstroPixelsPlus fork](https://github.com/thePunderWoman/AstroPixelsPlus).
All ported files carry attribution headers linking back to her repo.

Hardware-specific aspects of her fork (serial baud, servo pulse ranges, holo channel offsets) were intentionally left behind — this fork's values take precedence and are not overridden.

---

### Extended Holo Effects (`FlthyHoloExtras.h`)

New file adding 7 command groups to the holo projector system, inspired by the FlthyHPs sketch by Ryan Sondgeroth and ported verbatim (with adapted attribution header). All commands use `CommandEvent::process()` internally and were confirmed conflict-free with existing `MarcduinoHolo.h` prefixes.

| Command | Scope | Effect |
|---|---|---|
| `*SC00`–`*SC03` | All / Front / Rear / Top | Short circuit orange flicker (`HPx0077`) |
| `*CY00` | All | Cycle — spinning LED, random color (`HPA0040`) |
| `*PL00` | All | Dim Pulse — slow breathe, random color (`HPA0030`) |
| `*SB00`–`*SB03` | All / Front / Rear / Top | Solid Blue (`HPx0055`) |
| `*SW00`–`*SW03` | All / Front / Rear / Top | Solid White (`HPx0059`) |
| `*ML00` | All | Mode Loop — autonomous random cycle (`HPS9`) |
| `*RL00` | All | Reset to Loop — resets to random loop, not off |

**Integration:** included in `AstroPixelsPlus.ino` immediately after `MarcduinoHolo.h`.

**Web UI:** new "Extended Holo Effects" card added to `data/holos.html` with button groups for Short Circuit, Solid Blue, Solid White, Mode controls, and the all-holo effect shortcuts.

**`describeCmd()` coverage:** all `*SC`/`*CY`/`*PL`/`*SB`/`*SW`/`*ML`/`*RL` prefixes return human-readable labels in `data/app.js`.

---

### Dome Choreography Sequences (`DomeSequences.h`)

New file adding 17 named dome sequences plus 22 aliases — all reachable via the `DM:` Marcduino prefix (e.g. `DM:SCREAM\r`). Sequences use eased servo motion, coordinated holos/logics, PSI effects, and protoR2link body cues.

#### Adapted from source
Panel defines were remapped from her `P1–P13/PP1–PP6` indices (tuned to her dome wiring) to this fork's `DP1–DPB/DPP1–DPP4` names matching the `servoSettings[]` array positions here. Servo pulse range constants updated to match this fork's defaults (`800–2200 µs`). Per-panel calibration continues to live in NVS via the web UI — not in code.

#### Body link integration
All body-facing calls go through `sendBodyCommand()` (the protoR2link transport function), supporting both UART-primary and WiFi-fallback to protoArtoo. Direct `COMMAND_SERIAL.print()` calls from her fork were replaced throughout.

Two new protocol messages are sent over the link:
- `dome=seqon,N` — notifies protoArtoo a dome sequence is starting for `N` seconds (pauses autonomous body sounds/motion)
- `dome=seqoff` — notifies protoArtoo the sequence has ended (resumes autonomous behavior)
- `BD:<CUE>` — named body cue (e.g. `BD:SCREAM`, `BD:LEIA`) requesting sound/animation on the body side

#### PSI effects
Her Teeces `4Tx|N` serial calls (body-PSI-addressed) were replaced with direct duration-aware PSI API calls on this fork's dome-local `frontPSI`/`rearPSI` objects:

| Sequence | PSI call |
|---|---|
| Reset | `selectSequence(NORMAL)` — permanent |
| Alarm (10 s) | `selectSequence(ALARM, kDefault, 0, 10)` |
| Overload (12 s) | `selectSequence(FAILURE, kDefault, 0, 12)` |
| Scream (15 s) | `selectSequence(REDALERT, kDefault, 0, 15)` |
| Leia (36 s) | `selectSequence(LEIA, kDefault, 0, 36)` |
| Heart (10 s, front only) | `selectSequence(FLASHCOLOR, kDefault, 0, 10)` |
| Vader / Rock March (47 s) | `selectSequence(MARCH, kDefault, 0, 47)` |
| Cantina (15 s) | `selectSequence(FLASHCOLOR, kDefault, 0, 15)` |

#### Happy sound gate
`DM:PIES`, `DM:LOW`, and `DM:OPENALL` call `BD:HAPPY` on each toggle (random happy bank sound on the body). Gated by the `dm_happy_sound` NVS preference (default `true`) so it can be disabled for quieter operation without reflashing.

#### Non-blocking execution model (this fork)
The ported sequences were originally written in a **blocking** style (a `domeMove(..., wait=true)` busy-loop that spun on `servoDispatch.isActive()` while internally calling `AnimatedEvent::process()`, plus a `domeWaitTime()` busy-wait). On this ESP32 build that ran on the loop core (`mainLoop`) and could **stall core 1 until reboot** — the async web server (core 0) kept answering, so a command returned `ok` but no panels moved and subsequent commands were never drained.

They are now rewritten as **non-blocking ReelTwo `DO_*` animation scripts** that run on the shared `player` and are advanced one step per `AnimatedEvent::process()` — the same pattern as `MarcduinoSequence.h` (`:SE*`):

- Wave choreography uses **staggered `moveToPulse(idx, startDelay, moveTime, pos)`** start-delays (faithful to the old serialized timing) plus a single `DO_WAIT_MILLIS` for the envelope — no busy-loop.
- Timed holds use `DO_WAIT_MILLIS` / `DO_WAIT_SEC`; servo PWM cutoff uses the existing `schedulePanelRelease(mask, delayMs)` instead of `disable()` loops.
- Toggle sequences (`DM:LOW`, `DM:PIES`, `DM:OPENALL`) are an open/close animation pair; the handler picks one by toggle state.
- Loop/random sequences (Scream, Overload, Cantina, RockMarch, Bloom) loop via a backward `DO_WHILE` on a millis-deadline or iteration counter, with random selections held in file-scope statics.
- Delegating sequences (`DM:DISCO`, `DM:RANDOM`) **queue** their target `:SE`/`$` command via `enqueueMarcduinoCommand()` (drained on the main loop) rather than calling `processCommand()` re-entrantly from inside `player.animate()`.

Dispatch path: a `DM:*` handler sets `dome_pendingAnim` (an `AnimationStep`); `mainLoop()` drains it once via `player.animateOnce()` **outside** `player.animate()`, avoiding re-entrant `animateOnce()`. The old blocking helpers (`domeMove`, `domeWaitTime`) and the `dome_pendingSeq` function-pointer path were removed. `DM:LOW` stays a `MARCDUINO_ANIMATION` (not `MARCDUINO_ACTION`) so the bare token `LOW` is not macro-expanded to `0x0` in the registered command string.

Consequence: a direct panel command (`:OP01`) now **preempts** a running dome sequence (they share `player`) instead of being blocked behind it. `DM:LOW` open/close + a following `:OP01`/`:CL01` is hardware-verified to work without a reboot.

#### Command logging
All command ingress paths now write through `logCapture`, not raw `Serial.printf`, so the web console and `/api/logs` show commands received over USB serial, WiFi Marcduino, body-link UART/WiFi, WebSocket, and REST. The main-loop queue drain also logs `[CMD][source][dispatch] ...` immediately before `Marcduino::processCommand(...)`, making it visible when an accepted command actually reaches the Marcduino action layer.

#### Command reference

| Command | Effect |
|---|---|
| `DM:PIES` | Toggle pie panels open/close |
| `DM:LOW` | Toggle lower panels open/close |
| `DM:OPENALL` | Toggle all panels open/close |
| `DM:FLUTTER` | Flutter all panels to 75% then snap closed |
| `DM:BLOOM` | Pie panels ease open, wiggle, then close |
| `DM:SCREAM` | All panels burst open, red alert logics + holos, random flutter |
| `DM:CANTINA` | Alternating panel dance at 130 BPM, 15 s |
| `DM:LEIA` | Front holo Leia effect, logic Leia sequence, 36 s |
| `DM:VADER` | Imperial March — red logics/holos/PSI, 47 s |
| `DM:ROCKMARCH` | Imperial March alternate, 47 s |
| `DM:DISCO` | Triggers `:SE09` (disco sound is owned by SE09) |
| `DM:ALARM` | Pulsing red holos and logics, 10 s |
| `DM:HELLO` | Panel wave + logic scroll greeting |
| `DM:HEART` | Rainbow holos + sweet logic message, 10 s |
| `DM:OVERLOAD` | Failure logics + random panels sluggishly drift |
| `DM:RANDOM` | Pick one of 16 standard sequences at random |
| `DM:RESET` | Close all panels, reset holos to mode loop, logics to normal |

Aliases forward to fork-specific `:SE` sequences:

| Alias | Forwards to |
|---|---|
| `DM:TOPPANELS` | `:SE12` |
| `DM:WIGGLE` | `:SE16` |
| `DM:BYEBYE` | `:SE58` |

And to upstream sequences: `DM:WAVE→:SE02`, `DM:SESCREAM→:SE01`, `DM:SMIRK→:SE03`, `DM:OPENCLOSWAVE→:SE04`, `DM:BEEPCANTINA→:SE05`, `DM:SHORTCIRCUIT→:SE06`, `DM:CANTINA→:SE07`, etc.

**Re-entrancy:** a `dome_seqRunning` guard drops any new `DM:` call that arrives while a sequence is active.

**Integration:** included in `AstroPixelsPlus.ino` after `BodyLinkWiFi.h` (so `sendBodyCommand()` is in scope).

**Web UI:** new "Dome Sequences (DM: prefix)" card added at the top of `data/sequences.html` with sub-groups: Panels, Full Shows, Character, Reset, and a `dm_happy_sound` preference toggle.

**`describeCmd()` coverage:** all 17 named `DM:` commands return human-readable labels in `data/app.js`.

---

### Auto-Start Holo Random Mode on Boot

At the end of `setup()`, fires `HPS9` to put all three holo projectors into the autonomous random LED mode loop immediately on power-up. Gated by the `holo_boot_loop` NVS preference (default `true`).

**Runtime preference:** `holo_boot_loop` (bool, default `true`). Exposed on the Setup page under "Dome Behaviour" using the standard gadget preference toggle pattern. When disabled, holos stay in their last state on boot.

**NVS keys added in this port:**

| Key | Type | Default | Purpose |
|---|---|---|---|
| `holo_boot_loop` | bool | `true` | Fire `HPS9` at end of `setup()` to auto-start holo random mode |
| `dm_happy_sound` | bool | `true` | Play `BD:HAPPY` body sound on panel toggle sequences |

Both keys are added to the `/api/pref` allowlist and `defaultBoolForPrefKey()` in `AsyncWebInterface.h`.

---

## Build System & Tooling

Changes to build configuration, development workflow, and validation tools.

### PlatformIO Configuration
- Default env: `astropixelsplus`
- Partition table: `partitions/partitions_ota_spiffs.csv`
- Filesystem: SPIFFS
- `ESP32_ARDUINO_NO_RGB_BUILTIN` defined to prevent FastLED RMT conflicts
- Optimized for size (`-Os`)
- Platform: `espressif32@5.2.0`

### Makefile Documentation
Replaced the old Arduino.mk-relative workflow with PlatformIO/HTTP targets: `make build`, `make buildfs`, `make gate`, `make ota`, and `make uploadfs`. `BUILD_ENV`, `OTA_IP`, `FIRMWARE_BIN`, and `SPIFFS_BIN` are overridable, and user-specific overrides can live in gitignored `user.mk`.

### Verification Labels
Use `software-verified`, `controller-upload-verified`, `full-hardware-verified`, `partial`, and `full-hardware-required` when recording implementation validation status.

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

## Panel Command Numbering Fix (2026-05-27)

### Problem

During hardware testing, individual ring-panel commands (`:OP05`–`:OP07`) drove the wrong physical panels. `:OP05` moved P7, `:OP06` moved P11, and `:OP07` moved P13 — one slot off in each case — because the firmware mapped `:OPnn` to `servoSettings[]` **slot index n-1** rather than to **physical panel number n**. In addition, `:OP11` and `:OP12` were repurposed as group shortcuts (all pie / all dome), conflicting with MarcDuino V3's individual-panel numbering convention.

### Fix

The fix aligns this firmware with the MarcDuino V3 13-panel standard:

- `:OP01`–`:OP04` / `:CL01`–`:CL04` / `:OF01`–`:OF04` — ring panels P1–P4 (unchanged)
- `:OP07` / `:CL07` / `:OF07` — ring panel P7 (was `:OP05`)
- `:OP11` / `:CL11` / `:OF11` — ring panel P11 (was repurposed as "open all pie")
- `:OP13` / `:CL13` / `:OF13` — ring panel P13 (was missing entirely)
- `:OP08` / `:OP09` / `:OP10` / `:OP12` — pie panels PP1 / PP2 / PP4 / PP6 (community-standard aliases, unchanged)

### Operator migration for saved macros

If you have saved macros that relied on the old group shortcuts, update them:

| Old command | Old behavior | New command for same effect |
|-------------|-------------|----------------------------|
| `:OP11` | Open all pie panels | `:OP14` |
| `:CL11` | Close all pie panels | `:CL14` |
| `:OP12` | Open all dome panels | `:OP15` or `:OP00` |
| `:CL12` | Close all dome panels | `:CL15` or `:CL00` |
| `:OP05` | Moved P7 (wrong label) | `:OP07` (correct) |
| `:OP06` | Moved P11 (wrong label) | `:OP11` (correct) |
| `:OP07` | Moved P13 (wrong label) | `:OP13` (correct) |

`:OP14` / `:CL14` = **top panel group** (all pie panels — MarcDuino V3 style).  
`:OP15` / `:CL15` = **bottom panel group** (all ring panels — MarcDuino V3 style).

### Fork-specific pie panel namespace (`:OPP*` / `:CLP*` / `:OFP*`)

This fork adds a readable alternative command namespace for pie panels that does not require knowing the community-alias numbers:

| Panel | Community alias | Fork alias |
|-------|----------------|-----------|
| PP1   | `:OP08`        | `:OPP1`   |
| PP2   | `:OP09`        | `:OPP2`   |
| PP3   | *(no-op on standard MK4)* | `:OPP3` |
| PP4   | `:OP10`        | `:OPP4`   |
| PP5   | *(no-op on standard MK4)* | `:OPP5` |
| PP6   | `:OP12`        | `:OPP6`   |

Both addressing schemes target the same servo. The fork aliases are listed in the web UI pie panel table alongside the community aliases.

### PP5 and PP3 — unserviced slots

PP5 (upper-left wedge) and PP3 (upper-right wedge, hosts the Top Holo Projector) have no servo on a standard MK4 build. Their command slots exist in firmware with zero channel numbers. Any `:OPP3` / `:OPP5` command is accepted and silently skipped (`_moveServoToPulse` short-circuits on channel=0). If a builder wires a servo to those slots via the wiring config page, the commands become live with no firmware change.

PP5 was previously classified as `MINI_PANEL` (a body-panel type). It has been reclassified to `PIE_PANEL` so it participates correctly in group mask operations that target the pie wedge area.

### Dome sequence updates

All 17 `DM:*` dome sequences (`domeOpenClosePies`, `domeOpenCloseLow`, `domeOpenCloseAll`, `domeFlutter`, `domeBloom`, `domeScream`, `domeResetAll`, `domeCantina`, `domeRockMarch`, and others) were audited and updated to use identity-based slot constants (`D_P1`–`D_P13`, `D_PP1`–`D_PP6`) and to include PP3 and PP5 in sequences where the entire pie ring participates. On a standard MK4, PP3/PP5 slots are inactive no-ops; on a fully-wired dome, all 6 pie wedges will animate.

Sequences that are pure logic/holo/sound with no panel movement (`domeHeart`, `domeAlarm`, `domeDisco`, `domeVader`, `domeLeiaMode`) were not changed.

---

## Boot Reset Telemetry (2026-06-17)

The dome now captures `esp_reset_reason()` at the start of `setup()` and exposes it in both operator-visible telemetry paths:

- `/api/health` includes `reset_reason`, `reset_reason_code`, and `coredump_present`.
- The web/serial log ring records `[Boot] reset_reason=...` before heavier hardware initialization.

This is intended for hardware fault isolation after a reboot, especially distinguishing brownout/load resets from watchdog or panic resets during panel and holo choreography tests. `coredump_present` is reported with a weak optional coredump check; it is `false` when the firmware/platform does not link ESP-IDF coredump support.

---

## Dome Visual Preset Command (`DV:`) (2026-06-18)

The dome now accepts visual preset commands for body-owned sequences:

```text
DV:<NAME>
```

This gives the body controller a simple way to ask the dome for the same named light show that a dome-native sequence would use, without handing over panel or audio control. For example, `DV:ROCKMARCH` applies the red MARCH logic/PSI/holo look used by the dome's native Imperial March sequences.

Operator-facing behavior:

- Affects only dome visuals: front/rear logics, PSI, and holo LEDs.
- Does not move panels.
- Does not play sounds.
- Does not start a full `DM:*` dome sequence.
- Unknown preset names are logged and safely ignored.

Initial presets:

| Command | Visual preset source |
|---|---|
| `DV:ROCKMARCH` | Native Rock March / Vader red MARCH visuals |
| `DV:VADER` | Native Vader red MARCH visuals |
| `DV:ALARM` | Native Alarm visuals |
| `DV:LEIA` | Native Leia visuals |
| `DV:HEART` | Native Heart visuals |
| `DV:CANTINA` | Native Cantina visuals |
| `DV:SCREAM` | Native Scream visual-only portion |
| `DV:OVERLOAD` | Native Overload visual-only portion |
| `DV:HELLO` | Native Hello visual-only portion |
| `DV:RESET_VISUALS` | Reset holos/logics/PSI to normal/default loop |

`/api/health` now reports the last/current visual preset and command queue pressure. This helps sequence testing confirm what the dome applied without making the operator visually compare every detail by memory.

### Structured Visual Authoring Commands (`DL:` / `DT:` / `DH:`) (2026-06-21)

Added typed visual commands for the body sequence editor so operators can create custom light, text, and holo steps without typing raw Marcduino command strings:

- `DL:` sets logic/PSI modes with target, mode, color, and optional duration.
- `DT:` displays short FLD/RLD text, including one encoded newline for two-line messages.
- `DH:` applies holo effects such as flash, rainbow, wag, nod, solid color, and reset.

These commands are visual-only: they do not move panels, play audio, forward `DM:*`, or start dome-owned sequence state. Invalid combinations are logged and ignored safely. `/api/health` reports the last applied logic/text/holo authoring command plus apply/reject counters, giving the body/editor a machine-readable way to confirm what the dome accepted.

Hardware verification: a body-authored test sequence successfully drove red MARCH logics, a two-line FLD text message, and red holo flashes over the normal body-to-dome link, then restored normal visuals with `DV:RESET_VISUALS`. The dome stayed healthy with no command queue overflow or rejected visual commands.

---

## Notes

- This document intentionally focuses on concrete fork behavior/features.
- Local bench-specific diagnostics and personal wiring notes should stay outside this file (for example local `tasks/*` notes) unless they represent a reusable firmware behavior change.
