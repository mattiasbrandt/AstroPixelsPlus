# Task: Fix Panel Command Numbering Deviation

## Related artifacts

Use this index to navigate cold — slice agents should skim each one before
claiming work.

**Decision records (ADRs):**
- [`docs/adr/0003-panel-command-routing-hardcoded-switch.md`](../docs/adr/0003-panel-command-routing-hardcoded-switch.md) — amended 2026-05-25: routing invariant is address-bit, not slot-ordinal.
- [`docs/adr/0006-panel-address-bit-rename.md`](../docs/adr/0006-panel-address-bit-rename.md) — `PANEL_GROUP_N` → `PANEL_P*` / `PANEL_PP*` rationale.
- [`docs/adr/0007-pie-panel-opp-command-namespace.md`](../docs/adr/0007-pie-panel-opp-command-namespace.md) — `:OPP*` / `:CLP*` / `:OFP*` fork namespace.
- [`docs/adr/0008-pp5-reclassified-as-pie-panel.md`](../docs/adr/0008-pp5-reclassified-as-pie-panel.md) — PP5: `MINI_PANEL` → `PIE_PANEL`.
- [`docs/adr/0009-dome-sequences-identity-slot-constants.md`](../docs/adr/0009-dome-sequences-identity-slot-constants.md) — `DomeSequences.h` `D_P*` / `D_PP*` constants.

**Companion task & spec:**
- [`tasks/sequence-audit.md`](./sequence-audit.md) — per-sequence decisions for Slices D + E; MK4 safety analysis; verification gate split.

**Domain glossary updates:**
- [`CONTEXT.md`](../CONTEXT.md) — "Servo slot", "Panel address bit", "Unserviced panel", and the new "Panel command verbs" sections updated 2026-05-26.

**Verification & gating:**
- Hardware gate split (Mandatory on standard MK4 vs Recommended on fully-wired) lives in [`sequence-audit.md`](./sequence-audit.md#verification-gate) and is mirrored in this plan's "Status — slice tracker" section.

---

## Background / Problem Statement

During 2026-05-25 hardware testing, the body agent issued `:OP01`–`:OP07` to open panels 1–7
individually. The result was wrong: **P2 and P7 did not open; P11 and P13 opened instead.**

Root cause is a systematic deviation from the Marcduino standard:

> **Marcduino convention:** `:OPnn` controls physical panel number `nn`.  
> **Current firmware:** `:OPnn` controls the panel in `servoSettings[]` slot `nn-1`, which is NOT
> the physical panel number for slots 4–6 (P7, P11, P13).

### Why it happened

`servoSettings[]` assigns `PANEL_GROUP_N` by **array slot index**, not by physical panel number.
Slots 4, 5, 6 hold P7, P11, P13 but were given `PANEL_GROUP_5`, `PANEL_GROUP_6`, `PANEL_GROUP_7`
respectively. `panelTargetToMask()` maps numeric target → GROUP_N, so `:OP05`→GROUP_5→P7,
`:OP06`→GROUP_6→P11, `:OP07`→GROUP_7→P13.

Additionally, `:OP11` and `:OP12` were repurposed as **group shortcuts** (PIE_PANEL and
DOME_PANELS_MASK), conflicting with individual P11 and PP6 panel commands. No `:OP13` existed.

---

## Primary Goal

The most important goal is to preserve modern MarcDuino V3-style panel compatibility and keep the
MarcDuino command grammar compatible with other controller projects and solutions.

This plan intentionally targets the modern 13-panel MarcDuino model: individual panel commands use
`:OP01`-`:OP13` / `:CL01`-`:CL13` / `:OF01`-`:OF13`, while group shortcuts move out of the
individual panel-number range. Existing sequence behavior, common operator workflows, and
ReelTwo/AstroPixelsPlus command transport should remain stable unless a change is explicitly
called out as a documented V3-alignment migration.

This fix separates two concerns:

1. **Compatibility correction:** standard Marcduino-style commands such as `:OPnn`, `:CLnn`,
   and `:OFnn` address the physical panel number implied by `nn`.
2. **Fork enrichment:** where this fork has additional panels or useful grouped behaviors, add
   explicit commands without stealing or redefining standard individual panel command numbers.

Every servoed panel gets a proper individual command. Added commands are intentional, documented,
and usable from serial, REST/WebSocket, web UI, and sequences. The web interface stays aligned
with the firmware mapping in the same iteration.

---

## Compatibility Research Findings

Research pass 2026-05-26:

- **Classic MarcDuino v1.6 / R2 Touch:** panel commands use `:CCxx\r`; `:OP01`-`:OP10`
  are individual panels, `:OP00` opens all panels, and `:OP11` / `:OP12` are documented
  as "top panels" / "bottom panels" group shortcuts. The same numeric range applies to
  `:CLxx`, while `:SExx` launches built-in panel sequences such as wave, scream, cantina,
  Leia, quiet reset, and awake reset.
- **MarcDuino V3:** expanded the panel command range to `:OP01`-`:OP13` /
  `:CL01`-`:CL13` for individual panels, then moved the top/bottom group shortcuts to
  `:OP14` and `:OP15`. This is the closest community precedent for a 13-servo panel
  firmware.
- **ReelTwo / AstroPixelsPlus:** the public AstroPixelsPlus README documents a
  Marcduino-compatible serial receiver and generic `:OP`, `:CL`, and `:OF` panel
  command families, but does not document a richer P/PP namespace. Reeltwo's servo
  sequencer documentation describes sequence target bitmaps as servo-order based,
  which is separate from Marcduino command numbering.

Sources checked:
- CuriousMarc MarcDuino Command Reference:
  https://www.curiousmarc.com/r2-d2/marcduino-system/marcduino-software-reference/marcduino-command-reference
- MarcDuino V3 firmware command summary / source links:
  https://www.printed-droid.com/kb/marcduino-v3-firmware-commands/
- Neil Hutchison MarcDuinoMain `main.c` command comments:
  https://github.com/nhutchison/MarcDuinoMain/blob/master/main.c
- ReelTwo documentation index, servo sequencer description:
  https://reeltwo.github.io/Reeltwo/html/modules.html
- Upstream AstroPixelsPlus README:
  https://github.com/reeltwo/AstroPixelsPlus

**Compatibility conclusion:** this task is choosing MarcDuino V3-style 13-panel
addressing as the community-compatible end state. That means `:OP11` / `:CL11` /
`:OF11` and `:OP12` / `:CL12` / `:OF12` become individual panel aliases, not the
classic v1.6 top/bottom group shortcuts. The plan must treat this as an intentional
V3-alignment decision and document the break for any operator still using v1.6/R2 Touch
macros that expect `11` / `12` to mean groups.

**Do not claim "no compatibility break" for v1.6 saved macros.** The compatible path is:
keep the standard command grammar, align 13-panel numbering with MarcDuino V3, keep all
existing common sequences available, and document the moved group shortcut numbers clearly.

---

## Compatibility / Enrichment Requirements

- Keep Marcduino and ReelTwo command structure intact wherever there is an established meaning.
- Do not repurpose individual panel command numbers for group shortcuts.
- All servoed panels (P1, P2, P3, P4, P7, P11, P13, PP1, PP2, PP4, PP6) get working individual
  open/close/off commands.
- Unserviced panels (PP3, PP5) get commands that are no-ops by default but activate automatically
  if a builder wires a servo — no firmware change required.
- **Fork-specific pie panel namespace:** add `P` to every panel command prefix for pie panels:
  `:OPP1`–`:OPP6` (open), `:CLP1`–`:CLP6` (close), `:OFP1`–`:OFP6` (flutter), and matching
  prefixes for any other per-panel command type (calibration, etc.). This gives builders a clean,
  readable command namespace that does not require knowing the community-standard alias numbers.
  Community-standard aliases (`:OP08`, `:OP09`, `:OP10`, `:OP12`) are retained as working
  equivalents — both addressing schemes target the same servo.
- Group shortcut commands use MarcDuino V3-compatible non-individual numbers. `:OP14` /
  `:CL14` / `:OF14` and `:OP15` / `:CL15` / `:OF15` must be reviewed and named as
  **top panels** and **bottom panels** unless a source-backed ADR deliberately chooses
  different semantics. Do not label these only as "all pie" / "all ring" without proving
  that matches the community expectation for this build.
- Keep every relevant web page aligned with the command mapping. Button labels, command payloads,
  SVG highlights, grouped controls, and displayed panel numbering must describe the corrected
  physical panel behavior.

---

## Resolved Design Decision: Rename `PANEL_GROUP_N` constants

The `PANEL_GROUP_N` naming was the root cause of the bug. Sequential numbers with no panel
identity implied that slot index, group number, and command number were interchangeable — they
are not.

**Decision:** rename all `PANEL_GROUP_N` defines to builder-facing panel identity names:
`PANEL_P1`…`PANEL_P13` for ring panels, `PANEL_PP1`…`PANEL_PP6` for pie panels.
Multi-panel type shortcuts (`SMALL_PANEL`, `PIE_PANEL`, `DOME_PANELS_MASK`, etc.) keep their
existing names — they describe type/group behaviours, not individual panel addresses.

See ADR 0006 for rationale.

### Bit assignments — post-rename

| Constant     | Bit      | Slot | Panel | Notes                              |
|--------------|----------|------|-------|------------------------------------|
| `PANEL_P1`   | 1L << 14 | 0    | P1    | renamed from PANEL_GROUP_1         |
| `PANEL_P2`   | 1L << 15 | 1    | P2    | renamed from PANEL_GROUP_2         |
| `PANEL_P3`   | 1L << 16 | 2    | P3    | renamed from PANEL_GROUP_3         |
| `PANEL_P4`   | 1L << 17 | 3    | P4    | renamed from PANEL_GROUP_4         |
| `PANEL_P7`   | 1L << 18 | 4    | P7    | renamed from PANEL_GROUP_5         |
| `PANEL_P11`  | 1L << 19 | 5    | P11   | renamed from PANEL_GROUP_6         |
| `PANEL_P13`  | 1L << 20 | 6    | P13   | renamed from PANEL_GROUP_7         |
| `PANEL_PP1`  | 1L << 21 | 8    | PP1   | renamed from PANEL_GROUP_8         |
| `PANEL_PP2`  | 1L << 22 | 9    | PP2   | renamed from PANEL_GROUP_9         |
| `PANEL_PP4`  | 1L << 23 | 10   | PP4   | renamed from PANEL_GROUP_10        |
| `PANEL_PP5`  | 1L << 24 | 7    | PP5   | **new** — unserviced on MK4        |
| `PANEL_PP6`  | 1L << 25 | 11   | PP6   | **new** — was group-only           |
| `PANEL_PP3`  | 1L << 26 | 12   | PP3   | **new** — unserviced on MK4        |

Bit width verified: `uint32_t` is used throughout `ServoDispatch` and `ServoSequencer` —
no narrowing anywhere. Bits up to 31 are safe. `ServoStep` sequence frames use slot-index
bitmaps (independent of group bits) and are unaffected by this rename.

---

## Pie Panel Command Mapping

The community convention assigns pie panel individual commands to `:OP` slots corresponding
to fixed physical panels (electronics bays with no servo). This is consistent within the
community but non-obvious — `:OP08` opens PP1, not anything at physical position P8.

The fork-specific `:OPP` namespace makes this readable without breaking compatibility.

| Panel | Community standard | Fork alias   | Slot |
|-------|--------------------|--------------|------|
| PP1   | `:OP08`            | `:OPP1`      | 8    |
| PP2   | `:OP09`            | `:OPP2`      | 9    |
| PP3   | *(no-op by default)* | `:OPP3`    | 12   |
| PP4   | `:OP10`            | `:OPP4`      | 10   |
| PP5   | *(no-op by default)* | `:OPP5`    | 7    |
| PP6   | `:OP12`            | `:OPP6`      | 11   |

See ADR 0007 for rationale.

---

## Current State (Broken Mapping)

### `panelTargetToMask()` — `MarcduinoPanel.h` ~lines 23–45

| Case | Current mask       | Correct mask        |
|------|-------------------|---------------------|
| 5    | PANEL_GROUP_5 (→P7) | `false` / no-op (P5 fixed) |
| 6    | PANEL_GROUP_6 (→P11)| `false` / no-op (P6 fixed) |
| 7    | PANEL_GROUP_7 (→P13)| `PANEL_P7`          |
| 11   | PIE_PANEL ❌        | `PANEL_P11`         |
| 12   | DOME_PANELS_MASK ❌ | `PANEL_PP6`         |
| 13   | TOP_PIE_PANEL ❌    | `PANEL_P13`         |

Cases 8, 9, 10 are correct (PP1, PP2, PP4) — only renamed: PANEL_GROUP_8/9/10 → PANEL_PP1/PP2/PP4.
Cases 14 and 15 (existing group shortcuts) are unchanged.

---

## Required Changes

### 1. Add `PANEL_P*` / `PANEL_PP*` defines (additive) — `AstroPixelsPlus.ino` ~line 355

**Slice A: additive.** Add the new builder-facing constants *alongside* the
existing `PANEL_GROUP_1`..`PANEL_GROUP_10` defines. Both old and new resolve
to the same bit values for shared panels. The deprecated `PANEL_GROUP_N`
constants are removed later by **Slice J**, after all firmware references to
the old names have been switched (Slices B, C, D, E).

```cpp
// Individual panel address bits — named by builder-facing panel identity.
// Bits are preserved from the old PANEL_GROUP_N values where possible to avoid
// changing the effective bitmask patterns for panels 1-4, 7, 11, 13, PP1, PP2, PP4.
#define PANEL_P1   (1L << 14)  // ring panel P1   (slot 0)
#define PANEL_P2   (1L << 15)  // ring panel P2   (slot 1)
#define PANEL_P3   (1L << 16)  // ring panel P3   (slot 2)
#define PANEL_P4   (1L << 17)  // ring panel P4   (slot 3)
#define PANEL_P7   (1L << 18)  // ring panel P7   (slot 4)
#define PANEL_P11  (1L << 19)  // ring panel P11  (slot 5)
#define PANEL_P13  (1L << 20)  // ring panel P13  (slot 6)
#define PANEL_PP1  (1L << 21)  // pie panel  PP1  (slot 8)
#define PANEL_PP2  (1L << 22)  // pie panel  PP2  (slot 9)
#define PANEL_PP4  (1L << 23)  // pie panel  PP4  (slot 10)
#define PANEL_PP5  (1L << 24)  // pie panel  PP5  (slot 7,  unserviced MK4 default)
#define PANEL_PP6  (1L << 25)  // pie panel  PP6  (slot 11)
#define PANEL_PP3  (1L << 26)  // pie panel  PP3  (slot 12, unserviced MK4 default)

// Keep `PANEL_GROUP_1`..`PANEL_GROUP_10` in place until Slice J — every line
// gets a `// deprecated — remove in Slice J once nothing references` comment
// so the rationale is visible at the define site.
```

### 2. Update `servoSettings[]` — `AstroPixelsPlus.ino` ~line 409

Rename group constants in all panel slots, and add individual address bits to the three slots
that previously had none:

```cpp
{1,  800, 2200, PANEL_P1  | SMALL_PANEL},  /* slot 0:  P1  */
{2,  800, 2200, PANEL_P2  | SMALL_PANEL},  /* slot 1:  P2  */
{3,  800, 2200, PANEL_P3  | SMALL_PANEL},  /* slot 2:  P3  */
{4,  800, 2200, PANEL_P4  | SMALL_PANEL},  /* slot 3:  P4  */
{5,  800, 2200, PANEL_P7  | SMALL_PANEL},  /* slot 4:  P7  :OP07 */
{6,  800, 2200, PANEL_P11 | SMALL_PANEL},  /* slot 5:  P11 :OP11 */
{7,  800, 2200, PANEL_P13 | SMALL_PANEL},  /* slot 6:  P13 :OP13 */
{8,  800, 2200, PANEL_PP5 | PIE_PANEL},    /* slot 7:  PP5 :OPP5 (unserviced) — see ADR 0008 */
{9,  800, 2200, PANEL_PP1 | PIE_PANEL},    /* slot 8:  PP1 :OP08 / :OPP1 */
{10, 800, 2200, PANEL_PP2 | PIE_PANEL},    /* slot 9:  PP2 :OP09 / :OPP2 */
{11, 800, 2200, PANEL_PP4 | PIE_PANEL},    /* slot 10: PP4 :OP10 / :OPP4 */
{12, 800, 2200, PANEL_PP6 | PIE_PANEL},    /* slot 11: PP6 :OP12 / :OPP6 */
{13, 800, 2200, PANEL_PP3 | TOP_PIE_PANEL},/* slot 12: PP3 :OPP3 (unserviced) */
```

Also update the inline comment block at ~line 390 so the `:OPnn` column reflects the
corrected command numbers (P7→:OP07, P11→:OP11, P13→:OP13, PP6→:OP12).

### 3. Fix `panelTargetToMask()` — `MarcduinoPanel.h` ~line 23

This helper only covers dynamic/numeric target paths. It does **not** service the
literal `MARCDUINO_ACTION` registrations for commands such as `:OP05`, `:CL05`,
or `:OF05`; those handlers must be corrected separately in step 7 / Slice B.

```cpp
case 5:  return false;              // P5 — fixed panel, no servo
case 6:  return false;             // P6 — fixed panel, no servo
case 7:  mask = PANEL_P7;  return true;
case 8:  mask = PANEL_PP1; return true;
case 9:  mask = PANEL_PP2; return true;
case 10: mask = PANEL_PP4; return true;
case 11: mask = PANEL_P11; return true;   // individual P11 (was PIE_PANEL group shortcut)
case 12: mask = PANEL_PP6; return true;   // individual PP6 (was DOME_PANELS_MASK)
case 13: mask = PANEL_P13; return true;   // individual P13 (was TOP_PIE_PANEL)
// case 14: mask = ...;                    // MarcDuino V3 top-panels shortcut
// case 15: mask = ...;                    // MarcDuino V3 bottom-panels shortcut
// (cases 14, 15: source-align exact masks before implementation; do not call them
// "all pie" / "all ring" unless that is the documented outcome of the ADR review)
```

### 4. Fix `:OP11` / `:CL11` handlers — `MarcduinoPanel.h` ~line 668

Rename from OpenTopPanels/CloseTopPanels → OpenPanel11/ClosePanel11, targeting `PANEL_P11`.

### 5. Fix `:OP12` / `:CL12` handlers — `MarcduinoPanel.h` ~line 675

Rename from OpenBottomPanels/CloseBottomPanels → OpenPanelPP6/ClosePanelPP6, targeting
`PANEL_PP6`. This repurposes the `:OP12` slot from a group shortcut to an individual PP6
command, consistent with the community convention of using fixed-panel command slots for
pie panels.

### 6. Add `:OP13` / `:CL13` handlers — `MarcduinoPanel.h`

```cpp
MARCDUINO_ACTION(OpenPanel13,    :OP13, ({ /* open PANEL_P13 */ }))
MARCDUINO_ACTION(ClosePanel13,   :CL13, ({ /* close PANEL_P13 */ }))
MARCDUINO_ACTION(FlutterPanel13, :OF13, ({ /* flutter PANEL_P13 */ }))
```

### 7. Retarget literal `:OPnn` / `:CLnn` / `:OFnn` handlers — `MarcduinoPanel.h`

The standard panel commands are registered as literal `MARCDUINO_ACTION` handlers.
Do not rely on `panelTargetToMask()` alone for these commands.

- `:OP05` / `:CL05` / `:OF05`: rename to Panel5 handlers and make them safe no-ops
  because P5 is a fixed panel with no servo on this build. Keep the command
  recognized for Marcduino compatibility, but do not move any wired panel.
- `:OP06` / `:CL06` / `:OF06`: rename to Panel6 handlers and make them safe no-ops
  because P6 is a fixed panel with no servo on this build. Keep the command
  recognized for Marcduino compatibility, but do not move any wired panel.
- `:OP07` / `:CL07` / `:OF07`: rename to Panel7 handlers and retarget every open,
  close, flutter, release, and cancel path to `PANEL_P7`.
- `:OP11` / `:CL11` / `:OF11`: rename to Panel11 handlers and retarget every open,
  close, flutter, release, and cancel path to `PANEL_P11`.
- `:OP12` / `:CL12` / `:OF12`: rename to PanelPP6 handlers and retarget every open,
  close, flutter, release, and cancel path to `PANEL_PP6`.
- `:OP13` / `:CL13` / `:OF13`: add new Panel13 handlers targeting `PANEL_P13`.

This is the core regression fix for the observed `:OP05`–`:OP07` behavior. If
the literal 05/06/07 handler triples still reference deprecated `PANEL_GROUP_5`,
`PANEL_GROUP_6`, or `PANEL_GROUP_7`, the numbering bug is not fixed.

- `:OF11` **exists** (line ~835, `FlutterTopPanels` targeting `PIE_PANEL`). Repurpose
  to individual P11: rename to `FlutterPanel11`, target `PANEL_P11`. (Mirrors the
  `:OP11`/`:CL11` repurposing in step 4.)
- `:OF13` **does not exist**. Add fresh `FlutterPanel13` handler targeting `PANEL_P13`
  (alongside the `:OP13` / `:CL13` additions in step 6).

### 8. Add fork-specific pie panel namespace — `MarcduinoPanel.h`

Register **18 literal-prefix MARCDUINO_ACTION handlers**, one per command — same shape
as the existing `:OPnn` family. No new parser helper, no dynamic prefix matching, no
changes to `processMarcduinoCommandWithSource()` web ingress sync path required.

```cpp
MARCDUINO_ACTION(OpenPiePanel1,  :OPP1, ({
    cancelPanelRelease(PANEL_PP1);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP1);
}))
MARCDUINO_ACTION(ClosePiePanel1, :CLP1, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP1);
    schedulePanelRelease(PANEL_PP1);
}))
MARCDUINO_ACTION(FlutterPiePanel1, :OFP1, ({
    cancelPanelRelease(PANEL_PP1);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP1, 10, 50);
}))
// ... :OPP2/:CLP2/:OFP2 through :OPP6/:CLP6/:OFP6 — 18 handlers total
```

Calibration commands (`:MV`, `#SO`, `#SC`, `#SW`) keep their existing dynamic-prefix
form and target pie panels via the two-digit community-alias numbers (`08`, `09`, `10`,
`12`). A separate `:MVP1` / `#SOP1` calibration namespace is **out of scope** for this
task — revisit if builder demand justifies the additional 24 handlers + parser work.

PP3 and PP5 handlers are valid — they are no-ops unless `panelConfigLoad()` has
activated the slot (builder wired a servo). No special-casing needed; the mask finds
no active servo.

See ADR 0007 for rationale (namespace) and ADR 0008 (PP5 reclassification).

### 9. Fix `YodaClearMind` — `MarcduinoSequence.h` lines 441, 442, 448, 449

Four lines reference `PANEL_GROUP_6` (the only individual panel group mask in the entire file).
Rename to `PANEL_P11`:

```cpp
// line 441: cancelPanelRelease(PANEL_GROUP_6) → cancelPanelRelease(PANEL_P11)
// line 442: DO_SEQUENCE(SeqPanelAllOpen, PANEL_GROUP_6) → DO_SEQUENCE(SeqPanelAllOpen, PANEL_P11)
// line 448: DO_SEQUENCE(SeqPanelAllClose, PANEL_GROUP_6) → DO_SEQUENCE(SeqPanelAllClose, PANEL_P11)
// line 449: schedulePanelRelease(PANEL_GROUP_6) → schedulePanelRelease(PANEL_P11)
```

### 9b. Full sequence audit — `DomeSequences.h` + `MarcduinoSequence.h`

**Status:** higher bar set 2026-05-26 (user directive: "all sequences must be 100%
and perfect quality, no easy or quick fixes"). Detailed per-sequence audit lives in
[`sequence-audit.md`](./sequence-audit.md). Summary of work folded in:

- **Cross-cutting prerequisites (P1–P5):** identity-based slot constants in
  `DomeSequences.h` (rename `DPA`/`DPB`/`DPP3`-means-PP4 → `D_P*`/`D_PP*`);
  add `D_PP3` (slot 12) and `D_PP5` (slot 7); replace `lowerPanels[]` /
  `allPanels[]` arrays with semantically-correct versions; broaden
  `:SE12 TopPanelsShowcase` mask to `PIE_PANEL | TOP_PIE_PANEL`.
- **Per-sequence rewrites (17 DM:* sequences):** each `domeOpenClose*`,
  `domeFlutter`, `domeBloom`, `domeScream`, `domeResetAll`, `domeCantina`,
  etc. updated so PP3 and PP5 participate where the audit table specifies —
  no piggy-back calls, no runtime guards, no removed calls. The pure
  logic/holo/sound sequences (`domeHeart`, `domeAlarm`, `domeDisco`,
  `domeVader`, `domeLeiaMode`) are unaffected.
- **Helper rewrites:** `domeRandomPanels()` uses the corrected arrays;
  inline `allPanels[]` inside `domeScream` replaced with the shared array.
- **`MarcduinoSequence.h`:** YodaClearMind (this step) plus `:SE12` mask
  change (in prerequisite P5).
- **Proposed ADR 0009:** records the identity-based slot-constant naming
  rule for `DomeSequences.h` (mirrors ADR 0006's rationale for
  `PANEL_GROUP_N` → `PANEL_P*`).

**Hardware verification (revised 2026-05-26):** the firmware-layer safety
analysis in `sequence-audit.md` (MK4 safety section) proves
`ServoDispatchPCA9685::_moveServoToPulse` already short-circuits inactive
slots (`channel == 0`), and `isActive()` returns false for them — so any
`domeMove(D_PP3, …)` / `domeMove(D_PP5, …)` is a structural no-op on
standard MK4. **Ship gate runs on standard MK4** (PP3/PP5 inactive); the
ship criteria are: every sequence completes without grind/hang/crash, the
11 wired panels behave as designed, individual `:OP*` and `:OPP*` commands
hit the correct physical panel. **A fully-wired dome verification is
post-ship Recommended** to confirm choreography intent for PP3/PP5
participation — verifiable by any builder who later wires either slot.
See the Mandatory / Recommended split in `sequence-audit.md`.

`ServoStep` sequence frames (SeqPanelWave etc.) use slot-index bitmaps, not group
bits — they automatically pick up P11/P13/PP6 via slots 5/6/11 today. PP3 (slot 12)
and PP5 (slot 7) participate when wired. Visual order may need a separate frame
audit if the slot-order wave looks wrong on a fully-wired dome (deferred to
post-verification follow-up).

All other group mask references in MarcduinoSequence.h use `ALL_DOME_PANELS_MASK` or
`PIE_PANEL` — these are type bits, unaffected by the rename (and now correctly
include PP5 via ADR 0008).

Note: `ServoStep` sequence frames (SeqPanelWave etc.) use slot-index bitmaps, not group bits.
P7/P11/P13 are in slots 4/5/6 and participate correctly in library sequences already. Verify
visually on hardware that wave order looks sensible.

`isPanelServoByGroup()` (MarcduinoPanel.h ~line 62 and the identical copy in
AstroPixelsPlus.ino ~line 1716 and AsyncWebInterface.h ~line 1700) **requires no change**.
The mask `SMALL_PANEL | MEDIUM_PANEL | BIG_PANEL | PIE_PANEL | TOP_PIE_PANEL | MINI_PANEL`
correctly identifies every panel slot under the renamed taxonomy (PP5 matches via PIE_PANEL
post-reclassification; PP3 still matches via TOP_PIE_PANEL; MINI_PANEL stays in the mask
for upstream-ReelTwo compatibility — see ADR 0008).

#### `schedulePanelRelease()` / `cancelPanelRelease()` audit (complete)

Every call site touching an individual address bit lives in code that is already
rewritten by the rename. No additional call sites need attention:

- **MarcduinoPanel.h:** individual `cancelPanelRelease(PANEL_GROUP_N)` and
  `schedulePanelRelease(PANEL_GROUP_N)` literals inside `:OP0N` / `:CL0N` /
  `:OF0N` handlers are **not** automatically fixed by `panelTargetToMask()`.
  Slice B must update the literal handler triples directly, especially 05/06/07.
  The parameterised dynamic handlers (`:MV`, `#SO`, `:OF$`, etc.) take `group`
  as a variable from `panelTargetToMask()` and are covered by the helper update.
- **MarcduinoSequence.h:** the only individual mask literals are the four
  `PANEL_GROUP_6` lines in YodaClearMind (covered by step 9). All other release
  calls use `ALL_DOME_PANELS_MASK`, `PIE_PANEL`, or the no-arg form.
- **AstroPixelsPlus.ino:** function declarations (~line 652) and definitions
  (~line 1899) accept `uint32_t mask` — no literals to update.
- **No other files** call these functions outside `.pio/libdeps/`.

### 10. Update relevant web pages — `data/`

**Namespace convention (decided):** the panel SVG and operational buttons in the UI
fire **fork `:OPP*` commands for pie panels** (`:OPP1`–`:OPP6`, `:CLP1`–`:CLP6`,
`:OFP1`–`:OFP6`) and **post-rename `:OPnn` for ring panels** (`:OP01`–`:OP04`,
`:OP07`, `:OP11`, `:OP13`). One rule: every panel is addressed by its builder-
facing identity. Community aliases (`:OP08`–`:OP12`) continue to function via the
alias table but are not the primary UI commands.

#### File-by-file

**`data/panels.html`** (heavy):
- SVG IDs use the broken-mapping convention (`dp05` titled "P7", `dp11` titled
  "PP6", etc.). Rename to identity-based IDs: `p1`–`p4`, `p7`, `p11`, `p13`,
  `pp1`–`pp6`. Update every `onclick="domePanelClick('NN')"` to pass the panel
  identity (e.g. `'PP1'`, `'P7'`) instead of the command digit.
- Update the inline `domePanelClick()` implementation in this file. In the current
  code layout it lives in `data/panels.html`, not `data/app.js`. It must map
  identity strings to commands:
  `'P1'`–`'P4'`/`'P7'`/`'P11'`/`'P13'` → `:OP01`/`:OP02`/`:OP03`/`:OP04`/
  `:OP07`/`:OP11`/`:OP13`; `'PP1'`–`'PP6'` → `:OPP1`–`:OPP6`.
  Do not leave it concatenating `:OP` with the identity string, or SVG clicks
  will produce invalid commands such as `:OPPP1`.
- If a later cleanup intentionally extracts `domePanelClick()` into `data/app.js`,
  that extraction belongs to Slice F or to a coordinated F/G change where
  `panels.html` remains working in the same commit.
- Button tables: retarget `:OP05`/`:CL05`/`:OF05` → `:OP07`/`:CL07`/`:OF07`
  (P7), and add new rows for P11 (`:OP11`/`:CL11`/`:OF11`) and P13
  (`:OP13`/`:CL13`/`:OF13`). Drop the "Top Panels" / "Bottom Panels" group rows
  that used `:OP11`/`:OP12` — those numbers no longer mean groups.
- Replace pie-panel buttons that sent `:OP08`–`:OP12` with `:OPP1`–`:OPP6`. Add
  rows for PP3 and PP5 (no-op by default, become live when builder activates the
  slot via wiring config).
- Title/comment fixes: PP5 is labelled "fixed" — change to "unserviced (slot
  exists, no servo on standard MK4)". PP6 labelled "group only" — drop, PP6 is
  now individually addressable. PP3 stays "unserviced (hosts THP/HP3)".
- Add operator-facing group buttons for the non-individual shortcut numbers: `:OP00`
  (all), `:OP14` (top panels), `:OP15` (bottom panels). These replace the old
  classic-v1.6 `:OP11`/`:OP12` group buttons after the V3-alignment decision is
  documented. The exact top/bottom masks must be source-aligned before implementation.

**`data/app.js`** (light):
- `cmdSummary()` regexes (lines 205–214) match `:OP\d{2}` / `:CL\d{2}` /
  `:OF\d{2}` only. Add clauses for `:OPP\d` / `:CLP\d` / `:OFP\d` returning
  "Open/Close/Flutter pie panel n". Fix the "Open panel/group nn" label to
  resolve to the specific panel identity post-rename (or use a small lookup
  table keyed on command number).
- Do not assume `domePanelClick()` is in this file. In the current layout it is
  inline in `data/panels.html` and is owned by Slice F unless deliberately moved.

**`data/sequences.html`** (light):
- Line ~169 description for the Yoda sequence: replace "Opens panel group 6"
  with "Opens P11 (lower-left ring panel)". The wording "group 6" is the old
  PANEL_GROUP_6 nomenclature.

**`data/index.html`** (trivial):
- Command-input hint mentions `:OP01` — still valid post-rename, no change.

**`data/AGENTS.md`** (doc):
- Add `:OPP*` / `:CLP*` / `:OFP*` to the inventory of command families the UI
  understands.

#### Verification
- Click every SVG panel; confirm the correct physical panel moves and the
  highlight overlay tracks the right path.
- Confirm SVG IDs and onclick payloads no longer encode the broken
  command-number-as-slot-position assumption.
- Search `data/` once more after edits for any residual `dp0N` reference or
  `:OP11`/`:OP12` literal that means the old group shortcut.

### 11. Update fork documentation — `FORK_IMPROVEMENTS.md`

- Explain the panel command numbering fix in operator/user language.
- Document the `:OPP`/`:CLP`/`:OFP` pie panel namespace and its relationship to
  community-standard aliases.
- List moved group shortcut commands so operators can update saved macros.

---

## Work Slices (parallel-agent-grabbable)

The "Required Changes" sections above describe **what** changes; this section
groups them into **independently claimable slices** so multiple agents can work
in parallel. Each slice owns specific files — concurrent claims on the same
slice or overlapping file sets will merge-conflict.

### Slice DAG

```
              A (.ino — ADDITIVE foundation: adds PANEL_P*/PANEL_PP* alongside
              │           PANEL_GROUP_N; old constants kept so dependent files
              │           keep compiling)
              │
   ┌──────────┼──────────┬──────────┬──────────┬──────────┐
   ▼          ▼          ▼          ▼          ▼          ▼
   B          C          D          F          G          I
(MarcPanel)(MarcSeq)  (DomeSeq)  (panels    (app.js)  (FORK_IMPROVEMENTS)
                         │       .html)
                         ▼
                         E
                    (DomeSeq rewrites)
                         │
                         ▼
                         H  (sequences.html + AGENTS.md)
                       (after C lands)

   All of B/C/D/E/F (firmware refs only — F doesn't reference the constants)
   must land before Slice J starts:
                         ▼
                         J — Cleanup: remove now-unused PANEL_GROUP_N constants
                             (depends on B, C, D, E all landed)
```

After Slice A lands, **up to 5 agents can work concurrently** (B + C + D + F + G + I).
E waits for D (same file). H waits for C (depends on the YodaClearMind rename).
J is the final cleanup — runs only after every firmware reference to
`PANEL_GROUP_N` has been replaced.

### Slice A is additive — every intermediate state compiles

Slice A *adds* the new `PANEL_P*` / `PANEL_PP*` constants alongside the existing
`PANEL_GROUP_N` constants. Old names continue to resolve to the same bit values.
Slices B/C/D/E switch their references to the new names at their own pace.
**This means any combination of merged slices compiles and runs** — the
parallelism is real, not theoretical. Once every dependent slice has landed,
Slice J removes the now-orphaned `PANEL_GROUP_N` defines in one final cleanup.

### Slice table

| Slice | Title | Files owned | Depends on | Size | Status |
|-------|-------|-------------|------------|------|--------|
| **A** | Foundation: address bits (additive) + servoSettings + PP5 reclassification | `AstroPixelsPlus.ino` | — | S | ✅ Done |
| **B** | MarcduinoPanel: routing + handlers + `:OPP*` namespace | `MarcduinoPanel.h` | A | M | ✅ Done |
| **C** | MarcduinoSequence small fixes (YodaClearMind + `:SE12` mask) | `MarcduinoSequence.h` | A | XS | ✅ Done |
| **D** | DomeSequences foundation (constants + helper arrays + `D_PP3`) | `DomeSequences.h` | A | M | ✅ Done |
| **E** | DomeSequences per-sequence rewrites (11 sequences) | `DomeSequences.h` | D | L | ✅ Done |
| **F** | Web UI: `panels.html` SVG, inline click dispatch, button retargets | `data/panels.html` | A (spec) | M | ✅ Done |
| **G** | Web UI: `app.js` cmdSummary | `data/app.js` | A (spec) | S | ✅ Done |
| **H** | Web UI: `sequences.html` + AGENTS.md tidy | `data/sequences.html`, `data/AGENTS.md` | C | XS | ✅ Done |
| **I** | Operator docs | `FORK_IMPROVEMENTS.md` | A (spec) | M | ✅ Done |
| **J** | Cleanup: remove deprecated `PANEL_GROUP_N` constants | `AstroPixelsPlus.ino` | B, C, D, E all merged | XS | ✅ Done |

**Claim convention:** change `☐ Open` → `🔶 Claimed by <agent/handle>` when starting,
→ `🔍 In review (PR #N)` when PR opened, → `✅ Done (PR #N)` when landed.

---

### Slice A — Foundation: address bits (additive) + servoSettings + PP5 reclassification

**Files owned:** `AstroPixelsPlus.ino` (lines ~344 defines, ~390 comment block, ~409 servoSettings)
**Dependencies:** none — this is the firmware blocker for B, C, D, E
**Covers Required Changes:** §1 (additive constants — old `PANEL_GROUP_N` kept in place until Slice J), §2 (servoSettings update)
**Spec sources:** ADR 0006 (rename), ADR 0008 (PP5 reclassification)

**Scope (additive — every intermediate state compiles):**
- **Add** new `PANEL_P*` and `PANEL_PP*` constants per §1 *alongside* the existing
  `PANEL_GROUP_1`..`PANEL_GROUP_10` defines. Both old and new resolve to the
  same bit values for the panels they share (P1=P_GROUP_1, P7=P_GROUP_5, etc.).
- **Add** new `PANEL_PP3`, `PANEL_PP5`, `PANEL_PP6` constants (no old equivalent).
- Update `servoSettings[]` to use the new `PANEL_P*` / `PANEL_PP*` constants
  in the *individual address-bit* position. Slots 7 and 12 also gain entries
  per §2.
- Slot 7 (PP5): type bit `MINI_PANEL` → `PANEL_PP5 | PIE_PANEL` (ADR 0008).
- Slot 12 (PP3): add `{13, 800, 2200, PANEL_PP3 | TOP_PIE_PANEL}` entry.
- Update inline `:OPnn` comment block (~line 390) to show the corrected
  command-to-panel mapping.
- **Do NOT** delete `PANEL_GROUP_1`..`PANEL_GROUP_10` — they remain so that
  `MarcduinoPanel.h`, `MarcduinoSequence.h`, `DomeSequences.h` keep compiling
  until their slices switch references. Mark the old defines with a brief
  comment: `// deprecated — remove in Slice J once nothing references`.

**Acceptance:**
- `pio run -e astropixelsplus` succeeds (whole codebase compiles).
- Boot succeeds on hardware (panelConfigLoad applies cleanly).
- Existing commands (`:OP01`–`:OP04`) still drive the same physical panels
  (bit values preserved per ADR 0006 table; old constants still active).
- New constants `PANEL_P*`/`PANEL_PP*` are usable from any other translation unit.

**Out of scope:**
- Any change to `MarcduinoPanel.h`, `MarcduinoSequence.h`, `DomeSequences.h`,
  or `data/`. Those land in their own slices.
- Removal of `PANEL_GROUP_N` constants — that's Slice J, after all dependents land.

---

### Slice B — MarcduinoPanel: routing + handlers + `:OPP*` namespace

**Files owned:** `MarcduinoPanel.h` only
**Dependencies:** Slice A (needs `PANEL_P*` / `PANEL_PP*` constants to exist)
**Covers Required Changes:** §3 (panelTargetToMask), §4 (`:OP11`/`:CL11`), §5 (`:OP12`/`:CL12`), §6 (`:OP13`/`:CL13`), §7 (literal `:OP`/`:CL`/`:OF` handler triples), §8 (`:OPP*` namespace)
**Spec sources:** ADR 0007 (namespace), CONTEXT.md "Panel command verbs"

**Scope:**
- Fix `panelTargetToMask()` cases 5–7 and 11–13 per §3.
- Retarget the literal `:OP05`/`:CL05`/`:OF05`, `:OP06`/`:CL06`/`:OF06`,
  and `:OP07`/`:CL07`/`:OF07` handler triples per §7. `05` and `06` become
  recognized no-ops for fixed panels; `07` targets `PANEL_P7`.
- Rename `OpenTopPanels`/`CloseTopPanels`/`FlutterTopPanels` → `OpenPanel11`/`ClosePanel11`/`FlutterPanel11`, retarget to `PANEL_P11`.
- Rename `OpenBottomPanels`/`CloseBottomPanels`/`FlutterBottomPanels` → `OpenPanelPP6`/`ClosePanelPP6`/`FlutterPanelPP6`, retarget to `PANEL_PP6`.
- Add `OpenPanel13`/`ClosePanel13`/`FlutterPanel13` for `:OP13`/`:CL13`/`:OF13`.
- Add 18 literal-prefix MARCDUINO_ACTION handlers per §8 (`:OPP1`..6, `:CLP1`..6, `:OFP1`..6).

**Acceptance:**
- Build passes.
- On hardware: `:OP05` and `:OP06` do not move P7/P11/P13; `:OP07` opens P7
  (was wrong pre-fix), `:OP11` opens P11, `:OP13` opens P13.
- `:OPP1` opens PP1; `:OPP3`/`:OPP5` are silent no-ops on MK4.

**Out of scope:**
- Calibration-namespace pie commands (`:MVP1` etc.) — explicitly deferred in §8.

---

### Slice C — MarcduinoSequence small fixes

**Files owned:** `MarcduinoSequence.h` only
**Dependencies:** Slice A
**Covers Required Changes:** §9 (YodaClearMind), §9b prereq P5 (`:SE12` mask)

**Scope:**
- Change `:SE12 TopPanelsShowcase` mask (line ~225–229) from `PIE_PANEL` to `PIE_PANEL | TOP_PIE_PANEL`.
- Rename `PANEL_GROUP_6` → `PANEL_P11` in `YodaClearMind` (lines 441, 442, 448, 449).

**Acceptance:**
- Build passes.
- YodaClearMind opens P11 at cue point.
- `:SE12` drives all 6 pies when fully wired (PP3 included).

**Out of scope:**
- All other `:SE*` sequences — they're already mask-driven and correct.

---

### Slice D — DomeSequences foundation

**Files owned:** `DomeSequences.h` (constants + helper)
**Dependencies:** Slice A (for comment correctness — the new comments reference `PANEL_P*`/`PANEL_PP*` from A)
**Covers Required Changes:** §9b prereqs P1, P2, P3, P4 (constants, ADR 0009, no-piggyback rule, helper arrays)
**Spec sources:** ADR 0009 (rename), sequence-audit.md prerequisite tables

**Scope:**
- Rename slot constants per ADR 0009 (`DP1`..`DPP4` → `D_P1`..`D_P13` + `D_PP1`..`D_PP6`).
- Add `D_PP3 12` (new — slot 12 had no constant).
- Strip stale `PANEL_GROUP_N` / `MINI_PANEL` comments; replace with `PANEL_P*` / `PANEL_PP*` references.
- Define file-scope `ringPanels[]`, `piePanels[]`, `allPanels[]` arrays per audit P4.
- Rewrite `domeRandomPanels()` helper to use new arrays; rename `numLower` → `numRing`.
- Delete the inline `allPanels[]` literal inside `domeScream` (replaced by file-scope array — but the *sequence body* rewrite is Slice E).

**Acceptance:**
- Build passes.
- `DM:RANDOM` runs without grind on MK4 (uses the rewritten helper).

**Out of scope:**
- Per-sequence body rewrites (those are Slice E). After Slice D, sequence bodies still call the *old* identifiers via a temporary compat shim, OR Slice D + E land as a coordinated pair. **Implementation note:** since both touch `DomeSequences.h`, Slice E must follow D in the same agent's hands OR D must include a brief compat shim. Simplest path: D + E land as one PR with two distinct commits.

---

### Slice E — DomeSequences per-sequence rewrites

**Files owned:** `DomeSequences.h` (sequence bodies)
**Dependencies:** Slice D (needs new constants + arrays)
**Covers Required Changes:** §9b per-sequence rewrites
**Spec sources:** sequence-audit.md decision table

**Scope (per the audit table — Decision column is authoritative):**
- **Rewrite 11 sequences:** `domeOpenClosePies`, `domeOpenCloseLow`, `domeOpenCloseAll`, `domeFlutter`, `domeBloom`, `domeScream`, `domeResetAll`, `domeCantina`, `domeRockMarch`, `domeOverload`, `domeRandom`.
- **Identifier rename + 1-line comment:** `domeHelloThere`.
- **No change:** `domeHeart`, `domeAlarm`, `domeDisco`, `domeVader`, `domeLeiaMode`.

**Acceptance (matches "Mandatory" verification gate):**
- Build passes.
- Every `DM:*` runs on MK4 to completion without grind/hang/crash.
- Wired-panel choreography preserved per audit intent (P1–P4, P7, P11, P13, PP1, PP2, PP4, PP6 behave per design).
- `:OPP3`, `:OPP5` are silent no-ops on MK4 inside sequences (the inactive-slot guard path is exercised).

**Out of scope:**
- Fully-wired-dome choreography validation (PP3/PP5 visual participation) — that's Recommended, not a Slice E acceptance criterion. Captured in `sequence-audit.md` Recommended block.

---

### Slice F — Web UI: panels.html SVG + inline click dispatch + button retargets

**Files owned:** `data/panels.html`
**Dependencies:** Slice A spec (constants/mapping locked); does NOT depend on firmware compile — can develop against local web server with stubbed `/api/cmd`
**Covers Required Changes:** §10 panels.html row

**Scope:** see §10 → `data/panels.html` (heavy). SVG IDs renamed to identity-based,
onClicks pass identities, inline `domePanelClick()` maps identities to commands,
button table retargeted, group buttons updated to V3-aligned `:OP14`/`:OP15`
top/bottom panel shortcuts, labels corrected (PP5 "unserviced", PP6 individual).

**Acceptance:**
- Local web server (CLAUDE.md `python3 -m http.server 8080 --directory data`) — every click fires the expected command (verifiable via browser devtools network tab).
- SVG highlight overlay tracks the correct panel on hover/click.
- SVG clicks for pie panels send `:OPP1`–`:OPP6`, never invalid strings such as
  `:OPPP1`; P7/P11/P13 clicks send `:OP07`/`:OP11`/`:OP13`.

**Out of scope:**
- `app.js` changes (Slice G).
- `sequences.html` Yoda line (Slice H).

---

### Slice G — Web UI: app.js cmdSummary

**Files owned:** `data/app.js`
**Dependencies:** Slice A spec
**Covers Required Changes:** §10 app.js row

**Scope:** see §10 → `data/app.js` (light). Add `:OPP\d` / `:CLP\d` / `:OFP\d` regex clauses to `cmdSummary()` and improve command-number summaries with the corrected panel identities.

**Acceptance:**
- Browser devtools console shows correct command summaries for new commands.
- Functions work in isolation (testable without firmware via local web server).

---

### Slice H — Web UI: sequences.html + AGENTS.md tidy

**Files owned:** `data/sequences.html`, `data/AGENTS.md`
**Dependencies:** Slice C (Yoda rename lands)
**Covers Required Changes:** §10 sequences.html + AGENTS.md rows

**Scope:** Update Yoda sequence description line ~169 ("panel group 6" → "P11"). Add `:OPP*` family to AGENTS.md command-coverage inventory.

**Acceptance:** Visual / content review.

---

### Slice I — FORK_IMPROVEMENTS.md operator-facing doc

**Files owned:** `FORK_IMPROVEMENTS.md`
**Dependencies:** spec locked (i.e., this very task plan + ADRs 0006–0009) — does NOT depend on firmware compile
**Covers Required Changes:** §11

**Scope:** see §11. New section explaining the fix, the `:OPP*` namespace and aliases, list of repurposed group shortcuts, PP5 reclassification note, sequence audit summary.

**Acceptance:** Operator reading FORK_IMPROVEMENTS understands the changes and can update saved macros.

---

### Slice J — Cleanup: remove deprecated `PANEL_GROUP_N` constants

**Files owned:** `AstroPixelsPlus.ino` (the address-bit defines block)
**Dependencies:** Slices B, C, D, **and** E must all be merged. Verify before claiming.
**Covers Required Changes:** finalises §1 (which originally said "Remove all `PANEL_GROUP_1`–`PANEL_GROUP_10`")

**Scope:**
- Verify no firmware file (`MarcduinoPanel.h`, `MarcduinoSequence.h`,
  `DomeSequences.h`, `AstroPixelsPlus.ino` itself outside the defines block,
  and any other `.h`/`.ino` source) references `PANEL_GROUP_1`..`PANEL_GROUP_10`:
  ```bash
  grep -rn 'PANEL_GROUP_[0-9]' --include='*.h' --include='*.ino' --include='*.cpp' \
      /home/mattias/Documents/GitHub/AstroPixelsPlus/ | grep -v '\.pio/'
  ```
  Expected output: empty (or only matches inside the deprecated `#define` block
  being removed in this slice).
- Delete the `#define PANEL_GROUP_1`..`#define PANEL_GROUP_10` lines from
  `AstroPixelsPlus.ino`.
- Delete the "deprecated — remove in Slice J" comments added in Slice A.

**Acceptance:**
- `pio run -e astropixelsplus` succeeds.
- `grep PANEL_GROUP_ ` over the whole repo returns no matches outside
  `.pio/libdeps/` (upstream ReelTwo's own examples).
- Hardware boots and behaves identically to the pre-Slice-J state (this is a
  pure dead-code removal).

**Out of scope:**
- Any behavioural change. If `grep` finds a stray reference, the responsible
  Slice (B/C/D/E) is incomplete — re-open it; don't paper over from Slice J.

---

## Files to Change

| File | Section | Change | Slice |
|------|---------|--------|-------|
| `AstroPixelsPlus.ino` | defines ~344 | **Additively add** PANEL_P*/PANEL_PP* alongside existing PANEL_GROUP_1–10 | A |
| `AstroPixelsPlus.ino` | `servoSettings[]` ~409 | Switch to PANEL_P*/PANEL_PP*; add slots for PP3/PP5/PP6 | A |
| `AstroPixelsPlus.ino` | comment block ~390 | Correct `:OPnn` column for P7/P11/P13/PP6 | A |
| `AstroPixelsPlus.ino` | defines block | Remove deprecated PANEL_GROUP_1–10 (after B/C/D/E land) | J |
| `MarcduinoPanel.h` | `panelTargetToMask()` ~23 | Cases 5–7 / 11–13 corrected; 8–10 renamed | B |
| `MarcduinoPanel.h` | `:OP05`/`:CL05`/`:OF05`; `:OP06`/`:CL06`/`:OF06`; `:OP07`/`:CL07`/`:OF07` | Literal handler triples corrected: 05/06 no-op, 07 → PANEL_P7 | B |
| `MarcduinoPanel.h` | `:OP11`/`:CL11`/`:OF11` ~668/~835 | Retarget to PANEL_P11; rename handlers (Flutter*) | B |
| `MarcduinoPanel.h` | `:OP12`/`:CL12`/`:OF12` ~675/~842 | Retarget to PANEL_PP6; rename handlers | B |
| `MarcduinoPanel.h` | add `:OP13`/`:CL13`/`:OF13` | New triple for P13 | B |
| `MarcduinoPanel.h` | add `:OPP1-6`/`:CLP1-6`/`:OFP1-6` | 18 literal-prefix handlers (fork namespace) | B |
| `MarcduinoSequence.h` | YodaClearMind ~441 | 4 lines: PANEL_GROUP_6 → PANEL_P11 | C |
| `MarcduinoSequence.h` | `:SE12 TopPanelsShowcase` ~224 | Mask `PIE_PANEL` → `PIE_PANEL \| TOP_PIE_PANEL` | C |
| `DomeSequences.h` | slot constants ~14 | Identity-based rename (ADR 0009); add `D_PP3` | D |
| `DomeSequences.h` | `domeRandomPanels()` ~134 | Use new `ringPanels[]`/`piePanels[]` arrays | D |
| `DomeSequences.h` | file-scope arrays (new) | `ringPanels[]`, `piePanels[]`, `allPanels[]` | D |
| `DomeSequences.h` | 11 sequence bodies | Per-sequence rewrites per `sequence-audit.md` | E |
| `DomeSequences.h` | `domeScream` inline `allPanels[]` ~538 | Delete; use file-scope `allPanels[]` | E |
| `data/panels.html` | SVG IDs + buttons + inline `domePanelClick()` | Identity-based IDs; new `:OP*`/`:OPP*` payloads; identity dispatch fixed | F |
| `data/app.js` | `cmdSummary()` ~205 | New regex clauses and corrected panel identity summaries | G |
| `data/sequences.html` | Yoda description ~169 | "panel group 6" → "P11" | H |
| `data/AGENTS.md` | command-coverage inventory | Add `:OPP*`/`:CLP*`/`:OFP*` family | H |
| `FORK_IMPROVEMENTS.md` | new panel section | Operator-facing explanation of fix + namespace | I |
| `CONTEXT.md` | panel identity + verb sections | Updated 2026-05-26 (PP5 reclassification, `:OF` = flutter) | done in grilling |
| `docs/adr/0003-panel-command-routing-hardcoded-switch.md` | Amendments | Address-bit vs slot-ordinal clarification | done in grilling |
| `docs/adr/0006-panel-address-bit-rename.md` | new | PANEL_GROUP_N → PANEL_P*/PP* rationale | pre-existing |
| `docs/adr/0007-pie-panel-opp-command-namespace.md` | new | `:OPP*` namespace decision | pre-existing |
| `docs/adr/0008-pp5-reclassified-as-pie-panel.md` | new | PP5: MINI_PANEL → PIE_PANEL | done in grilling |
| `docs/adr/0009-dome-sequences-identity-slot-constants.md` | new | `D_*` slot constants in `DomeSequences.h` | done in grilling |
| `tasks/sequence-audit.md` | new | Per-sequence decision table + MK4 safety analysis | done in grilling |

---

## Acceptance Criteria

- `:OPnn` opens the physical panel with identity `nn`: P1–P4 (01–04), P7 (07), P11 (11),
  P13 (13). Commands 05 and 06 are graceful no-ops (P5, P6 are fixed panels).
- `:CLnn` closes and `:OFnn` flutters the same panels by the same numbers.
  (`:OF*` runs the `SeqPanelAllFlutter` animation in this firmware — not a power-off
  command. See CONTEXT.md "Panel command verbs".)
- `:OP08`, `:OP09`, `:OP10`, `:OP12` open PP1, PP2, PP4, PP6 respectively (community standard,
  retained as aliases).
- `:OPPn` opens pie panel PPn for n = 1–6. No-op for PP3 and PP5 unless a builder has wired
  those slots.
- `:CLPn` closes and `:OFPn` flutters pie panel PPn.
- Group shortcut commands (cases 14, 15) still work and use command numbers that do not
  collide with individual panel numbers.
- Marcduino / ReelTwo-compatible command semantics are preserved.
- Fork-only commands are documented as additions, not replacements.
- `YodaClearMind` (`$720`) opens and closes P11 at the correct cue point.
- Wave and ordered sequences verified on hardware — P7, P11, P13 participate in visually
  sensible order.
- Relevant web pages show correct panel labels, command payloads, group controls, SVG highlights.
- `:OP01`–`:OP04`, `:OP07`, `:OP11`, `:OP13` verified individually on live hardware.
- `:OPP1`, `:OPP2`, `:OPP4`, `:OPP6` verified on live hardware.

### Verification gate split (see `sequence-audit.md` for full Mandatory/Recommended lists)

**Mandatory on standard MK4 (PP3 + PP5 inactive) — ship blockers:**
- All sequences complete without grind / hang / crash / boot fault.
- 11 wired panels behave exactly as the per-sequence audit specifies.
- `:OPP3` and `:OPP5` are silent no-ops (confirms inactive-slot guard path).

**Recommended (post-ship, when a fully-wired dome is available):**
- PP3 and PP5 participate in choreographed sequences per the audit's intent.
- `:OP14` / `:OPP3` / `:OPP5` drive the wired panels visibly.
- `domeOpenCloseLow` does NOT move PP5 (regression check for the "PP5 dropped from ring sequence" fix).

---

## Risks

### Active risks (require mitigation)

- **Classic v1.6 `:OP11`/`:OP12` group shortcuts are repurposed by V3 alignment.**
  Any saved R2 Touch macro or external controller using `:OP11` for "top panels" or
  `:OP12` for "bottom panels" will now drive individual P11 / PP6 panel aliases.
  This is consistent with MarcDuino V3's 13-panel numbering, but it is still a
  compatibility break for classic v1.6 assumptions. Mitigation: Slice I
  (`FORK_IMPROVEMENTS.md`) documents the move; group effects remain reachable at
  `:OP14` (top panels) and `:OP15` (bottom panels), with exact masks verified before
  implementation.
- **PP5 reclassification (ADR 0008) changes existing sequences.** Every
  `PIE_PANEL`-targeted call now includes PP5 on a fully-wired dome.
  Pre-rename, those sequences excluded PP5 because of its `MINI_PANEL` type
  bit. Mitigation: explicitly listed in `sequence-audit.md` per-sequence
  decisions; intended behaviour for `:SE12`, `domeBloom`, etc.
- **Multi-slice coordination drift.** With 10 slices (A–J) and up to 5 agents
  in parallel, slice boundaries may slip (e.g., Slice B agent updates a
  `:OF*` handler comment and accidentally touches a constant Slice A owns).
  Mitigation: file-ownership column in the slice table is explicit; agents
  must check `git diff` before pushing and confirm they touched only owned
  files.
- **Stale `PANEL_GROUP_N` references after Slice J.** If any downstream fork
  or future contributor's branch references the old constants, their merge
  will break. Mitigation: Slice J `grep` step catches in-tree references;
  external forks need separate communication (release notes / FORK_IMPROVEMENTS).

### Resolved risks (left here so reviewers don't re-investigate)

- **Slice A in-isolation build break** — resolved by additive Slice A + Slice J cleanup.
- **`:OF*` semantics ambiguity** ("off" vs flutter) — resolved 2026-05-26: `:OF*` is **flutter**.
  See CONTEXT.md "Panel command verbs". Task plan acceptance criteria updated.
- **MK4 safety for inactive PP3/PP5 slots** — resolved: `_moveServoToPulse` already
  guards on `channel != 0`. See `sequence-audit.md` "MK4 safety" section.
- **Bit width** — resolved: `uint32_t` throughout `ServoDispatch` and `ServoSequencer`;
  bits up to 31 are safe. No truncation risk.

### Rollback strategy

ESP32 firmware ships via OTA — a bad upload could leave the device in an
unrecoverable state, so rollback discipline matters.

**Per-slice rollback** (preferred):
- Each slice merges as its own commit (or short series of commits) on `main`.
  Slice merges are individually revertable via `git revert <merge-commit>`.
- Slices A and J are the only ones that touch `AstroPixelsPlus.ino` constants.
  If Slice A introduces a wrong bit value, revert A; B/C/D/E referring to the
  new names will fail to compile, which is the correct fail-loud signal — fix A
  forward, don't try to patch B/C/D/E around it.

**Full-fix rollback** (if hardware verification reveals a fundamental issue):
- Slice J is reversible by re-adding the `PANEL_GROUP_N` defines (they're
  pure additive constants — the OTA upload would just have one more `#define`
  block).
- Slices B/C/D/E/F/G/H/I are independently revertable; reverting any one of
  them leaves the others functional because Slice A is additive (old names
  still work).
- The **only** non-revertable scenario is Slice A's `servoSettings[]` slot
  assignment if a builder has already saved custom wiring config in NVS that
  conflicts with the new bit assignments. Bit values are preserved per ADR 0006
  for existing panels (P1-P4, P7, P11, P13, PP1, PP2, PP4) so this scenario
  is bounded to PP3/PP5/PP6 slots, which were previously unconfigurable.

**Pre-OTA safety net:**
- Keep the prior firmware `.bin` checked into a known location (build artifact
  archive or `releases/` branch). If post-upload behaviour is wrong, re-flash
  the prior `.bin` via the OTA endpoint or USB before any debugging.

---

## Status — slice tracker

Mirrors the Work Slices table above. Update the slice's status line in BOTH
places when claiming, opening a PR, or merging.

### Spec approval
- [ ] Plan re-approved after the 2026-05-26 expansion (ADRs 0008/0009 added, sequence audit folded in, `:OF`=flutter semantic fix, MK4 safety gate)

### Slice A — `AstroPixelsPlus.ino` foundation
- [x] PANEL_P*/PANEL_PP* defines added (additive, alongside existing PANEL_GROUP_1–10)
- [x] PANEL_GROUP_1–10 defines annotated `// deprecated — remove in Slice J`
- [x] servoSettings[] slots 0–6: switched to PANEL_P*-named constants
- [x] servoSettings[] slot 7: PANEL_PP5 | PIE_PANEL (ADR 0008)
- [x] servoSettings[] slots 11, 12: PANEL_PP6, PANEL_PP3 entries added/updated
- [x] servoSettings[] inline `:OPnn` comment block corrected
- [x] Whole-codebase build still passes (every dependent file still references the deprecated names; that's expected pre-J)

### Slice B — `MarcduinoPanel.h` rewrite
- [x] panelTargetToMask() cases 5/6 → false (fixed panels)
- [x] panelTargetToMask() cases 7/11/12/13 corrected; 8/9/10 renamed
- [x] Literal :OP05/:CL05/:OF05 handlers changed to recognized no-ops
- [x] Literal :OP06/:CL06/:OF06 handlers changed to recognized no-ops
- [x] Literal :OP07/:CL07/:OF07 handlers retargeted to PANEL_P7
- [x] :OP11/:CL11/:OF11 retargeted to PANEL_P11; handlers renamed
- [x] :OP12/:CL12/:OF12 retargeted to PANEL_PP6; handlers renamed
- [x] :OP13/:CL13/:OF13 handlers added (new) for PANEL_P13
- [x] 18 :OPPn/:CLPn/:OFPn literal-prefix handlers added (PP1–PP6)

### Slice C — `MarcduinoSequence.h` small fixes
- [x] YodaClearMind: 4 lines PANEL_GROUP_6 → PANEL_P11
- [x] :SE12 TopPanelsShowcase mask broadened to `PIE_PANEL | TOP_PIE_PANEL`

### Slice D — `DomeSequences.h` foundation
- [x] Slot constants renamed per ADR 0009 (D_P1..D_P13 + D_PP1..D_PP6, plus new D_PP3)
- [x] Stale PANEL_GROUP_N / MINI_PANEL comments replaced
- [x] File-scope `ringPanels[]`, `piePanels[]`, `allPanels[]` arrays defined
- [x] `domeRandomPanels()` helper rewritten (uses new arrays, `numLower` → `numRing`)
- [x] Inline `allPanels[]` inside `domeScream` deleted (replaced by file-scope reference in Slice E)

### Slice E — `DomeSequences.h` per-sequence rewrites
- [x] `domeOpenClosePies` — all 6 pies, identity-sequential order
- [x] `domeOpenCloseLow` — ring panels only (PP5 dropped)
- [x] `domeOpenCloseAll` — all 13 panels via `allPanels[]`
- [x] `domeFlutter` — two-phase ring → pie sweep
- [x] `domeBloom` — all 6 pies
- [x] `domeScream` — all 13 panels via shared `allPanels[]`
- [x] `domeResetAll` — all 13 closed via `allPanels[]`
- [x] `domeCantina` — PP3 paired with PP4 on top-pie beat, 130 BPM preserved
- [x] `domeRockMarch` — extended to full ring-panel march
- [x] `domeOverload` — no body change (cascades from helper fix in D)
- [x] `domeRandom` — no body change (cascades from helper fix in D)
- [x] `domeHelloThere` — identifier rename + intentional-design comment
- [x] No-change sequences confirmed: `domeHeart`, `domeAlarm`, `domeDisco`, `domeVader`, `domeLeiaMode`

### Slice F — `data/panels.html`
- [x] SVG IDs renamed to identity-based (`p1`..`p13`, `pp1`..`pp6`)
- [x] onClick handlers updated to pass identity strings
- [x] Inline `domePanelClick()` maps identity strings → commands per the new convention
- [x] Button table retargeted to new `:OPnn` numbers + `:OPP*` for pies
- [x] P11, P13 individual buttons added
- [x] PP3, PP5 placeholder buttons added (no-op on MK4, live when wired)
- [x] `:OP14` / `:OP15` group buttons added (replace removed `:OP11`/`:OP12` group rows)
- [x] `:OP14` / `:OP15` group button labels and masks reviewed as V3-style top/bottom panels, not assumed all-pie/all-ring
- [x] PP5 label fixed ("fixed" → "unserviced"); PP6 label fixed ("group only" → individual)

### Slice G — `data/app.js`
- [x] `cmdSummary()` recognises `:OPP\d`, `:CLP\d`, `:OFP\d` patterns
- [x] `cmdSummary()` resolves corrected `:OPnn` aliases to builder-facing panel identities

### Slice H — `data/sequences.html` + `data/AGENTS.md`
- [x] Yoda sequence description ("panel group 6" → "P11")
- [x] AGENTS.md command-coverage inventory adds `:OPP*` / `:CLP*` / `:OFP*`

### Slice I — `FORK_IMPROVEMENTS.md`
- [x] Panel command numbering fix explained operator-facing
- [x] `:OPP` namespace + community alias relationship documented
- [x] Repurposed group shortcuts listed (`:OP11`→P11, `:OP12`→PP6, `:OP13`→P13; top/bottom group effects moved to `:OP14`/`:OP15`)
- [x] PP5 reclassification note (ADR 0008 in operator language)
- [x] Sequence audit summary (what's new in DM:* / what intentionally didn't change)

### Slice J — Cleanup (deferred until B + C + D + E all merged)
- [x] `grep PANEL_GROUP_` over the repo returns no in-tree matches (excluding `.pio/libdeps/`)
- [x] `PANEL_GROUP_1`..`PANEL_GROUP_10` `#define`s removed from `AstroPixelsPlus.ino`
- [x] "deprecated — remove in Slice J" comments removed (were attached to the removed `#define` lines)
- [x] Full build passes (SUCCESS, 84.1% flash / 18.7% RAM)
- [ ] Hardware boot OK — pending OTA upload

### Build + integration
- [x] All slices done; full build passes
- [ ] OTA upload done

### Codex review fixes (post-merge, 2026-05-27)
Four findings from independent Codex GPT review — two confirmed bugs, two intentional decisions documented.

- [x] **[BUG — FIXED]** `:OP14`/`:CL14`/`:OF14` and `:OP15`/`:CL15`/`:OF15` were unreachable.
  The `$`-wildcard handlers (`:OP$`, `:CL$`, `:OF$`) parse their suffix as **hex** via `strtol(cmd, 0, 16)`,
  so `:OP14` → `0x14` = 20 = meaningless bitmask. Six literal `MARCDUINO_ACTION` handlers added to
  `MarcduinoPanel.h` (before the per-panel literals block) to intercept before the wildcard fires.
- [x] **[BUG — FIXED]** `panelSlotCommand()` in `AsyncWebInterface.h` had stale slot→command mapping:
  slot 4 was `":OP05"` instead of `":OP07"`, slot 5 `":OP06"` instead of `":OP11"`, slot 6 `":OP07"`
  instead of `":OP13"`, slots 7/11/12 were `""` instead of `":OPP5"`/`":OPP6"`/`":OPP3"`. Corrected
  with identity-based row comments. Stale comment ("PP6 (slot 11, group-only)") replaced.
- [x] **[INTENTIONAL — documented]** `DOME_DANCE_PANELS_MASK` excludes `TOP_PIE_PANEL` (PP3).
  PP3 mounts THP/HP3; dance sequences must not disturb the holo projector mount. On standard MK4,
  PP3 is also unserviced (channel=0 short-circuits `_moveServoToPulse`). Comment added to `AstroPixelsPlus.ino`.
- [x] **[INTENTIONAL — no change]** V3 group labels "Open Top (Pie)" / "Open Bottom (Ring)" are correct:
  `panelTargetToMask()` case 14 = `PIE_PANEL | TOP_PIE_PANEL` = all pie = dome top;
  case 15 = `DOME_PANELS_MASK` = all ring = dome bottom. Labels match the physical geometry.
- [x] Full build re-verified after fixes: SUCCESS, 84.1% flash / 18.7% RAM

### MK4 Mandatory hardware verification (ship blockers)
- [ ] :OP01–:OP04, :OP07, :OP11, :OP13 verified individually
- [ ] :OPP1, :OPP2, :OPP4, :OPP6 verified via fork namespace
- [ ] :OPP3 and :OPP5 are silent no-ops on MK4
- [ ] YodaClearMind opens/closes P11 at cue point
- [ ] Every DM:* and :SE* runs to completion without grind/hang on MK4
- [ ] Wave sequences visually correct with P7/P11/P13 (no regression on wired 11 panels)

### Recommended (post-ship, fully-wired dome)
- [ ] PP3/PP5 choreography intent confirmed per `sequence-audit.md` Recommended block
