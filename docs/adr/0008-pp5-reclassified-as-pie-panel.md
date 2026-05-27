# ADR 0008 — Reclassify PP5 from MINI_PANEL to PIE_PANEL

**Status:** Accepted
**Date:** 2026-05-25

## Context

In the upstream ReelTwo `ServoDispatch.h` taxonomy, panel servos carry a **type
bit** describing their physical character (`SMALL_PANEL`, `MEDIUM_PANEL`,
`BIG_PANEL`, `PIE_PANEL`, `TOP_PIE_PANEL`, `MINI_PANEL`) alongside an **address
bit** (`PANEL_P*` / `PANEL_PP*`) that makes the slot individually addressable.

Historically, slot 7 (PP5 on the MK4 dome) was assigned the `MINI_PANEL` type
bit while the other pie panels carried `PIE_PANEL` (PP1, PP2, PP4, PP6) or
`TOP_PIE_PANEL` (PP3). There is no documented rationale for PP5 being singled
out as `MINI_PANEL` — it is a pie panel by every builder-facing criterion
(physical position, role in choreography, expected response to "all pie panels"
group commands).

The companion rename in [ADR 0006](0006-panel-address-bit-rename.md) reclaims
several Marcduino command numbers for individual ring-panel addressing
(`:OP11` → P11, `:OP12` → PP6, `:OP13` → P13). This pushes the canonical
"all pie panels" group command to `:OP14`, which currently maps to
`PIE_PANEL | TOP_PIE_PANEL` — a mask that excludes PP5 because PP5 carries
`MINI_PANEL`.

A builder who wires a servo to PP5 and then issues `:OP14` ("all pie") would
reasonably expect all six pie panels to move. Under the old taxonomy, PP5
would silently stay put.

## Decision

Change PP5's `servoSettings[]` type bit from `MINI_PANEL` to `PIE_PANEL`:

```cpp
// Before
{8, 800, 2200, MINI_PANEL},                  /* slot 7: PP5 (unserviced) */

// After
{8, 800, 2200, PANEL_PP5 | PIE_PANEL},       /* slot 7: PP5 (unserviced) */
```

`panelTargetToMask()` case 14 remains `PIE_PANEL | TOP_PIE_PANEL` — no change
needed there. PP5 is now naturally included in the "all pie" group when wired.

The `MINI_PANEL` `#define` remains in `AstroPixelsPlus.ino` for upstream
compatibility (it is defined in `Reeltwo/src/ServoDispatch.h`) but is no longer
referenced by any servo entry in this firmware. `isPanelServoByGroup()` keeps
`MINI_PANEL` in its accepted mask for the same compatibility reason.

## Consequences

- `:OP14` ("all pie") drives all six pie panels (PP1–PP6) once the builder
  activates PP5 via the wiring config. No special-casing.
- PP5 participates in any future sequence frame that targets `PIE_PANEL`. This
  matches builder intuition; the prior exclusion was an accident of taxonomy,
  not a deliberate choreography decision.
- The `MINI_PANEL` type bit becomes unused within this firmware. It is
  preserved in upstream ReelTwo and may be repurposed by future builders or
  forks that have a panel with genuinely distinct "mini door" semantics. No
  reader of this firmware should infer a hidden meaning from its presence in
  `isPanelServoByGroup()`.
- The MK4 wiring config UI and PP5's "unserviced by default" behaviour are
  unchanged. `panelConfigLoad()` still zeroes inactive slots at boot.
