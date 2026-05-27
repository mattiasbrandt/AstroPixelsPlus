# ADR 0003 — panelTargetToMask() retained as hardcoded switch

**Status:** Accepted  
**Date:** 2026-05-23

## Context

`panelTargetToMask()` in `MarcduinoPanel.h` maps `:OPnn` command targets to
`PANEL_GROUP_x` bitmasks via a hardcoded switch statement. The dynamic wiring
config feature makes physical channel assignments runtime-configurable, but the
command routing (which `:OPnn` drives which servo slot) remains fixed: slot 0 always
responds to `:OP01`, slot 1 to `:OP02`, etc.

During design, replacing the hardcoded switch with a runtime lookup table was
evaluated (Tier 3 dynamic config). This would allow operators to assign any panel
to any `:OPnn` command at runtime.

## Decision

Retain the hardcoded switch. The Tier 3 dynamic config was explicitly evaluated and
deferred during an earlier design session (see `tasks/design-notes-dynamic-panel-config.md`).
The dynamic wiring config feature in scope solves the physical channel assignment
problem — which servo wire goes where. It does not attempt to solve command routing
flexibility.

Inactive slots are excluded from command routing by calling `setServo()` with
`group = 0`, which zeroes the group bits in `fServos[]`. The command routing switch
itself is unchanged; it simply finds no servos matching those group bits for inactive
slots.

## Consequences

- `:OP01` always drives the servo(s) carrying the `PANEL_P1` address bit (formerly
  `PANEL_GROUP_1`). On the default MK4 wiring that is slot 0, but the slot index is
  not the invariant — the **address bit** is. The operator configures the physical
  channel that carries each panel-identity bit, not the command mapping itself.
- A future implementation of full command-routing flexibility (Tier 3) must replace
  `panelTargetToMask()` with a runtime table and make `PANEL_P*` / `PANEL_PP*` bit
  assignments dynamic — a significantly larger change.
- Sequence choreography (wave, marching ants, etc.) continues to operate on fixed
  slot ordering with hardcoded timing. Inactive slots cause silent pauses at their
  step — documented limitation.

## Amendments

- **2026-05-25** — Address-bit constants `PANEL_GROUP_1`..`PANEL_GROUP_10` were
  renamed to builder-facing `PANEL_P*` / `PANEL_PP*` identifiers (see
  [ADR 0006](0006-panel-address-bit-rename.md)). The routing decision in this ADR
  is unchanged; the consequence statement above was rewritten to reflect that the
  invariant is address-bit based, not slot-ordinal.
