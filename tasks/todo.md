# Task: Convert DM:* dome sequences to non-blocking ReelTwo animations

**Branch:** `fix/dm-command-dispatch` (continue — do not branch from main)
**Goal:** Eliminate the blocking dome-sequence execution model that hangs core 1
(`mainLoop`) until reboot. Replace `domeMove(wait)` busy-loops + `dome_pendingSeq`
dispatch with the project's existing non-blocking pattern (`MARCDUINO_ANIMATION` +
`DO_*` steps on `player`, `SEQUENCE_PLAY_ONCE`, `schedulePanelRelease`), matching
`MarcduinoSequence.h` (`:SE*`).

## Acceptance criteria
- `DM:LOW` opens/closes ring panels (P1-P4, P7, P11, P13) with the wave choreography.
- `mainLoop` (core 1) never blocks for the duration of a sequence.
- A `DM:*` sequence followed by a direct `:OP01`/`:CL01` works WITHOUT reboot.
- No `dome_pendingSeq`, no `domeMove(wait=true)` busy-loop, no `domeWaitTime` busy-loop remain.
- `pio run -e astropixelsplus` builds; firmware fits flash.
- Upper/pie panels NOT exercised on hardware during test (PP* hardware unsafe per user).

## Root-cause framing (why the rewrite, not a point fix)
Static analysis (see handoff) shows dispatch is uniform — `:OP01` and `DM:*` both go
through `player.animateOnce()`. The ONLY behavioral difference is the body: `:OP01` is
fire-and-forget; `DM:*` bodies call `domeMove(wait=true)` →
`while (servoDispatch.isActive(idx)) AnimatedEvent::process();` on core 1. API stays
responsive (core 0 async server) while core 1 is stuck → "ok but no movement, reboot
required." The rewrite removes the entire blocking class instead of bounding one loop.

## Design (matches `:SE*` in MarcduinoSequence.h)
- Each `domeXxx()` becomes a `MARCDUINO_ANIMATION(NameXxx, DM:XXX)` DO_* script on `player`.
- Wave choreography uses **staggered** `servoDispatch.moveToPulse(idx, startDelay, moveTime, pos)`
  in a single `DO_ONCE`, then one `DO_WAIT_MILLIS(envelope)` — non-blocking, preserves the wave.
- Inter-move / settle delays → `DO_WAIT_MILLIS` / `DO_WAIT_SEC`.
- Servo power cutoff after close → `schedulePanelRelease(mask, delayMs)` (no `disable()` loop).
- Body cues / LED / holo → `DO_ONCE` / `DO_COMMAND`.
- `domeBeginSequence(secs)` in first `DO_ONCE`; `domeEndSequence()` in `DO_RESET`.
- Toggle sequences (Low/Pies/All): single animation, branch at step 0 via
  `DO_WHILE(dome_*Open, kCloseLabel)` / `DO_GOTO`.
- Keep the Marcduino command queue (correct, unrelated) and `dome_seqRunning` (body coordination).

## Slices (incremental, verify each)
- [x] **Slice 0 — infra**: `RING_PANELS_MASK` + `domeStaggerMove()` helper. (commit 0e117b8)
- [x] **Slice 1 — tracer bullet (DM:LOW)**: `domeLowOpen`/`domeLowClose`. **Hardware-verified** on
      10.0.0.21 — wave runs, no hang, `:OP01` works after. (commit 0e117b8)
- [x] **Slice 2a — linear**: Heart, Alarm, Vader, Leia, ResetAll, Flutter, HelloThere. (commit be1b650)
- [x] **Slice 2b — toggle**: Pies, OpenAll. (commit 626ee40)
- [x] **Slice 3 — loop/random/delegate**: Scream, Overload, Cantina, RockMarch, Bloom, Disco, Random.
      (commit 9944151)
- [x] **Slice 4 — cleanup**: removed `dome_pendingSeq`, blocking `domeMove`/`domeWaitTime`.
- [x] **Slice 5 — verify/doc**: build clean (flash 84.0%), compat dry-run OK, `FORK_IMPROVEMENTS.md` updated.

## Status: ALL 16 DM:* sequences migrated to non-blocking. Hardware-tested subset: DM:LOW (ring) PASS.
Pie-touching sequences are software-verified only (pie hardware unsafe to actuate). RockMarch is
ring-only and could be hardware-tested in a follow-up.

## Working notes
- `:OP*` (MarcduinoPanel.h) moves by GROUP MASK; dome sequences move by ARRAY INDEX (D_P*=slot).
  Slot↔panel mapping in servoSettings[] is correct (slot0=P1..slot6=P13); 11 panels active.
- `moveToPulse` overloads: `(num,pos)`, `(num,moveTime,pos)`, `(num,startDelay,moveTime,pos)`.
- `AnimationPlayer::end()`→`reset()` clears `fAnimation`/`fFlags` — player goes idle cleanly.
- Reentrancy guard in `AnimatedEvent::process()` becomes irrelevant once sequences stop pumping it.

## Verification story (fill in as we go)
- Slice 0+1 build: **PASS** — `pio run -e astropixelsplus` SUCCESS, Flash 83.9%, RAM 19.0%.
  Artifact: `DM:LOW` registered (no `DM:0x0`); `[DOME] dispatching pendingAnim` present.
- Slice 1 hardware (DM:LOW + :OP01 after): **PASS** (2026-05-29, OTA to 10.0.0.21, commit 0e117b8).
  Ring open/close wave ran; logs show `[DOME] dispatching pendingAnim`; device stayed
  responsive (no core-1 hang); `:OP01`/`:CL01` worked WITHOUT reboot — original failure mode gone.

## Behavior note discovered during Slice 1 verification
Panel commands (`:OP*`) and dome `DM:*` sequences share the same `player`, so a panel
command now PREEMPTS a running dome sequence (animateOnce ends the current animation).
In the old blocking model this couldn't happen (core 1 was locked). This is generally
desirable (responsiveness) but can leave panels mid-state if a sequence is interrupted.
Decide in a later slice whether `DM:*` should guard against preemption or expose a cancel.
