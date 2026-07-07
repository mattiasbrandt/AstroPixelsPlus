# Task: Consolidate Dome Panel Movement APIs

## Problem

Troubleshooting body-driven RockMarch exposed that dome panel movement currently
has multiple overlapping control surfaces with different semantics:

- `DM:*` dome sequences call `servoDispatch.moveToPulse(...)` directly in C++.
- `:OP/:CL/:OF` Marcduino panel commands express panel intent by physical panel
  identity and go through the panel sequence/release path.
- `:SM` exposes raw `servoDispatch` slot pulse movement over Marcduino.

This made debugging unnecessarily hard. Static mapping looked correct on both
body and dome, but runtime behavior still did not match expectations because
`:SM` does not mean the same thing as panel open/close intent.

## What We Learned

- Dome panel NVS wiring was not custom: `/api/panels/config` showed 0 panel
  channel overrides and slots `0..6 = P1,P2,P3,P4,P7,P11,P13`.
- Body was sending literal `:SM0...:SM6` over UART once the request format was
  corrected.
- The first body attempt used the wrong `:SM` argument order. Dome/ReelTwo
  expects:

  ```text
  :SM<slot>,<moveTimeMs>,<pulse>
  ```

  not `:SM<slot>,<pulse>,<moveTimeMs>`.

- Even with corrected argument order, `:SM0,150,2200` opened P1 only about 15%
  on this dome, while `:SM0,150,800` fully closed it.
- `:SM` is raw pulse control. Hardcoded `800/2200` is not a portable open/close
  contract for calibrated/mechanical panel behavior.
- Dome-owned sequences worked because they call typed C++ movement APIs directly
  and bypass Marcduino suffix parsing.
- Raw `:SM` also hit the same deferred `Marcduino::getCommand()` trap previously
  found in panel calibration commands. A synchronous ingress intercept was added
  so `:SM` now logs `[SM-exec]` and executes before queueing.

## Why This Is Frustrating / Risky

The system currently has too many ways to move panels:

1. direct C++ slot/pulse movement,
2. Marcduino panel intent commands (`:OP/:CL/:OF`),
3. raw Marcduino servo movement (`:SM`).

They differ by coordinate system, calibration behavior, release-timer handling,
logging visibility, and failure mode. This creates avoidable ambiguity between
body and dome agents and makes static code review insufficient for understanding
runtime panel behavior.

## Recommended Direction

External choreography should not use `:SM`.

Preferred contracts:

- Full dome choreography: `DM:<sequence>` commands, owned by the dome.
- Individual panel intent: `:OPxx/:CLxx/:OFxx`, using physical panel identity
  numbers.
- Raw `:SM`: diagnostic/calibration-only, not production body choreography.

For body RockMarch specifically, use either:

```text
DM:ROCKMARCH
```

or, if the body must step panels itself:

```text
P1  :OP01 / :CL01
P2  :OP02 / :CL02
P3  :OP03 / :CL03
P4  :OP04 / :CL04
P7  :OP07 / :CL07
P11 :OP11 / :CL11
P13 :OP13 / :CL13
```

## Proposed Follow-up Work

- Define one internal dome panel movement API that owns panel identity,
  calibration, release timers, and logging.
- Make `:OP/:CL/:OF` and `DM:*` sequences call that internal API instead of each
  path hand-rolling movement semantics.
- Document `:SM` as raw diagnostic pulse control and explicitly not suitable for
  choreography.
- Consider adding a calibrated slot/percentage command only if raw slot control
  remains necessary, e.g. `:SP<slot>,<moveMs>,<percent>`, where percent is
  normalized through `servoDispatch.scaleToPos(...)`.
- Add stronger runtime logs for panel movement showing command source, physical
  panel identity, slot, channel, target semantic, and resolved pulse.

## Acceptance Criteria

- Body-side documentation and sequence code no longer use `:SM` for RockMarch or
  other production panel choreography.
- Firmware docs clearly distinguish panel intent commands from raw servo
  diagnostic commands.
- There is a single recommended external contract for panel choreography.
- Future debugging can answer "which physical panel did this command intend to
  move?" from logs without cross-referencing multiple mapping systems.
