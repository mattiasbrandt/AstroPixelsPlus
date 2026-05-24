---
description: "Dynamic servo wiring config — per-slot PCA9685 channel assignment and test UI for panel and holo boards"
created-date: 2026-05-23
implemented-date: 2026-05-24
spec-reference: docs/adr/0001-physical-channel-numbers-in-ui.md, docs/adr/0002-setservo-for-runtime-slot-override.md, docs/adr/0003-panel-command-routing-hardcoded-switch.md, docs/adr/0004-holo-servosettings-starts-at-pin-17.md
---

# Plan: Dynamic Servo Wiring Configuration UI

**Status:** Implemented on `main` — pending hardware verification (F9 flash + F10 empirical channel walk-through).
**Scope:** Medium-large (4–5 files, new NVS schema, new API endpoints, new UI sections
in two pages, firmware refactor prerequisite)  
**Prerequisite:** None — replaces the need for a physical rewire guess-and-check cycle

> **Fork highlight:** If implemented correctly this is a significant improvement
> for the broader R2 builder community. The channel ambiguity problem (which
> physical PCA9685 header maps to which firmware slot) affects every builder
> using a PCA9685-based dome. Replacing that guesswork with a point-and-test web
> UI — wire it once, then identify channels from the computer — is a genuine
> quality-of-life advancement beyond the upstream firmware. Covers both panel
> servos and holo projector servos. When shipped, `FORK_IMPROVEMENTS.md` should
> call this out prominently.

---

## Implementation Results (2026-05-24)

Landed across the following commits on `main` (in order):

| Commit | Scope |
|---|---|
| `5728db6` | R1 — `servoSettings[]` defaults corrected, `HARDWARE_WIRING.md` rewritten silkscreen-first, ADR 0004 added (holo pin off-by-one). |
| `6f81b19` | F1–F11 — slot constants + defaults, `panelConfigLoad()`/`holoConfigLoad()`, four API endpoints, raw I2C writer, `holoSweepPoll()` state machine, two UI sections, ADRs 0001/0002/0003, CONTEXT.md, this plan, design notes, `FORK_IMPROVEMENTS.md`. |
| `e81cc41` | Operator-facing warning when an active slot has no command-routing group bits (plan-mandated, missed in original feature commit). |
| `e9b5135` | Logging overhaul — moved `LogCapture` instance earlier so boot-time wiring logs reach the web log viewer; added structured info/warn/error logs to all new endpoints and to both load functions. |
| `2121040`, `85ab880` | `FORK_IMPROVEMENTS.md` entry rewritten in builder/operator language. |
| `66d0d63` | Codex review round 1 — three fixes (see *Post-implementation review fixes* below). |
| `afe6eaf` | Codex review round 2 — too-many-slots accepted; numeric-prefix `5bad` and `truejunk` accepted. |
| `803f8a8` | Codex review round 3 — whitespace treated as value terminator (`5 garbage` accepted); Reload didn't stop active test. |
| `6a6e16d` | Codex review round 4 — six UI `.finally()` sites silently desynced from server on stop-call failure. |
| `f9fd999` | Bench-flash polish — suppressed false `[E][Preferences.cpp:50] nvs_open failed: NOT_FOUND` boot noise by opening NVS namespaces read-write (silently creates on first boot, no flash writes after). |

### Deviations from the original spec

- **Default arrays placed after `servoSettings[]`** (in F1) instead of "immediately before `panelConfigLoad()`" — functionally equivalent (same file scope, same visibility) but reads differently.
- **GET response carries extra `board` and `slot_count` fields** not in the original API contract. Harmless extras; the UI ignores them but they aid debug via `curl`.
- **Added a Reload button** in both UI sections (not in spec, not forbidden). Re-fetches from server — discards local edits without resetting to factory defaults, so the no-reset-to-defaults rule from the spec still holds.
- **F11 (`FORK_IMPROVEMENTS.md` entry) was completed before F10 (empirical channel verification).** Dependency reversal: the entry describes what was built, makes no empirical claims, so this is harmless. F10 will still be done on hardware.

### Plan-instruction misses caught and fixed in follow-up commits

- **Group-bit safety warning** (edge case table line "`getGroup(i)` returns 0 for an active slot") was missing from the original feature commit. Added in `e81cc41` and `e9b5135` (now properly routes through `logCapture`).
- **Two-commit-group structure** (R1 separate from F1–F11) was initially squashed into the working tree as one batch. Split into separate commits after the user pointed it out.

### Post-implementation review fixes

GPT Codex performed three review rounds on 2026-05-24. Each round caught defects in the previous round's surface — a pattern worth noting on its own (see the meta note at the end of this section).

**Round 1 — `66d0d63`:**

1. **UI stuck-on test on row deactivation.** Unchecking the Active checkbox for the currently-testing row only reset the button label and cleared `activeTestIdx` — it never called `/api/servo/stop`. Result: panels stayed held open under PWM after the row went inactive; holo sweeps kept running in `mainLoop()`. Both UI handlers now POST `/api/servo/stop` first and reset state inside `.finally()`.
2. **Lax `strtol()` parsing.** Both `wiringConfigParseBody()` and `servoTestParseBody()` used `strtol(s, nullptr, 10)`, which returns 0 for non-numeric input (`"channel":"bad"`, `"channel":true`) — and that 0 was silently accepted as channel 0. A direct POST could pulse CH0 or save channel-0 assignments. Switched to the end-pointer form; rejects with `"channel must be a number"` if no digit was consumed, before the existing 0–15 range check.
3. **Silent partial NVS save.** `wiringConfigSave()` ignored the return values of `Preferences.putUChar()`/`putBool()` (0 = flash write failed). Endpoint could return `{ok:true}` after a partial save. Now checks each write, short-circuits the loop on failure, and the POST handler returns the existing 500 with the `[API] Error: ... NVS save failed` log line.

**Round 2 — `afe6eaf`:**

4. **Too many slots silently accepted.** `wiringConfigParseBody()` stopped iterating once `parsed == expectedSlots` and returned success without checking whether more `{...}` rows remained before the array terminator. A POST with 14 panel slots would save the first 13 silently. Added a trailing-objects scan; rejects with `"too many slots (expected exactly N)"`.
5. **Numeric-prefix and keyword-prefix garbage accepted.** Round 1's end-pointer `strtol()` only rejected wholly non-numeric input — `5bad` still parsed as 5 because the cursor *did* advance. Same for `truejunk` / `falsejunk` via `String::startsWith()`. Introduced an `isJsonValueTerminator()` helper and required the token to end at a structural terminator.

**Round 3 — `803f8a8`:**

6. **Whitespace treated as a value terminator.** Round 2's helper accepted whitespace as a valid terminator, so `"channel": 5 garbage` and `"active": true garbage` parsed cleanly — the parser stopped at the space and the space itself passed the check. Whitespace is a *separator*, not a *terminator*. Renamed the helper to `jsonValueEndsCleanly(const char *)` with a stricter algorithm (skip whitespace, *then* require `,`, `}`, `]`, or NUL). All four call sites updated.
7. **Reload didn't stop active test.** `loadPanelWiring()` / `loadHoloWiring()` re-rendered the table from `/api/{panels,holos}/config` without stopping any active test. Clicking Reload while a panel was held open reset every row's button visually, but the server-side test kept the panel under PWM (or the holo sweep state machine kept flipping phases). Both loaders now POST `/api/servo/stop` first if `activeTestIdx >= 0`, then clear state, then fetch.

**Round 4 — `6a6e16d`:**

8. **Six UI `.finally()` sites silently desynced on stop-call failure.** Every wiring-config UI site that POSTs `/api/servo/stop` was using `.finally()` to reset state. `.finally()` fires regardless of HTTP status AND on network errors — so a 401 / 500 / timeout would silently desync the UI from the server: button shows the idle label, the row checkbox shows inactive, or the table reloads, while the panel stays held open or the holo sweep keeps running server-side. Switched all six sites to `.then() + throw on !r.ok + .catch()`. On failure: test-button stay in ■ Close / ■ Stop; checkbox revert to checked; Reload aborted. Error toasts name the operator-visible consequence ("panel may still be held open", "sweep may still be running"). Same root cause as Round 1 item 1 — that fix patched the named case but kept `.finally()` everywhere else.

**Meta note — why each round caught more.** Each round's previous fix patched the *exact* case the previous report named without surveying the surrounding code shape. Round 2 hit because round 1 didn't audit the rest of the parser surface. Round 3 hit because round 2 only checked `isTerminator(*end)` for the named tokens. Round 4 hit because round 1's `.finally()` fix wasn't generalised to the other five sites with the same shape. The pattern only breaks when fixes generalise — rename the helper to push the right algorithm by name (round 3), audit every site sharing the same anti-pattern (round 4). Lesson: when a report names one case, treat it as a *sample* of a class, not the whole class. Search for the shape, not the symptom.

None of these defects would have surfaced through F9/F10 hardware verification — they require direct API clients sending malformed bodies, NVS-failure scenarios, transient stop-call failures, or specific UI sequencing flows that hands-on operator testing rarely exercises. External API review is genuinely catching issues that hardware testing cannot.

### Verification status

- **Local build** — clean across all commits (RAM 18.6%, Flash 84.0%).
- **Local browser smoke** — structural only (pages serve 200, markup well-formed, div balance correct). Interactive UI flow (table render, conflict highlighting, save banner) not driven manually.
- **Bench boot smoke (USB-only, no PCA9685 attached)** — **passed 2026-05-24** after flashing through `f9fd999`. Single clean POWERON_RESET, both `[Wiring] … config loaded` summary lines fire before `Ready`, no crash loop, no Guru Meditation, WiFi joins and web server comes up on the expected IP. Expected absences: I2C scan finds nothing (no boards), ServoDispatch logs four `Wire.cpp Error 263` probes for the missing PCA9685s and stops. The `[E][Preferences.cpp:50] NOT_FOUND` boot noise was eliminated in `f9fd999`.
- **Hardware flash + Marcduino compat matrix on full dome** — **pending** (F9 full).
- **Empirical channel walk-through using the new test UI** — **pending** (F10). MK4 channel defaults in code are best-available information; F10 is the ground-truth confirmation step. Any discrepancy at F10 requires a small correction commit to `defaultPanelCh[]` / `defaultHoloCh[]`.

### Operator-visible logging summary

All wiring-config events route through `logCapture` (visible in the web log viewer) using the existing `[Subsystem]` / inline-marker convention. See `e9b5135` commit message for the full catalogue. Sample lines:

- Boot: `[Wiring] Panel config loaded: 11 active, 2 inactive (NVS overrides: 0 channel, 0 active flag)`
- NVS override applied: `[Wiring] Panel P3 channel override: CH7 (default CH3)`
- Test pulse: `[Wiring] Test pulse: panels CH3 held open ...`
- Auto-stop: `[Wiring] Auto-stopping previous test on panels CH3 before starting new pulse`
- Validation rejection: `[API] panels/config rejected: channel 5 is assigned to slots 2 and 4`
- Save success: `[API] panels/config saved: 11 active, 2 inactive — reboot required to apply`

---

## Objective

Give any R2 builder a point-and-test web UI for wiring their dome servo board — so that servo channels can be identified, assigned, and verified from the computer without physical guesswork, dome disassembly, or reflashing.

---

## Success / Acceptance Criteria

All of the following must be true for the feature to be considered complete:

- [ ] A builder with a stock MK4 complex dome and no prior NVS configuration gets correct panel behaviour out of the box (no configuration required).
- [ ] A builder who has physically wired panels in a non-standard order can correct the mapping entirely from the web UI, save, reboot, and have all Marcduino commands route to the correct physical panels.
- [ ] Every panel slot and holo slot can be individually tested from the UI — a test opens/sweeps the servo and holds it until the operator stops it.
- [ ] Starting a new test automatically stops any running test on that board. No manual stop step required.
- [ ] Marking a slot inactive causes it to receive no PWM output and be silently skipped by all Marcduino sequences — with no crash or error.
- [ ] Duplicate active channel assignments are blocked both client-side (amber warning, Save disabled) and server-side (HTTP 400 before any NVS write).
- [ ] A direct `POST /api/panels/config` with invalid data (out-of-range channel, wrong slot count, duplicate active channels) is rejected with 400 and no partial NVS write.
- [ ] Config saved via the UI survives reboot and is reflected in `GET /api/panels/config` and `GET /api/holos/config`.
- [ ] All existing `:OP`, `:CL`, `:SE` sequences that currently work continue to work on all active slots after the feature ships.
- [ ] `FORK_IMPROVEMENTS.md` documents the feature as a highlighted advancement.

---

## Problem Statement

The current firmware hard-codes PCA9685 channel→servo slot mapping in a `PROGMEM`
array (`servoSettings[]` in `AstroPixelsPlus.ino`). This creates two compounding
problems:

**1. Channel indexing mismatch (root cause of wiring confusion):**
`ServoDispatchPCA9685` uses 1-indexed channel numbers internally:
```
physical_header = firmware_channel - 1
```
Firmware `{1, ...}` → PCA9685 hardware register for channel 0 → **physical header
labeled "0"** on the Adafruit board. The documentation has historically described
these as firmware channel numbers (e.g. "P1 → CH1"), which builders correctly
interpreted as "connect to the header labeled 1" — one position too high. This is
the verified root cause of the +1 offset observed in the 2026-05-21 integration test.

**2. No runtime configurability:**
Even if a builder wires correctly, there is no mechanism to mark a panel as absent
(e.g. P11 not yet wired), adjust a channel without reflashing, or verify which
physical panel responds to which command — without guessing, sending commands, and
physically inspecting the dome.

**Physical context:** dome panels are hinged, servo wires are routed through the
dome interior, the slipring connects dome to body, and the dome is typically not
within line-of-sight from the bench computer. Iterative physical rewiring is not a
reasonable debugging strategy. The correct solution is software: wire it once, then
use the UI to discover and confirm the mapping.

---

## Goals

1. Allow per-slot PCA9685 channel assignment from the web UI, persisted in NVS,
   for both panel servos (PCA9685 @ 0x40) and holo projector servos (PCA9685 @ 0x41).
2. Allow marking slots as inactive (no servo wired) so sequences skip them cleanly.
3. Provide a per-slot test button:
   - **Panels:** opens the servo and holds it open until the operator explicitly
     closes it. The operator walks to the dome, observes which panel moved, returns
     and confirms or closes. No auto-close.
   - **Holos:** sweeps the axis to one extreme, pauses 1 second, sweeps to the other
     extreme, pauses 1 second, repeats until the operator stops it. Unambiguous
     identification of axis and direction.
4. Pre-populate all UI defaults from the corrected MK4 wiring so a standard build
   needs zero NVS configuration.
5. Show physical silkscreen channel numbers (0–15 as printed on the Adafruit board)
   everywhere in the UI. The 1-indexed firmware convention is an internal detail,
   never surfaced to the operator.
6. Detect and block save on duplicate active channel assignments (same channel on
   two active slots would cause hardware conflict).
7. Do not break existing Marcduino sequence choreography for active slots.

---

## Non-Goals (explicitly deferred)

- **Marcduino slot reassignment**: the `:OPnn` command for each panel slot remains
  fixed by index. P1 always responds to `:OP01`, etc.
- **Panel label editing**: panel names (P1, P7, PP4…) are MK4 defaults shown as
  read-only. No custom naming.
- **Sequence timing compensation**: wave/flutter/marching-ants sequences have
  hardcoded per-step delays. Inactive slots will cause a brief pause at that step.
  Known limitation — documented, not fixed.
- **PCA9685 I2C address configuration**: panel board stays 0x40, holo board stays
  0x41. Not configurable.
- **Reset-to-defaults button**: operators correct individual rows manually. The
  table is always pre-populated with MK4 defaults when NVS is absent.

---

## Implementation Sequence

This feature ships as one branch with two distinct commit groups:

### Commit group 1 — Refactor (prerequisite, no user-visible behaviour change)

1. **Fix `servoSettings[]` default channel values** to match physical silkscreen
   reality. Currently the firmware uses 1-indexed values that map P1 to physical
   header 0 while docs say CH1=P1. The refactor corrects all default channel values
   so the code matches the documented MK4 wiring (P1 at header 1, P2 at header 2,
   etc. — as the current dome is physically wired). Verify against actual hardware.
2. **Update `docs/HARDWARE_WIRING.md`** in the same commit: rewrite the channel
   mapping table to show physical silkscreen numbers explicitly, add a note that
   firmware uses 1-indexed channels internally (hidden from builders), and correct
   any channel values that changed. Code and docs move together.

### Commit group 2 — Feature (dynamic config UI)

The dynamic config built on top of the corrected refactor.

---

## Architecture

### Key ReelTwo API finding

`ServoDispatchPCA9685` copies `servoSettings[]` into its own internal `fServos[]`
state eagerly at construction time (global scope, before `setup()` runs). However,
it exposes a post-construction override method:

```cpp
virtual void setServo(uint16_t num, uint8_t pin, uint16_t startPulse,
                      uint16_t endPulse, uint16_t neutralPulse, uint32_t group)
```

This updates channel, group, and pulse widths on an existing slot directly in
`fServos[]`. Called early in `setup()` after NVS is readable, it correctly overrides
the eagerly-copied defaults. **No construction reordering, no pointer tricks, no
PROGMEM removal required.**

- `pin = 0` + `group = 0`: marks a slot as having no servo, excluded from all masks.  
  **⚠️ PROGMEM safety:** `pin = 0` must be applied only via post-construction
  `setServo()`, never as a `{0, ...}` entry in `servoSettings[]`. The ReelTwo
  constructor executes `fLastLength[state->channel - 1]` without a zero-check —
  a PROGMEM pin of 0 underflows the array at construction time.
- `pin = physicalChannel + 1` — panel board (0x40): silkscreen 0–15 → firmware 1–16
- `pin = 16 + physicalChannel + 1` — holo board (0x41): silkscreen 0–15 → firmware 17–32

**Chip-boundary formula** (from `ServoDispatchPCA9685.h`):
```
chip    = (pin - 1) / 16   → 0 = 0x40 board, 1 = 0x41 board, ...
channel = (pin - 1) % 16
```
Concrete values for holo slots (physical CH0–5 on 0x41 board):

| Physical CH | Firmware pin | chip | board |
|-------------|-------------|------|-------|
| 0           | 17          | 1    | 0x41  |
| 1           | 18          | 1    | 0x41  |
| 2           | 19          | 1    | 0x41  |
| 3           | 20          | 1    | 0x41  |
| 4           | 21          | 1    | 0x41  |
| 5           | 22          | 1    | 0x41  |

Pin 16 (physical CH15 on 0x40) still addresses the panel board — the holo board
starts at firmware pin 17. The previous firmware defaults {16, 17, 18, 19, 20, 21}
were therefore wrong by one: slot that should use 0x41 CH0 was using 0x40 CH15.

`disable()` on ServoDispatch does NOT affect group masks — it only resets animation
timing state. `setServo()` is the correct API for both channel override and inactive
slot handling.

### Affected layers

```
panels.html  (new collapsed "Servo Wiring Config" section, placed low)
holos.html   (new collapsed "Holo Wiring Config" section, placed low)
    │ fetch GET  /api/panels/config
    │ fetch POST /api/panels/config
    │ fetch GET  /api/holos/config
    │ fetch POST /api/holos/config
    │ fetch POST /api/servo/test   (panels: open/hold; holos: sweep)
    │ fetch POST /api/servo/stop   (stops active holo sweep or closes open panel)
    ▼
AsyncWebInterface.h  (new route handlers)
    │ read/write NVS namespace "panels" for panel config
    │ read/write NVS namespace "holos" for holo config
    │ direct I2C write to 0x40 / 0x41 for raw channel test
    ▼
AstroPixelsPlus.ino  (panelConfigLoad + holoConfigLoad in setup())
    │ reads NVS overrides
    │ calls servoDispatch.setServo() for each slot
    ▼
ServoDispatchPCA9685  (ReelTwo — unchanged)
```

### Why direct I2C for the test endpoint

The test pulse must reach any channel 0–15 on either board, regardless of whether
that channel is assigned to any slot yet. Routing through `servoDispatch.setServo()`
or slot-based move methods requires knowing the slot index. Direct PCA9685 register
writes (standard LED_ON/LED_OFF registers via Wire) are simpler, stateless, and
slot-independent. Uses the same Wire pattern already present in the codebase.

Holo test uses the same raw I2C approach but fires a sweep sequence rather than a
single pulse: sweep to one extreme → 1 second hold → sweep to other extreme →
1 second hold → repeat until `/api/servo/stop` is called.

---

## NVS Schema

### Panel servo config — namespace `panels`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `pc_ch0` … `pc_ch12` | `uint8` | MK4 physical channel per slot | Physical silkscreen channel (0–15) on 0x40 board. Stored as physical; firmware adds 1 on use. |
| `pc_act0` … `pc_act12` | `bool` | true for wired slots | **Source of truth for active/inactive.** Whether slot has a physical servo wired. Channel value is ignored when active=false. |

> **active=false vs channel=255:** The `pc_act` bool is the sole source of truth.
> Channel 255 is NOT used as a sentinel — the channel field always holds a valid 0–15
> value. Firmware reads `pc_act` first; if false, it calls `setServo(i, 0, ..., 0)`
> regardless of the stored channel. This avoids ambiguity and keeps the API contract simple.

**MK4 defaults** (corrected to physical silkscreen, post-refactor):

| Slot | Panel | Physical CH | Active | Marcduino |
|------|-------|------------|--------|-----------|
| 0    | P1    | 1          | true   | :OP01     |
| 1    | P2    | 2          | true   | :OP02     |
| 2    | P3    | 3          | true   | :OP03     |
| 3    | P4    | 4          | true   | :OP04     |
| 4    | P7    | 5          | true   | :OP05     |
| 5    | P11   | 6          | true   | :OP06     |
| 6    | P13   | 7          | true   | :OP07     |
| 7    | PP5 ¹ | 0 (unused) | false  | —         |
| 8    | PP1   | 9          | true   | :OP08     |
| 9    | PP2   | 10         | true   | :OP09     |
| 10   | PP4   | 11         | true   | :OP10     |
| 11   | PP6   | 12         | true   | :OP11     |
| 12   | PP3 ¹ | 0 (unused) | false  | —         |

> ¹ PP5 (slot 7, `MINI_PANEL`) and PP3 (slot 12, `TOP_PIE_PANEL`) are **unserviced
> panels** — physical dome positions with firmware slots but no servo wired on the
> standard MK4 build. They appear in the UI as inactive rows with their panel label
> shown, allowing a builder who adds a servo to activate them. Panel identity mapping
> is assumed from the dome diagram; F10 empirical verification confirms.
>
> ⚠️ Default channel values above reflect the documented MK4 wiring after the
> refactor commit corrects the 1-indexed mismatch. Exact values must be verified
> empirically using the test UI after the feature ships (verification step 8).
> The defaults represent the best available information, not guaranteed-correct values.

### Holo servo config — namespace `holos`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `hc_ch0` … `hc_ch5` | `uint8` | Physical channel on 0x41 board | Physical silkscreen channel (0–15) on 0x41 board. |
| `hc_act0` … `hc_act5` | `bool` | true | Whether holo servo is wired. |

**MK4 defaults** (physical channels on 0x41 board):

| Slot | Holo | Axis | Physical CH | Active |
|------|------|------|------------|--------|
| 0    | FHP (HP1) | Horizontal | 0 | true |
| 1    | FHP (HP1) | Vertical   | 1 | true |
| 2    | THP (HP3) | Horizontal | 2 | true |
| 3    | THP (HP3) | Vertical   | 3 | true |
| 4    | RHP (HP2) | Vertical   | 4 | true |
| 5    | RHP (HP2) | Horizontal | 5 | true |

> **Chip-boundary confirmed:** firmware pins 17–22 map to 0x41 physical CH0–5.
> Firmware pin 16 = 0x40 CH15 (still panel board). Previous firmware defaults
> {16, 17, 18, 19, 20, 21} were off-by-one: slot 0 was on 0x40 CH15, not 0x41 CH0.
> The corrected defaults above (physical CH0–5 on 0x41, firmware pins 17–22) are
> derived from the verified formula. Empirical test-UI verification (step 8) confirms
> axis identity and direction, not the board-address mapping.

---

## API Endpoints

### `GET /api/panels/config`

Returns current panel config (NVS values, falling back to MK4 defaults).

```json
{
  "slots": [
    { "index": 0, "label": "P1",  "channel": 1, "active": true,  "cmd": ":OP01" },
    { "index": 5, "label": "P11", "channel": 6, "active": false, "cmd": ":OP06" }
  ]
}
```

### `POST /api/panels/config`

Saves panel config to NVS. Requires reboot to take effect.

```json
{ "slots": [ { "index": 0, "channel": 1, "active": true }, ... ] }
```

Response: `{ "ok": true, "reboot_required": true }`

**Server-side validation (applied before any NVS write):**
- `slots` array must be present and have exactly `NUM_PANEL_SLOTS` (13) entries
- Each `index` must be 0–12 and match its array position
- Each `channel` must be 0–15
- No two active slots may share the same channel (duplicate → 400 with error message)
- Invalid requests return HTTP 400 with `{ "error": "<reason>" }` — no partial writes

Client-side conflict detection is still required (for UX), but the API is the
enforcement boundary. A direct POST bypassing the UI must not be able to save invalid state.

### `GET /api/holos/config` / `POST /api/holos/config`

Same structure as panel config endpoints. Slots 0–5 corresponding to the holo table
above. Labels fixed (e.g. "FHP (HP1) — H", "FHP (HP1) — V").

Server-side validation mirrors panels: 6 slots required, channel 0–15, no duplicate
active channels per board.

### `POST /api/servo/test`

Sends a raw PWM pulse to a specific channel on a specific board. Does not time out automatically — it remains active until `/api/servo/stop` is called or another test is started on the same board.

```json
{ "board": "panels", "channel": 3 }
```

- `board`: `"panels"` (I2C 0x40) or `"holos"` (I2C 0x41)
- `channel`: physical silkscreen 0–15; clamped server-side, 400 if out of range

**PWM math (PCA9685 at 50 Hz):**
```
off_count = round(pulse_µs × 4096 × 50 / 1_000_000)
           = round(pulse_µs × 0.2048)

panel open  : 1800 µs → off_count ≈ 369
panel closed: 1200 µs → off_count ≈ 246
holo extreme A: 2000 µs → off_count ≈ 410
holo extreme B: 1000 µs → off_count ≈ 205
holo / panel neutral: 1500 µs → off_count ≈ 307
```

These are approximations for identification purposes. They do not need to match
the configured startPulse/endPulse of each slot exactly — the goal is visible
movement, not precision positioning.

**Complete I2C transaction:**
```cpp
uint8_t boardAddr = (board == "panels") ? 0x40 : 0x41;
uint8_t reg = 6 + channel * 4;  // PCA9685 LED0_ON_L base; each channel occupies 4 bytes
Wire.beginTransmission(boardAddr);
Wire.write(reg);           // select the channel's register block
Wire.write(0);             // LED_ON_L  = 0
Wire.write(0);             // LED_ON_H  = 0
Wire.write(lowByte(count));   // LED_OFF_L
Wire.write(highByte(count));  // LED_OFF_H (bit 4 set = full off; not used here)
Wire.endTransmission();
```

**State tracking (server-side):** the firmware tracks one active test per board
`(board, channel)`. A new `/api/servo/test` request on any channel of a board
automatically stops the previous test on that board before starting the new one.
This is enforced server-side — the UI does not need to send an explicit stop first.

For holo sweep, the handler fires a state-machine loop:
extreme A → 1 s hold → extreme B → 1 s hold → repeat until `/api/servo/stop`.

Response: `{ "ok": true, "board": "panels", "channel": 3 }`

### `POST /api/servo/stop`

Stops the active test on a board.

```json
{ "board": "panels" }
```

Body requires only `board` — the server already tracks which channel is active.
Sending `channel` is accepted but ignored. 400 if `board` is not `"panels"` or `"holos"`.
No-op (200) if no test is active on that board.

**What stop writes via raw I2C:**
- `"panels"` board: writes closed position (1200 µs, off_count ≈ 246) to the active channel
- `"holos"` board: writes neutral position (1500 µs, off_count ≈ 307) to the active channel

After writing the stop pulse, the server clears the active-test state for that board.
ReelTwo regains normal control of all slots on the next servo update cycle.

---

## Firmware Changes (`AstroPixelsPlus.ino`)

### `panelConfigLoad()` — called early in `setup()`

```cpp
void panelConfigLoad() {
    Preferences prefs;
    prefs.begin("panels", true);
    for (int i = 0; i < NUM_PANEL_SLOTS; i++) {
        char keyC[10], keyA[10];
        snprintf(keyC, sizeof(keyC), "pc_ch%d", i);
        snprintf(keyA, sizeof(keyA), "pc_act%d", i);

        bool active = prefs.isKey(keyA) ? prefs.getBool(keyA, true) : defaultPanelActive[i];
        uint8_t physCh = prefs.isKey(keyC) ? prefs.getUChar(keyC, defaultPanelCh[i])
                                           : defaultPanelCh[i];

        uint32_t group   = active ? servoDispatch.getGroup(i) : 0;
        uint8_t  pin     = active ? (physCh + 1) : 0;  // 0x40: silkscreen → firmware pin
        uint16_t start   = servoDispatch.getStart(i);
        uint16_t end     = servoDispatch.getEnd(i);
        uint16_t neutral = servoDispatch.getNeutral(i);

        servoDispatch.setServo(i, pin, start, end, neutral, group);
    }
    prefs.end();
}
```

`holoConfigLoad()` mirrors `panelConfigLoad()` with two differences: the NVS namespace
("holos", keys `hc_ch` / `hc_act`), and the pin formula for the 0x41 board:

```cpp
void holoConfigLoad() {
    Preferences prefs;
    prefs.begin("holos", true);
    for (int i = HOLO_SLOT_OFFSET; i < HOLO_SLOT_OFFSET + NUM_HOLO_SLOTS; i++) {
        int slotIdx = i - HOLO_SLOT_OFFSET;  // 0–5; used for NVS keys and default arrays
        char keyC[10], keyA[10];
        snprintf(keyC, sizeof(keyC), "hc_ch%d",  slotIdx);
        snprintf(keyA, sizeof(keyA), "hc_act%d", slotIdx);

        bool active   = prefs.isKey(keyA) ? prefs.getBool(keyA,  defaultHoloActive[slotIdx])
                                           : defaultHoloActive[slotIdx];
        uint8_t physCh = prefs.isKey(keyC) ? prefs.getUChar(keyC, defaultHoloCh[slotIdx])
                                           : defaultHoloCh[slotIdx];

        uint32_t group   = active ? servoDispatch.getGroup(i) : 0;
        uint8_t  pin     = active ? (16 + physCh + 1) : 0;  // 0x41: physical CH0 → firmware pin 17
        uint16_t start   = servoDispatch.getStart(i);
        uint16_t end     = servoDispatch.getEnd(i);
        uint16_t neutral = servoDispatch.getNeutral(i);

        servoDispatch.setServo(i, pin, start, end, neutral, group);
    }
    prefs.end();
}
```

### Constants

```cpp
#define NUM_PANEL_SLOTS   13   // servo slots 0–12 in servoSettings[]
#define NUM_HOLO_SLOTS     6   // servo slots HOLO_SLOT_OFFSET through HOLO_SLOT_OFFSET+5
#define HOLO_SLOT_OFFSET  13   // index of first holo slot in servoSettings[]; verify against actual array
```

These keep firmware and API in sync. `HOLO_SLOT_OFFSET` must be verified against the
actual `servoSettings[]` array length before use — if the refactor adds or removes
panel slots the offset shifts.

### Default tables (used when NVS is absent)

```cpp
// Physical silkscreen channels for each panel slot (0x40 board, 0-indexed)
static const uint8_t defaultPanelCh[NUM_PANEL_SLOTS] =
    { 1, 2, 3, 4, 5, 6, 7, 0, 9, 10, 11, 12, 0 };
//    P1 P2 P3 P4 P7 P11 P13 PP5 PP1 PP2  PP4  PP6  PP3
// PP5 (slot 7) and PP3 (slot 12) have no servo on standard MK4; channel 0 stored but ignored.

static const bool defaultPanelActive[NUM_PANEL_SLOTS] =
    { true, true, true, true, true, true, true, false,
      true, true, true, true, false };
//    P1    P2    P3    P4    P7    P11   P13   PP5(no servo)
//    PP1   PP2   PP4   PP6   PP3(no servo)

// Physical silkscreen channels for each holo slot (0x41 board, 0-indexed)
static const uint8_t defaultHoloCh[NUM_HOLO_SLOTS] = { 0, 1, 2, 3, 4, 5 };
//                                                    FHP-H FHP-V THP-H THP-V RHP-V RHP-H
static const bool defaultHoloActive[NUM_HOLO_SLOTS] = { true, true, true, true, true, true };
```

### Call site in `setup()`

```cpp
Wire.begin();
panelConfigLoad();   // before SetupEvent::ready()
holoConfigLoad();    // before SetupEvent::ready()
SetupEvent::ready(); // triggers ServoDispatchPCA9685::setup() → first I2C write
```

`panelConfigLoad()` and `holoConfigLoad()` must execute before `SetupEvent::ready()`
because that call triggers the PCA9685 hardware reset and mode configuration — after
which servo positions begin being driven. The `setServo()` calls update `fServos[]`
before any I2C channel writes occur.

---

## Web UI

### `panels.html` — new "Servo Wiring Config" section

Placed below operational panel controls and SVG diagram. **Collapsed by default.**
Header: "Servo Wiring Config ▶" — expands on click.

Subtitle: *Panel servo board (PCA9685 @ 0x40) — channel numbers match the silkscreen
on your board.*

**Table:**

| Panel | Active | Channel (0–15) | Command | Test |
|-------|--------|----------------|---------|------|
| P1    | ☑      | `[ 1 ▾]`       | :OP01   | ▶    |
| P11   | ☐      | `[ 6 ▾]`       | :OP06   | ▶    |
| …     |        |                |         |      |

**Test button states:**
- **▶** → POST `/api/servo/test {board: "panels", channel}`. Button becomes **■ Close**.
- **■ Close** → POST `/api/servo/stop {board: "panels"}`. Returns to **▶**.

Only one panel can be in test-open state at a time. Server-side, opening a second
channel automatically stops the first (the server tracks the active test and closes
it before starting the new one). The UI mirrors this by resetting the previous row's
button to ▶ when a new test is started.

**Conflict detection:** if an active slot shares a channel with another active slot,
both rows highlight amber and Save is disabled. Warning: *"Channel N is assigned to
more than one active slot."*

**Save button:** POSTs to `/api/panels/config`. Shows "Reboot required" banner
(consistent with `setup.html` pattern).

---

### `holos.html` — new "Holo Wiring Config" section

Placed below operational holo controls. **Collapsed by default.**

Subtitle: *Holo servo board (PCA9685 @ 0x41) — channel numbers match the silkscreen
on your board.*

**Grouped by projector:**

```
FHP — Front Holo Projector (HP1)
  Horizontal  [ 0 ▾]  ☑  ↔ sweep  ▶
  Vertical    [ 1 ▾]  ☑  ↕ sweep  ▶

RHP — Rear Holo Projector (HP2)
  Vertical    [ 4 ▾]  ☑  ↕ sweep  ▶
  Horizontal  [ 5 ▾]  ☑  ↔ sweep  ▶

THP — Top Holo Projector (HP3)
  Horizontal  [ 2 ▾]  ☑  ↔ sweep  ▶
  Vertical    [ 3 ▾]  ☑  ↕ sweep  ▶
```

**Test button (holos):** fires sweep sequence (extreme A → 1s pause → extreme B →
1s pause → repeat). Button becomes **■ Stop**. Stopping returns servo to neutral.
Axis icon (↔ or ↕) indicates which physical direction to observe.

Same conflict detection and Save/reboot-required pattern as panels section.

---

## Sequence Compatibility

When `setServo(i, 0, 0, 0, 0, 0)` is called for an inactive slot:
- `pin = 0`: the `fServos[i].channel != 0` guard in `ServoDispatchPCA9685` skips
  all I2C writes for this slot
- `group = 0`: excluded from all mask comparisons in `_moveServosToPulse()` and
  related methods — invisible to `:OP00`, `:CL00`, `:OP12`, `ALL_DOME_PANELS_MASK`,
  `PIE_PANELS_MASK`, every sequence

**Known limitation:** wave/flutter/marching-ants sequences have hardcoded per-step
delays. A deactivated mid-sequence slot causes a silent pause at that step. This is
a ReelTwo AnimationPlayer constraint, out of scope. To be noted in
`FORK_IMPROVEMENTS.md` when this feature ships.

---

## Edge Cases & Handling

| Scenario | Planned Behaviour |
|----------|-------------------|
| NVS absent on first boot | Default arrays used; behaviour identical to before refactor for MK4 wiring. No user action required. |
| NVS partially written (power loss mid-save) | POST uses a single `prefs.begin/end` block. If the block completes, all keys are written. If it doesn't complete (power loss), previously written keys may be stale — reboot will read a mix of old and new. Mitigation: the GET endpoint always returns the full current state; a second Save corrects it. No silent corruption. |
| Operator assigns the same physical channel to two active slots | Blocked client-side (amber warning + disabled Save) and server-side (HTTP 400 before any NVS write). |
| Operator assigns a channel outside 0–15 | Blocked server-side (HTTP 400). UI channel dropdown is constrained to 0–15 — not reachable from UI. |
| Operator marks a slot inactive but assigns a valid channel | Channel value is stored but ignored at boot (`pc_act` is the sole source of truth). UI greys out the channel dropdown for inactive rows to prevent confusion. |
| Panel and holo boards share no channels (different I2C addresses) | Each board validates channels independently. A channel conflict on 0x40 has no relationship to the same channel number on 0x41. No cross-board conflict detection needed. |
| Test is active when firmware reboots | PCA9685 re-initialises on next boot. ReelTwo sets all channels to their firmware-configured positions. Test state is lost; the servo returns to its configured position. No stuck-open panel. |
| Second test request arrives while a test is active on the same board | Server closes previous test (writes stop pulse), then starts new test. No race condition in the async handler because ESP32 processes web requests on a single task in the AsyncWebServer pattern. |
| `getGroup(i)` returns 0 for an active slot | Means the slot has no group bits in `servoSettings[]` — treated as belonging to no command group. Passing group=0 to `setServo()` for an active slot is functionally equivalent to marking it inactive for Marcduino routing. Add a serial warning log if this occurs during `panelConfigLoad()`. |
| Holo sweep state machine interrupted by another web request | The sweep flag is cleared atomically before writing the stop pulse. If a new test arrives mid-sweep, the sequence is: clear sweep flag → write stop pulse → write new test pulse → set new sweep flag. |
| `servoSettings[]` slot count differs from `HOLO_SLOT_OFFSET + NUM_HOLO_SLOTS` | `HOLO_SLOT_OFFSET` must be verified against the actual array length before R1 is committed. A static assert or compile-time check is recommended. |
| Builder has fewer than 12 dome panels wired | Unwired slots are marked inactive via the UI; they are excluded from all sequences. No change to how wired slots behave. |

---

## Verification Steps

- [ ] Refactor build passes cleanly; no behaviour change for standard MK4 wiring
- [ ] `HARDWARE_WIRING.md` channel table matches corrected `servoSettings[]` defaults
- [ ] `GET /api/panels/config` returns correct MK4 defaults on fresh NVS
- [ ] `POST /api/panels/config` roundtrips: save → reboot → GET returns saved values
- [ ] Same for `/api/holos/config`
- [ ] `POST /api/servo/test {board: "panels", channel: N}` moves servo on
  that physical channel of 0x40 board; holds open; `POST /api/servo/stop {board: "panels"}` closes it
- [ ] `POST /api/servo/test {board: "holos", channel: N}` fires sweep on
  that physical channel of 0x41 board; `POST /api/servo/stop {board: "holos"}` returns to neutral
- [ ] Inactive slot: `:OP06` for inactive P11 does nothing, no crash, no PWM output
- [ ] `:OP00` / `:CL00` correctly skips inactive slots
- [ ] Duplicate active channel: Save button disabled, amber warning shown
- [ ] Panels page: wiring section collapsed by default, expands on click
- [ ] Holos page: wiring section collapsed by default, expands on click
- [ ] Save → reboot → verify NVS-overridden channel moves correct physical panel
- [ ] Upload SPIFFS; verify UI loads from device (not local server)
- [ ] **Empirical channel verification**: use test UI to confirm every panel slot
  and holo slot channel. Update NVS defaults in code if any discrepancy found.
- [ ] Update `FORK_IMPROVEMENTS.md`

---

## Step-by-Step Tasks

---

### Refactor (one commit — code and docs move together)

- [ ] **R1. Fix `servoSettings[]` defaults and `HARDWARE_WIRING.md`**
  - **Task:** Correct all 19 slots in `servoSettings[]` from 1-indexed firmware values to
    the values that produce correct physical silkscreen channel routing for MK4 wiring.
    Panel slots: `pin = physical + 1` (physical CH1=P1 → firmware {2,...}). Holo slots:
    `pin = 16 + physical + 1` (physical CH0 on 0x41 → firmware {17,...}).
    **Current holo pins `{16, 17, 18, 19, 20, 21}` are wrong by one** — pin 16 addresses
    0x40 CH15 (the panel board), not 0x41 CH0. Corrected holo pins must be
    `{17, 18, 19, 20, 21, 22}`. Same +1 offset problem as panels.
    Slots 7 (`MINI_PANEL`) and 12 (`TOP_PIE_PANEL`) retain their PROGMEM group bits —
    no servo is wired on MK4 but the entries remain for protocol compatibility.
    `panelConfigLoad()` zeroes them at boot via `setServo(i, 0, ..., 0)` because
    `defaultPanelActive[7] = false` and `defaultPanelActive[12] = false`.
    Rewrite `HARDWARE_WIRING.md` channel table in the same commit so code and docs are always in sync.
  - **Files:**
    - `AstroPixelsPlus.ino` (servoSettings[], lines ~379–403): Update firmware pin values
      for all 13 panel slots and 6 holo slots. Add an inline comment above the array
      explaining 1-indexed convention and the physical → firmware formula.
    - `docs/HARDWARE_WIRING.md`: Rewrite the servo channel mapping table to show physical
      silkscreen numbers (0-indexed). Add a callout box noting that firmware adds 1
      internally and that this detail is never surfaced to builders.
  - **Dependencies:** None — holo pin fix is a bug correction; panel behaviour unchanged for correct wiring.
  - **Validation:**
    - `pio run -e astropixelsplus` builds cleanly with no warnings.
    - `:OP01` drives physical CH1 on the 0x40 board (not CH0 as before the refactor).
    - P13 (slot 6, physical CH7) still routes correctly.
    - Manually diff old vs new `servoSettings[]` to confirm no unintended slot changes.

---

### Feature

- [ ] **F1. Constants and default arrays**
  - **Task:** Define named constants for slot counts and offset, and static arrays for
    MK4 default channel assignments and active state. These are the fallback values used
    when NVS is absent and must match the corrected `servoSettings[]` from R1.
  - **Files:**
    - `AstroPixelsPlus.ino`: Add `#define NUM_PANEL_SLOTS 13`, `#define NUM_HOLO_SLOTS 6`,
      `#define HOLO_SLOT_OFFSET 13` near other defines at the top. Add
      `defaultPanelCh[]`, `defaultPanelActive[]`, `defaultHoloCh[]`, `defaultHoloActive[]`
      static const arrays immediately before `panelConfigLoad()`. Each array entry must
      carry an inline comment with the panel/holo identity it represents.
  - **Dependencies:** R1 — arrays must match the corrected `servoSettings[]` values.
  - **Validation:** Verify array lengths equal their corresponding `#define` constants.
    Build passes.

- [ ] **F2. `panelConfigLoad()` and `holoConfigLoad()`**
  - **Task:** Implement both load functions. Each reads NVS, falls back to the default
    arrays from F1 if a key is absent, and calls `servoDispatch.setServo()` for every
    slot. Inactive slots get `pin=0, group=0`. Panel pin formula: `physCh + 1`. Holo
    pin formula: `16 + physCh + 1`. Neutral pulse read with `servoDispatch.getNeutral(i)`.
    Add both call sites in `setup()` before `SetupEvent::ready()`.
  - **Files:**
    - `AstroPixelsPlus.ino`: Add `panelConfigLoad()` function (NVS namespace "panels",
      keys `pc_ch%d` / `pc_act%d`, iterates slots 0–NUM_PANEL_SLOTS-1). Add
      `holoConfigLoad()` function (namespace "holos", keys `hc_ch%d` / `hc_act%d`,
      iterates HOLO_SLOT_OFFSET through HOLO_SLOT_OFFSET+NUM_HOLO_SLOTS-1; uses
      `slotIdx = i - HOLO_SLOT_OFFSET` for key and array indexing). Add both calls
      in `setup()` after `Wire.begin()` and before `SetupEvent::ready()`. Add inline
      comment at the call site explaining why ordering is load-bearing.
  - **Dependencies:** F1 (constants and arrays), R1 (corrected PROGMEM defaults so
    `getGroup(i)` returns the right group bits).
  - **Validation:**
    - Fresh NVS: firmware behaviour identical to before the refactor for MK4 wiring.
    - Manually write one NVS key (`pc_ch0 = 3`) via a test sketch or NVS tool; reboot;
      verify `:OP01` now drives physical CH3 instead of CH1.
    - Mark a slot inactive (`pc_act5 = false`); reboot; verify `:OP06` produces no
      PWM output and no crash.

- [ ] **F3. `GET /api/panels/config` and `POST /api/panels/config`**
  - **Task:** Add two async route handlers. GET reads NVS namespace "panels", falls back
    to default arrays, and returns a JSON array of 13 slot objects
    `{index, label, channel, active, cmd}`. POST validates the body (13 slots, indexes
    0–12, channels 0–15, no duplicate active channels) and writes to NVS atomically —
    all keys or none. Returns `{ok, reboot_required}` on success or `{error}` on 400.
  - **Files:**
    - `AsyncWebInterface.h`: Two new route handlers registered in the web server setup
      block. Implement a shared `panelConfigRead()` helper for GET and a
      `panelConfigWrite()` helper for POST validation + NVS write. Logic:
      - GET: open "panels" namespace read-only; for each slot, read pc_ch%d / pc_act%d
        or use defaults; build JSON response.
      - POST: parse JSON body; validate slot count, index range, channel range, no
        duplicate active channels; write all keys inside a single prefs.begin/end block.
  - **Dependencies:** F1, F2 (default arrays and constants available).
  - **Validation:**
    - `GET /api/panels/config` on fresh NVS: returns 13 slots with MK4 defaults.
    - `POST` with valid body → 200 `{ok: true, reboot_required: true}` → reboot → GET
      returns the saved values.
    - `POST` with two active slots sharing channel 5 → 400 `{error: "..."}`, NVS unchanged.
    - `POST` with channel value 16 → 400.
    - `POST` with 12 slots → 400.

- [ ] **F4. `GET /api/holos/config` and `POST /api/holos/config`**
  - **Task:** Mirror of F3 for holo slots. Namespace "holos", keys `hc_ch%d` / `hc_act%d`,
    6 slots. Slot labels are fixed strings: "FHP (HP1) — H", "FHP (HP1) — V",
    "THP (HP3) — H", "THP (HP3) — V", "RHP (HP2) — V", "RHP (HP2) — H".
    Same server-side validation rules as panels (6 slots, channel 0–15, no duplicate
    active channels).
  - **Files:**
    - `AsyncWebInterface.h`: Two additional route handlers, sharing validation logic with
      F3 where possible.
  - **Dependencies:** F1, F2.
  - **Validation:** Same roundtrip and rejection tests as F3, applied to holo endpoints.

- [ ] **F5. `POST /api/servo/test` and `POST /api/servo/stop`**
  - **Task:** Raw I2C test endpoint. Tracks one active test per board (two global state
    variables: `panelTestChannel` and `holoTestChannel`, defaulting to -1). On new test
    request: if the board already has an active test, write the stop pulse first; then
    write the open/start pulse and update state. For panel: write 1800 µs (≈369 counts)
    and hold. For holo: start a non-blocking sweep loop (state machine or FreeRTOS task
    — resolve async pattern by checking existing `AsyncWebInterface.h` before implementing).
    Stop endpoint: write closed pulse (panels: 1200 µs ≈246) or neutral pulse (holos:
    1500 µs ≈307) to the tracked channel, then clear state.
    PWM count formula: `round(µs × 4096 × 50 / 1_000_000)`.
    I2C transaction: `Wire.beginTransmission(addr); Wire.write(6 + ch*4); write 4 bytes
    (LED_ON_L=0, LED_ON_H=0, LED_OFF_L, LED_OFF_H); Wire.endTransmission()`.
  - **Files:**
    - `AsyncWebInterface.h`: Two new route handlers. Two file-scope state variables
      (panel active channel, holo active channel). A `writePwm(uint8_t boardAddr,
      uint8_t channel, uint16_t count)` helper wrapping the I2C transaction — used
      by both test and stop. For holo sweep: implement as a state machine polled in
      the async handler or a FreeRTOS task (match the pattern already in the codebase).
  - **Dependencies:** F1 (board address constants). `SetupEvent::ready()` must have been
    called before any test request — always true for web requests.
  - **Validation:**
    - `POST /api/servo/test {board: "panels", channel: 3}`: servo on 0x40 CH3 opens and
      holds. Verify with a meter or physical observation.
    - `POST /api/servo/stop {board: "panels"}`: servo closes. State cleared (second stop
      is a no-op returning 200).
    - `POST /api/servo/test {board: "panels", channel: 5}` while CH3 is open: CH3 closes
      automatically before CH5 opens. No manual stop needed.
    - `POST /api/servo/test {board: "holos", channel: 0}`: servo on 0x41 CH0 sweeps
      back and forth. Stop returns to neutral.
    - `POST /api/servo/test` with channel 16 → 400.

- [ ] **F6. `panels.html` — Servo Wiring Config section**
  - **Task:** Add a collapsed `<details>` section below the SVG diagram and existing
    operational controls. On expand: fetch `GET /api/panels/config` and render a table
    with one row per slot (panel label, active checkbox, channel 0–15 dropdown, Marcduino
    command label, test button). Inactive rows grey out the channel dropdown and disable
    the test button. Implement conflict detection on every checkbox/dropdown change:
    highlight duplicate active channels amber, disable Save. Test button posts to
    `/api/servo/test`; becomes ■ Close while active; second test auto-resets the
    previous row's button via JS state tracking. Save posts to `/api/panels/config`
    and shows reboot-required banner on success (match `setup.html` pattern exactly).
    Collapse state persists via `localStorage` (match whatever pattern existing pages use).
  - **Files:**
    - `data/panels.html`: Add `<details id="wiring-config">` section with inline JS
      or a module function in `app.js` handling fetch, render, conflict detection, and
      test state. Prefer `app.js` if other page sections already use it for fetch logic.
  - **Dependencies:** F3, F5.
  - **Validation:**
    - Section is collapsed on first page load; state remembered after navigation away and back.
    - Dropdown shows 0–15; default values match MK4 defaults.
    - Change two active slots to same channel: both rows turn amber, Save disabled.
    - Resolve conflict: amber clears, Save re-enables.
    - Test button: correct visual state transitions, only one active at a time.
    - Save → reboot-required banner appears.

- [ ] **F7. `holos.html` — Holo Wiring Config section**
  - **Task:** Mirror of F6 for holo config. Grouped layout by projector (FHP, RHP, THP)
    with axis label (H/V) and sweep direction icon (↔ or ↕). Test button fires sweep;
    stop returns to neutral. Same conflict detection and Save/reboot pattern.
  - **Files:**
    - `data/holos.html`: Add `<details id="holo-wiring-config">` section. Grouped
      rendering: iterate slots, group consecutive rows by holo identity. Axis icon
      determined by slot label suffix.
  - **Dependencies:** F4, F5.
  - **Validation:** Same as F6, applied to holo page.

- [ ] **F8. Build and local web server test**
  - **Task:** Compile firmware; run local Python server; exercise both wiring config
    sections in browser. Check browser console for JS errors and `tail -f` the HTTP log
    for 404s.
  - **Files:** No changes — verification step.
  - **Dependencies:** F1–F7.
  - **Validation:**
    - `pio run -e astropixelsplus` — clean build, no warnings.
    - `python3 -m http.server 8080 --directory data` — both pages load, sections expand,
      table renders with correct defaults, conflict detection fires, no console errors.

- [ ] **F9. Flash firmware and SPIFFS; verify on hardware**
  - **Task:** Upload firmware and SPIFFS separately. Confirm wiring config sections load
    from device. Confirm all pre-existing panel and holo commands still work.
  - **Files:** No changes — verification step.
  - **Dependencies:** F8.
  - **Commands:**
    ```
    pio run -e astropixelsplus -t upload
    pio run -e astropixelsplus -t uploadfs
    pio device monitor -p /dev/ttyUSB0 -b 115200
    ```
  - **Validation:**
    - Device boots cleanly; no NVS errors in serial log.
    - `:OP01` drives the expected physical panel.
    - UI wiring sections load from device IP.

- [ ] **F10. Empirical channel verification with test UI**
  - **Task:** Use the test button for every slot to confirm channel identity. Walk to
    dome for panels; observe sweep direction for holos. Correct any default values in
    `defaultPanelCh[]` / `defaultHoloCh[]` if empirical results differ from the table.
    Commit corrections if any.
  - **Files (if corrections needed):** `AstroPixelsPlus.ino` (default arrays from F1).
  - **Dependencies:** F9.
  - **Validation:** Every slot test drives the documented physical panel or holo axis.
    No surprises. This step is the ground truth — the plan's defaults are best-available
    information, not guaranteed correct.

- [ ] **F11. `FORK_IMPROVEMENTS.md` entry**
  - **Task:** Add a prominent entry describing the feature, its motivation (eliminates
    physical channel guesswork for all builders), and the scope (both panel and holo
    boards). Note the inline test functionality and the NVS persistence.
  - **Files:** `FORK_IMPROVEMENTS.md`.
  - **Dependencies:** F10 complete (empirically verified).
  - **Validation:** Entry present; accurate description; no placeholder language.

---

## Risks, Clarifications & Interventions Required

| # | Risk / Open Point | Severity | Resolution |
|---|-------------------|----------|------------|
| 1 | ~~Holo chip-boundary formula~~ | — | **Resolved.** `chip=(pin-1)/16`, `channel=(pin-1)%16`. Holo pin = `16 + physCh + 1`. |
| 2 | ~~`getNeutral()` API~~ | — | **Resolved.** `servoDispatch.getNeutral(i)` confirmed in ReelTwo 23.5.3. |
| 3 | ~~Holo sweep async pattern~~ | — | **Resolved from code.** All timed deferred work uses `millis()`-based deadline variables polled in `mainLoop()` (`sPanelReleaseAtMs`, `sSoundInitAtMs`, etc.). The `xTaskCreatePinnedToCore` in setup() creates the WiFi/web event loop on Core 0; `mainLoop()` runs on Core 1. Holo sweep uses the same pattern: flag + deadline variable polled in `mainLoop()`. No FreeRTOS task needed. |
| 4 | **Default channel values may differ from actual hardware** | Low | The `defaultPanelCh[]` table is derived from documented MK4 wiring, not empirical measurement. F10 is the ground-truth step — any discrepancies found there require code corrections before F11. |
| 5 | ~~Raw I2C test writes vs ReelTwo servo update cycle~~ | — | **Resolved from code.** `AnimatedEvent::process()` only drives I2C when a servo is actively animating. At rest, no writes occur. Raw test writes persist until a Marcduino command moves that channel. No contention. |
| 6 | ~~`getGroup(i)` correctness after R1~~ | — | **Resolved from code.** Group fields (`PANEL_GROUP_x`, `MINI_PANEL`, `HOLO_HSERVO`, etc.) are independent columns from pin values in `servoSettings[]`. R1 only touches pin values. No corruption possible. |
| 7 | ~~`HOLO_SLOT_OFFSET` value~~ | — | **Resolved from code.** Array has exactly 13 panel slots (0–12) and 6 holo slots (13–18), total 19. `HOLO_SLOT_OFFSET = 13`, `NUM_HOLO_SLOTS = 6` confirmed correct. Add `static_assert` in F1 to guard against future array changes. |
| 9 | **Holo servoSettings[] pins off by one** | High | **Found in code.** Current pins `{16,17,18,19,20,21}` are wrong — pin 16 addresses 0x40 CH15 (panel board), not 0x41 CH0. Corrected in R1 to `{17,18,19,20,21,22}`. This is the same +1 offset as panels. Empirical verification (F10) confirms axis identity after fix. |
| 8 | **NVS 15-character key length limit** | Low | All current keys are within limit (`pc_act12` = 8 chars). Verify before adding any new keys. |

---

## Implementation Quality Standard

This feature ships as a highlighted fork improvement. No quick fixes, no workarounds
that paper over edge cases:

- **Inline code comments:** all non-obvious lines must carry a short comment explaining
  the WHY — the channel formula, the PROGMEM safety constraint, the ordering requirement
  before `SetupEvent::ready()`, the group-bit semantics. If a future developer would
  need to re-read this plan to understand a line of code, the line needs a comment.
  This applies to firmware (`.ino`, `.h`), API handlers, and UI JavaScript alike.
- `setServo()` is the verified correct ReelTwo API for post-construction slot
  override. It is not a hack — it is the documented virtual interface.
- Raw I2C test writes must not corrupt PCA9685 state for other channels.
- Conflict detection is enforced server-side (API returns 400 on invalid input);
  client-side detection is required for UX but is not the enforcement boundary.
- NVS absent = MK4 defaults, indistinguishable from explicitly saved MK4 values.
- The collapsed section default must persist across page navigations (use
  localStorage or consistent collapse state, matching the existing UI pattern).

---

## References

| Resource | Purpose |
|----------|---------|
| `docs/adr/0001-physical-channel-numbers-in-ui.md` | Why UI shows silkscreen 0–15, not firmware 1–16 |
| `docs/adr/0002-setservo-for-runtime-slot-override.md` | Why `setServo()` is used instead of construction reordering or PROGMEM modification |
| `docs/adr/0003-panel-command-routing-hardcoded-switch.md` | Why `panelTargetToMask()` is retained as a hardcoded switch |
| `docs/adr/0004-holo-servosettings-starts-at-pin-17.md` | Why holo board (0x41) starts at firmware pin 17, not 16; original pins were wrong by one |
| `CONTEXT.md` | Domain glossary: physical channel, firmware channel, servo slot, panel identity, FHP/HP1 equivalences |
| `tasks/design-notes-dynamic-panel-config.md` | Earlier design session notes; Tier 1/2/3 scope discussion; context for what was deferred |
| `tasks/protoR2-testing.md` | 2026-05-21 integration test notes; wiring offset table; original discovery of +1 channel shift |
| `AstroPixelsPlus.ino` (lines ~379–403) | `servoSettings[]` array — the PROGMEM defaults being corrected in R1 |
| `AsyncWebInterface.h` | Existing async web server setup; pattern to follow for new route registration |
| `ReelTwo ServoDispatchPCA9685.h` (lines 299–315) | `setServo()` implementation; (lines 424–425) chip-boundary formula |
| `docs/HARDWARE_WIRING.md` | Channel mapping documentation — rewritten in R1 |
| `FORK_IMPROVEMENTS.md` | Fork change log — updated in F11 |
