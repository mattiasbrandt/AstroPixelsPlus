# ADR 0006 — Rename PANEL_GROUP_N to panel-identity address bits

**Status:** Accepted  
**Date:** 2026-05-25

## Context

`PANEL_GROUP_N` defines a sequential bitmask where N historically equalled the
`servoSettings[]` slot index + 1. This implied a relationship between slot index,
group number, and Marcduino command number that was never correct for panels
beyond P4.

The consequence was a real bug: slot 4 holds P7 but was assigned `PANEL_GROUP_5`,
causing `:OP05` to open P7 and `:OP07` to open P13. The name `PANEL_GROUP_5`
gave no indication of which physical panel it controlled — a reader had to cross-
reference `servoSettings[]`, `panelTargetToMask()`, and the panel wiring table
simultaneously to understand the mapping.

The word "group" also conflated two distinct concepts that share the same bitmask
mechanism: individual panel address bits (one bit selects one servo slot) and
multi-panel shortcut masks (`PIE_PANEL`, `DOME_PANELS_MASK` — one value selects
many slots). This made it easy to pass the wrong constant type without a compile
error.

## Decision

Rename all `PANEL_GROUP_N` defines to use builder-facing panel identity names:

- Ring panels: `PANEL_P1`, `PANEL_P2`, `PANEL_P3`, `PANEL_P4`, `PANEL_P7`,
  `PANEL_P11`, `PANEL_P13`
- Pie panels: `PANEL_PP1`, `PANEL_PP2`, `PANEL_PP3`, `PANEL_PP4`, `PANEL_PP5`,
  `PANEL_PP6`

Bit positions for the existing panels are preserved (e.g. `PANEL_P7` = `1L<<18`,
same bit as the old `PANEL_GROUP_5`). Three new bits are added for panels that
previously had no individual address bit: `PANEL_PP5` (bit 24), `PANEL_PP6`
(bit 25), `PANEL_PP3` (bit 26).

Multi-panel type constants (`SMALL_PANEL`, `PIE_PANEL`, `DOME_PANELS_MASK`, etc.)
are unchanged — they describe type or group behaviour, not individual panel
addresses, and their names already make that clear.

## Consequences

- Code is self-documenting: `PANEL_P7` in a `servoSettings[]` entry immediately
  identifies which physical panel the slot controls. The slot-index confusion that
  caused the original bug cannot recur.
- The distinction between individual address bits and group shortcuts is now
  visible in the name — `PANEL_P11` is an address bit, `PIE_PANEL` is a type flag.
- Bit width verified safe: `uint32_t` is used consistently throughout
  `ServoDispatch` and `ServoSequencer`; bits up to 31 are available.
- Any downstream code or fork referencing the old `PANEL_GROUP_N` constants by
  name must be updated. This is a compile-time break — no silent runtime
  regression is possible.
- `ServoStep` sequence frames use slot-index bitmaps (separate from group bits)
  and are unaffected by this rename.
