# AstroPixelsPlus Documentation

> Documentation index for AstroPixelsPlus firmware — ESP32 controller for R2-D2 dome visuals (logics, holoprojectors, panels) with async web control and Marcduino protocol compatibility.

---

## Available Documentation

| Document | Purpose | Audience |
|---|---|---|
| [Project README](../README.md) | Project overview, WiFi setup, Marcduino command reference | All users |
| [Fork Improvements](../FORK_IMPROVEMENTS.md) | Async web migration, protocol extensions, build system changes | Maintainers |
| [Critical Findings](../CRITICAL_FINDINGS.md) | Stability fixes, known bugs, confirmed-safe behavior | Maintainers |
| [REST API Reference](../REST_API_ENDPOINTS_SUMMARY.md) | HTTP endpoints — smoke, fire, CBI, DataPanel, state, health | Integrators |
| [Code Review Index](../CODE_REVIEW_INDEX.md) | Navigation guide for all code review documentation | Reviewers |
| [Code Review Quick Start](../CODE_REVIEW_QUICK_START.md) | 30-second checklist, 12 common issues, PR workflow | Reviewers |
| [ESP32 Code Review Template](../ESP32_CODE_REVIEW_TEMPLATE.md) | Deep reference: anti-patterns, OTA, WebSocket, memory | Reviewers |
| [Wiring Diagram (PNG)](../Wiring-Diagram.png) | Hardware pin and I2C wiring reference | Hardware builders |
| [Wiring Diagram (PDF)](../Wiring-Diagram.pdf) | Printable hardware wiring guide | Hardware builders |

---

## Quick Links

- **[Hardware & Wiring →](#hardware--wiring)**
- **[Command Reference →](#command-reference)**
- **[Setup Guide →](#setup-guide)**
- **[Reeltwo Library →](#reeltwo-library-references)**
- **[Troubleshooting →](#troubleshooting-quick-reference)**
- **[Code Review →](#code-review-documentation)**

---

## Suggested Reading Order

### New Builders — first-time hardware setup
1. [Hardware & Wiring](#hardware--wiring) — connect the board correctly before powering on
2. [Setup Guide](#setup-guide) — flash firmware and configure WiFi
3. [Configuration Commands](#configuration-commands) — basic preference setup
4. [Marcduino Commands](#marcduino-commands) — learn dome control commands
5. [Troubleshooting Quick Reference](#troubleshooting-quick-reference) — if anything goes wrong

### Integrators — controlling via serial or HTTP API
1. [Marcduino Commands](#marcduino-commands) — full command syntax
2. [REST API Endpoints](../REST_API_ENDPOINTS_SUMMARY.md) — HTTP control surface
3. [Fork Improvements → Marcduino Protocol Extensions](../FORK_IMPROVEMENTS.md#marcduino-protocol-extensions) — added aliases and compatibility handlers

### Maintainers / Developers
1. [Project README](../README.md) — base command reference and library overview
2. [Fork Improvements](../FORK_IMPROVEMENTS.md) — async migration rationale and all behavioral deltas
3. [Critical Findings](../CRITICAL_FINDINGS.md) — bugs fixed; read before new feature work
4. [Code Review Documentation](#code-review-documentation) — quality and safety standards
5. Source: `AstroPixelsPlus.ino`, `AsyncWebInterface.h`, `MarcduinoPanel.h`

---

## Hardware & Wiring

**Wiring diagrams:**
- [Wiring-Diagram.png](../Wiring-Diagram.png) — hardware pin and I2C wiring (inline)
- [Wiring-Diagram.pdf](../Wiring-Diagram.pdf) — printable guide

**Board:** AstroPixels (ESP32) — [we-make-things.co.uk](https://we-make-things.co.uk/product/astropixels/)

### ESP32 Pin Assignments

| Pin | Signal | Component |
|-----|--------|-----------|
| 21 | SDA | I2C data — servo controllers |
| 22 | SCL | I2C clock — servo controllers |
| 15 | DATA | Front Logic Displays (FLD) |
| 33 | DATA | Rear Logic Display (RLD) |
| 32 | DATA | Front PSI lights |
| 23 | DATA | Rear PSI lights |
| 25 | DATA | Front Holoprojector RGB |
| 26 | DATA | Rear Holoprojector RGB |
| 27 | DATA | Top Holoprojector RGB |
| 16 | RX2 | Serial2 — Marcduino in (2400 baud) |
| 17 | TX2 | Serial2 — Marcduino out (2400 baud) |

### I2C Devices

| Address | Device | Role |
|---------|--------|------|
| 0x40 | PCA9685 | Dome panel servos (Ch 0–12) |
| 0x41 | PCA9685 | Holoprojector servos (Ch 0–5) |

### Panel Servo Mapping (PCA9685 at 0x40)

| Channel | Panel | Description |
|---------|-------|-------------|
| Ch 0 | P1 | Small — front lower left |
| Ch 1 | P2 | Small — front lower right |
| Ch 2 | P3 | Small — left side lower |
| Ch 3 | P4 | Small — right side lower |
| Ch 4 | P5 | Medium — Magic Panel (fixed, no servo) |
| Ch 5 | P6 | Large — front upper |
| Ch 7 | P7 | Medium — left upper side |
| Ch 10 | P10 | Rear upper |
| Ch 11 | P11 | Top pie panel |

**Moving panels (7 total):** P1, P2, P3, P4, P7, P10, P11

### Holo Servo Mapping (PCA9685 at 0x41)

| Channels | Holoprojector |
|----------|---------------|
| Ch 0–1 | Front Holo (Horizontal / Vertical) |
| Ch 2–3 | Rear Holo (Horizontal / Vertical) |
| Ch 4–5 | Top Holo (Horizontal / Vertical) |

### Power Notes

> All WS2812B LED strips require a **separate 5V supply** — do not power from the ESP32 3.3V rail.
> Servos need a **5–6V supply** (not USB power).

---

## Command Reference

All Marcduino commands are terminated by `\r` (carriage return). The `@` prefix is optional on logic commands and is silently ignored.

### Marcduino Commands

#### Logic Display Commands

| Command | Effect |
|---------|--------|
| `@0T1` | All logics — Normal |
| `@0T2` | All logics — Flashing Color |
| `@0T3` | All logics — Alarm |
| `@0T4` | All logics — Failure (+ synced holo animation, auto-resets after 11 s) |
| `@0T5` | All logics — Red Alert / Scream (+ synced holo scream, auto-resets after 7 s) |
| `@0T6` | All logics — Leia (+ synced front holo Leia message, auto-resets after 45 s) |
| `@0T11` | All logics — March |
| `@0T12` | All logics — Rainbow |
| `@0T15` | All logics — Lights Out |
| `@0T22` | All logics — Fire |
| `@1T#` | Front logics only (same effect numbers as above) |
| `@2T#` | Rear logics only (same effect numbers as above) |
| `@1MText` | Set front logics scrolling text (scrolls left) |
| `@2MText` | Set rear logics scrolling text (scrolls left) |
| `@3MAstromech` | Set rear logic display text |
| `@1P60` / `@1P61` | Front logics font: Latin / Aurebesh |
| `@2P60` / `@2P61` | Bottom front logics font: Latin / Aurebesh |
| `@3P60` / `@3P61` | Rear logic font: Latin / Aurebesh |

#### APLE Sequence Format

```
@APLE[L][EE][C][S][NN]
```

| Field | Values | Meaning |
|-------|--------|---------|
| L | 0=All · 1=Front · 2=Rear | Logic target |
| EE | 00–23 · 99 | Effect (see table below) |
| C | 0–9 | Color (0=default · 1=Red · 2=Orange · 3=Yellow · 4=Green · 5=Cyan · 6=Blue · 7=Purple · 8=Magenta · 9=Pink) |
| S | 1–9 | Speed / sensitivity (5=default) |
| NN | 00–99 | Duration in seconds (00=continuous) |

**Effect codes (EE):**

| Code | Effect | Code | Effect |
|------|--------|------|--------|
| 00 | Normal reset | 12 | Mic Bright |
| 01 | Alarm | 13 | Mic Rainbow |
| 02 | Failure | 14 | Lights Out |
| 03 | Leia | 15 | Display Text |
| 04 | March | 16 | Text Scroll Left |
| 05 | Single Color | 17 | Text Scroll Right |
| 06 | Flashing Color | 18 | Text Scroll Up |
| 07 | Flip Flop | 19 | Roaming Pixel |
| 08 | Flip Flop Alt | 21 | Vertical Scan |
| 09 | Color Swap | 22 | Fire |
| 10 | Rainbow | 23 | PSI Color Wipe |
| 11 | Red Alert | 99 | Random |

**Quick examples:**

```
@APLE51000   Solid Red (continuous)
@APLE30000   Leia (continuous)
@APLE100500  Rainbow at speed 5 (continuous)
@APLE225000  Fire (continuous)
@APLE54008   Solid Green for 8 seconds
@APLE63315   Flashing Yellow at speed 3 for 15 seconds
```

#### Panel Commands

| Command | Effect |
|---------|--------|
| `:CL00` | Close all panels |
| `:OP00` | Open all panels |
| `:OF00` | Flutter all panels |
| `:SE##` | Run predefined sequence ## |
| `:OP##` | Open panel ## (01–12) |
| `:CL##` | Close panel ## (01–12) |
| `:OF##` | Flutter panel ## |
| `:SF##$#` | Set servo easing for panel ## |
| `:MV##dddd` | Move panel to temporary position (no persistence) |
| `#SO##dddd` | Store open position for panel ## |
| `#SC##dddd` | Store closed position for panel ## |
| `#SW##` | Swap open/closed positions for panel ## |

Panel calibration accepts `0000–0180` as degrees or `0544–2500` as direct pulse-width values.

#### Holo Commands

| Command | Effect |
|---------|--------|
| `*ON00` | All holos on (dim cycle) |
| `*OF00` | All holos off |
| `*ON01` / `*ON02` / `*ON03` | Front / Rear / Top holo on |
| `*OF01` / `*OF02` / `*OF03` | Front / Rear / Top holo off |
| `*RD00` / `*RD01` / `*RD02` / `*RD03` | Random servo movement — all / front / rear / top |
| `*HN00` / `*HN01` / `*HN02` / `*HN03` | Nod animation — all / front / rear / top |
| `*HPS3##` | Pulse LED effect (00=all · 01=front · 02=rear · 03=top) |
| `*HPS6##` | Rainbow LED effect (00=all · 01=front · 02=rear · 03=top) |
| `*HP0##` | Holo position: 0=down · 1=center · 2=up · 3=left · 4=upper-left · 5=lower-left · 6=right · 7=upper-right |
| `*ST00` | Reset all holos (servos + LEDs) |
| `*CH##` | Center holo ## |

> **Note:** Light effects (`*HPS…`) control LEDs only. Movement commands (`*RD`, `*HN`) control servos only. They are independent.

#### Configuration Commands

| Command | Effect |
|---------|--------|
| `#APWIFI` | Toggle WiFi |
| `#APWIFI0` / `#APWIFI1` | WiFi off / on |
| `#APREMOTE` | Toggle Droid Remote (ESPNOW) support |
| `#APREMOTE0` / `#APREMOTE1` | Droid Remote off / on |
| `#APZERO` | Factory reset — clears all saved preferences including WiFi |
| `#APRESTART` | Restart ESP32 |

### REST API Endpoints

Base URL: `http://192.168.4.1` (default AP) or `http://astropixels.local`

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/state` | Runtime state JSON (heap, WiFi, sleep, droid name, …) |
| GET | `/api/health` | I2C health summary (panel + holo controller status) |
| GET | `/api/diag/i2c?force=1` | Deep I2C bus scan with device details |
| POST | `/api/cmd` | Send Marcduino command (`cmd=:OP00`) |
| POST | `/api/sleep` | Enter soft sleep mode (gates most commands) |
| POST | `/api/wake` | Exit sleep, restore active state |
| POST | `/api/smoke` | Smoke effect — `state=on\|off` |
| POST | `/api/fire` | Fire strip — `state=on\|off` |
| GET | `/api/cbi` | Charge Bay Indicator current state |
| POST | `/api/cbi` | CBI control — `action=flicker\|disable` + optional `duration` |
| GET | `/api/datapanel` | DataPanel current state |
| POST | `/api/datapanel` | DataPanel control — `action=flicker\|disable` + optional `duration` |

All write endpoints require authentication via `X-AP-Token` header or `token` POST parameter (when a token is configured). Commands are blocked with HTTP 423 when the system is in sleep mode.

Full details: [REST_API_ENDPOINTS_SUMMARY.md](../REST_API_ENDPOINTS_SUMMARY.md)

---

## Setup Guide

### 1. Flash Firmware

**Web Installer (easiest — no toolchain required):**
```
https://reeltwo.github.io/AstroPixels-Installer/
```
*Requires Chrome or Edge.*

**PlatformIO CLI:**
```bash
pio run -e astropixelsplus             # Build
pio run -e astropixelsplus -t upload   # Upload firmware via USB
pio run -e astropixelsplus -t uploadfs # Upload web UI assets to SPIFFS
```

> **Important:** Firmware and filesystem are uploaded separately. Always run `uploadfs` after adding or changing files in `data/`. OTA updates only replace firmware — web assets still require USB `uploadfs`.

### 2. Connect to WiFi

Default Access Point credentials:

| Setting | Value |
|---------|-------|
| SSID | `AstroPixels` |
| Password | `Astromech` |
| Web UI | http://192.168.4.1 |
| mDNS | http://astropixels.local |

Change credentials via the **Setup** page in the web UI. Use `#APZERO` + restart to factory-reset.

### 3. Serial Monitor

```bash
pio device monitor -p /dev/ttyUSB0 -b 115200
```

Marcduino commands arrive on **Serial2** at **2400 baud** (RX=16, TX=17) and are echoed to USB Serial at 115200 baud.

### 4. Over-the-Air (OTA) Updates

Once WiFi is configured:
```bash
# Set in platformio.ini:  upload_port = 192.168.4.1
pio run -e astropixelsplus -t upload
```

Or use the **Firmware** page at `/firmware.html` to upload a `.bin` file via browser.

### 5. Web UI Pages

| Page | URL Path | Function |
|------|----------|----------|
| Home / Health | `/` | System status, quick actions, sleep/wake |
| Panels | `/panels.html` | Panel servo control and calibration |
| Holos | `/holos.html` | Holoprojector light effects and servo positions |
| Logics | `/logics.html` | Logic display effects, text, custom APLE builder |
| Sequences | `/sequences.html` | Choreographed panel + effect sequences |
| Sound | `/sound.html` | DFPlayer audio control |
| Setup | `/setup.html` | WiFi, droid name, Artoo, preferences |
| Serial | `/serial.html` | Serial port and baud configuration |
| Remote | `/remote.html` | Droid Remote ESPNOW host/secret pairing |
| Firmware | `/firmware.html` | OTA firmware update |

### 6. Validate Compatibility

```bash
# Dry-run to list command matrix entries
python3 tools/command_compat_matrix.py --dry-run

# Execute against live device
python3 tools/command_compat_matrix.py

# Test specific command family
python3 tools/command_compat_matrix.py --group panels
```

---

## Reeltwo Library References

AstroPixelsPlus is built on the **ReelTwo** R2-D2 animatronics library.

| Resource | Link |
|----------|------|
| ReelTwo GitHub | https://github.com/reeltwo/Reeltwo |
| ReelTwo Releases | https://github.com/reeltwo/Reeltwo/releases |
| AstroPixels Wiki | https://github.com/reeltwo/AstroPixelsPlus/wiki |
| Web Installer | https://reeltwo.github.io/AstroPixels-Installer/ |
| Wokwi Logic Playground | https://wokwi.com/projects/347975094870475347 |

### Key ReelTwo Modules Used

| Module | Role |
|--------|------|
| `dome/Logics.h` | Logic display base classes |
| `dome/LogicEngineController.h` | Logic animation controller |
| `dome/HoloLights.h` | Holoprojector RGB LED control |
| `dome/NeoPSI.h` | PSI RGB lights |
| `ServoDispatchPCA9685.h` | PCA9685 I2C servo controller driver |
| `ServoSequencer.h` | Choreographed servo sequences |
| `core/Marcduino.h` | MarcDuino command parser macros |
| `wifi/WifiAccess.h` | WiFi AP/Client management |

### MARCDUINO_ACTION Pattern

All command handlers use the ReelTwo macro:

```cpp
MARCDUINO_ACTION(CommandName, @0T1, ({
    FLD.selectSequence(LogicEngineRenderer::NORMAL);
    RLD.selectSequence(LogicEngineRenderer::NORMAL);
}))
```

**Source files by command prefix:**

| Prefix | Handler File | Domain |
|--------|-------------|--------|
| `@` | `MarcduinoLogics.h`, `MarcduinoSequence.h` | Logics, APLE sequences |
| `:` | `MarcduinoPanel.h` | Panels, calibration |
| `*` | `MarcduinoHolo.h` | Holoprojectors |
| `$` | `MarcduinoSound.h` | DFPlayer sound |
| `#AP` | `AstroPixelsPlus.ino` | Configuration |

### pinned dependency versions (platformio.ini)

```ini
lib_deps =
    https://github.com/reeltwo/Reeltwo#23.5.3
    https://github.com/adafruit/Adafruit_NeoPixel#1.15.4
    FastLED@3.10.3
    DFRobotDFPlayerMini#V1.0.6
    ESPAsyncWebServer@3.5.1
    AsyncTCP@3.3.2
```

---

## Code Review Documentation

For development quality and safety standards:

| Document | Purpose |
|----------|---------|
| [CODE_REVIEW_INDEX.md](../CODE_REVIEW_INDEX.md) | Navigation guide — which document to use when |
| [CODE_REVIEW_README.md](../CODE_REVIEW_README.md) | Overview, priority levels, file-by-file guidance |
| [CODE_REVIEW_QUICK_START.md](../CODE_REVIEW_QUICK_START.md) | 30-second pre-commit checklist, 12 common issues |
| [ESP32_CODE_REVIEW_TEMPLATE.md](../ESP32_CODE_REVIEW_TEMPLATE.md) | Deep ESP32 best-practice reference (1 600+ lines) |

**Pre-commit checklist (30 seconds):**
- No `delay()` in `loop()` or request handlers
- All heap allocations null-checked
- No hardcoded credentials
- Marcduino command buffer (64 bytes) not exceeded
- Commands validated before dispatch
- `FORK_IMPROVEMENTS.md` updated if behavior changed

---

## Troubleshooting Quick Reference

### Boot Crashes

**Symptom:** Crash on startup, serial shows panic or assert.

- Confirm `ESP32_ARDUINO_NO_RGB_BUILTIN` is defined in `platformio.ini` build flags (prevents FastLED RMT conflict).
- The legacy `WebPages.h` static-init heap crash (`vApplicationGetIdleTaskMemory`) does **not** apply to the current `AsyncWebInterface.h` build.
- Check serial at 115200 baud for the actual panic backtrace.

### WiFi Won't Start

1. Factory reset: send `#APZERO` via serial, then restart.
2. Default AP: SSID=`AstroPixels`, Password=`Astromech`.
3. Explicitly enable WiFi: `#APWIFI1`.
4. Monitor serial at 115200 for WiFi init messages.

### Servos Not Responding

1. I2C scan: confirm `0x40` and `0x41` appear (`scan_i2c()` output on serial).
2. Wiring: SDA=21, SCL=22.
3. Servos need a **separate 5–6V supply** — not USB power.
4. Ensure `USE_I2C_ADDRESS` is **not** defined (I2C slave mode disables servo control).

### Marcduino Commands Ignored

1. Sending device Serial baud must be **2400**.
2. Commands must end with `\r` (carriage return) — not `\n`.
3. Serial2 pins: RX=16, TX=17.
4. Check serial echo at 115200 to confirm receipt.
5. If system is in sleep mode, most commands are gated — send `#SE14` or `POST /api/wake` first.

### Web UI Blank or Not Loading

1. Upload SPIFFS assets: `pio run -e astropixelsplus -t uploadfs`.
2. Check serial for `SPIFFS mount failed` or related errors.
3. Clear browser cache — stale cached assets can cause blank pages.
4. Try direct IP `http://192.168.4.1` if mDNS (`astropixels.local`) is unavailable.

### LEDs Not Working

1. Data pins: FLD=15, RLD=33, Holos=25/26/27, PSI front=32, PSI rear=23.
2. Build flags: `ESP32_ARDUINO_NO_RGB_BUILTIN` must be defined.
3. LEDs need **separate 5V supply** — not 3.3V from ESP32.
4. Test sequence: `@0T2` (all logics flashing color).

### OTA Update Fails

1. WiFi must be stable — device and uploading machine on same network.
2. Sketch binary must fit in OTA partition (~1.4 MB).
3. Firewall: allow port 3232.
4. Fall back to USB upload if OTA is unstable.

### Sleep Mode — Commands Blocked

System in sleep mode gates most Marcduino commands and returns HTTP 423 on API calls.

- Restore active state: `POST /api/wake` or send `:SE14` via serial.

---

## Related External Resources

| Resource | Link |
|----------|------|
| AstroPixels Board | https://we-make-things.co.uk/product/astropixels/ |
| Original Firmware | https://github.com/reeltwo/AstroPixelsPlus |
| ReelTwo Library | https://github.com/reeltwo/Reeltwo |
| MarcDuino Protocol | https://www.curiousmarc.com/r2-d2/marcduino-system |
| R2 Builders Club | https://astromech.net/forums/ |
| NeoPixel Library | https://github.com/adafruit/Adafruit_NeoPixel |
| FastLED Library | https://github.com/FastLED/FastLED |
| DFRobot DFPlayer | https://github.com/DFRobot/DFRobotDFPlayerMini |
| ESPAsyncWebServer | https://me-no-dev.github.io/ESPAsyncWebServer/ |
