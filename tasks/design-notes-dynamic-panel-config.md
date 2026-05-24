---
name: dynamic-panel-config-design
description: Design evaluation of making panel servo config dynamic per builder vs MK4 lock-in — decision and rationale
metadata:
  type: project
---

Evaluated whether to make dome panel servo configuration dynamic (per-builder) from the web UI. Decided to keep it locked to Mr. Baddeley MK4 Complex Dome. This was a deliberate, reasoned decision — not an oversight.

**Why:** The complexity cost of a generic system outweighs the benefit for a fork with a well-defined target hardware. A fully dynamic system would essentially be a different product.

## The three layers that need separating (if you ever revisit)

The firmware currently conflates three things in `servoSettings[]`:

1. **Physical address** — PCA9685 channel the servo wire is plugged into (0–15)
2. **Panel identity** — dome-community standard name (P1, P7, PP4, etc.)
3. **Logical command slot** — which `:OPnn` number moves that panel (01–11)

A dynamic system would need all three to be independently configurable: "ch3 → P7 → :OP05".

## Canonical panel naming reference

**Printed-droid panel numbering** is the right authoritative source if this is ever revisited:
- Ring panels: P1–P14 (not all have servos, depends on build)
- Pie panels: PP1–PP6 (not all have servos)
- Fixed/no-servo positions: Magic Panel (P5/MP), PSIs (P8, P14), FLD (P12), etc.

Panel type (ring vs pie) is implicit in the identity name — drives which aggregate mask (`ALL_DOME_PANELS_MASK` vs `PIE_PANELS_MASK`) a panel participates in.

## Complexity tiers evaluated

**Tier 1 — Active slot toggle (low complexity, high value):**
Store a bitmask in NVS marking which `:OPnn` slots have a physical servo. AND it against `ALL_DOME_PANELS_MASK` at boot. Sequences automatically skip empty slots. SVG greys out inactive panels.

**Tier 2 — Label overrides (medium complexity):**
NVS-stored label per slot. Web UI reads and displays builder-defined names. Pure UI config, firmware unchanged.

**Tier 3 — Dynamic channel-to-group assignment (high complexity, not worth it):**
Move `servoSettings[]` out of PROGMEM, make `panelTargetToMask()` a runtime lookup, re-register servos with ReelTwo at boot. Real ESP32 heap cost, startup complexity, and ReelTwo sequencer integration risk.

## Unsolved even with dynamic config

Sequence choreography (wave, marching ants, etc.) assumes a specific panel count and ordering. A builder with 4 ring panels instead of 7 would get odd-looking timing. Sequences operating on an active-only subset might work mechanically but look wrong aesthetically. No clean solution here.

## Current state is correct

The MK4 mapping is fully documented in:
- `servoSettings[]` inline comments (`AstroPixelsPlus.ino`)
- `CLAUDE.md` cross-reference table
- `panels.html` labels and SVG titles

All three stay in sync. Anyone picking up the fork knows exactly what physical panel maps to what command. The lock-in is a feature, not a limitation.

**How to apply:** If someone asks about adding generic dome support or multi-builder config — refer to this evaluation. Tier 1 (active slot toggle) is the only tier worth considering without a major rewrite. Tiers 2+3 belong in a different, more general product.
