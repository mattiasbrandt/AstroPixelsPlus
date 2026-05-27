# ADR 0009 — DomeSequences.h slot constants use builder-facing panel identities

**Status:** Accepted
**Date:** 2026-05-26

## Context

`DomeSequences.h` is the ported Janiuk choreography file. It defines its own
slot-index constants for use inside hardcoded `domeMove(slot, position, time)`
calls:

```cpp
#define DP1   0   /* Small Panel 1 — PANEL_GROUP_1 */
#define DP2   1   /* Small Panel 2 — PANEL_GROUP_2 */
...
#define DP5   4   /* P7  (small upper ring panel)        — PANEL_GROUP_5  :OP05 */
#define DP6   5   /* P11 (lower-left ring panel)         — PANEL_GROUP_6  :OP06 */
#define DPA   6   /* P13 (lower-front ring panel, FLD)   — PANEL_GROUP_7  :OP07 */
#define DPB   7   /* unused (ch 8 unconnected)           — MINI_PANEL           */
#define DPP1  8   /* PP1 (pie panel 1)                   — PANEL_GROUP_8  :OP08 */
...
```

These constants share the original Janiuk fork's naming convention — slot-index
ordering (`DP1`..`DP6`, then `DPA`/`DPB`, then `DPP1`..`DPP4`) — which has three
problems:

1. **The names lie.** `DPA` is P13, `DPB` is PP5, `DPP3` is PP4 (not PP3),
   `DPP4` is PP6 (not PP4). A reader sees `domeMove(DPP3, ...)` and reasonably
   expects PP3 — but the call actually moves PP4. The same trap that ADR 0006
   identified for `PANEL_GROUP_N` and renamed away from.
2. **Slot 12 (PP3) has no constant.** Every helper array (`lowerPanels`,
   `allPanels`) silently skips PP3 because there's no `DPxx` to put in the
   array, even when PP3 is wired.
3. **The "unused" comments rot.** Slot 7 was labelled `MINI_PANEL` /
   "unused (ch 8 unconnected)". Post-ADR-0008 it carries PP5 and may be wired;
   the comment is now actively misleading.

The companion panel-command-numbering fix (`tasks/panel-command-numbering-fix.md`,
ADR 0006) renamed `PANEL_GROUP_N` → builder-facing `PANEL_P*` / `PANEL_PP*`
identifiers for exactly this reason. `DomeSequences.h` exhibits the same problem
locally and benefits from the same fix.

## Decision

Rename every slot-index constant in `DomeSequences.h` to use the builder-facing
panel identity, with a `D_` prefix so the constant remains visually distinct
from the address-bit constants in `AstroPixelsPlus.ino`:

| Old name | Slot | Panel identity | New name |
|----------|------|----------------|----------|
| `DP1` | 0 | P1 | `D_P1` |
| `DP2` | 1 | P2 | `D_P2` |
| `DP3` | 2 | P3 | `D_P3` |
| `DP4` | 3 | P4 | `D_P4` |
| `DP5` | 4 | P7 | `D_P7` |
| `DP6` | 5 | P11 | `D_P11` |
| `DPA` | 6 | P13 | `D_P13` |
| `DPB` | 7 | PP5 | `D_PP5` |
| `DPP1` | 8 | PP1 | `D_PP1` |
| `DPP2` | 9 | PP2 | `D_PP2` |
| `DPP3` | 10 | PP4 | `D_PP4` |
| `DPP4` | 11 | PP6 | `D_PP6` |
| *(none — slot skipped)* | 12 | PP3 | `D_PP3` (new) |

Comments next to each constant reference the new `PANEL_P*` / `PANEL_PP*`
address-bit names from `AstroPixelsPlus.ino` (post-ADR-0006). Stale
`PANEL_GROUP_N` / `MINI_PANEL` comments are removed.

The corresponding helper arrays (`tasks/sequence-audit.md` prerequisite P4) become:

```cpp
static const uint8_t ringPanels[] = { D_P1, D_P2, D_P3, D_P4, D_P7, D_P11, D_P13 };
static const uint8_t piePanels[]  = { D_PP1, D_PP2, D_PP3, D_PP4, D_PP5, D_PP6 };
static const uint8_t allPanels[]  = { D_P1, D_P2, D_P3, D_P4, D_P7, D_P11, D_P13,
                                      D_PP1, D_PP2, D_PP3, D_PP4, D_PP5, D_PP6 };
```

Pie order is identity-sequential (PP1 → PP6) — see `sequence-audit.md` Q-1.

## Consequences

- Every `domeMove(D_P13, ...)` call self-documents which panel it drives. The
  PP5-as-P13's-shadow choreography bug (sequences pairing `DPA` + `DPB`) becomes
  visually obvious at the call site and gets rewritten as part of the per-sequence
  audit, not papered over.
- PP3 is a first-class participant in every "all panels" sequence by virtue of
  appearing in the new `allPanels[]` array. The silent skip is fixed by naming.
- Every existing call site in the 919-line `DomeSequences.h` is touched (`DP*` →
  `D_P*` rename across ~150+ references). This is a compile-time break — no
  silent runtime regression possible. Downstream forks that included this file
  unchanged need a corresponding refactor.
- The `D_` prefix prevents accidental collision with the address-bit constants
  in `AstroPixelsPlus.ino` (`PANEL_P1`, `PANEL_PP5`, etc.). Constants in
  `DomeSequences.h` are *slot indices* (0..12); address bits are bitmasks. The
  prefix makes the distinction visible at a glance.
- This decision deliberately mirrors ADR 0006's "names that cannot lie"
  principle. Future contributors writing new sequence functions will reach
  for identity-based names by precedent.
