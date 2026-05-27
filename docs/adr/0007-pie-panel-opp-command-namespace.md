# ADR 0007 — Fork-specific pie panel command namespace (:OPP / :CLP / :OFP)

**Status:** Accepted  
**Date:** 2026-05-25

## Context

The Marcduino community convention assigns individual pie panel commands to `:OP`
slots that correspond to fixed physical panel positions — electronics bays with
no servo. P8 (Rear PSI), P9 (Rear Logic Display), P10, and P12 (Front Logic
Display) are fixed panels, so their `:OPnn` command slots are repurposed for
individually addressable pie panels:

| Command | Fixed position | Pie panel |
|---------|---------------|-----------|
| `:OP08` | P8 (Rear PSI) | PP1       |
| `:OP09` | P9 (Rear Logic) | PP2     |
| `:OP10` | P10           | PP4       |
| `:OP12` | P12 (Front Logic) | PP6  |

This is internally consistent but non-obvious. A builder reading `:OP08` in a
script cannot tell whether it targets a ring panel at physical position 8 or pie
panel PP1. The mapping requires a lookup table that exists nowhere in the command
string itself.

## Decision

Add a fork-specific pie panel command namespace using a `P`-infixed prefix for
every panel command type:

- **Open:** `:OPP1`–`:OPP6`
- **Close:** `:CLP1`–`:CLP6`
- **Off:** `:OFP1`–`:OFP6`
- Any other per-panel command type (calibration, etc.) follows the same rule:
  insert `P` after the operation letters.

The `:OPPn` form reads as "Open Pie Panel n" and directly encodes the pie panel
identity in the command string. No lookup is required.

Community-standard aliases (`:OP08`, `:OP09`, `:OP10`, `:OP12`) are retained
as working equivalents — both addressing schemes target the same servo slot.

All six pie panel positions are covered: PP1, PP2, PP4, PP6 (servoed on standard
MK4) and PP3, PP5 (unserviced on standard MK4 but builder-activatable). The
unserviced commands are graceful no-ops by default; they activate automatically
when `panelConfigLoad()` enables the slot at runtime — no firmware change needed.

## Consequences

- Builders can use `:OPP1` to open PP1 without knowing the community-standard
  alias is `:OP08`.
- Both namespaces are fully functional simultaneously; existing controllers and
  scripts using the community-standard numbers continue to work unchanged.
- Operators learning the fork are exposed to the cleaner namespace first;
  `FORK_IMPROVEMENTS.md` explains the community alias relationship.
- Two working names for the same panel increases documentation surface. Every
  operator-facing document must explain both or consistently prefer one.
- `:OP12` is repurposed from its previous use as a "all ring panels" group
  shortcut to the individual PP6 command. Any saved macro using `:OP12` for the
  group effect must be updated. The group effect remains accessible at case 15
  in `panelTargetToMask()`.

## Implementation note (literal-prefix handlers)

Each of the 18 `:OPP*` / `:CLP*` / `:OFP*` commands is implemented as its own
`MARCDUINO_ACTION` with a literal 5-character prefix — the same shape as the
existing `:OPnn` family. A dynamic-prefix alternative (one `:OPP` handler that
parses the trailing digit via a new `parseSingleDigitTarget()` helper) was
considered and rejected because:

1. Dynamic-prefix handlers require the async web ingress path to handle the
   prefix synchronously in `processMarcduinoCommandWithSource()` before the
   command's memory becomes invalid (see the comment block above the `:MV`
   handler in `MarcduinoPanel.h`). Literal-prefix handlers have no such
   constraint — they match the command verbatim and dispatch immediately.
2. The handler count (18) is small enough that explicit handlers stay readable;
   "DRY" via a parser helper does not pay for itself at this size.
3. The literal-prefix shape mirrors the existing `:OPnn` family, keeping
   `MarcduinoPanel.h` internally consistent.

Calibration commands (`:MV`, `#SO`, `#SC`, `#SW`) keep their existing
dynamic-prefix form and continue to target pie panels via the two-digit
community-alias numbers (`08`, `09`, `10`, `12`). A pie-specific
`:MVP1` / `#SOP1` calibration namespace is **out of scope** until builder
demand justifies the additional handlers + parser work.
