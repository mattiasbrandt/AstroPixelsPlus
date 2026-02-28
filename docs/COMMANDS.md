# AstroPixelsPlus — Extended Command Reference

This document covers the extended Marcduino commands added in this fork for smoke,
fire, CBI, DataPanel, and holo position/movement controls. For the baseline command
set (panels, basic holos, logics, sound) see the main `README.md`.

---

## Table of Contents

1. [Command Protocol Overview](#command-protocol-overview)
2. [Feature Toggles](#feature-toggles)
3. [BadMotivator — Smoke Commands](#badmotivator--smoke-commands)
4. [FireStrip — Fire Commands](#firestrip--fire-commands)
5. [CBI — Charge Bay Indicator Commands](#cbi--charge-bay-indicator-commands)
6. [DataPanel Commands](#datapanel-commands)
7. [Holo Projector — Position Commands](#holo-projector--position-commands)
8. [Holo Projector — Random Movement Commands](#holo-projector--random-movement-commands)
9. [ReelTwo Command Structure Reference](#reeltwo-command-structure-reference)

---

## Command Protocol Overview

All commands follow the MarcDuino serial protocol and must be terminated with `\r`
(carriage return). Commands can be delivered via:

- **Serial2** — TTL header at 2400 baud (hardware Marcduino daisy-chain)
- **REST API** — `POST /api/cmd` with body `cmd=<command>`
- **WebSocket** — `/ws` text frame

**Prefix domains:**

| Prefix | Domain        | File                  |
|--------|---------------|-----------------------|
| `#`    | Configuration / effects gadgets | `MarcduinoEffects.h`, `AstroPixelsPlus.ino` |
| `:`    | Panel servos  | `MarcduinoPanel.h`    |
| `*`    | Holo projectors | `MarcduinoHolo.h`   |
| `@`    | Logics / sequences | `MarcduinoLogics.h`, `MarcduinoSequence.h` |

> **64-byte buffer limit:** The MarcDuino protocol truncates commands silently at
> 64 bytes. Keep commands, including parameters, well within this limit.

---

## Feature Toggles

The smoke, fire, CBI, and DataPanel commands are **compiled out by default**.
Each gadget is gated by a build flag in `platformio.ini`:

```ini
# platformio.ini — build_flags section
-DAP_ENABLE_BADMOTIVATOR=1   # Enable smoke (#BM*) commands
-DAP_ENABLE_FIRESTRIP=1      # Enable fire (#FIRE*) commands
-DAP_ENABLE_CBI=1            # Enable charge bay (#CBI*) commands
-DAP_ENABLE_DATAPANEL=1      # Enable data panel (#DP*) commands
```

**Default state:** All four flags are set to `0` (disabled). Commands sent while a
feature is disabled are silently ignored by the command parser.

To enable a gadget, change the relevant flag from `0` to `1` in `platformio.ini`
and rebuild/reflash. Hardware for the gadget must also be wired and configured in
`AstroPixelsPlus.ino`.

---

## BadMotivator — Smoke Commands

**Source:** `MarcduinoEffects.h` · **Enabled by:** `-DAP_ENABLE_BADMOTIVATOR=1`

The BadMotivator gadget drives a smoke/fog machine attached to the R2 bad-motivator
dome port. Commands call into the `BadMotivator` class from the Reeltwo library
(`dome/BadMotivator.h`).

### Safety constants (enforced in firmware)

| Constant | Value | Meaning |
|---|---|---|
| `SMOKE_MAX_CONTINUOUS_MS` | 30 000 ms | Hard upper limit for a single smoke pulse |
| `SMOKE_COOLDOWN_MS` | 5 000 ms | Minimum rest between activations |

---

### `#BMSMOKE` — Start Smoke

**Syntax:** `#BMSMOKE\r`

**Parameters:** None

**Description:**  
Activates the smoke machine immediately and runs continuously until `#BMSTOP` is
received or the safety timeout expires.

**Example:**
```
#BMSMOKE\r
```

**Safety notes:**
- Do not run for extended periods without adequate ventilation.
- Sustained use without cooldown can overheat some fog-machine elements.
- Always pair with a `#BMSTOP` or use `#BMPULSE` for time-bounded activation.

---

### `#BMSTOP` — Stop Smoke

**Syntax:** `#BMSTOP\r`

**Parameters:** None

**Description:**  
Immediately deactivates the smoke machine, halting any continuous or pulsed operation.

**Example:**
```
#BMSTOP\r
```

**Safety notes:**
- Safe to call at any time, including when the smoke machine is already off.
- Use as an emergency cutoff during automation sequences.

---

### `#BMPULSE` — Timed Smoke Pulse

**Syntax:** `#BMPULSE<seconds>\r`

**Parameters:**

| Parameter | Type | Range | Description |
|---|---|---|---|
| `<seconds>` | Integer | 1–30 | Duration of smoke burst in whole seconds |

**Description:**  
Activates the smoke machine for a fixed number of seconds, then automatically
stops. The duration is converted internally to milliseconds before being passed to
`badMotivator.pulseSmoke()`.

**Examples:**
```
#BMPULSE5\r     → Smoke for 5 seconds
#BMPULSE15\r    → Smoke for 15 seconds
#BMPULSE30\r    → Smoke for 30 seconds (maximum)
```

**Safety notes:**
- Maximum allowed value is **30** seconds; values outside 1–30 are rejected with
  a debug log message and no action taken.
- Prefer `#BMPULSE` over `#BMSMOKE`/`#BMSTOP` pairs for automation sequences to
  guarantee the smoke machine does not stay on indefinitely if a stop command is lost.

---

### `#BMAUTO` — Automatic Random Smoke Mode

**Syntax:** `#BMAUTO,<min>,<max>\r`

**Parameters:**

| Parameter | Type | Range | Description |
|---|---|---|---|
| `<min>` | Integer | 1–60 | Minimum interval between activations (seconds) |
| `<max>` | Integer | `min`–60 | Maximum interval between activations (seconds) |

**Description:**  
Enables automatic, randomly-timed smoke bursts. The BadMotivator library picks a
random interval between `<min>` and `<max>` seconds and triggers smoke puffs
autonomously. Both values are converted to milliseconds before passing to
`badMotivator.setAutoMode()`.

**Examples:**
```
#BMAUTO,5,15\r    → Random smoke every 5–15 seconds
#BMAUTO,30,60\r   → Gentle background puffing every 30–60 seconds
#BMAUTO,1,5\r     → Rapid, frequent smoke bursts (use with caution)
```

**Safety notes:**
- `<min>` must be ≥ 1 and ≤ 60. `<max>` must be ≥ `<min>` and ≤ 60.
- Parameters outside these bounds are rejected (no action, debug log only).
- Send `#BMSTOP` to exit auto mode before sending other smoke commands.
- The comma separator between parameters is **required**; omitting it causes the
  command to be rejected.

---

## FireStrip — Fire Commands

**Source:** `MarcduinoEffects.h` · **Enabled by:** `-DAP_ENABLE_FIRESTRIP=1`

FireStrip drives a WS2812B LED strip configured to simulate fire effects using the
Reeltwo `FireStrip` class (`dome/FireStrip.h`). Brightness is internally capped at
`FIRE_MAX_BRIGHTNESS = 200` to protect LED longevity.

### Safety constants (enforced in firmware)

| Constant | Value | Meaning |
|---|---|---|
| `FIRE_MAX_BRIGHTNESS` | 200 | Hard brightness cap applied to all fire effects |

---

### `#FIRESPARK` — Brief Fire Spark

**Syntax:** `#FIRESPARK<ms>\r`

**Parameters:**

| Parameter | Type | Range | Description |
|---|---|---|---|
| `<ms>` | Integer | 1–5000 | Duration of spark effect in milliseconds |

**Description:**  
Triggers a short, single flame/spark animation for the specified number of
milliseconds, then the strip returns to its idle state. Calls `fireStrip.spark()`.

**Examples:**
```
#FIRESPARK100\r    → Quick 100 ms spark flash
#FIRESPARK500\r    → Half-second flicker
#FIRESPARK5000\r   → 5-second spark (maximum)
```

**Safety notes:**
- Maximum is **5000 ms** (5 seconds). Values outside 1–5000 are rejected.
- Spark is a transient effect; `#FIRESTOP` is not needed after it completes.

---

### `#FIREBURN` — Sustained Fire Effect

**Syntax:** `#FIREBURN<ms>\r`

**Parameters:**

| Parameter | Type | Range | Description |
|---|---|---|---|
| `<ms>` | Integer | 1–10000 | Duration of burn animation in milliseconds |

**Description:**  
Runs the full fire-simulation animation (heat-map color cycling, rising flame
patterns) for the specified duration at capped brightness. Calls
`fireStrip.burn(milliseconds, FIRE_MAX_BRIGHTNESS)`.

**Examples:**
```
#FIREBURN2000\r     → 2-second fire display
#FIREBURN10000\r    → 10-second fire display (maximum)
```

**Safety notes:**
- Maximum is **10 000 ms** (10 seconds). Values outside 1–10 000 are rejected.
- The brightness cap (`FIRE_MAX_BRIGHTNESS = 200`) is enforced in the firmware call;
  even if the library supports higher values, the cap cannot be exceeded via this
  command path.
- Do not use near flammable materials or place fire-strip LED arrays in enclosed
  spaces without adequate heat management.

---

### `#FIRESTOP` — Stop Fire Effect

**Syntax:** `#FIRESTOP\r`

**Parameters:** None

**Description:**  
Immediately halts any active fire or spark animation and sets the strip to off.
Calls `fireStrip.stop()`.

**Example:**
```
#FIRESTOP\r
```

**Safety notes:**
- Safe to call at any time, including when no fire effect is running.
- Use as the last command in any fire-effects sequence to guarantee a clean off state.

---

## CBI — Charge Bay Indicator Commands

**Source:** `MarcduinoEffects.h` · **Enabled by:** `-DAP_ENABLE_CBI=1`

CBI commands control the R2-D2 Charge Bay Indicator light board. The Reeltwo
`ChargeBayIndicator` class (`body/ChargeBayIndicator.h`) manages the LED patterns.

---

### `#CBION` — Charge Bay On

**Syntax:** `#CBION\r`

**Parameters:** None

**Description:**  
Enables the charge bay indicator, activating its default display pattern.
Calls `chargeBayIndicator.enable()`.

**Example:**
```
#CBION\r
```

---

### `#CBIOFF` — Charge Bay Off

**Syntax:** `#CBIOFF\r`

**Parameters:** None

**Description:**  
Disables the charge bay indicator, turning off all its LEDs.
Calls `chargeBayIndicator.disable()`.

**Example:**
```
#CBIOFF\r
```

---

### `#CBISET` — Charge Bay Set Pattern

**Syntax:** `#CBISET<pattern>\r`

**Parameters:**

| Parameter | Type | Range | Description |
|---|---|---|---|
| `<pattern>` | Integer | 0–9 | Pattern index to activate |

**Description:**  
Sets the charge bay indicator to a specific display pattern. Pattern semantics are
defined by the Reeltwo `ChargeBayIndicator` class; refer to the Reeltwo library
documentation for the full pattern list. Calls `chargeBayIndicator.setPattern()`.

**Examples:**
```
#CBISET0\r    → Pattern 0 (typically default/idle)
#CBISET5\r    → Pattern 5
#CBISET9\r    → Pattern 9 (maximum index)
```

**Safety notes:**
- Values outside 0–9 are rejected with a debug log message.
- The indicator must be enabled (`#CBION`) before `#CBISET` has a visible effect
  on some pattern implementations.

**Feature toggle note:** If `-DAP_ENABLE_CBI=0` (default), this command is compiled
out. Sending it has no effect and produces no error response.

---

## DataPanel Commands

**Source:** `MarcduinoEffects.h` · **Enabled by:** `-DAP_ENABLE_DATAPANEL=1`

DataPanel commands control the R2-D2 data panel display board. The Reeltwo
`DataPanel` class (`body/DataPanel.h`) manages the LED patterns.

> **REST API alternative:** The web interface also exposes `POST /api/datapanel`
> with `action=flicker` or `action=disable` for web-based control. These REST calls
> translate internally to Reeltwo `DP*` commands; they are separate from and
> complementary to the Marcduino `#DP*` command path.

---

### `#DPON` — Data Panel On

**Syntax:** `#DPON\r`

**Parameters:** None

**Description:**  
Enables the data panel, activating its default display pattern.
Calls `dataPanel.enable()`.

**Example:**
```
#DPON\r
```

---

### `#DPOFF` — Data Panel Off

**Syntax:** `#DPOFF\r`

**Parameters:** None

**Description:**  
Disables the data panel, turning off all its LEDs.
Calls `dataPanel.disable()`.

**Example:**
```
#DPOFF\r
```

---

### `#DPSET` — Data Panel Set Pattern

**Syntax:** `#DPSET<pattern>\r`

**Parameters:**

| Parameter | Type | Range | Description |
|---|---|---|---|
| `<pattern>` | Integer | 0–9 | Pattern index to activate |

**Description:**  
Sets the data panel to a specific display pattern. Pattern semantics are defined by
the Reeltwo `DataPanel` class; refer to the Reeltwo library documentation for the
full pattern list. Calls `dataPanel.setPattern()`.

**Examples:**
```
#DPSET0\r    → Pattern 0 (typically default/idle)
#DPSET3\r    → Pattern 3
#DPSET9\r    → Pattern 9 (maximum index)
```

**Safety notes:**
- Values outside 0–9 are rejected with a debug log message.
- The panel should be enabled (`#DPON`) before `#DPSET` for patterns to be visible.

**Feature toggle note:** If `-DAP_ENABLE_DATAPANEL=0` (default), this command is
compiled out. Sending it has no effect and produces no error response.

---

## Holo Projector — Position Commands

**Source:** `MarcduinoHolo.h` · **Always enabled** (part of core holo control)

These commands position holo projector servos to fixed named positions. They
translate to Reeltwo `HPF`/`HPR`/`HPT` servo movement commands.

### Position encoding

The `*HP` command format encodes both position and holo unit in three digits:

```
*HP<PP><N>\r

PP = position code (two digits, leading zero)
N  = holo number:  1 = Front (F)
                   2 = Rear  (R)
                   3 = Top   (T)
```

Position codes and their mapping to Reeltwo commands:

| Position Code | Position Name | Reeltwo Command Suffix |
|---|---|---|
| `00` | Down | `1010` |
| `10` | Center / Home | `1011` |
| `20` | Up | `1012` |
| `30` | Left | `1013` |
| `40` | Upper Left | `1014` |
| `50` | Lower Left | `1015` |
| `60` | Right | `1016` |
| `70` | Upper Right | `1017` |
| `80` | Lower Right | `1018` |

Valid range: `001`–`803`. The general `*HP` handler validates position (1–803) and
holo digit (1–3) before dispatching. Invalid values are rejected with a debug log.

---

### Front Holo (`N=1`) Position Commands

| Command | Position | Reeltwo Command |
|---|---|---|
| `*HP001\r` | Down | `HPF1010` |
| `*HP101\r` | Center | `HPF1011` |
| `*HP201\r` | Up | `HPF1012` |
| `*HP301\r` | Left | `HPF1013` |
| `*HP401\r` | Upper Left | `HPF1014` |
| `*HP501\r` | Lower Left | `HPF1015` |
| `*HP601\r` | Right | `HPF1016` |
| `*HP701\r` | Upper Right | `HPF1017` |
| `*HP801\r` | Lower Right | `HPF1018` |

**Examples:**
```
*HP001\r    → Front holo points down
*HP101\r    → Front holo returns to center
*HP701\r    → Front holo points upper-right
```

---

### Rear Holo (`N=2`) Position Commands

| Command | Position | Reeltwo Command |
|---|---|---|
| `*HP002\r` | Down | `HPR1010` |
| `*HP102\r` | Center | `HPR1011` |
| `*HP202\r` | Up | `HPR1012` |
| `*HP302\r` | Left | `HPR1013` |
| `*HP402\r` | Upper Left | `HPR1014` |
| `*HP502\r` | Lower Left | `HPR1015` |
| `*HP602\r` | Right | `HPR1016` |
| `*HP702\r` | Upper Right | `HPR1017` |
| `*HP802\r` | Lower Right | `HPR1018` |

**Examples:**
```
*HP002\r    → Rear holo points down
*HP102\r    → Rear holo returns to center
*HP202\r    → Rear holo points up
```

---

### Top Holo (`N=3`) Position Commands

| Command | Position | Reeltwo Command |
|---|---|---|
| `*HP003\r` | Down | `HPT1010` |
| `*HP103\r` | Center | `HPT1011` |
| `*HP203\r` | Up | `HPT1012` |
| `*HP303\r` | Left | `HPT1013` |
| `*HP403\r` | Upper Left | `HPT1014` |
| `*HP503\r` | Lower Left | `HPT1015` |
| `*HP603\r` | Right | `HPT1016` |
| `*HP703\r` | Upper Right | `HPT1017` |
| `*HP803\r` | Lower Right | `HPT1018` |

**Examples:**
```
*HP003\r    → Top holo points down
*HP103\r    → Top holo returns to center
*HP803\r    → Top holo points lower-right
```

---

### Positioning safety notes

- Holo servos are driven by the PCA9685 controller at I2C address `0x41`
  (Ch 0–1: Front H/V, Ch 2–3: Rear H/V, Ch 4–5: Top H/V).
- Verify servo travel limits in `servoSettings` before running sweep commands.
  Exceeding physical travel range can strip servo gears.
- Always home servos (`*HP101`, `*HP102`, `*HP103`) before power-down to avoid
  mechanical binding on next startup.
- `*ST00` resets all holos to their startup positions (`HPA0000`) and can be
  used as an emergency stop.

---

## Holo Projector — Random Movement Commands

**Source:** `MarcduinoHolo.h` · **Always enabled**

These commands engage continuous random servo movement on individual holo projectors,
giving an "alive" scanning appearance. They dispatch Reeltwo `HP?104` commands.

---

### `*RD01` — Front Holo Random Move

**Syntax:** `*RD01\r`

**Parameters:** None

**Description:**  
Starts continuous random servo movement on the Front holoprojector.
Dispatches `HPF104` to the Reeltwo holo controller.

**Example:**
```
*RD01\r
```

---

### `*RD02` — Rear Holo Random Move

**Syntax:** `*RD02\r`

**Parameters:** None

**Description:**  
Starts continuous random servo movement on the Rear holoprojector.
Dispatches `HPR104` to the Reeltwo holo controller.

**Example:**
```
*RD02\r
```

---

### `*RD03` — Top Holo Random Move

**Syntax:** `*RD03\r`

**Parameters:** None

**Description:**  
Starts continuous random servo movement on the Top holoprojector.
Dispatches `HPT104` to the Reeltwo holo controller.

**Example:**
```
*RD03\r
```

---

### Random movement notes

- Random movement runs until a stop or position command is issued to that holo.
- To stop random movement on a specific holo, send a position command such as
  `*HP101` (Front center) or `*ST00` (reset all).
- Auto-twitch mode (`*HD08`/`*HD09`) provides choreographed random movement
  managed by the Reeltwo sequencer; `*RD0n` gives manual per-holo random mode.

---

## ReelTwo Command Structure Reference

All holo and logic commands in this firmware route through the Reeltwo
`CommandEvent::process()` pipeline. Understanding the underlying `HP*` command
format helps when building custom sequences.

### Holo projector command format

```
HP<target><command>[|<repeat>]

target:  A = All holos
         F = Front holo
         R = Rear holo
         T = Top holo

command:
  0000          → Off
  005<n>        → Set LED color index n (0–9)
  096           → Off (LED blackout)
  0030          → Dim pulse random color (LED effect)
  0040          → Dim cycle random color (LED effect)
  006           → Rainbow color cycle (LED effect)
  104           → Random servo movement
  105[|<N>]     → Wag left/right N times
  106[|<N>]     → Nod up/down N times
  1010          → Servo → Down
  1011          → Servo → Center
  1012          → Servo → Up
  1013          → Servo → Left
  1014          → Servo → Upper-Left
  1015          → Servo → Lower-Left
  1016          → Servo → Right
  1017          → Servo → Upper-Right
  1018          → Servo → Lower-Right
  HPS<N>[|<T>]  → Holo sequence N for T seconds
```

### MARCDUINO_ACTION macro (ReelTwo)

New commands in this firmware are registered using:

```cpp
MARCDUINO_ACTION(HandlerName, <prefix>COMMAND, ({
    // Handler code runs when command is received
}))
```

The macro binds a command string to an inline handler. `Marcduino::getCommand()`
inside the handler returns everything after the command token (i.e., the parameter
string). This is the pattern used for all `#BM*`, `#FIRE*`, `#CBI*`, `#DP*`,
`*RD0n`, and `*HP` commands in this firmware.

### Feature-gated compilation

Commands for optional hardware gadgets are wrapped in conditional compilation:

```cpp
// In MarcduinoEffects.h — extern declarations
#if AP_ENABLE_BADMOTIVATOR
extern BadMotivator badMotivator;
#endif

// In AsyncWebInterface.h — include gating
#if AP_ENABLE_FIRESTRIP
#include "dome/FireStrip.h"
#endif
```

If a feature flag is `0`, the corresponding `MARCDUINO_ACTION` handlers are not
compiled and the gadget objects are not declared. The command string is never
registered in the parser dispatch table, so the firmware simply ignores any
matching command received at runtime — no error, no crash.

---

*Source files: `MarcduinoEffects.h`, `MarcduinoHolo.h`, `AsyncWebInterface.h`  
Reeltwo library: https://github.com/reeltwo/Reeltwo*
