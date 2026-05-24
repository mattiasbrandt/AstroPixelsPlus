# ADR 0002 — setServo() for post-construction servo slot override

**Status:** Accepted  
**Date:** 2026-05-23

## Context

The dynamic wiring config feature requires applying NVS-loaded channel assignments
and active/inactive state to the servo dispatch at boot — before any panel commands
are processed. Three approaches were evaluated:

**Option A — Delay construction:** Move `ServoDispatchPCA9685` from global scope
into `setup()` using a pointer or placement new. Construct after NVS load. Requires
changing the global declaration and all dependent globals (`ServoSequencer`,
`AnimationPlayer`) that reference it, or introducing a macro/reference alias.
High blast radius.

**Option B — Modify `servoSettings[]` before construction:** Remove `PROGMEM`,
then use a global constructor or early-init hook to modify the array before
`ServoDispatchPCA9685` constructs. ESP32 NVS requires `nvs_flash_init()` which
Arduino calls during framework init — not safely available in global constructors.
Unreliable.

**Option C — Use `setServo()` post-construction:** `ServoDispatchPCA9685` exposes
a virtual `setServo(num, pin, startPulse, endPulse, neutralPulse, group)` method
that updates the internal `fServos[]` state directly. Called early in `setup()`
after NVS is readable and before `SetupEvent::ready()` triggers the first I2C write.

## Decision

Use **`setServo()` (Option C)**. This is a documented virtual method on the
`ServoDispatch` base class interface, not a workaround. `panelConfigLoad()` and
`holoConfigLoad()` are called in `setup()` before `SetupEvent::ready()`, applying
NVS channel overrides and zeroing group bits for inactive slots via `setServo()`.

Key details:
- `pin = 0, group = 0` marks a slot as having no servo, excluded from all I2C writes
  and all mask-based command routing
- `pin = physicalChannel + 1` converts silkscreen number to firmware convention
- `disable()` on ServoDispatch is insufficient — it resets animation timing only,
  does not affect group masks or command routing

## Consequences

- `ServoDispatchPCA9685` remains a global object. No construction reordering needed.
- `panelConfigLoad()` must execute before `SetupEvent::ready()` — the call order
  in `setup()` is load-bearing.
- The feature depends on `setServo()` remaining part of the ReelTwo public API.
  Verified present in ReelTwo 23.5.3.
- `servoSettings[]` can remain `PROGMEM` (the constructor eagerly copies it; NVS
  overrides are applied afterwards via `setServo()`).
