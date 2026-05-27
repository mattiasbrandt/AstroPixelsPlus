# Sequence Audit — Panel Coverage Under Renamed Mapping

**Status:** All decisions locked 2026-05-26 — ready for implementation approval
**Parent task:** `panel-command-numbering-fix.md`
**Related ADRs:**
- 0003 (amended) — routing invariant is address-bit, not slot-ordinal
- 0006 — `PANEL_GROUP_N` → `PANEL_P*` / `PANEL_PP*` rename
- 0007 — `:OPP*` / `:CLP*` / `:OFP*` pie panel namespace
- 0008 — PP5 reclassified from `MINI_PANEL` to `PIE_PANEL`
- 0009 — `DomeSequences.h` identity-based slot constants (`D_P*` / `D_PP*`)

## Why this audit exists

The rename + PP5 reclassification doesn't *cause* incorrect choreography, but it
*exposes* it. Pre-rename, `DPB` (slot 7) was "unused — MINI_PANEL" and any
`domeMove(DPB, ...)` was harmless. Post-reclassification, slot 7 carries PP5 and
may be wired by builders. Every sequence that pairs `DPA` (P13) with `DPB` (PP5)
will move PP5 with P13 — a choreography that is wrong by accident, not design.

Likewise, slot 12 (PP3) has no `DP*` constant at all; every "all panels" array in
`DomeSequences.h` silently skips PP3 even when wired.

Per the user directive (2026-05-26): **"all sequences must be 100% and perfect
quality, no easy or quick fixes."** Each sequence must have an *intentional*
choreography — not a runtime guard, not a removed call, not a piggy-backed
`domeMove`.

## MK4 safety — inactive slots are safe at the firmware layer

A reasonable concern: if a standard MK4 dome has PP3 (slot 12) and PP5 (slot 7)
unwired and a sequence issues `domeMove(D_PP5, …)` or `domeMove(D_PP3, …)`,
does that grind a servo, hang the sequence, or crash the firmware?

**Answer: no.** The ReelTwo `ServoDispatchPCA9685` already treats `channel == 0`
as the "no servo on this slot" sentinel, and `panelConfigLoad()` in
`AstroPixelsPlus.ino` is the source that sets `channel = 0` for every inactive
slot at boot. The relevant trace:

1. `panelConfigLoad()` (~line 485 in `AstroPixelsPlus.ino`) for any inactive
   slot calls `setServo(i, /*pin=*/0, ..., /*group=*/0)`. Comment in code:
   *"pin=0 + group=0 is the ReelTwo convention for 'no servo on this slot'."*
2. `ServoDispatchPCA9685::_moveServoToPulse` (`Reeltwo/src/ServoDispatchPCA9685.h`)
   guards on `channel != 0`:
   ```cpp
   virtual void _moveServoToPulse(uint16_t num, uint32_t startDelay,
                                   uint32_t moveTime, uint16_t startPos,
                                   uint16_t pos) override
   {
       if (num < numServos && fServos[num].channel != 0)
       {
           // … only proceeds if channel is non-zero
       }
   }
   ```
3. `ServoDispatchPCA9685::isActive(num)` returns `false` for inactive slots —
   `setServo` calls `state->init()` which zeroes the move-state fields
   (`finishTime`, `lastMoveTime`, `finishPos`), and `isActive()` is
   `(finishTime != 0 || lastMoveTime != 0 || finishPos != 0)`.
4. `disable(num)` is also a safe no-op for inactive slots — it just calls
   `init()` again.

**Trace through `domeMove(D_PP5, pos, ms, /*wait=*/true)` on a standard MK4:**

| Step | What happens |
|------|-------------|
| `servoDispatch.moveToPulse(7, ms, pos)` | Ultimately reaches `_moveServoToPulse(7, …)`. |
| `_moveServoToPulse` checks `fServos[7].channel != 0` | **False** (zeroed by panelConfigLoad) → silent return, no PCA9685 write, no servo command. |
| `while (servoDispatch.isActive(7))` | `isActive(7)` returns false immediately → loop exits with zero iterations. |
| `domeMove` returns | No hang, no grind, no I2C traffic to a non-existent channel. |

**`schedulePanelRelease`** on inactive slots is equally safe — the release path
iterates slots where `(group & mask) != 0`, and inactive slots have `group == 0`
post-`panelConfigLoad`, so they're filtered out.

**`applyPanelCalibrationToMask`** (used by `:MV`, `#SO`, etc.) iterates slots
where `(group & mask) != 0` — same filter, same skip.

### Consequences for the audit

- **No code-level safety changes needed.** No `if (panelActive(slot))` wrappers
  in DomeSequences.h. The safety guarantee lives at the
  `ServoDispatchPCA9685::_moveServoToPulse` layer where it belongs (and is
  validated by every other ReelTwo project that uses inactive slots).
- **Pacing on standard MK4 vs fully-wired dome is slightly different** —
  a `wait=true` call on an inactive slot returns instantly, so a sequence
  that includes PP3/PP5 with their own `wait=true` would beat slightly
  faster on standard MK4. For the audit's choreography decisions (PP3 paired
  with PP4 in cantina, PP5 included in the canonical pie sweep), the pacing
  is dominated by the *wired* panel in the call group, so the visual is
  fine on both configurations.
- **Pre-existing cosmetic limitation** — `ServoStep` library frames over
  `ALL_DOME_PANELS_MASK` (e.g. `SeqPanelWave`) tick the per-step delay even
  on inactive slots, so a wave gets a brief pause where the missing panel
  would have been. Already documented in FORK_IMPROVEMENTS.md, not
  introduced by this fix.

## Cross-cutting prerequisites (apply to every sequence)

These changes happen once and unlock the per-sequence edits:

| # | Change | Rationale |
|---|--------|-----------|
| P1 | ✅ **Done (Slice D)** — `DP1`..`DPP4` renamed to `D_P1`..`D_P13`, `D_PP1`..`D_PP6`. `D_PP3` (slot 12) and `D_PP5` (slot 7) added. Stale `PANEL_GROUP_N` / `MINI_PANEL` comments stripped. | Same principle as ADR 0006: opaque slot constants (`DPA`, `DPB`, `DPP3`-means-PP4) lie. Identity-based names cannot. |
| P2 | ✅ **ADR 0009 written** (`docs/adr/0009-dome-sequences-identity-slot-constants.md`). Records the rename table, the `D_` prefix convention, and the new helper arrays. | Surprising-without-context: a future contributor will wonder why DomeSequences.h has its own slot constants vs. just using slot numbers; the ADR records that identity-naming is the rule. |
| P3 | ✅ **Done (Slice E)** — every piggy-backed `D_PP5` / `D_PP3` call replaced with intentional choreography per the per-sequence decisions below. No runtime guards added. | Mattias directive: no piggy-backed calls, no runtime guards. |
| P4 | ✅ **Helper arrays standardised** (names confirmed 2026-05-26): <br>• `ringPanels[]` = `{D_P1, D_P2, D_P3, D_P4, D_P7, D_P11, D_P13}` (7 entries — ring only)<br>• `piePanels[]` = `{D_PP1, D_PP2, D_PP3, D_PP4, D_PP5, D_PP6}` (6 entries — all pies, identity-sequential PP1→PP6)<br>• `allPanels[]` = `ringPanels ++ piePanels` (13 entries) | Existing `lowerPanels[]` mis-classes PP5 as ring; existing `allPanels[]` skips PP3. Both replaced with semantically-correct arrays. |
| P5 | ✅ **Done (Slice C)** — `:SE12 TopPanelsShowcase` mask changed from `PIE_PANEL` to `(PIE_PANEL \| TOP_PIE_PANEL)`. | Today excludes PP3 silently. Sequence name implies "top panels" — should include every pie including the top centre PP3. |

## Per-sequence intent table

For each sequence: observed current behaviour (from code reading at lines listed),
proposed intended choreography (the *correct* version), and a Decision column for
your grill responses.

Notation in the "Touches" column lists the panels in the visual/temporal order
they participate, not slot order.

### DM:* sequences — direct slot calls in `DomeSequences.h`

| Sequence | Lines | Current behaviour (observed) | Proposed intent | Decision ✅ |
|----------|-------|------------------------------|-----------------|--------------|
| `domeOpenClosePies` (`DM:PIES`) | 194–250 | Pies-only choreography. Touches PP1, PP2, PP4, PP6 in a stagger then reverses. **Skips PP3 and PP5 entirely.** | Touch **all 6 pies** in identity-sequential order (PP1 → PP2 → PP3 → PP4 → PP5 → PP6). Same stagger pattern, just extended to 6 entries. Reverse identically. | ✅ Rewrite. Iterate `piePanels[]` in canonical order (Q-1). |
| `domeOpenCloseLow` (`DM:LOW`) | 251–330 | "Low" (ring) panel choreography. Touches P1, P2, P3, P4, P7, P11, P13 — **and also PP5 (via DPB)** as if it were a ring panel. | Touch **ring panels only**: `ringPanels[]` (7 entries). PP5 is a pie, not a ring — remove. | ✅ Rewrite. Drop every `D_PP5` (was `DPB`) call from this function. |
| `domeOpenCloseAll` (`DM:OPENALL`) | 331–417 | All 12 currently-defined panels. **Skips PP3.** | All 13 panels via `allPanels[]`. PP3 participates in the same stagger pattern as the other pies. | ✅ Rewrite. Iterate `allPanels[]` in canonical order (ring first, pies PP1→PP6). |
| `domeFlutter` (`DM:FLUTTER`) | 418–472 | All 12 panels flutter in slot order, then snap closed. **Skips PP3.** Pairs P13 with PP5 as ring. | Flutter `ringPanels[]` then `piePanels[]` as separate ordered passes (so the visual is "ring wave → pie wave" rather than slot-order jumble). PP3 included via piePanels; PP5 fluttered as a pie. | ✅ Rewrite. Two-phase: ring sweep then pie sweep. |
| `domeBloom` (`DM:BLOOM`) | 473–505 | Pie panels ease open, wiggle, close. Touches PP1, PP2, PP4, PP6 only. **Skips PP3 and PP5.** | All 6 pies bloom together via `piePanels[]`. PP3 (top centre) is the most expressive bloom panel — definitely included. | ✅ Rewrite. Iterate `piePanels[]`. |
| `domeScream` (`DM:SCREAM`) | 506–586 | All 12 panels burst open. Local `allPanels[]` at line 538 **skips PP3**. | All 13 panels burst. Replace the local `allPanels[]` with the shared array from prerequisite P4. | ✅ Rewrite. Remove the local array literal; use the file-scope `allPanels[]`. |
| `domeOverload` (`DM:OVERLOAD`) | 587–627 | Calls `domeRandomPanels()` helper for sluggish drift. Inherits helper bugs. | No body-level change once helper is fixed. | ✅ No change to body. Helper fix (below) cascades through. |
| `domeHeart` (`DM:HEART`) | 628–644 | **No panel motion.** Logic + holo + PSI light show only. | No change. | ✅ No change. |
| `domeAlarm` (`DM:ALARM`) | 645–663 | **No panel motion.** Logic + holo + PSI + sound. | No change. | ✅ No change. |
| `domeDisco` (`DM:DISCO`) | 664–673 | **No panel motion.** Stub / sends to body only. | No change. | ✅ No change. |
| `domeVader` (`DM:VADER`) | 674–692 | **No panel motion.** Logic + holo + PSI + sound (47s). | No change. | ✅ No change. |
| `domeRockMarch` (`DM:ROCKMARCH`) | 693–711 | Touches **only P1** once. Otherwise logic/holo/sound. | Rewrite as full ring-panel march: step through `ringPanels[]` in rock-beat timing. Pies stay out. | ✅ Rewrite per Q-2. |
| `domeHelloThere` (`DM:HELLO`) | 712–739 | Touches **only P1** in a wave (single panel waves to greet). | Intentional. Rename `DP1`→`D_P1`. Add inline comment marking the single-panel design as deliberate. | ✅ Per Q-3: identifier rename only, plus a one-line comment. |
| `domeLeiaMode` (`DM:LEIA`) | 740–760 | **No panel motion.** Holo + holo LED + sound (36s). | No change. | ✅ No change. |
| `domeResetAll` (`DM:RESET`) | 761–800 | Close-all then reset subsystems. **Skips PP3.** | Close all 13 via `allPanels[]`. Critical that PP3 doesn't get left open after `:OPP3`. | ✅ Rewrite. Iterate `allPanels[]`; PP3 included. |
| `domeCantina` (`DM:CANTINA`) | 801–894 | 130 BPM 15-second dance, alternating panel sets. **Skips PP3.** | PP3 pairs with PP4 on every beat where PP4 fires — read as a "top wedge" pair. Preserves the 130 BPM count exactly. PP5 fires on its identity-sequential beat alongside the other pies. | ✅ Per Q-4. Every `D_PP4` cantina call becomes `{D_PP4, D_PP3}` simultaneously. |
| `domeRandom` (`DM:RANDOM`) | 895–973 | Wrapper that calls `domeRandomPanels()` helper. | No body-level change once helper is fixed. | ✅ No change to body. |

### Helpers in `DomeSequences.h`

| Helper | Lines | Current | Proposed | Decision ✅ |
|--------|-------|---------|----------|--------------|
| `domeRandomPanels()` | ~134–167 | `lowerPanels[] = {DP1..DP6, DPA, DPB}` (includes PP5 as lower); `piePanels[] = {DPP1..DPP4}` (skips PP3, PP5). | Use prerequisite P4's arrays: `ringPanels[]` (7) and `piePanels[]` (6). Caller signature stays the same (`numLower`, `numPie`); the underlying picks are now correct. Rename the parameter `numLower` → `numRing` for clarity. | ✅ Rewrite. |
| Local `allPanels[]` inside `domeScream` | 538 | `{DPP1..DPP4, DP1..DP6, DPA, DPB}` — 12 entries, skips PP3 | Delete the local literal; reference the shared file-scope `allPanels[]` from prerequisite P4. | ✅ Rewrite. |

### `:SE*` sequences in `MarcduinoSequence.h`

Most `:SE*` handlers are mask-driven (`ALL_DOME_PANELS_MASK`) and inherently
correct — they target every slot carrying any panel type bit. Only the ones
with a narrower mask need attention:

| Sequence | Lines | Current mask | Proposed mask | Decision ✅ |
|----------|-------|--------------|---------------|--------------|
| `:SE12 TopPanelsShowcase` | 224–229 | `PIE_PANEL` (excludes PP3 because TOP_PIE_PANEL bit) | `PIE_PANEL \| TOP_PIE_PANEL` — the case-14 "all 6 pies" mask. | ✅ Change mask. Prerequisite P5. |
| `:SE16 PanelWiggleSequence` | 271–278 | `ALL_DOME_PANELS_MASK` | Unchanged (already covers all 13). | ✅ No change. |
| `YodaClearMind` (`$720`) | 441–449 | `PANEL_GROUP_6` (= individual P11) | `PANEL_P11`. Already covered in main task plan step 9. | ✅ Covered by main task plan. |
| All other `:SE*` | various | `ALL_DOME_PANELS_MASK` or pure logic/holo | Unchanged — mask already covers all 13 slots, including PP3 (via TOP_PIE_PANEL) and PP5 (via PIE_PANEL post-ADR-0008). | ✅ No change. |

## Decisions that need your input

These are the choreography-design calls that the audit *can't* answer from code alone:

### Q-1 — Pie panel ordering ✅ DECIDED (2026-05-26)

**Decision:** identity-sequential `PP1 → PP2 → PP3 → PP4 → PP5 → PP6` as the
canonical "all pies" ordering. Traces a smooth angular sweep around the dome
(counter-clockwise from below — PP1 at bottom-front, advancing right-then-up,
across the top, down the left, back to start). Used by `domeOpenClosePies`,
`domeBloom`, `domeOpenCloseAll`, `domeFlutter`, `domeResetAll`.

Sequences with intentional non-canonical orders (e.g. `domeCantina`'s
syncopation) are called out per-sequence rather than overridden — see
decision Q-4 for cantina specifics.

### Q-2 — `domeRockMarch` single-panel touch ✅ DECIDED (2026-05-26)

**Decision:** rewrite as a full ring-panel march. Step through `ringPanels[]`
(P1 → P2 → P3 → P4 → P7 → P11 → P13) in rock-beat timing — the `DM:ROCKMARCH`
name implies a marching cadence, and the single-panel-touch in the current code
reads as a port stub. Pies stay out (this is a ring-panel march, not a full
dome event); if a pie variant is wanted later it earns its own DM:* command.

### Q-3 — `domeHelloThere` single-panel wave ✅ DECIDED (2026-05-26)

**Decision:** intentional. Single panel "waves hello" — keep P1 as the wave
panel (renamed to `D_P1`). Add a short inline comment in the function so a
future reader doesn't mistake it for a stub: `// Wave P1 only — intentional
single-panel greeting gesture, not a port stub.`

### Q-4 — PP3 placement in `domeCantina` ✅ DECIDED (2026-05-26)

**Decision:** PP3 pairs with PP4 on the top-pie beat. Geometric reasoning —
PP3 sits adjacent to PP4 (top-right of dome top) and reads as a natural "top
wedge" pair. Wherever `domeCantina` currently moves `D_PP4` (was `DPP3` in old
naming), the new code moves `{D_PP4, D_PP3}` as a unit. Preserves the 130 BPM
count exactly; no beat-position rebalance needed.

PP5 is already addressed by Q-1's canonical pie ordering (`PP1→…→PP6`) and
the per-sequence decisions below — it joins the cantina patterns at its
identity-sequential position alongside the other pies.

### Q-5 — Helper array naming ✅ DECIDED (2026-05-26)

**Decision:** `ringPanels[]`, `piePanels[]`, `allPanels[]`. Recorded in
prerequisite P4 above and ADR 0009.

### Q-6 — ADR 0009 worth writing? ✅ DECIDED (2026-05-26)

**Decision:** yes. Written at `docs/adr/0009-dome-sequences-identity-slot-constants.md`.

## Verification gate

Firmware ships when the **Mandatory** items pass on a standard MK4 dome.
Items in the **Recommended** block confirm choreography intent for builders
who wire PP3 and/or PP5 — they are valuable but not ship blockers, because
the firmware-layer safety analysis above proves the sequences cannot grind
or hang on the missing slots.

### Mandatory (ship blockers — standard MK4, PP3 and PP5 inactive)

The MK4 safety section above proves these are structurally guaranteed; this
checklist confirms an implementation actually behaves as the analysis predicts.

1. Every `DM:*` sequence runs to completion without:
   - servo grind, jitter, or audible distress on any wired panel,
   - hang or busy-wait when the sequence reaches a PP3 or PP5 call,
   - I2C errors or boot crash,
   - visible regression on the 11 wired panels (P1–P4, P7, P11, P13, PP1,
     PP2, PP4, PP6).
2. Every `:SE*` panel sequence (`:SE01`–`:SE56`, excluding the logic/holo-only
   ones) runs to completion with the same conditions as item 1.
3. The new `:OP*` and `:OPP*` individual commands open the correct *physical*
   panel:
   - `:OP01`–`:OP04` open P1–P4.
   - `:OP07` opens P7. `:OP11` opens P11. `:OP13` opens P13.
   - `:OP08`–`:OP10` and `:OP12` open PP1, PP2, PP4, PP6 (community aliases).
   - `:OPP1`, `:OPP2`, `:OPP4`, `:OPP6` open PP1, PP2, PP4, PP6 (fork
     namespace, same physical panels as the community aliases).
   - `:OPP3` and `:OPP5` are silent no-ops on MK4 (confirms the inactive-slot
     guard path is hit, not an error).
4. `YodaClearMind` (`$720`) opens P11 at the cue point and closes it after the
   choreography window.
5. `:SE02 / SeqPanelWave` over `ALL_DOME_PANELS_MASK` is visually equivalent
   to pre-fix behaviour on MK4 (the existing slot-7 / slot-12 pauses remain
   — pre-existing cosmetic, not a regression introduced by this fix).
6. Per-mask `schedulePanelRelease()` correctly releases PWM for participating
   panels at the end of each sequence. PP3 and PP5 slots are filtered out by
   the `(group & mask) != 0` check (verified by absence of grinding on the
   slot-7/slot-12 channels — which is automatic since no panel is attached).

### Recommended (post-ship, when a fully-wired dome is available)

These confirm the audit's choreography decisions for PP3 and PP5. Verifiable
by any builder who wires either slot after firmware ships. Any of these
failing is a follow-up issue, not a release rollback.

7. `:OPP3` and `:OPP5` individually open the wired panels.
8. `:OP14` (case-14 "all pie") drives all 6 pies — including PP3 and PP5.
9. `domeBloom` (`DM:BLOOM`) blooms all 6 pies; PP3 (top centre) participates.
10. `domeCantina` (`DM:CANTINA`) — PP3 fires on the same beat as PP4 (top
    wedge pair), preserving the 130 BPM cadence; PP5 fires in identity-sequential
    position alongside the other pies.
11. `domeOpenClosePies`, `domeOpenCloseAll`, `domeFlutter`, `domeResetAll`,
    `domeScream`, `domeRandom`, `domeOverload` — PP3 and PP5 participate per
    the audit's per-sequence intent.
12. `domeOpenCloseLow` (`DM:LOW`) — confirms PP5 does NOT move (the explicit
    "drop PP5 from this sequence" fix). Sequence stays ring-panel-only.
13. `:SE12 TopPanelsShowcase` — PP3 participates (was excluded pre-fix because
    of `PIE_PANEL`-only mask).

### Out of scope (deferred)

These are tracked elsewhere and not gating items:

- ServoStep frame slot-order vs. physical-position-order (`SeqPanelWave`'s
  cosmetic ordering on a fully-wired dome) — surfaces only after item 8/9
  reveals whether it actually looks wrong with PP3/PP5 active. Pre-existing.
- Individual `:SE*` choreography rebalancing — the audit fixes coverage, not
  aesthetic timing decisions on existing sequences that already cover all 13
  slots via mask.
- Re-tuning sequence-by-sequence speeds for the new larger panel set (may
  need slight wait-time bumps in `domeFlutter` etc. so PP3/PP5 inclusion
  doesn't compress visible motion on a fully-wired dome). Decide after the
  Recommended verification reveals whether timing actually drifts.
