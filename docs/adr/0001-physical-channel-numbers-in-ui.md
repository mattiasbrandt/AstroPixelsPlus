# ADR 0001 — Physical silkscreen channel numbers in UI

**Status:** Accepted  
**Date:** 2026-05-23

## Context

`ServoDispatchPCA9685` uses 1-indexed firmware channel numbers internally:
`firmware_channel = physical_channel + 1`. Firmware `{1, ...}` addresses the
PCA9685 hardware register for channel 0 — the header physically labeled "0" on the
Adafruit board.

Historically, firmware and documentation described channel assignments using
firmware channel numbers (e.g. "P1 → CH1"). Builders correctly read this as
"connect to the connector labeled 1" — one position too high. This 1-indexed/
0-indexed mismatch is the verified root cause of the +1 wiring offset observed
across all newly-wired panel servos in the 2026-05-21 integration test.

Two options for the wiring config UI:
- Show firmware channel numbers (1–16): matches `servoSettings[]` values in code
- Show physical silkscreen numbers (0–15): matches what the builder sees on the board

## Decision

The UI shows **physical silkscreen channel numbers (0–15)** exclusively. The
conversion to firmware convention (`physical + 1`) is performed once in the firmware
layer and never surfaced to the operator or builder. NVS stores physical channel
numbers. Documentation (HARDWARE_WIRING.md) is written in terms of physical channels.

## Consequences

- Builders can wire directly from UI values to the connector labeled with that number.
- Firmware must add 1 when converting stored physical channel to the `pin` argument
  of `setServo()`.
- Future developers reading NVS values or the config API will see 0-indexed numbers
  that differ from `servoSettings[]` by 1 — this ADR explains why.
- The corrected `servoSettings[]` default values (post-refactor) use firmware channel
  numbers internally but are derived from physical channel values in NVS.
