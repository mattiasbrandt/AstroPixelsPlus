# ADR 0011 — Live-apply wiring config saves

**Status:** Accepted
**Date:** 2026-07-06

## Context

The wiring config UI lets a builder assign physical PCA9685 silkscreen channels
to panel and holo servo slots, and mark slots active or inactive. Before this
decision, saving a config wrote NVS and returned `reboot_required:true`; the new
routing only took effect on the next boot when `panelConfigLoad()` and
`holoConfigLoad()` ran before `SetupEvent::ready()`.

That reboot requirement made commissioning slower than necessary. The firmware
already uses `ServoDispatch::setServo()` for post-construction slot override
(ADR 0002), and the same call is valid at runtime as long as the implementation
preserves calibration pulses and restores command group bits from immutable
slot defaults.

Raw servo tests also share the same operator workflow. They write directly to a
physical PCA9685 channel to identify wiring, bypassing slot routing. If a save
changes routing while a raw servo test is active, the old channel can remain
under PWM and make the UI lie about the current hardware state.

## Decision

Saving panel or holo wiring config live-applies the routing immediately:

- Validate the posted slots.
- Reject duplicate active channels.
- Persist the config to NVS.
- Stop any active raw servo test on the same board.
- Re-apply that board's runtime slot routing with `setServo()`.
- Return `{"ok":true,"applied":true,"reboot_required":false}`.

The apply step changes routing only. It does not send close, open, neutral, or
settle pulses to every configured slot. The only movement-like write during a
save is the best-effort raw servo test stop if a test was active.

NVS persistence is the durable result. Direct I2C writes used to stop raw servo
tests are best-effort cleanup and are not transaction participants. The firmware
does not attempt rollback after a successful save.

Runtime apply restores command group bits from the immutable `servoSettings[]`
defaults rather than from `servoDispatch.getGroup()`. This matters because an
inactive slot has already been applied with `group=0`; reactivating it must
restore its original Marcduino command membership.

## Consequences

- Builders can save a corrected wiring map and immediately test the affected
  Marcduino command without rebooting.
- `panelConfigLoad()` and `holoConfigLoad()` still run at boot before
  `SetupEvent::ready()`; boot ordering from ADR 0002 remains load-bearing.
- Raw servo tests remain channel-based commissioning tools, not operational
  control commands.
- Existing clients that check `reboot_required` still receive the field, but it
  is now `false` on successful wiring config saves.
- If NVS save fails, routing is not live-applied.
