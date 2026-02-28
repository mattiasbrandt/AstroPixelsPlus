# AstroPixelsPlus — Complete Command Reference

This document covers **all** Marcduino commands supported by the AstroPixelsPlus firmware.
including core commands and extended features.

| Category | Commands | Source |
|----------|----------|--------|
| **Configuration** | `#AP*` | `AstroPixelsPlus.ino` |
| **Panels** | `:CL*`, `:OP*`, `:OF*`, `:SE*`, `:SF`, `:MV`, `#SO`, `#SC`, `#SW` | `MarcduinoPanel.h`, `MarcduinoSequence.h` |
| **Logics** | `@*T*`, `@*M`, `@*P` | `MarcduinoLogics.h` |
| **Holos** | `*ON*`, `*OF*`, `*ST*`, `*HP*`, `*RD*`, `*HD*`, `*HW*`, `*HN*`, `*CO`, `*CH`, `*RC`, `*TE` | `MarcduinoHolo.h` |
| **Sound** | `$*` | `MarcduinoSound.h` |
| **Smoke** | `#BM*` | `MarcduinoEffects.h` |
| **Fire** | `#FIRE*` | `MarcduinoEffects.h` |
| **CBI** | `#CBI*` | `MarcduinoEffects.h` |
| **DataPanel** | `#DP*` | `MarcduinoEffects.h` |

---

## Table of Contents

1. [Command Protocol Overview](#command-protocol-overview)
2. [Feature Toggles](#feature-toggles)
3. [REST API Endpoints](#rest-api-endpoints)
4. [Configuration Commands](#configuration-commands)
5. [Panel Commands](#panel-commands)
6. [Logic Display Commands](#logic-display-commands)
7. [Holo Projector Commands](#holo-projector-commands)
8. [Sound Commands](#sound-commands)
9. [Sequence Commands](#sequence-commands)
10. [Panel Calibration Commands](#panel-calibration-commands)
11. [Dynamic Panel Group Commands](#dynamic-panel-group-commands)
12. [Direct Servo Commands](#direct-servo-commands)
13. [Holo Projector — Position Commands](#holo-projector--position-commands)
14. [Holo Projector — Random Movement Commands](#holo-projector--random-movement-commands)
15. [Extended Logic Effects](#extended-logic-effects)
16. [Extended Holo Commands](#extended-holo-commands)
17. [BadMotivator — Smoke Commands](#badmotivator--smoke-commands)
18. [FireStrip — Fire Commands](#firestrip--fire-commands)
19. [CBI — Charge Bay Indicator Commands](#cbi--charge-bay-indicator-commands)
20. [DataPanel Commands](#datapanel-commands)
21. [ReelTwo Command Structure Reference](#reeltwo-command-structure-reference)

1. [Command Protocol Overview](#command-protocol-overview)
2. [Feature Toggles](#feature-toggles)
3. [REST API Endpoints](#rest-api-endpoints)
4. [Configuration Commands](#configuration-commands)
5. [Panel Commands](#panel-commands)
6. [Logic Display Commands](#logic-display-commands)
7. [Holo Projector Commands](#holo-projector-commands)
8. [Holo Projector — Position Commands](#holo-projector--position-commands)
9. [Holo Projector — Random Movement Commands](#holo-projector--random-movement-commands)
10. [BadMotivator — Smoke Commands](#badmotivator--smoke-commands)
11. [FireStrip — Fire Commands](#firestrip--fire-commands)
12. [CBI — Charge Bay Indicator Commands](#cbi--charge-bay-indicator-commands)
13. [DataPanel Commands](#datapanel-commands)
14. [ReelTwo Command Structure Reference](#reeltwo-command-structure-reference)
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
## REST API Endpoints

In addition to the Marcduino serial protocol, gadget commands are accessible via
dedicated REST API endpoints for web-based control.

### Authentication

All gadget endpoints require authentication. Include the API token in the
`Authorization` header or as a query parameter:

```bash
curl -H "Authorization: Bearer YOUR_API_TOKEN" http://192.168.4.1/api/smoke
```

### POST /api/smoke

**Description:** Control BadMotivator smoke effects.

**Parameters (form-data or query):**

| Parameter | Values | Description |
|-----------|--------|-------------|
| `state` | `on`, `off` | Turn smoke on (BMON) or off (BMOFF) |

**Examples:**

```bash
# Turn smoke on
curl -X POST http://192.168.4.1/api/smoke -d "state=on"

# Turn smoke off
curl -X POST http://192.168.4.1/api/smoke -d "state=off"
```

**Response:**

```json
{"ok":true,"state":"on"}
```

### POST /api/fire

**Description:** Control FireStrip fire effects.

**Parameters (form-data or query):**

| Parameter | Values | Description |
|-----------|--------|-------------|
| `state` | `on`, `off` | Turn fire effect on (FS11000) or off (FSOFF) |

**Examples:**

```bash
# Turn fire on
curl -X POST http://192.168.4.1/api/fire -d "state=on"

# Turn fire off
curl -X POST http://192.168.4.1/api/fire -d "state=off"
```

**Response:**

```json
{"ok":true,"state":"on"}
```

### GET /api/cbi

**Description:** Get current Charge Bay Indicator state.

**Response:**

```json
{"state":"unknown","gadget":"cbi"}
```

> Note: State is always `unknown` as the firmware does not track CBI state.

### POST /api/cbi

**Description:** Control Charge Bay Indicator effects.

**Parameters (form-data or query):**

| Parameter | Values | Description |
|-----------|--------|-------------|
| `action` | `flicker`, `disable` | Effect to activate |
| `duration` | Integer (seconds) | Duration in seconds (default: 6 for flicker, 8 for disable) |

**Examples:**

```bash
# Flicker effect for 6 seconds
curl -X POST http://192.168.4.1/api/cbi -d "action=flicker"

# Disable for 5 seconds
curl -X POST http://192.168.4.1/api/cbi -d "action=disable&duration=5"
```

**Response:**

```json
{"ok":true,"action":"flicker","duration":"6"}
```

### GET /api/datapanel

**Description:** Get current DataPanel state.

**Response:**

```json
{"state":"unknown","gadget":"datapanel"}
```

> Note: State is always `unknown` as the firmware does not track DataPanel state.

### POST /api/datapanel

**Description:** Control DataPanel effects.

**Parameters (form-data or query):**

| Parameter | Values | Description |
|-----------|--------|-------------|
| `action` | `flicker`, `disable` | Effect to activate |
| `duration` | Integer (seconds) | Duration in seconds (default: 6 for flicker, 8 for disable) |

**Examples:**

```bash
# Flicker effect for 6 seconds
curl -X POST http://192.168.4.1/api/datapanel -d "action=flicker"

# Disable for 3 seconds
curl -X POST http://192.168.4.1/api/datapanel -d "action=disable&duration=3"
```

**Response:**

```json
{"ok":true,"action":"flicker","duration":"6"}
```

---

## Configuration Commands

**Source:** `AstroPixelsPlus.ino` · **Always enabled**

### `#APWIFI[0|1]` — WiFi Control

**Syntax:** `#APWIFI\r`, `#APWIFI0\r`, or `#APWIFI1\r`

**Description:** Toggle WiFi, turn off, or turn on.

### `#APREMOTE[0|1]` — Droid Remote Control

**Syntax:** `#APREMOTE\r`, `#APREMOTE0\r`, or `#APREMOTE1\r`

**Description:** Toggle Droid Remote ESPNOW support, turn off, or turn on.

### `#APZERO` — Factory Reset

**Syntax:** `#APZERO\r`

**Description:** Clears all preferences including WiFi settings. Use with caution.

### `#APRESTART` — Restart

**Syntax:** `#APRESTART\r`

**Description:** Restarts the ESP32.

---




---
## Panel Commands

**Source:** `MarcduinoPanel.h` · **Always enabled**

Panel commands control the dome panel servos via the PCA9685 controller at I2C address `0x40`.

### `:CL00` — Close All Panels

**Syntax:** `:CL00\r`

**Description:** Closes all dome panels simultaneously.

### `:OP00` — Open All Panels

**Syntax:** `:OP00\r`

**Description:** Opens all dome panels simultaneously.

### `:OF00` — Flutter All Panels

**Syntax:** `:OF00\r`

**Description:** Flutters (rapidly opens and closes) all dome panels.

### `:CL<N>` — Close Panel N

**Syntax:** `:CL<NN>\r`

**Parameters:**

| Parameter | Range | Description |
|-----------|-------|-------------|
| `<NN>` | 01–12 | Panel number to close |

**Examples:**

```
:CL01\r    → Close panel 1
:CL12\r    → Close panel 12
```

### `:OP<N>` — Open Panel N

**Syntax:** `:OP<NN>\r`

**Parameters:**

| Parameter | Range | Description |
|-----------|-------|-------------|
| `<NN>` | 01–12 | Panel number to open |

### `:OF<N>` — Flutter Panel N

**Syntax:** `:OF<NN>\r`

**Parameters:**

| Parameter | Range | Description |
|-----------|-------|-------------|
| `<NN>` | 01–12 | Panel number to flutter |

### `:SF<servo>$<easing>` — Set Servo Easing

**Syntax:** `:SF<servo>$<easing>\r`

**Parameters:**

| Parameter | Range | Description |
|-----------|-------|-------------|
| `<servo>` | 0–15 or $ | Servo channel (0–15) or $ for all |
| `<easing>` | 0–9 | Easing curve type |

**Easing Types:**

| Value | Type |
|-------|------|
| 0 | Linear |
| 1 | Ease In Quad |
| 2 | Ease Out Quad |
| 3 | Ease In Out Quad |
| 4 | Ease In Cubic |
| 5 | Ease Out Cubic |
| 6 | Ease In Out Cubic |
| 7 | Ease In Quart |
| 8 | Ease Out Quart |
| 9 | Ease In Out Quart |

---
## Logic Display Commands

**Source:** `MarcduinoLogics.h` · **Always enabled**

Logic commands control the front and rear logic LED displays.

### `@0T<N>` — All Logics Set Effect

**Syntax:** `@0T<N>\r`

**Parameters:**

| Parameter | Effect |
|-----------|--------|
| `N` | 1 = Normal, 2 = Flash, 3 = Alarm, 4 = Failure, 5 = Red Alert, 6 = Leia, 11 = March |

**Examples:**

```
@0T1\r     → All logics normal
@0T5\r     → All logics red alert
@0T11\r    → All logics march pattern
```

### `@1T<N>` — Front Logics Set Effect

**Syntax:** `@1T<N>\r`

Same effect codes as `@0T`. Applies only to front logic displays.

### `@2T<N>` — Rear Logics Set Effect

**Syntax:** `@2T<N>\r`

Same effect codes as `@0T`. Applies only to rear logic display.

### `@1M<text>` — Front Logics Scroll Text

**Syntax:** `@1M<text>\r`

**Description:** Sets the top front logics to scroll the specified text left.

**Example:**

```
@1MHello\r    → Scroll "Hello" on front logics
```

### `@2M<text>` — Front Bottom Logics Scroll Text

**Syntax:** `@2M<text>\r`

**Description:** Sets the bottom front logics to scroll the specified text left.

### `@3M<text>` — Rear Logics Scroll Text

**Syntax:** `@3M<text>\r`

**Description:** Sets the rear logics to scroll the specified text left.

### `@1P60` / `@1P61` — Front Logics Font

**Syntax:** `@1P60\r` or `@1P61\r`

**Description:** Sets front logics font (60 = Latin, 61 = Aurabesh).

### `@2P60` / `@2P61` — Front Bottom Logics Font

**Syntax:** `@2P60\r` or `@2P61\r`

### `@3P60` / `@3P61` — Rear Logics Font

**Syntax:** `@3P60\r` or `@3P61\r`

---
## Holo Projector Commands

**Source:** `MarcduinoHolo.h` · **Always enabled**

### `*ON<N>` — Holo On

**Syntax:** `*ON<N>\r`

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `N` | 0 = All, 1 = Front, 2 = Rear, 3 = Top |

### `*OF<N>` — Holo Off

**Syntax:** `*OF<N>\r`

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `N` | 0 = All, 1 = Front, 2 = Rear, 3 = Top |

### `*ST00` — Holo Reset

**Syntax:** `*ST00\r`

**Description:** Resets all holos to startup positions and disables effects.

---
## Sound Commands

**Source:** `MarcduinoSound.h` · **Always enabled**

Sound commands control audio playback via the DFPlayer Mini module (when enabled via web UI).

### `$<number>` — Play Track

**Syntax:** `$<number>\r`

**Parameters:**

| Parameter | Range | Description |
|-----------|-------|-------------|
| `<number>` | 1–9999 | Track number to play |

**Example:**
```
$1\r     → Play track 1
$42\r    → Play track 42
```

### `$c` — Stop Playback

**Syntax:** `$c\r`

**Description:** Stops current audio playback.

### `$s` — Stop Random

**Syntax:** `$s\r`

**Description:** Disables random sound playback mode.

### `$R` — Enable Random (Long Interval)

**Syntax:** `$R\r`

**Description:** Enables random sound playback with longer intervals.

### `$r` — Enable Random (Short Interval)

**Syntax:** `$r\r`

**Description:** Enables random sound playback with shorter intervals.

### Volume Control

| Command | Description |
|---------|-------------|
| `$-\r` | Volume down |
| `$m\r` | Volume mid |
| `$+\r` | Volume up |
| `$f\r` | Volume max |

---
## Sequence Commands

**Source:** `MarcduinoSequence.h` · **Always enabled**

### General Sequences

| Command | Name | Description |
|---------|------|-------------|
| `:SE00\r` | Stop | Stop all sequences |
| `:SE01\r` | Scream | Full scream sequence |
| `:SE02\r` | Wave | Panel wave |
| `:SE03\r` | Smirk Wave | Smirk wave combo |
| `:SE04\r` | Open/Close Wave | Open-close wave |

### Mode Resets

| Command | Name | Description |
|---------|------|-------------|
| `:SE10\r` | Quiet Mode | Sleep mode entry |
| `:SE11\r` | Full Awake | Full awake mode |
| `:SE12\r` | Top Showcase | Top panels demo |
| `:SE13\r` | Mid Awake | Mid awake mode |
| `:SE14\r` | Awake Plus | Awake+ mode |
| `:SE15\r` | Scream No Panels | Scream without panels |
| `:SE16\r` | Panel Wiggle | Panel wiggle animation |

### Panel Sequences

| Command | Name | Description |
|---------|------|-------------|
| `:SE50\r` | Scream No Panel | Scream without panel movement |
| `:SE51\r` | Scream Panel | Scream with panels |
| `:SE52\r` | Wave Panel | Wave with panels |
| `:SE53\r` | Smirk Wave Panel | Smirk wave with panels |
| `:SE54\r` | Open Wave | Open wave sequence |
| `:SE55\r` | Marching Ants | Marching ants animation |
| `:SE56\r` | Faint | Faint animation |
| `:SE57\r` | Rhythmic | Rhythmic pattern |
| `:SE58\r` | Wave Bye Bye | Wave goodbye sequence |

---
## Panel Calibration Commands

**Source:** `MarcduinoPanel.h` · **Always enabled**

### `:MV` — Move Panel (Temporary)

**Syntax:** `:MV<group><pos>\r`

Move panel group to temporary position.

### `#SO` — Save Open Position

**Syntax:** `#SO<group><pos>\r`

Store open position for panel group.

### `#SC` — Save Closed Position

**Syntax:** `#SC<group><pos>\r`

Store closed position for panel group.

### `#SW` — Swap Open/Closed

**Syntax:** `#SW<group>\r`

Swap open and closed positions.

### Target Groups

- `00` = All panels
- `01`–`10` = Panel groups
- `11` = Pie group
- `12` = Lower dome
- `13` = Top center
- `14` = Top panels
- `15` = Bottom panels

### Value Formats

- Degrees: `0000`–`0180`
- Pulse width: `0544`–`2500`

---
## Dynamic Panel Group Commands

**Source:** `MarcduinoPanel.h` · **Always enabled**

### Basic Group Commands

| Command | Description |
|---------|-------------|
| `:OP$\r` | Open panel group |
| `:CL$\r` | Close panel group |
| `:OF$\r` | Flutter panel group |

### Choreography Commands

| Command | Description |
|---------|-------------|
| `:OW$\r` | Wave panel group |
| `:OWF$\r` | Fast wave |
| `:OWC$\r` | Open/close wave |
| `:OCR$\r` | Open/close repeat |
| `:OC$\r` | Open/close group |
| `:OCL$\r` | Open/close long |
| `:OMA$\r` | Marching ants |
| `:OAP$\r` | Alternate panel |
| `:OD$\r` | Dance pattern |
| `:OS$\r` | Shake pattern |

---
## Direct Servo Commands

**Source:** `MarcduinoPanel.h` · **Always enabled**

### `:SQ` — Set Servo Position

**Syntax:** `:SQ<servo><pos>\r`

Directly set servo position (bypasses panel logic).

### `:SL` — Set Servo Limits

**Syntax:** `:SL<servo><min><max>\r`

Set servo travel limits.

### `:SM` — Move Servos

**Syntax:** `:SM<group><pos>\r`

Move servo group to position.
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
## Extended Logic Effects

**Source:** `MarcduinoLogics.h` · **Always enabled**

### Rainbow Effect

- `@0T12\r` — All logics rainbow
- `@1T12\r` — Front logics rainbow  
- `@2T12\r` — Rear logics rainbow

### Lights Out Effect

- `@0T15\r` — All logics lights out
- `@1T15\r` — Front logics lights out
- `@2T15\r` — Rear logics lights out

### Fire Effect

- `@0T22\r` — All logics fire
- `@1T22\r` — Front logics fire
- `@2T22\r` — Rear logics fire

### Pulse Effect

- `@0T24\r` — All logics pulse
- `@1T24\r` — Front logics pulse
- `@2T24\r` — Rear logics pulse

---
## Extended Holo Commands

**Source:** `MarcduinoHolo.h` · **Always enabled**

### Twitch Control

| Command | Description | Reeltwo |
|---------|-------------|---------|
| `*HD07\r` | Disable auto-twitch | `HPS7` |
| `*HD08\r` | Enable default twitch | `HPS8` |
| `*HD09\r` | Enable random twitch | `HPS9` |

### Wag Commands

| Command | Description | Reeltwo |
|---------|-------------|---------|
| `*HW00\r` | Wag all holos | `HPA105|5` |
| `*HW03\r` | Wag top holo | `HPT105|5` |

### Nod Commands

| Command | Description | Reeltwo |
|---------|-------------|---------|
| `*HN00\r` | Nod all holos | `HPA106|5` |
| `*HN03\r` | Nod top holo | `HPT106|5` |

### Compatibility Commands

- `*CO<params>\r` — Color set (compatibility)
- `*CH<params>\r` — Center holo
- `*RC<params>\r` — RC-center
- `*TE<params>\r` — Test mode

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
