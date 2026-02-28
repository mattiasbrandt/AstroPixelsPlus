# AstroPixelsPlus — Setup & Installation Guide

> **Beginner-friendly guide** for first-time builders and R2-D2 enthusiasts.  
> This guide covers everything from flashing your first firmware to verifying each
> gadget feature works correctly.

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [Prerequisites](#2-prerequisites)
3. [Build Flags Reference (`platformio.ini`)](#3-build-flags-reference-platformioini)
4. [Compile-Time vs Runtime Enablement](#4-compile-time-vs-runtime-enablement)
5. [Step-by-Step First-Time Setup](#5-step-by-step-first-time-setup)
6. [Web UI Enablement Process](#6-web-ui-enablement-process)
7. [Hardware Wiring Overview](#7-hardware-wiring-overview)
8. [Verification Steps](#8-verification-steps)
9. [Feature Toggle System](#9-feature-toggle-system)
10. [Safety Checklist](#10-safety-checklist)
11. [Reeltwo Library Integration](#11-reeltwo-library-integration)
12. [Troubleshooting](#12-troubleshooting)

---

## 1. Quick Start

If you have your AstroPixels board, a USB cable, and PlatformIO installed, this gets you running in five commands:

```bash
# 1. Clone the repository
git clone https://github.com/your-fork/AstroPixelsPlus.git
cd AstroPixelsPlus

# 2. Install PlatformIO CLI (if not already installed)
pip install platformio

# 3. Build the firmware
pio run -e astropixelsplus

# 4. Upload firmware (connect board via USB first)
pio run -e astropixelsplus -t upload

# 5. Upload web UI assets to SPIFFS filesystem
pio run -e astropixelsplus -t uploadfs
```

After upload, connect to the **AstroPixels** Wi-Fi network (password: `Astromech`) and open **http://192.168.4.1** in your browser.

> **Note:** Firmware (`upload`) and filesystem (`uploadfs`) are two separate steps. You need **both** for the web UI to work.

---

## 2. Prerequisites

### Hardware

| Item | Details |
|---|---|
| AstroPixels board | [Order here](https://we-make-things.co.uk/product/astropixels/) |
| USB-A to Micro-USB cable | For initial flash; data cable (not charge-only) |
| 5 V power supply | For LEDs and servos — separate from USB |
| WS2812B LED strips | Front/Rear Logic, Holos, PSI |
| PCA9685 servo controllers | Two required: 0x40 (panels), 0x41 (holos) |
| Servos | Up to 12 dome panels + 6 holo axes |

### Software

| Tool | Version | Notes |
|---|---|---|
| [PlatformIO](https://platformio.org) | 6.x or later | Recommended: VS Code + PlatformIO extension |
| Python | 3.8+ | Required for PlatformIO CLI |
| Git | Any | For cloning and updates |

### Optional (for advanced features)

- **DFPlayer Mini** — for sound playback (`AP_ENABLE_SOUND`)
- **MAX7219 LED chain** — for Charge Bay Indicator (`AP_ENABLE_CBI`) or Data Panel (`AP_ENABLE_DATAPANEL`)

---

## 3. Build Flags Reference (`platformio.ini`)

All gadget features are enabled or disabled by **build flags** in `platformio.ini`. This is the canonical way to include or exclude hardware-specific code at compile time, keeping flash usage lean.

### Full `platformio.ini` Example

```ini
[platformio]
default_envs = astropixelsplus
src_dir = .

[env:astropixelsplus]
platform = https://github.com/platformio/platform-espressif32.git#v5.2.0
board = esp32dev
framework = arduino
lib_archive = true

; Serial monitor
monitor_speed = 115200
monitor_filters =
    default
    esp32_exception_decoder
    send_on_enter

; Source filter (include everything in project root)
build_src_filter =
    +<*>

; --- Library Dependencies (pinned for reproducible builds) ---
lib_deps =
    https://github.com/reeltwo/Reeltwo#23.5.3
    https://github.com/adafruit/Adafruit_NeoPixel#1.15.4
    fastled/FastLED@3.10.3
    https://github.com/DFRobot/DFRobotDFPlayerMini#V1.0.6
    mathieucarbou/ESPAsyncWebServer@3.5.1
    mathieucarbou/AsyncTCP@3.3.2

board_build.filesystem = spiffs
build_type = release

; --- Build Flags ---
build_flags =
    -DCORE_DEBUG_LEVEL=1

    ; =====================================================
    ; GADGET FEATURE FLAGS  (0 = disabled, 1 = enabled)
    ; =====================================================

    ; Droid Remote ESPNOW support (requires SMQ hardware)
    -DAP_ENABLE_DROID_REMOTE=0

    ; Bad Motivator LED gadget (wired to AUX5 / PIN 19)
    -DAP_ENABLE_BADMOTIVATOR=0

    ; Fire Strip LED effect (wired to AUX4 / PIN 18)
    -DAP_ENABLE_FIRESTRIP=0

    ; Charge Bay Indicator (MAX7219 chain, AUX1/2/3)
    -DAP_ENABLE_CBI=0

    ; Data Panel (MAX7219 chain, shares AUX1/2/3 with CBI)
    -DAP_ENABLE_DATAPANEL=0

    ; =====================================================
    ; IDENTITY
    ; =====================================================

    ; Droid display name shown on boot scroll and web UI
    -DAP_DROID_NAME=\"AstroPixels\"

    ; =====================================================
    ; PIN OVERRIDES  (change only if your wiring differs)
    ; =====================================================
    -DPIN_AUX4=18
    -DPIN_AUX5=19

    ; =====================================================
    ; ESP32 / COMPILER FLAGS  (do not change)
    ; =====================================================
    -DESP32_ARDUINO_NO_RGB_BUILTIN
    -mfix-esp32-psram-cache-issue
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
    -Os

; --- Port Configuration ---
; Uncomment and set for your OS:
; Linux/macOS:
;upload_port = /dev/ttyUSB0
;monitor_port = /dev/ttyUSB0
; Windows:
;upload_port = COM8
;monitor_port = COM8
;upload_speed = 115200
```

### Build Flag Quick Reference

| Flag | Default | Purpose |
|---|---|---|
| `AP_ENABLE_DROID_REMOTE` | `0` | ESPNOW Droid Remote controller pairing |
| `AP_ENABLE_BADMOTIVATOR` | `0` | Bad Motivator LED gadget |
| `AP_ENABLE_FIRESTRIP` | `0` | Fire Strip LED effect |
| `AP_ENABLE_CBI` | `0` | Charge Bay Indicator (MAX7219) |
| `AP_ENABLE_DATAPANEL` | `0` | Data Panel (MAX7219) |
| `AP_DROID_NAME` | `"AstroPixels"` | Droid name on boot scroll + web branding |
| `PIN_AUX4` | `18` | Auxiliary pin 4 (Sound RX / FireStrip data) |
| `PIN_AUX5` | `19` | Auxiliary pin 5 (Sound TX / BadMotivator / RLD clock) |
| `CORE_DEBUG_LEVEL` | `1` | ESP-IDF log verbosity (0=none, 5=verbose) |

---

## 4. Compile-Time vs Runtime Enablement

AstroPixelsPlus uses a **two-layer control model**. Understanding which layer applies to which setting saves a lot of confusion.

### Compile-Time (Build Flags)

**What:** C preprocessor `#define` values set in `platformio.ini`.  
**When to use:** For features that require different hardware wiring or different code being compiled in. If the flag is `0`, the code for that gadget is **not compiled at all** — saving flash and RAM.  
**How to change:** Edit `platformio.ini`, then **rebuild and re-upload** firmware.

```ini
; To enable Bad Motivator:
-DAP_ENABLE_BADMOTIVATOR=1

; Then rebuild:
; pio run -e astropixelsplus -t upload
```

**Compile-time flags include:**
- `AP_ENABLE_BADMOTIVATOR`
- `AP_ENABLE_FIRESTRIP`
- `AP_ENABLE_CBI`
- `AP_ENABLE_DATAPANEL`
- `AP_ENABLE_DROID_REMOTE`
- `AP_DROID_NAME`

### Runtime (Preferences / Web UI / Serial Commands)

**What:** Settings stored in ESP32 non-volatile storage (NVS Preferences), changed without reflashing.  
**When to use:** For operational toggles like WiFi on/off, sound module type, volume, serial baud rates.  
**How to change:** Via the **web UI**, via a **Marcduino serial command**, or via the `/api/*` REST endpoints.

```
# Examples (send over serial or web API):
#APWIFI1      → Enable WiFi
#APWIFI0      → Disable WiFi
#APRESTART    → Restart ESP32
#APZERO       → Factory reset (clears ALL preferences)
```

**Runtime preferences include:**
- WiFi SSID and password
- WiFi enabled/disabled
- Sound module type and volume
- Serial baud rates
- Panel servo calibration (open/close positions)
- Droid remote hostname and secret
- Artoo telemetry baud rate

### Decision Guide

```
┌─────────────────────────────────────────────────────────────────┐
│ Do you need to add/remove a physical gadget?                    │
│                                                                 │
│  YES → Use build flag in platformio.ini, rebuild & re-upload   │
│  NO  → Use web UI, serial command, or REST API                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. Step-by-Step First-Time Setup

Follow these steps in order for a clean first-time installation.

### Step 1 — Install PlatformIO

**Option A: VS Code (recommended for beginners)**
1. Install [VS Code](https://code.visualstudio.com/)
2. Open Extensions (`Ctrl+Shift+X` / `Cmd+Shift+X`)
3. Search **PlatformIO IDE** and install it
4. Restart VS Code

**Option B: Command line only**
```bash
pip install platformio
```

### Step 2 — Clone the Repository

```bash
git clone https://github.com/your-fork/AstroPixelsPlus.git
cd AstroPixelsPlus
```

### Step 3 — Configure Your Port

Find your ESP32's serial port:
- **Linux:** `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
- **macOS:** `ls /dev/cu.usbserial*`
- **Windows:** Device Manager → Ports (COM & LPT) → look for CP210x or CH340

Uncomment and set the port in `platformio.ini`:

```ini
; Linux/macOS:
upload_port = /dev/ttyUSB0
monitor_port = /dev/ttyUSB0

; Windows:
; upload_port = COM8
; monitor_port = COM8
```

### Step 4 — Enable Your Gadget Features

Edit `platformio.ini` and set flags for the hardware you have connected.  
**Start with everything at `0`** for a baseline test, then enable one gadget at a time.

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=1
    -DAP_ENABLE_DROID_REMOTE=0   ; change to 1 if you have a Droid Remote
    -DAP_ENABLE_BADMOTIVATOR=0   ; change to 1 if Bad Motivator is wired
    -DAP_ENABLE_FIRESTRIP=0      ; change to 1 if Fire Strip is wired
    -DAP_ENABLE_CBI=0            ; change to 1 if Charge Bay Indicator present
    -DAP_ENABLE_DATAPANEL=0      ; change to 1 if Data Panel present
    -DAP_DROID_NAME=\"AstroPixels\"
    -DPIN_AUX4=18
    -DPIN_AUX5=19
    -DESP32_ARDUINO_NO_RGB_BUILTIN
    -mfix-esp32-psram-cache-issue
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
    -Os
```

### Step 5 — Build and Upload Firmware

```bash
# Build only (check for errors first)
pio run -e astropixelsplus

# Upload firmware to board
pio run -e astropixelsplus -t upload
```

Watch the serial monitor during upload:
```bash
pio device monitor -p /dev/ttyUSB0 -b 115200
```

Expected boot output (first time):
```
=== I2C DIAGNOSTICS ===
Initializing I2C on SDA=21, SCL=22
✓ I2C device found at address 0x40 PCA9685 (Panels) ← EXPECTED
✓ I2C device found at address 0x41 PCA9685 (Holos) ← EXPECTED
=== END I2C DIAGNOSTICS ===
Ready
```

### Step 6 — Upload the Web UI Filesystem

The web interface lives in the `data/` folder and must be uploaded separately:

```bash
pio run -e astropixelsplus -t uploadfs
```

> **Important:** If the board shows `Failed to mount read only filesystem` in serial output, it means `uploadfs` was not run yet — or the SPIFFS partition is corrupted. Run `uploadfs` and restart.

### Step 7 — First Boot Verification

1. Open serial monitor (`pio device monitor -b 115200`)
2. The board should scroll `... AstroPixels ...` (or your droid name) on the logic displays
3. Wait for the `Ready` message in serial
4. Connect to WiFi SSID **AstroPixels** (password: **Astromech**)
5. Open **http://192.168.4.1** in your browser
6. You should see the AstroPixels web interface home page

### Step 8 — Change Default WiFi Password

**Do this immediately.** Go to the web UI:
1. Navigate to **Setup** → **WiFi**
2. Change the password from `Astromech` to something unique
3. Click **Save** and wait for the board to restart

---

## 6. Web UI Enablement Process

The web interface is served from the SPIFFS filesystem (`data/` directory) via the async web server.

### SPIFFS Asset Upload

The web UI consists of static HTML/CSS/JavaScript files that must be uploaded separately from the firmware:

```bash
# Upload web assets (run after every change to data/ files)
pio run -e astropixelsplus -t uploadfs

# Or via VS Code: PlatformIO sidebar → astropixelsplus → Platform → Upload Filesystem Image
```

> **Note:** OTA (Over-the-Air) firmware updates only update the **firmware binary**, not the SPIFFS filesystem. If you update the web UI files, you must upload the filesystem via USB.

### Web Pages Overview

| URL | Purpose |
|---|---|
| `/` | Home — system health, quick actions, sleep/wake |
| `/panels.html` | Dome panel control + calibration |
| `/holos.html` | Holoprojector lights + servo positions |
| `/logics.html` | Logic display effects and text |
| `/sequences.html` | Predefined panel + holo choreography |
| `/sound.html` | Sound module control |
| `/serial.html` | MarcDuino serial and Artoo settings |
| `/wifi.html` | WiFi SSID/password configuration |
| `/setup.html` | Gadget feature toggles + droid naming |
| `/remote.html` | Droid Remote ESPNOW pairing |
| `/firmware.html` | OTA firmware upload |

### REST API Endpoints

For automation or integration, the board exposes a REST API:

```bash
# Get system state
curl http://192.168.4.1/api/state

# Send a MarcDuino command
curl -X POST http://192.168.4.1/api/cmd -d "cmd=:OP00"

# Get health status
curl http://192.168.4.1/api/health

# Full I2C diagnostics
curl http://192.168.4.1/api/diag/i2c?force=1

# Enter soft sleep mode
curl -X POST http://192.168.4.1/api/sleep

# Wake from sleep
curl -X POST http://192.168.4.1/api/wake
```

### WebSocket Live Updates

The web UI connects to `/ws` for real-time state updates. If you build custom integrations, connect a WebSocket client to `ws://192.168.4.1/ws` to receive live state/health JSON broadcasts.

### Accessing the Board by Name

If the board connects to your existing WiFi network (client mode instead of AP mode), mDNS allows hostname access:

```
http://astropixels.local
```

> mDNS only works in client mode (when the board joins your network). In the default Access Point mode, use the IP `192.168.4.1`.

---

## 7. Hardware Wiring Overview

### ESP32 Pin Assignments

```
┌────────────────────────────────────────────────────────────────┐
│                    AstroPixels ESP32                           │
│                                                                │
│  LED OUTPUTS                                                   │
│  PIN 15  ──→  Front Logic Displays (FLD) — WS2812B data       │
│  PIN 33  ──→  Rear Logic Display (RLD) — WS2812B data         │
│  PIN 32  ──→  Front PSI — WS2812B data                        │
│  PIN 23  ──→  Rear PSI — WS2812B data                         │
│  PIN 25  ──→  Front Holo RGB — WS2812B data                   │
│  PIN 26  ──→  Rear Holo RGB — WS2812B data                    │
│  PIN 27  ──→  Top Holo RGB — WS2812B data                     │
│                                                                │
│  I2C BUS (Servo Controllers)                                   │
│  PIN 21  ──→  SDA                                             │
│  PIN 22  ──→  SCL                                             │
│                                                                │
│  SERIAL                                                        │
│  PIN 16  ──→  Serial2 RX  (MarcDuino commands in)            │
│  PIN 17  ──→  Serial2 TX  (pass-through out)                  │
│                                                                │
│  AUXILIARY PINS (multipurpose — depends on build flags)        │
│  PIN 2   ──→  AUX1: CBI Load                                  │
│  PIN 4   ──→  AUX2: CBI Clock                                 │
│  PIN 5   ──→  AUX3: CBI Data In                              │
│  PIN 18  ──→  AUX4: Sound RX / FireStrip data                 │
│  PIN 19  ──→  AUX5: Sound TX / BadMotivator / RLD curved clk  │
└────────────────────────────────────────────────────────────────┘
```

### I2C Servo Controllers (PCA9685)

Two PCA9685 boards are required:

```
I2C Address 0x40 — Dome Panels Controller
  Ch  0  →  Small Panel 1  (front lower left)
  Ch  1  →  Small Panel 2  (front lower right)
  Ch  2  →  Small Panel 3  (left side lower)
  Ch  3  →  Small Panel 4  (right side lower)
  Ch  4  →  Medium Panel 5 (Magic Panel — fixed, no servo)
  Ch  5  →  Large Panel 6  (front upper)
  Ch  6  →  Mini Panel A
  Ch  7  →  Mini Panel B
  Ch  8  →  Pie Panel 7
  Ch  9  →  Pie Panel 8
  Ch 10  →  Pie Panel 9
  Ch 11  →  Pie Panel 10
  Ch 12  →  Top Center Panel

I2C Address 0x41 — Holoprojectors Controller
  Ch  0  →  Front Holo — Horizontal servo
  Ch  1  →  Front Holo — Vertical servo
  Ch  2  →  Top Holo — Horizontal servo
  Ch  3  →  Top Holo — Vertical servo
  Ch  4  →  Rear Holo — Vertical servo
  Ch  5  →  Rear Holo — Horizontal servo
```

> **I2C address jumpers:** Set the address on each PCA9685 board using the A0–A5 solder jumpers. For 0x40, all jumpers open (default). For 0x41, bridge only the A0 jumper.

### Gadget-Specific Wiring

#### Bad Motivator (AP_ENABLE_BADMOTIVATOR=1)

The Bad Motivator is a dome gadget with LEDs that smoke/spark. It connects to a single data pin.

```
BadMotivator data pin  →  PIN_AUX5 (default PIN 19)
Power (5V)             →  5V supply (not ESP32)
GND                    →  Common ground
```

> If `PIN_AUX5` conflicts with your sound module TX, you can remap using `-DPIN_AUX5=X` in `platformio.ini`.

#### Fire Strip (AP_ENABLE_FIRESTRIP=1)

The Fire Strip is an LED strip with a fire animation effect.

```
FireStrip data pin  →  PIN_AUX4 (default PIN 18)
Power (5V)          →  5V supply (not ESP32)
GND                 →  Common ground
```

> **Conflict note:** PIN_AUX4 doubles as Sound module RX. Do not enable both `AP_ENABLE_FIRESTRIP=1` and a sound module on the same pin without remapping.

#### Charge Bay Indicator / Data Panel (AP_ENABLE_CBI=1 or AP_ENABLE_DATAPANEL=1)

Both gadgets use a MAX7219 LED driver chain connected over SPI-like signals:

```
CBI / Data Panel  →  AUX3 (PIN 5)  — Data In
CBI / Data Panel  →  AUX2 (PIN 4)  — Clock
CBI / Data Panel  →  AUX1 (PIN 2)  — Load / CS
Power (5V)        →  5V supply
GND               →  Common ground
```

> You can enable CBI and DataPanel simultaneously on the same chain — they share the same `LedControlMAX7221` object.

#### Sound Module (DFPlayer Mini)

Sound is configured at runtime, not via a build flag. Wire it and enable it from the web UI:

```
DFPlayer Mini TX  →  PIN 18 (AUX4 / Sound RX)
DFPlayer Mini RX  →  PIN 19 (AUX5 / Sound TX)
DFPlayer Mini VCC →  5V supply
DFPlayer Mini GND →  Common ground
```

Then in the web UI: **Setup** → **Sound** → select `DFPlayer Mini`.

#### RSeries Logic Displays (Alternative Logic Hardware)

If you use RSeries logic displays instead of standard AstroPixel displays:

```ini
; In AstroPixelsPlus.ino, uncomment the relevant line:
#define USE_RSERIES_FLD         ; Front Logic Display (RSeries)
#define USE_RSERIES_RLD         ; Rear Logic Display (RSeries, flat)
#define USE_RSERIES_RLD_CURVED  ; Rear Logic Display (RSeries, curved — uses AUX5 as clock)
```

> These are code-level defines, not `platformio.ini` build flags. Edit `AstroPixelsPlus.ino` directly and rebuild.

### Wiring Diagram

See the included wiring diagram files in the repository root:
- `Wiring-Diagram.pdf` — Full-resolution wiring reference
- `Wiring-Diagram.png` — Quick-reference image

---

## 8. Verification Steps

Verify each feature independently after wiring. Use the serial monitor and web UI together.

### 8.1 — I2C Bus (Servo Controllers)

At boot, the firmware scans I2C and prints results to serial:

```
Expected output:
  ✓ I2C device found at address 0x40  PCA9685 (Panels) ← EXPECTED
  ✓ I2C device found at address 0x41  PCA9685 (Holos) ← EXPECTED
  ✓ Found 2 I2C device(s)
```

If you see `❌ NO I2C DEVICES FOUND!`, check SDA/SCL wiring and I2C address jumpers.

You can also force a full I2C scan at any time via API:
```bash
curl http://192.168.4.1/api/diag/i2c?force=1
```

### 8.2 — Logic Displays (FLD + RLD)

**Test command** (send via serial or web UI → Logics):

```
@0T2     →  All logics: Flashing Color (visible blink on all displays)
@0T1     →  All logics: Normal (return to default)
@1MHello →  Front logics: scroll text "Hello"
@3MWorld →  Rear logic: scroll text "World"
```

**Expected:** LEDs light up with the specified effect. If dark, check:
- Data pin wiring (FLD → PIN 15, RLD → PIN 33)
- LED strip 5V power connected
- `ESP32_ARDUINO_NO_RGB_BUILTIN` flag is set in `platformio.ini`

### 8.3 — PSI Lights

```
; No standard standalone test command — PSI follows logic state
@0T3   →  Alarm mode — PSI lights change color with logics
@0T1   →  Return to normal
```

**Expected:** PSI lights cycle colors in sync with logic sequences.

### 8.4 — Holoprojectors

**LEDs only:**
```
*ON01  →  Front Holo on
*OF01  →  Front Holo off
*ON00  →  All Holos on
*OF00  →  All Holos off
```

**Servos (movement):**
```
*RD01  →  Front Holo random movement
*HN01  →  Front Holo nod
*ST00  →  Stop all holo servos
```

**Position test:**
```
*HP001  →  Front Holo to position: down
*HP101  →  Front Holo to position: center
*HP201  →  Front Holo to position: up
```

**Expected:** Holo LEDs illuminate and servos move. If LEDs work but servos do not, check the 0x41 I2C address and servo power supply.

### 8.5 — Dome Panels

```
:OP00   →  Open ALL panels
:CL00   →  Close ALL panels
:OP01   →  Open Panel 1 only
:CL01   →  Close Panel 1 only
:SE01   →  Run Panel Sequence 1 (choreography)
```

**Expected:** Servos move panels. If no movement:
1. Check I2C 0x40 is detected at boot
2. Check servo 5–6V power (servos need more current than USB can supply)
3. Verify servo min/max pulse range in `servoSettings[]`

### 8.6 — Bad Motivator (if enabled)

```
; Send via serial or web API:
BMON   →  Bad Motivator on
BMOFF  →  Bad Motivator off
```

**Expected:** LEDs on the Bad Motivator gadget illuminate/animate.

### 8.7 — Fire Strip (if enabled)

```
FSON   →  Fire Strip on
FSOFF  →  Fire Strip off
```

**Expected:** Fire animation plays on the LED strip.

### 8.8 — Charge Bay Indicator / Data Panel (if enabled)

```
CB11111  →  CBI all indicators on
CB00000  →  CBI off
DP11111  →  Data Panel indicators on
DP00000  →  Data Panel off
```

### 8.9 — Sound Module (if configured)

Send a test sound via the web UI (Sound page → Play) or via serial:
```
$1   →  Play track 1
$85  →  Common R2 sound (bank-dependent on your SD card)
```

**Expected:** Audio plays through the speaker connected to the DFPlayer.

### 8.10 — WiFi and Web Interface

1. Connect to SSID **AstroPixels** (password: **Astromech**)
2. Open `http://192.168.4.1`
3. Verify the home page loads with system health metrics
4. Check that free heap is shown and is above ~100 KB

If the page is blank or shows a filesystem error, re-run:
```bash
pio run -e astropixelsplus -t uploadfs
```

### 8.11 — OTA Firmware Update (after initial USB setup)

1. Connect to the board's WiFi
2. In VS Code PlatformIO: change `upload_port = 192.168.4.1` in `platformio.ini`
3. Run `pio run -e astropixelsplus -t upload`
4. Or use the web UI → **Firmware** page → upload a `.bin` file directly

---

## 9. Feature Toggle System

AstroPixelsPlus uses a **layered feature toggle system** that operates at both compile time and runtime.

### Layer 1: Build Flags (Compile-Time Gate)

Build flags control which code is **compiled into the firmware binary**. Features gated behind a `0` flag generate zero binary overhead — no code, no RAM, no flash.

```c
// Internal guard pattern used throughout the code:
#ifndef AP_ENABLE_BADMOTIVATOR
#define AP_ENABLE_BADMOTIVATOR 0
#endif

#if AP_ENABLE_BADMOTIVATOR
BadMotivator badMotivator(PIN_AUX5);
#endif
```

This means:
- Setting a flag to `0` = feature **does not exist** in this firmware build
- Setting a flag to `1` = feature code is included and the gadget object is instantiated
- **You must rebuild and re-upload after changing flags**

### Layer 2: Runtime Preferences (NVS)

Runtime preferences are stored in ESP32 NVS flash under the `"astro"` namespace. They persist across reboots and power cycles until cleared with `#APZERO`.

| Preference Key | Type | Purpose |
|---|---|---|
| `wifi` | bool | WiFi enabled/disabled |
| `ssid` | string | WiFi SSID |
| `pass` | string | WiFi password |
| `ap` | bool | Access Point vs client mode |
| `remote` | bool | Droid Remote enabled |
| `msound` | int | Sound module type |
| `mvolume` | int | Sound volume (0–1000) |
| `dname` | string | Droid display name |
| `mserial` | bool | MarcDuino serial enabled |
| `mserial2` | int | Serial2 baud rate |
| `artoo` | bool | Artoo telemetry enabled |
| `msoundlocal` | bool | Local sound execution enabled |
| `badmot` | bool | Bad Motivator runtime enable |
| `firest` | bool | Fire Strip runtime enable |
| `cbienb` | bool | CBI runtime enable |
| `dpenab` | bool | Data Panel runtime enable |

### Layer 3: Command-Level Toggles (Runtime Commands)

Some behaviors can be toggled during operation without touching preferences:

```
#APWIFI0    →  Disable WiFi (takes effect after reboot)
#APWIFI1    →  Enable WiFi
#APREMOTE0  →  Disable Droid Remote
#APREMOTE1  →  Enable Droid Remote
#APZERO     →  Factory reset — CLEARS ALL PREFERENCES
#APRESTART  →  Restart the ESP32
```

### Layer 4: Sleep Mode (Operational Mode)

A soft sleep mode silences all outputs while keeping the ESP32, WiFi, and web server online:

```bash
curl -X POST http://192.168.4.1/api/sleep   # Enter sleep
curl -X POST http://192.168.4.1/api/wake    # Wake up
```

While in sleep mode:
- Logic displays → lights out
- PSI lights → lights out
- Holos → off
- Panels → closed
- Sound → suspended
- Incoming MarcDuino commands are blocked (except wake-profile commands)

This is useful for exhibitions — one button puts R2 into standby without power cycling.

---

## 10. Safety Checklist

Before powering on with all hardware connected, run through this checklist.

### Power Safety

- [ ] LEDs and servos are powered by a **dedicated 5V supply**, not the ESP32's 3.3V or USB
- [ ] The 5V supply has enough current capacity (estimate: 60mA per holo, ~10mA/LED for logic strips at full brightness, 1–2A per servo at stall)
- [ ] Common ground is shared between the ESP32, 5V supply, and all peripherals
- [ ] No bare wires touching metal dome parts

### Servo Safety

- [ ] Servo pulse range is set correctly in `servoSettings[]` (default 800–2200 µs)
- [ ] Panels physically clear their full range of motion before running `:OP00`
- [ ] Run `:CL00` before powering off to return panels to closed/safe position
- [ ] Do not leave servos powered and commanded to a position for extended periods without mechanical support

### WiFi Security

- [ ] Default password **Astromech** has been changed via the web UI
- [ ] If operating in client mode (joining your LAN), be aware the web UI has no login authentication — treat it as a trusted-network tool only

### Firmware Safety

- [ ] **`#APZERO` clears all preferences** — use with caution; you will need to reconfigure WiFi and all settings after a factory reset
- [ ] Keep a note of your WiFi credentials — if you forget the password and need to recover, use `#APZERO` over serial (USB connection required)
- [ ] After enabling a new gadget build flag, always test with the serial monitor open so you can see any boot-time errors before closing the enclosure

### I2C Health Check

- [ ] Both PCA9685 controllers appear in the boot I2C scan (0x40 and 0x41)
- [ ] If either is missing, do **not** run panel/holo commands — servos may behave unexpectedly with partial I2C detection
- [ ] Use `curl http://192.168.4.1/api/health` to check I2C status after boot

---

## 11. Reeltwo Library Integration

AstroPixelsPlus is built on the **ReelTwo** R2-D2 component library by [reeltwo](https://github.com/reeltwo/Reeltwo). Understanding the library helps when troubleshooting or extending the firmware.

### What ReelTwo Provides

| Component | ReelTwo Class | Used In |
|---|---|---|
| Front Logic Displays | `AstroPixelFLD<PIN>` | `AstroPixelsPlus.ino` |
| Rear Logic Display | `AstroPixelRLD<PIN>` | `AstroPixelsPlus.ino` |
| RSeries FLD (alt) | `LogicEngineDeathStarFLD<PIN>` | `AstroPixelsPlus.ino` (ifdef) |
| Front/Rear PSI | `AstroPixelFrontPSI<PIN>` | `AstroPixelsPlus.ino` |
| Holoprojectors | `HoloLights` | `AstroPixelsPlus.ino` |
| Fire Strip | `FireStrip` | `dome/FireStrip.h` |
| Bad Motivator | `BadMotivator` | `dome/BadMotivator.h` |
| Charge Bay Indicator | `ChargeBayIndicator` | `body/ChargeBayIndicator.h` |
| Data Panel | `DataPanel` | `body/DataPanel.h` |
| Servo dispatcher | `ServoDispatchPCA9685` | PCA9685 I2C servo control |
| Servo sequencer | `ServoSequencer` | Choreographed panel animations |
| Animation player | `AnimationPlayer` | Timed sequences |
| MarcDuino parser | `Marcduino` (macro) | Command dispatch |

### The MarcDuino Command Pattern

All command handlers use the `MARCDUINO_ACTION` macro from ReelTwo. This is important to know if you add new commands:

```cpp
// Pattern: MARCDUINO_ACTION(HandlerName, CommandPrefix, ({ code }))
MARCDUINO_ACTION(AllLogicsNormal, @0T1, ({
    FLD.selectSequence(LogicEngineRenderer::NORMAL);
    RLD.selectSequence(LogicEngineRenderer::NORMAL);
}))
```

The macro registers the handler globally. Commands arrive via:
- Serial2 (2400 baud, MarcDuino-compatible devices)
- USB Serial (115200 baud, for testing)
- WiFi API (`POST /api/cmd`)
- WebSocket (`/ws`)

All paths converge into `Marcduino::processCommand(player, cmd)`.

### AnimatedEvent Loop

ReelTwo uses a cooperative animation loop. In `loop()`:

```cpp
void loop() {
    mainLoop();
}

void mainLoop() {
    AnimatedEvent::process();  // ReelTwo drives all LED and servo animations here
    // ... your code ...
}
```

**Do not add blocking delays inside `loop()`.** ReelTwo animations depend on `AnimatedEvent::process()` being called as fast as possible.

### Library Version

The pinned version is `Reeltwo#23.5.3` in `platformio.ini`. If you update it, re-run the full build and verify all hardware functions as expected — ReelTwo API changes occasionally between releases.

### ReelTwo Documentation and Resources

| Resource | URL |
|---|---|
| ReelTwo GitHub | https://github.com/reeltwo/Reeltwo |
| ReelTwo Releases | https://github.com/reeltwo/Reeltwo/releases |
| Logic Engine Playground (Wokwi) | https://wokwi.com/projects/347975094870475347 |
| AstroPixels Board | https://we-make-things.co.uk/product/astropixels/ |
| Original AstroPixelsPlus | https://github.com/reeltwo/AstroPixelsPlus |
| Original Wiki | https://github.com/reeltwo/AstroPixelsPlus/wiki |
| MarcDuino Protocol Reference | https://www.curiousmarc.com/r2-d2/marcduino-system |
| R2 Builders Club Forums | https://astromech.net/forums/ |

---

## 12. Troubleshooting

### Board Won't Upload

**Symptom:** `No device found on /dev/ttyUSB0` or `A fatal error occurred: Failed to connect to ESP32`

**Steps:**
1. Check USB cable is a **data cable** (not charge-only)
2. Try a different USB port on your computer
3. Hold the **BOOT** button on the ESP32 while clicking Upload, then release once upload starts
4. On Linux, add yourself to the `dialout` group: `sudo usermod -aG dialout $USER` then log out and back in
5. Verify port name with `ls /dev/ttyUSB*` (Linux) or Device Manager (Windows)

### Web UI is Blank or Shows "Failed to mount filesystem"

**Symptom:** Browser shows nothing, or serial shows `Failed to mount read only filesystem`

**Fix:**
```bash
pio run -e astropixelsplus -t uploadfs
```

Then restart the board. The SPIFFS filesystem must be uploaded separately from the firmware.

### Can't Connect to WiFi Access Point

**Symptom:** SSID "AstroPixels" not visible, or password rejected

**Steps:**
1. Confirm the board booted (check serial monitor for `Ready`)
2. Default SSID: **AstroPixels**, Password: **Astromech**
3. If you changed and forgot the credentials, send `#APZERO` over USB serial to factory-reset
4. Enable WiFi via serial: `#APWIFI1` then `#APRESTART`

### I2C Devices Not Found

**Symptom:** Serial shows `❌ NO I2C DEVICES FOUND!` or one PCA9685 is missing

**Steps:**
1. Check SDA (PIN 21) and SCL (PIN 22) wiring to both PCA9685 boards
2. Verify I2C address jumpers: 0x40 = default (no jumpers), 0x41 = A0 only
3. Check 3.3V or 5V power to the PCA9685 boards (they require power to respond on I2C)
4. Try shorter I2C cables — long cables can cause glitches
5. Disconnect servos and LEDs temporarily to rule out a power issue affecting I2C

### Servos Not Moving

**Symptom:** Panel/holo commands accepted but no servo movement

**Checklist:**
1. I2C scan shows 0x40 at boot — if not, fix I2C first
2. Servo power supply (5–6V, separate from ESP32 USB) is connected and adequate
3. Servo connectors are fully inserted into PCA9685 (signal/+/GND orientation)
4. `servoSettings[]` pulse range (800–2200 µs) is appropriate for your servos

### LEDs Not Lighting Up

**Symptom:** Logic displays, PSI, or holo LEDs are dark

**Checklist:**
1. 5V power connected to LED strips (ESP32 3.3V is too low for WS2812B)
2. Data wire connected to the correct pin (see section 7)
3. `ESP32_ARDUINO_NO_RGB_BUILTIN` flag is present in `platformio.ini` build_flags
4. Test with: `@0T2` (flashing color) — this forces all logics active

### MarcDuino Commands Ignored

**Symptom:** Commands sent over serial are not doing anything

**Checklist:**
1. Commands must end with `\r` (carriage return), not `\n` or nothing
2. Serial2 baud rate is **2400** — this is slower than most serial devices, verify your sender
3. USB serial for testing is at **115200** baud
4. Verify the command format: `:SE00`, `*ON01`, `@0T1`, `#APWIFI` (exact prefix required)

### Sound Not Playing

**Symptom:** Sound commands accepted but no audio

**Checklist:**
1. DFPlayer Mini is wired: TX→PIN18, RX→PIN19
2. SD card is inserted in DFPlayer with correct folder structure (`/mp3/0001.mp3`)
3. Sound module is configured in the web UI (Setup → Sound) or `MARC_SOUND_PLAYER` is set
4. `msoundlocal` preference is true (check via `/api/state`)
5. Volume is set above 0 in web UI

### OTA Update Fails

**Symptom:** PlatformIO reports upload error over WiFi

**Checklist:**
1. Board and computer must be on the same WiFi network
2. `upload_port` in `platformio.ini` is set to the board's IP address
3. Sketch binary fits in the OTA partition (~1.4 MB limit)
4. If OTA is unreliable, fall back to USB upload

### Boot Crash / Restart Loop

**Symptom:** Serial shows repeated reboot messages

**Most common causes:**
1. `AP_ENABLE_CBI=1` or `AP_ENABLE_DATAPANEL=1` but MAX7219 hardware not connected — disable the flag and rebuild
2. `AP_ENABLE_BADMOTIVATOR=1` or `AP_ENABLE_FIRESTRIP=1` without hardware — disable and rebuild
3. Power supply insufficient — servo stall currents can cause brown-out resets

**Recovery:**
1. Disable all gadget flags in `platformio.ini`, rebuild, and re-upload clean firmware
2. Re-enable flags one at a time and verify hardware is connected before each

### Factory Reset (Last Resort)

If the board is in an unknown state and the web UI is inaccessible:

1. Connect via USB and open serial monitor at 115200 baud
2. Send: `#APZERO`
3. Board clears all preferences and restarts
4. Default WiFi is restored: SSID **AstroPixels**, password **Astromech**
5. Re-configure the board from scratch via the web UI

---

*For firmware change history, see [`FORK_IMPROVEMENTS.md`](../FORK_IMPROVEMENTS.md).*  
*For known issues and code-level findings, see [`CRITICAL_FINDINGS.md`](../CRITICAL_FINDINGS.md).*  
*For API endpoint reference, see [`REST_API_ENDPOINTS_SUMMARY.md`](../REST_API_ENDPOINTS_SUMMARY.md).*
