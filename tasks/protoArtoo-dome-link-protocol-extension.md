# protoArtoo Body Controller — Dome Link Protocol Extension Report

**Date:** 2026-04-26  
**Dome firmware:** AstroPixelsPlus (branch `main`, commit ahead of `fe61532`)  
**Relevant dome file:** `DomeSequences.h` (new), `BodyLinkWiFi.h` / `sendBodyCommand()` (existing transport)  
**Scope:** Changes needed in protoArtoo to handle two new message types arriving over the protoR2link

---

## 1. Context

The dome firmware has gained a new choreography system (`DomeSequences.h`) exposing 17 named sequences under the `DM:` Marcduino prefix. These sequences coordinate panels, holos, and logic displays on the dome side, and dispatch two new message types to the body over the existing protoR2link transport (UART primary, WiFi/UDP fallback):

1. **`dome=seqon,N` / `dome=seqoff`** — sequence lifecycle coordination
2. **`BD:<CUE>`** — named body cues requesting sound and/or animation

Neither message type is currently parsed by protoArtoo's `parseDomeRxLine()`. Both arrive as unknown messages and increment `domeRxUnknownCount`. Without these additions, dome sequences produce no body-side behavior.

The existing transport infrastructure — UART serial ring buffer, UDP receive socket, `parseDomeRxLine()` dispatch, `DomeLinkTask` 10 ms poll loop — is unchanged and correct. This is an additive change only.

---

## 2. New Message Type 1 — Sequence Lifecycle (`dome=seqon,N` / `dome=seqoff`)

### Purpose

When the dome starts a choreographed sequence it sends `dome=seqon,N` where `N` is the maximum duration in seconds. The body should:
- **Pause autonomous behaviors** (random chatter, random body motion, anything self-initiated) for the duration
- Allow the dome's named cue (`BD:`) to arrive and be acted on without interference

When the dome finishes (or on clean abort), it sends `dome=seqoff`. The body should resume autonomous behaviors.

The `N` value is a **safety timeout only** — it defines the maximum window the body should stay in suppressed mode if `dome=seqoff` never arrives (dome crash, link drop mid-sequence). Normal operation: `dome=seqoff` arrives before the timeout.

### Wire format

Both messages arrive as newline-terminated strings on the same channel as existing dome commands:

```
dome=seqon,15\r
dome=seqoff\r
```

### Complete timing reference (extracted from DomeSequences.h)

| Dome command | `dome=seqon` duration | Notes |
|---|---|---|
| `DM:PIES` | 12 s | Toggle pie panels open/close |
| `DM:LOW` | 15 s | Toggle lower panels open/close |
| `DM:OPENALL` | 10 s | Toggle all panels open/close |
| `DM:FLUTTER` | 10 s | All panels flutter to 75%, snap closed |
| `DM:BLOOM` | 8 s | Pie panels ease open, wiggle, close |
| `DM:SCREAM` | 15 s | All panels burst open, red alert, random flutter |
| `DM:OVERLOAD` | 12 s | Random panels sluggishly drift, failure logics |
| `DM:HEART` | 10 s | Rainbow holos, sweet logic message |
| `DM:ALARM` | 10 s | Pulsing red holos and logics |
| `DM:DISCO` | 46 s | Delegates to `:SE09` — see note below |
| `DM:VADER` | 47 s | Imperial March — red logics/holos |
| `DM:ROCKMARCH` | 47 s | Imperial March alternate |
| `DM:HELLO` | 4 s | Panel wave + logic scroll greeting |
| `DM:LEIA` | 36 s | Front holo Leia effect, logic Leia mode |
| `DM:CANTINA` | 17 s | 130 BPM alternating panel dance |
| `DM:RESET` | 4 s | Close all panels, reset all subsystems |
| `DM:RANDOM` | *none* | Delegates to `:SE##` — see note below |

> **DISCO note:** `DM:DISCO` dispatches `:SE09` (non-blocking), then immediately sends `dome=seqoff`. The `dome=seqon,46` / `dome=seqoff` pair arrives nearly simultaneously — random behaviors are stopped for only a few milliseconds before being resumed by `dome=seqoff`. The actual disco track arrives a moment later as `$D` (sent directly by `:SE09`, not as a `BD:` cue) and will interrupt whatever the AudioTask started. **There is no meaningful crash protection for DISCO** via the safety timeout: because `dome=seqoff` clears `domeSeqActive = false` and `domeSeqActiveUntilMs = 0` almost immediately, the poll-loop timeout check `if (domeSeqActive && millis() > ...)` never triggers. If the dome crashes after `dome=seqon,46` but before the `dome=seqoff` that follows within milliseconds, the body will auto-resume after 46 seconds — but this is an extremely narrow crash window in practice.

> **RANDOM note:** `DM:RANDOM` dispatches one of 15 `:SE##` / `$###` sequences directly. Each of those sequences calls `sendBodyCommand()` for their own body cues. `DM:RANDOM` does **not** wrap with `dome=seqon/off` to avoid double-coordination.

### Required implementation — `parseDomeRxLine()`

In `src/drivers/dome_rx_parser.cpp` (or wherever the dome RX parser lives), add before the unknown-count fallthrough:

```cpp
if (strncmp(line, "dome=seqon,", 11) == 0) {
    unsigned int secs = (unsigned int)atoi(line + 11);
    domeSeqActiveUntilMs = millis() + ((uint32_t)secs * 1000UL);
    domeSeqActive = true;
    audioTaskStopRandom();
    motionTaskStopRandom();
    return;
}
if (strcmp(line, "dome=seqoff") == 0) {
    domeSeqActive = false;
    domeSeqActiveUntilMs = 0;
    audioTaskStartRandom();
    motionTaskStartRandom();
    return;
}
```

Use whatever internal function names already handle `$s` (stop random) and `$R` (start random) behavior — don't duplicate the logic, just call the same code path.

### Required implementation — safety timeout in `DomeLinkTask`

In the `DomeLinkTask` poll loop (10 ms tick), add:

```cpp
if (domeSeqActive && millis() > domeSeqActiveUntilMs) {
    domeSeqActive = false;
    audioTaskStartRandom();
    motionTaskStartRandom();
}
```

This fires only if `dome=seqoff` was never received — dome crash, link drop, or sequence overrun.

**Important:** suppression scope is autonomous/random behaviors only. Any operator-triggered command from the controller must still execute normally during a dome sequence. Do not gate manual controls behind `domeSeqActive`.

**`BD:` cues must never be gated by `domeSeqActive`.** The `BD:` handler in `parseDomeRxLine()` calls `handleDomeCue()` unconditionally — no `domeSeqActive` check. `BD:` cues arrive from the dome precisely because a dome sequence is running; suppressing them defeats their entire purpose. Similarly, incoming `$`, `:SE`, `:OP`, `:CL` commands from the controller pass through as normal.

### Required header additions — `dome_link.h` (or equivalent)

```cpp
extern volatile bool     domeSeqActive;
extern volatile uint32_t domeSeqActiveUntilMs;
```

Define these in the `.cpp` translation unit where the DomeLinkTask lives:

```cpp
volatile bool     domeSeqActive        = false;
volatile uint32_t domeSeqActiveUntilMs = 0;
```

---

## 3. New Message Type 2 — Named Body Cues (`BD:<CUE>`)

### Purpose

Named cues are the dome's way of saying "I am doing X right now, please do your half." The dome handles its own panels/holos/logics locally; the body handles sound and any coordinated body motion. The `BD:` prefix carries the semantic intent, not a raw command.

### Wire format

```
BD:SCREAM\r
BD:LEIA\r
BD:HAPPY\r
```

The cue name is always uppercase, no spaces, no trailing digits.

### Required implementation — `parseDomeRxLine()`

Add a `BD:` prefix handler in `parseDomeRxLine()`:

```cpp
if (strncmp(line, "BD:", 3) == 0) {
    handleDomeCue(line + 3);   // passes "SCREAM", "LEIA", "HAPPY", etc.
    return;
}
```

### Complete cue inventory

The following cues are dispatched by `DomeSequences.h`. "When sent" gives the calling context so you can understand body-side timing.

---

#### `BD:SCREAM`
- **Sent by:** `domeScream()` — at sequence start, before panel movement begins
- **Dome duration:** `dome=seqon,15` (15 seconds total)
- **Dome doing:** all panels burst open, red alert logics and holos, random panel flutter, closes at ~13 s
- **Recommended sound:** `$6` — random from bank 6 (scream sounds, tracks 126–150)
- **Recommended body action:** suppress random motion for duration; optionally trigger a startle pose if hardware supports it
- **Note:** immediately followed at sequence end by a conditional `BD:HAPPY` when panels close (see BD:HAPPY)

---

#### `BD:HAPPY`
- **Sent by:** `domeOpenClosePies()`, `domeOpenCloseLow()`, `domeOpenCloseAll()` — on every open AND close toggle. Also sent at the end of `domeScream()` when panels close.
- **Dome doing:** panel opening or closing motion
- **Recommended sound:** `$3` — random from bank 3 (happy sounds, tracks 51–75). Short clips fit a panel toggle moment.
- **Recommended body action:** short friendly gesture (arm wave, leg shimmy) if available
- **Gating note:** this cue is gated on the dome side by the `dm_happy_sound` NVS preference (default `true`). If the user disables that preference, `BD:HAPPY` is never sent. The body must not assume it arrives with every panel sequence.
- **Frequency note:** fires on every panel toggle — rapid repeated toggling can produce multiple `BD:HAPPY` calls in quick succession. AudioTask should interrupt/restart rather than queue. If your AudioTask queues, add a same-cue cooldown in `handleDomeCue()`.

---

#### `BD:OVERLOAD`
- **Sent by:** `domeOverload()` — at sequence start
- **Dome duration:** `dome=seqon,12` (12 seconds)
- **Dome doing:** failure logics, holos short circuit, random panels sluggishly drift open then snap closed
- **Recommended sound:** `$4` — random from bank 4 (sad/distressed sounds, tracks 76–100)
- **Recommended body action:** sluggish/glitchy body motion for ~10 s; return to idle

---

#### `BD:ALARM`
- **Sent by:** `domeAlarm()` — at sequence start
- **Dome duration:** `dome=seqon,10` (10 seconds)
- **Dome doing:** pulsing red holos (`HPA0021|10`), alarm logics and PSI (10 s, auto-reset)
- **Recommended sound:** a specific alarm clip from bank 1 (general sounds, tracks 1–25). Pick a track with ~8–10 s runtime.
- **Recommended body action:** alert/attention pose if available

---

#### `BD:VADER`
- **Sent by:** `domeVader()` — at sequence start
- **Dome duration:** `dome=seqon,47` (47 seconds)
- **Dome doing:** Imperial March — red flashing holos (47 s), MARCH sequence on all logics and PSIs (47 s, auto-reset)
- **Recommended sound:** `$M` (Imperial March). Long track (~47 s) — let it run to completion.
- **Recommended body action:** hold still in an attention pose for ~47 s; suppress random motion entirely

---

#### `BD:ROCKMARCH`
- **Sent by:** `domeRockMarch()` — at sequence start
- **Dome duration:** `dome=seqon,47` (47 seconds)
- **Dome doing:** same visual as VADER (red holos, MARCH logics/PSI, 47 s)
- **Recommended sound:** if you have a distinct rock version of the Imperial March in your sound library, assign it a specific bank 9 track number and use `$9xx`. Otherwise fall back to `$M`. This is the one cue with an open sound assignment — decide based on your sound library.
- **Recommended body action:** same as VADER

---

#### `BD:LEIA`
- **Sent by:** `domeLeiaMode()` — at sequence start
- **Dome duration:** `dome=seqon,36` (36 seconds)
- **Dome doing:** front holo Leia LED sequence (`HPS101|36`), rear/top holos off, LEIA logics and PSI (36 s, auto-reset)
- **Recommended sound:** `$L` (Leia message, bank 7 track 1 = file 151). Runtime ~34–36 s.
- **Recommended body action:** hold still; suppress random chatter for 36 s — this is a moment sequence

---

#### `BD:CANTINA`
- **Sent by:** `domeCantina()` — at sequence start, before panel dance begins
- **Dome duration:** `dome=seqon,17` (17 seconds; dance runs 15 s, 2 s buffer)
- **Dome doing:** 130 BPM alternating panel dance (15 s), white flash holos, blue flash logics, all auto-reset
- **Recommended sound:** `$C` (Cantina, bank 9 track 5). Full orchestral Cantina. The dome dance is 15 s — let the track continue beyond the panel movement.
- **Recommended body action:** rhythmic motion at 130 BPM if capable (923 ms per beat)

---

#### `BD:HEART`
- **Sent by:** `domeHeart()` — at sequence start
- **Dome duration:** `dome=seqon,10` (10 seconds)
- **Dome doing:** front/rear/top holos rainbow cycle (`HPx006|10`), FLD scrolls "You're / Wonderful", front PSI FLASHCOLOR (10 s, auto-reset)
- **Recommended sound:** a specific sweet/kind clip from bank 3 (happy). Pick a track that feels heartwarming rather than general-happy. ~8–10 s.
- **Recommended body action:** gentle friendly gesture if available

---

#### `BD:HELLO`
- **Sent by:** `domeHelloThere()` — at sequence start
- **Dome duration:** `dome=seqon,4` (4 seconds)
- **Dome doing:** FLD scrolls "Hello / There", RLD scrolls "General Kenobi", DP1 waves open and closed
- **Recommended sound:** a greeting clip from bank 1 (general sounds). Keep it short — the dome sequence is only 4 s.
- **Recommended body action:** friendly wave gesture if available

---

#### `BD:RESET`
- **Sent by:** `domeResetAll()` — triggered by `DM:RESET` command
- **Dome duration:** `dome=seqon,4` (4 seconds)
- **Dome doing:** close all panels, reset holos to random mode loop, logics and PSI to NORMAL
- **Recommended sound:** `$O` (stop/mute) — silence. The dome is returning to idle; the body should do the same.
- **Recommended body action:** cancel any in-progress animation; return to neutral idle pose

---

## 4. Files to Change in protoArtoo

| File | Change | Priority |
|---|---|---|
| `src/drivers/dome_rx_parser.cpp` | Add `dome=seqon,N` and `dome=seqoff` handlers before unknown-count fallthrough | Required |
| `src/drivers/dome_rx_parser.cpp` | Add `BD:` prefix handler calling `handleDomeCue(line + 3)` | Required |
| `src/tasks/dome_link.cpp` (or wherever `DomeLinkTask` lives) | Declare `domeSeqActive` / `domeSeqActiveUntilMs`; add safety-timeout check in poll loop | Required |
| `include/dome_link.h` (or equivalent) | `extern volatile bool domeSeqActive; extern volatile uint32_t domeSeqActiveUntilMs;` | Required |
| `src/tasks/dome_cue_handler.cpp` *(new file)* | `handleDomeCue(const char* cue)` — routes each cue to AudioTask + motion | Required |
| `include/dome_cue_handler.h` *(new file)* | Declare `void handleDomeCue(const char* cue);` | Required |

---

## 5. `handleDomeCue()` — Implementation Sketch

```cpp
// src/tasks/dome_cue_handler.cpp
#include "dome_cue_handler.h"
// include AudioTask header, MotionTask header, etc.

void handleDomeCue(const char* cue) {
    if (strcmp(cue, "SCREAM") == 0) {
        audioTaskPlay("$6");            // random scream bank
        // motionTaskStartle() if available
    }
    else if (strcmp(cue, "HAPPY") == 0) {
        audioTaskPlay("$3");            // random happy bank
        // motionTaskFriendlyGesture() if available
    }
    else if (strcmp(cue, "OVERLOAD") == 0) {
        audioTaskPlay("$4");            // random sad/distressed bank
        // motionTaskSluggish(10000) if available
    }
    else if (strcmp(cue, "ALARM") == 0) {
        audioTaskPlayTrack(/* bank 1 alarm clip number */);
        // motionTaskAlert() if available
    }
    else if (strcmp(cue, "VADER") == 0) {
        audioTaskPlay("$M");            // Imperial March
        // motionTaskAttentionPose(47000)
    }
    else if (strcmp(cue, "ROCKMARCH") == 0) {
        audioTaskPlay("$M");            // or specific bank 9 track number
        // motionTaskAttentionPose(47000)
    }
    else if (strcmp(cue, "LEIA") == 0) {
        audioTaskPlay("$L");            // Leia message track
        // motionTaskHoldStill(36000)
    }
    else if (strcmp(cue, "CANTINA") == 0) {
        audioTaskPlay("$C");            // Cantina track
        // motionTaskRhythmic(923, 15000) if available
    }
    else if (strcmp(cue, "HEART") == 0) {
        audioTaskPlayTrack(/* sweet bank 3 clip number */);
        // motionTaskFriendlyGesture()
    }
    else if (strcmp(cue, "HELLO") == 0) {
        audioTaskPlayTrack(/* bank 1 greeting clip number */);
        // motionTaskWave() if available
    }
    else if (strcmp(cue, "RESET") == 0) {
        audioTaskStop();                // $O — silence
        // motionTaskReturnToIdle()
    }
    // Unknown cue — log and ignore, never crash
}
```

Use the actual AudioTask/MotionTask function names already in protoArtoo. Do not duplicate audio or motion logic — call the same code paths that handle `$6`, `$3`, `$M`, `$L`, `$C` from the Marcduino parser.

---

## 6. Ordering and Timing Notes

**Normal message order per sequence:**
1. `dome=seqon,N` → body pauses random behaviors
2. `BD:<CUE>` → body starts sound/action (arrives within a few ms of seqon)
3. *(dome runs for up to N seconds — heartbeat maintained throughout, see below)*
4. `dome=seqoff` → body resumes random behaviors

This order holds for every sequence **except DM:DISCO** (see special case below). Sound triggered by `BD:` overlaps with the suppressed-random window intentionally — the dome and body run their halves in parallel.

**Body link heartbeat is maintained during blocking sequences.** The dome's `domeWaitTime()` helper calls `handleBodyLinkHeartbeat()` on every iteration in addition to pumping the event loop. The heartbeat function is internally rate-limited to 1 Hz, so the body will continue to receive `#APHB` frames at the normal cadence even during a 47-second Vader sequence. You should not see the dome link drop to `Waiting` state mid-sequence.

**BD:HAPPY after BD:SCREAM.** `domeScream()` sends `BD:SCREAM` at sequence start, then sends `BD:HAPPY` (gated by `dm_happy_sound` preference) when panels close at ~13 s in. The body receives `BD:SCREAM`, plays a scream sound, then ~13 s later receives `BD:HAPPY` and plays a happy sound. This is the intended arc. Process each cue independently as it arrives.

**BD:HAPPY fires on every panel toggle.** `DM:PIES`, `DM:LOW`, and `DM:OPENALL` send `BD:HAPPY` on each open and close. Rapid repeated toggling produces multiple calls in quick succession. AudioTask should interrupt/restart rather than queue. If your AudioTask queues, add a same-cue cooldown in `handleDomeCue()`.

**DM:DISCO special case — only exception to the normal order.** The body will receive `dome=seqon,46`, then `dome=seqoff` within milliseconds, then `$D` a short time later from `:SE09` directly (not via a `BD:` cue). The practical message sequence is:
1. `dome=seqon,46` → `audioTaskStopRandom()`
2. `dome=seqoff` (milliseconds later) → `audioTaskStartRandom()`
3. `$D` (shortly after) → AudioTask plays disco track, interrupting whatever just restarted

The brief window between step 2 and step 3 means `audioTaskStartRandom()` may start a random sound that gets immediately cut off by `$D`. This is an acceptable blip — `$D` takes priority. Ensure your AudioTask treats `$D` as an interrupt-and-play, not an enqueue. Do not suppress random behaviors for the full 46 seconds for DISCO; that window is never held.

---

## 7. Acceptance Criteria

- `dome=seqon,N` received → `domeSeqActive = true`, `domeSeqActiveUntilMs = millis() + N*1000`, random sound/motion paused
- `dome=seqoff` received → `domeSeqActive = false`, random sound/motion resumed
- Safety timeout: if `dome=seqoff` never arrives, body auto-resumes after `N` seconds (test by disconnecting dome link mid-sequence)
- `BD:SCREAM` → body plays a scream-bank sound
- `BD:LEIA` → body plays `$L` (Leia track, ~36 s)
- `BD:VADER` → body plays `$M` (Imperial March, ~47 s)
- `BD:CANTINA` → body plays `$C` (Cantina track)
- `BD:HAPPY` → body plays a short happy-bank sound
- `BD:RESET` → body stops sound and returns to idle
- `BD:` with unknown cue name → logged, ignored, no crash
- `BD:` cues arrive and are acted on normally even while `domeSeqActive == true` (no gating)
- Existing `$`, `:SE`, `:OP`, `:CL`, `#APSL`, `#APWU` parsing unaffected (regression check)
- Manual operator commands execute normally while `domeSeqActive == true`
- DM:DISCO: `$D` plays without being queued behind any random sound restart (interrupt-and-play semantics)
