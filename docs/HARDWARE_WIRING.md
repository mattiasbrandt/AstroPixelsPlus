<!-- OMO_INTERNAL_INITIATOR -->

# Hardware Wiring Guide — AstroPixelsPlus Gadgets

**Covers:** BadMotivator (smoke), FireStrip (LED), CBI, DataPanel  
**Board:** AstroPixels (ESP32-based) — https://we-make-things.co.uk/product/astropixels/  
**Firmware:** AstroPixelsPlus (this repository)  
**Reeltwo library:** https://github.com/reeltwo/Reeltwo (pinned: `23.5.3`)

---

## Table of Contents

1. [AstroPixels Board Pin Reference](#1-astropixels-board-pin-reference)
2. [FireStrip — WS2812B LED Strip](#2-firestrip--ws2812b-led-strip)
3. [BadMotivator — Smoke Generator](#3-badmotivator--smoke-generator)
4. [CBI — Charge Bay Indicator](#4-cbi--charge-bay-indicator)
5. [DataPanel — Data Panel](#5-datapanel--data-panel)
6. [CBI + DataPanel Combined Chain](#6-cbi--datapanel-combined-chain)
7. [Power Budget](#7-power-budget)
8. [Pin Conflict Reference](#8-pin-conflict-reference)
9. [Enabling Gadgets in Firmware](#9-enabling-gadgets-in-firmware)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. AstroPixels Board Pin Reference

### Core Pin Assignments

```cpp
// I2C (for PCA9685 servo controllers — do not repurpose)
PIN_SDA  = 21    // I2C Data
PIN_SCL  = 22    // I2C Clock

// RGB LED outputs (WS2812B NeoPixel data pins)
PIN_FRONT_LOGIC = 15   // Front Logic Displays
PIN_REAR_LOGIC  = 33   // Rear Logic Display
PIN_FRONT_PSI   = 32   // Front PSI
PIN_REAR_PSI    = 23   // Rear PSI
PIN_FRONT_HOLO  = 25   // Front Holoprojector
PIN_REAR_HOLO   = 26   // Rear Holoprojector
PIN_TOP_HOLO    = 27   // Top Holoprojector

// Serial2 (Marcduino commands — do not repurpose)
SERIAL2_RX = 16
SERIAL2_TX = 17

// Auxiliary pins — shared/multipurpose (see §8 for conflicts)
PIN_AUX1 = 2    // CBI LOAD  (chip-select)
PIN_AUX2 = 4    // CBI CLK
PIN_AUX3 = 5    // CBI DIN   (MOSI)
PIN_AUX4 = 18   // FireStrip data  /OR/  Sound RX (Serial1)
PIN_AUX5 = 19   // BadMotivator relay  /OR/  Sound TX (Serial1)
                //                    /OR/  RLD clock (RSeries curved only)
```

### AUX Header on AstroPixels Board

```
AstroPixels board — AUX connector (physical pin order may vary, verify on silkscreen):

  ┌──────────────────────────────────────────────────────┐
  │  AUX1   AUX2   AUX3   AUX4   AUX5   3V3   5V   GND │
  │  GP2    GP4    GP5    GP18   GP19                    │
  └──────────────────────────────────────────────────────┘

  AUX1 (GPIO 2)  → CBI LOAD   / CS
  AUX2 (GPIO 4)  → CBI CLK
  AUX3 (GPIO 5)  → CBI DIN    / MOSI
  AUX4 (GPIO 18) → FireStrip data  [conflicts with Sound RX]
  AUX5 (GPIO 19) → BadMotivator relay signal  [conflicts with Sound TX]
```

> **Note:** The 3V3 header is **logic only**. The board's 5V header is the same rail as
> USB/VIN and is **not** a high-current distribution rail. Use a dedicated external 5V
> supply (fused) for LED strips and MAX7221 devices, and treat the board 5V pin as a
> convenience tap only — see §7.

To explicitly pin the two gadget GPIOs at their defaults in PlatformIO:

```ini
-DPIN_AUX4=18
-DPIN_AUX5=19
```

---

## 2. PCA9685 Servo Controller Wiring (Core Infrastructure)

**Required for:** All servo-based dome functions (panels + holoprojectors)  
**Not optional:** These boards are essential infrastructure — unlike the optional gadgets (FireStrip, BadMotivator, etc.), you cannot operate the dome without servo controllers.

The AstroPixels firmware uses **two** Adafruit PCA9685 16-channel PWM servo driver boards communicating over I2C to control all dome servos:

| Board | I2C Address | Controls | Servos |
|-------|-------------|----------|--------|
| **Panel Controller** | 0x40 | Dome panels P1-P13 | 7 moving panels + 1 fixed (Magic Panel) |
| **Holo Controller** | 0x41 | Holoprojectors (FHP, RHP, THP) | 6 servos (2 per holo: H/V) |

### Why PCA9685?

The ESP32 cannot directly drive 13+ servos. The PCA9685 provides:  
- 16 independent PWM channels per board (12-bit resolution)  
- I2C control using only 2 GPIO pins (SDA/SCL)  
- Built-in PWM generation (no CPU overhead)  
- Expandable: Can chain up to 62 boards on same I2C bus  
- Isolated servo power (critical — see Power section below)

**Reeltwo integration:** `ServoDispatchPCA9685` class handles all communication  
**Reference:** [Adafruit PCA9685 Guide](https://learn.adafruit.com/16-channel-pwm-servo-driver)

---

### I2C Wiring (Logic Side)

Connect both PCA9685 boards to the AstroPixels board I2C bus:

```
AstroPixels Board                PCA9685 #1 (Panels)           PCA9685 #2 (Holos)
┌──────────────┐                 ┌──────────────┐              ┌──────────────┐
│              │                 │              │              │              │
│  GPIO 21     ├────SDA──────────┤SDA           ├────SDA───────┤SDA           │
│  (I2C SDA)   │                 │              │              │              │
│              │                 │              │              │              │
│  GPIO 22     ├────SCL──────────┤SCL           ├────SCL───────┤SCL           │
│  (I2C SCL)   │                 │              │              │              │
│              │                 │              │              │              │
│  3.3V        ├────VCC──────────┤VCC           ├────VCC───────┤VCC           │
│  (logic)     │    (⚠️ NOT 5V!) │              │              │              │
│              │                 │              │              │              │
│  GND         ├────GND──────────┤GND           ├────GND───────┤GND           │
│  (common)    │                 │              │              │              │
└──────────────┘                 └──────────────┘              └──────────────┘
```

| Signal | AstroPixels Pin | PCA9685 Pin | Wire | Notes |
|--------|-----------------|-------------|------|-------|
| **SDA** | GPIO 21 | SDA | 22-24 AWG | I2C data — daisy chain between boards |
| **SCL** | GPIO 22 | SCL | 22-24 AWG | I2C clock — daisy chain between boards |
| **VCC** | 3.3V header | VCC | 22-24 AWG | **Logic power only** — 3.3V, NOT servo 5V |
| **GND** | GND | GND | 22-24 AWG | **Critical:** Common ground reference |

> ⚠️ **CRITICAL:** The PCA9685 VCC pin powers the board's **logic circuitry only** (3.3V-5V tolerant).  
> This is **NOT** the servo power — servos have a **separate** power input (see below).

---

### Address Configuration (Solder Jumpers)

Each PCA9685 board has address jumpers (A0-A5) that must be configured before use:

**Panel Controller — Address 0x40 (default):**
- All jumpers **OPEN** (no solder bridges)
- This is the factory default — no modification needed

**Holo Controller — Address 0x41:**
- Bridge **A0** only (solder a bridge across the A0 pads)
- A1-A5 remain open

```
PCA9685 Address Jumpers (bottom of board):

A5  A4  A3  A2  A1  A0  │  Address
────────────────────────┼──────────
○   ○   ○   ○   ○   ○   │  0x40 (default)
○   ○   ○   ○   ○   ●   │  0x41 (Holo controller)
```

**⚠️ Configure addresses BEFORE powering on.**  
The firmware scans for 0x40 and 0x41 at boot — if not found, panel/holo commands will fail.

---

### Servo Power Wiring (HIGH CURRENT — ISOLATED)

This is the **most critical** wiring section. Servos require **5-6V at high current** (up to 1A per servo when stalled). **NEVER** power servos from the AstroPixels board or USB — you will damage the ESP32.

**Typical R2 Dome Power Architecture:**

```
Body Battery (e.g., 12V)
        │
        ▼
Slip Ring (dome rotation)
        │
        ▼
DC-DC Step-Down Converter (Buck)
        │ (5V or 6V output)
        ├──────────────────────────┐
        │                          │
   ┌────┴────┐                ┌────┴────┐
   │ V+  GND │                │ V+  GND │
   │ (5-6V)  │                │ (5-6V)  │
   └────┬────┘                └────┬────┘
        │                          │
   ┌────┴────┐                ┌────┴────┐
   │PCA9685  │                │PCA9685  │
   │#1 0x40  │                │#2 0x41  │
   │(Panels) │                │(Holos)  │
   │         │                │         │
   │V+───V+  │                │V+───V+  │
   │GND──GND │                │GND──GND │
   │ │ │ │ │ │                │ │ │ │ │ │
   │Servo 0-15│                │Servo 0-5 │
   └─────────┘                └─────────┘
        │                          │
        └──────────┬───────────────┘
                   │
                   ▼
          AstroPixels GND (common)
```

**Power Requirements:**

- **Input:** 12V (or your body battery voltage) from slip ring
- **Converter:** DC-DC buck converter (step-down) to 5V or 6V
- **Current:** 5A minimum, 10A recommended (for all servos + headroom)
- **Voltage:** Match your servo specs (5V or 6V — check servo datasheets)

> **Alternative power sources:** You can also use a dedicated 5V/6V PSU, battery pack,
> or RC BEC/UBEC. The key requirements are correct voltage, sufficient current,
> and electrical isolation from the logic power (see below).

```
Servo Power Supply (5-6V, 5A+ recommended)
┌──────────────────────────────────────────┐
│  External BEC / UBEC / Battery           │
│  - Voltage: 5V or 6V (match your servos) │
│  - Current: 5A minimum (10A recommended) │
└──────────────────┬───────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
   ┌────┴────┐           ┌────┴────┐
   │ V+  GND │           │ V+  GND │
   │ (term)  │           │ (term)  │
   └────┬────┘           └────┬────┘
        │                     │
   ┌────┴────┐           ┌────┴────┐
   │PCA9685  │           │PCA9685  │
   │#1 0x40  │           │#2 0x41  │
   │(Panels) │           │(Holos)  │
   │         │           │         │
   │V+───V+  │           │V+───V+  │
   │GND──GND │           │GND──GND │
   │ │ │ │ │ │           │ │ │ │ │ │
   │Servo 0-15│           │Servo 0-5 │
   └─────────┘           └─────────┘
```

**Power Wiring Details:**

| Connection | From | To | Wire Gauge | Fuse |
|------------|------|-----|------------|------|
| **V+ (servo power)** | DC-DC + | PCA9685 V+ terminal | 18-20 AWG | 5A |
| **GND (servo ground)** | DC-DC - | PCA9685 GND terminal | 18-20 AWG | — |
| **GND (common)** | DC-DC - | AstroPixels GND | 22-24 AWG | — |

| Connection | From | To | Wire Gauge | Fuse |
|------------|------|-----|------------|------|
| **V+ (servo power)** | BEC/UBEC + | PCA9685 V+ terminal | 18-20 AWG | 5A |
| **GND (servo ground)** | BEC/UBEC - | PCA9685 GND terminal | 18-20 AWG | — |
| **GND (common)** | BEC/UBEC - | AstroPixels GND | 22-24 AWG | — |

> ⚠️ **CRITICAL — Common Ground:**  
> The servo power supply GND **must** connect to both:  
> 1. The PCA9685 GND terminals (servo reference)  
> 2. The AstroPixels GND (logic reference)  
> Without this common ground, I2C communication will fail and servos will behave erratically.

> ⚠️ **CRITICAL — Power Isolation:**
> - **NEVER** connect the DC-DC converter 5V output to the AstroPixels 5V pin
> - **NEVER** power servos from USB or the board's 5V header
> - Servo power (V+/GND) is **completely isolated** from logic power (VCC/GND) on the PCA9685
> - **NEVER** connect the BEC 5V to the AstroPixels 5V pin  
> - **NEVER** power servos from USB or the board's 5V header  
> - Servo power (V+/GND) is **completely isolated** from logic power (VCC/GND) on the PCA9685

**Servo Connector Pinout (3-pin):**

```
Servo connector (female, looking at connector):
┌─────────────┐
│  Sig  +  -  │
│   │   │  │  │
│   ▼   ▼  ▼  │
│  PWM 5V GND │
└─────────────┘
         │
         └── Servo cable: usually Orange/Red/Brown or White/Red/Black
```

---

### Servo Channel Mapping

**Panel Controller (0x40) — Channels 0-12:**

| Channel | Panel | Type | Notes |
|---------|-------|------|-------|
| 0 | P1 | Small | Front lower left |
| 1 | P2 | Small | Front lower right |
| 2 | P3 | Small | Left side lower |
| 3 | P4 | Small | Right side lower |
| 4 | P5 | — | **Magic Panel (fixed, no servo)** |
| 5 | P6 | Large | Front upper |
| 6 | — | — | Unused |
| 7 | P7 | Medium | Left upper side |
| 8 | — | — | Unused |
| 9 | — | — | Unused |
| 10 | P10 | Medium | Rear upper |
| 11 | P11 | Large | Top pie panel |
| 12 | P13 | Medium | Center top |

**Holo Controller (0x41) — Channels 0-5:**

| Channel | Holo | Axis | Notes |
|---------|------|------|-------|
| 0 | Front (FHP) | Horizontal | Left/right movement |
| 1 | Front (FHP) | Vertical | Up/down movement |
| 2 | Rear (RHP) | Horizontal | Left/right movement |
| 3 | Rear (RHP) | Vertical | Up/down movement |
| 4 | Top (THP) | Horizontal | Left/right movement |
| 5 | Top (THP) | Vertical | Up/down movement |

---

### Wiring Checklist (Before First Power-On)

- [ ] **Address jumpers configured:** 0x40 = all open, 0x41 = A0 bridged  
- [ ] **I2C wiring:** SDA (GPIO 21), SCL (GPIO 22) daisy-chained between boards  
- [ ] **Logic power:** PCA9685 VCC connected to 3.3V (NOT 5V)  
- [ ] **Servo power:** Separate 5-6V supply connected to V+ and GND terminals  
- [ ] **Common ground:** Servo PSU GND connected to both PCA9685 GND and AstroPixels GND  
- [ ] **Servo connectors:** PWM signal toward inside of PCA9685 terminals, GND toward edge  
- [ ] **Power validation:** Use multimeter to verify 5-6V on V+ terminals before connecting servos

---

### Troubleshooting PCA9685 Issues

**Issue: I2C scan shows "No devices found"**  
1. Check SDA/SCL wiring continuity (GPIO 21/22)  
2. Verify address jumpers are correct (0x40 all open, 0x41 A0 bridged)  
3. Verify PCA9685 VCC has 3.3V (board LED should light)  
4. Ensure common ground between servo PSU and AstroPixels  
5. Try shorter I2C cables (keep under 30cm if possible)

**Issue: Servos don't move but I2C works**  
1. Check servo power supply voltage (5-6V) and current capacity (5A+)  
2. Verify servos are plugged into correct channels  
3. Check servo connector orientation (PWM toward board center)  
4. Verify panel calibration has been performed (`:MV` commands or web UI)  
5. Check `/api/health` for I2C status and error counts

**Issue: Servos jitter or behave erratically**  
1. **Inadequate power supply** — most common cause. Upgrade to higher-current BEC  
2. **Ground loop** — ensure single-point ground connection  
3. **Long servo wires** — keep servo leads under 30cm if possible  
4. **USB power** — never run servos while USB-powered; use external PSU only

---

## 3. FireStrip — WS2812B LED Strip

## 2. FireStrip — WS2812B LED Strip

### What it does

The FireStrip animates electrical spark and fire effects using a short NeoPixel strip.
Triggered during the Bad Motivator sequence (smoke + panels + fire) or standalone via
Marcduino commands.

**Reeltwo source:** [`src/dome/FireStrip.h`](https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/dome/FireStrip.h)

### Default Parameters

| Parameter | Value |
|---|---|
| Data pin | GPIO 18 (PIN_AUX4) |
| LED count | 8 |
| Color order | GRB (standard WS2812B) |
| Brightness | 100 / 255 |
| LED type | WS2812B |

### Wiring Diagram

```
AstroPixels Board                        WS2812B LED Strip
┌──────────────┐                         ┌─────────────────────────┐
│              │                         │                         │
│  AUX4 (GP18)├──22 AWG signal──────────►DIN                      │
│              │                         │  [8 LEDs]               │
│  5V header   ├──20 AWG positive─┐      │                         │
│              │                  │  ┌───►5V                       │
│  GND         ├──20 AWG ground───┤  │   │                         │
│              │                  │  │  ─┤GND                      │
└──────────────┘                  │  │   └─────────────────────────┘
                                  │  │
                         External 5V supply
                         (see §7 — power budget)
                         ┌────────────┐
                         │  5V PSU    ├──────► 5V bus
                         │            ├──────► GND bus
                         └────────────┘
```

### Physical Connections

| FireStrip wire | Connects to | Wire gauge |
|---|---|---|
| DIN (data in) | GPIO 18 (AUX4) | 22–24 AWG |
| 5V | External 5V supply | 20 AWG |
| GND | Common GND (board + supply) | 20 AWG |

### Connector Recommendation

- For detachable builds, use a locking 3-pin connector for the strip: **JST-SM 3-pin** or **JST-XH 3-pin**.
- Keep the data line on its own pin (do not combine with power in the same crimp).

> **Power injection note:** For strips longer than 8 LEDs or runs longer than ~30 cm,
> add a 1000 µF / 6.3 V capacitor across 5V and GND close to the strip connector.
> Use 20 AWG wire for power leads; signal wire may be 22–24 AWG.

### Resistor on Data Line (Recommended)

Place a **300–500 Ω resistor** in series on the data line, as close as possible to the
AUX4 pin. This protects the ESP32 GPIO and suppresses ringing on long signal leads.

```
  GPIO 18 ──[330 Ω]──► DIN of first LED
```

### 3.3V-to-5V Level Shifting (Recommended)

WS2812B data is 5V-tolerant, but many strips are unreliable with a 3.3V data signal.
For best results (especially with longer leads), add a **74AHCT125** (or 74HCT14) level shifter:

```
ESP32 GPIO18 (3.3V) -> 74AHCT125 -> WS2812B DIN (5V)

5V rail  -----------------------+-> 74AHCT125 VCC
GND rail -----------------------+-> 74AHCT125 GND
```

### Firmware Declarations

```cpp
// AstroPixelsPlus.ino (existing declarations — shown for reference)
#define PIN_AUX4 18

#if AP_ENABLE_FIRESTRIP
#if USE_FIRESTRIP_TEMPLATE
    FireStrip<PIN_AUX4> fireStrip;          // FastLED path (default)
#else
    FireStrip fireStrip(PIN_AUX4);          // Adafruit NeoPixel path
#endif
#endif
```

### Marcduino Commands

| Command | Effect |
|---|---|
| `FSOFF` | Turn off all LEDs |
| `FS0` | Turn off all LEDs |
| `FS1<ms>` | Spark animation for `<ms>` milliseconds (e.g. `FS1500`) |
| `FS2<ms>` | Fire animation for `<ms>` milliseconds (e.g. `FS22500`) |

These are also used in the reset sequence inside `resetSequence()`:
```cpp
CommandEvent::process(F("FSOFF\n"));  // Fire Strip Off
```

### Alternative Pin Options

The template variant `FireStrip<PIN>` accepts any digital-capable GPIO:

```cpp
// Alternative: use AUX3 (GPIO 5) if AUX4 is needed for sound
FireStrip<5> fireStrip;

// Alternative: use AUX1 (GPIO 2) — note GPIO 2 is the boot LED on most ESP32 boards
// Avoid GPIO 2 during boot; it must not be held HIGH at power-on.
```

---

## 3. BadMotivator — Smoke Generator

> **⚠️ SAFETY — READ BEFORE WIRING**
>
> The Bad Motivator uses a **smoke/fog machine** which generates heat and glycol-based
> aerosol. Before wiring and operating:
>
> 1. **Ventilation:** The smoke machine chamber and heating element reach **150–200 °C**
>    internally. Ensure the mounted enclosure has airflow clearance of at least 20 mm on
>    all sides of the fog unit.
> 2. **Fire risk:** Never leave the droid unattended while the smoke unit is active or
>    has been recently activated (element takes ~5 minutes to cool).
> 3. **Fluid:** Use only purpose-formulated fog machine fluid. Water or improvised fluids
>    will damage the heating element and may produce unsafe fumes.
> 4. **Cooldown enforcement:** The Reeltwo firmware enforces a **1-minute mandatory
>    pause** between activations and auto-cuts the relay after **6.5 seconds maximum**.
>    Do not bypass these limits.
> 5. **Wiring insulation:** Use heat-resistant wire (rated ≥ 105 °C) for any wires
>    routed near the smoke unit. Minimum 18 AWG for relay output wiring.
> 6. **Relay contact rating:** The relay module must be rated for the smoke machine's
>    operating voltage and current. Most mini smoke units draw 2–5 A at 12 V. Use a
>    relay rated for at least **10 A** at the supply voltage.

### What it does

The BadMotivator controls a **relay module** that switches power to a mini smoke/fog
machine. A separate LED ring or strip (visual) is typically added alongside — the smoke
is the primary effect. The relay is energised by a digital GPIO output signal from the
ESP32 (3.3 V logic, active HIGH).

**Reeltwo source:** [`src/dome/BadMotivator.h`](https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/dome/BadMotivator.h)

### Default Parameters

| Parameter | Value |
|---|---|
| Control pin | GPIO 19 (PIN_AUX5) |
| Relay signal | Active HIGH (logic HIGH = relay ON = smoke ON) |
| Max activation time | 6,500 ms (6.5 seconds, enforced by firmware) |
| Minimum pause between activations | 60,000 ms (1 minute, enforced by firmware) |

### Wiring Diagram

```
AstroPixels Board               Relay Module (5V coil, e.g. SRD-05VDC-SL-C)
┌──────────────┐                ┌─────────────────────────────────────────┐
│              │  22 AWG        │  ┌──────────────────────────────────┐   │
│  AUX5 (GP19)├───────────────►│  IN (signal)                        │   │
│              │                │  │                                  │   │
│  5V header   ├───────────────►│  VCC                               │   │
│              │                │  │                                  │   │
│  GND         ├───────────────►│  GND                               │   │
│              │                │  └──────────────────────────────────┘   │
└──────────────┘                │                                         │
                                │  COM ───────────────────────────────────┼──► + of smoke supply
                                │  NO  ───────────────────────────────────┼──► + of smoke machine
                                │  NC  (not connected)                    │
                                └─────────────────────────────────────────┘

Smoke machine power circuit:
                                     Relay NO contact
  External 12 V supply (+) ──────────────────────────────► Smoke machine (+)
  External 12 V supply (-) ──────────────────────────────► Smoke machine (-)
```

### Relay Module Selection

Use a **5 V coil relay module** with an opto-isolated input (protects the ESP32).
The module must have:

- Coil voltage: 5 V (supplied from board 5 V header)
- Control logic: 3.3 V-compatible input (most opto-isolated modules work at 3.3 V)
- Contact rating: ≥ 10 A / 30 VDC (for 12 V smoke machines)

Recommended modules: SRD-05VDC-SL-C-based boards, Songle SRD, or equivalent.

### Physical Connections — Relay Module

| Signal | From | To relay | Wire gauge |
|---|---|---|---|
| IN (control) | GPIO 19 (AUX5) | IN pin | 22–24 AWG |
| VCC | Board 5 V | VCC pin | 22 AWG |
| GND | Common GND | GND pin | 22 AWG |
| COM | Smoke supply positive (+) | COM terminal | 18 AWG (min) |
| NO | Smoke machine positive (+) | NO terminal | 18 AWG (min) |
| Smoke supply negative (−) | Direct | Smoke machine (−) | 18 AWG (min) |

### Smoke Machine Placement and Mounting

```
  Dome cross-section (top view, schematic):

       ┌────────────────────────────────────┐
       │           Dome Interior             │
       │                                    │
       │    ┌──────────────────────────┐    │
       │    │  Smoke unit              │    │
       │    │  [≥20mm clearance all    │    │
       │    │   sides — heat safety]   │    │
       │    └──────────────────────────┘    │
       │             ↑ smoke exits          │
       │         through opening            │
       └────────────────────────────────────┘

  Notes:
  - Mount smoke unit so output nozzle points toward dome opening
  - Use silicone adhesive or nylon standoffs — avoid metal brackets
    directly contacting the heating element body
  - Route fluid tubing away from any hot surfaces
```

### Firmware Declarations

```cpp
// AstroPixelsPlus.ino (existing declarations — shown for reference)
#define PIN_AUX5 19

#if AP_ENABLE_BADMOTIVATOR
    BadMotivator badMotivator(PIN_AUX5);
#endif
```

### Marcduino Commands

| Command | Effect |
|---|---|
| `BMON` | Activate smoke (max 6.5 s, 1-minute cooldown enforced) |
| `BMOFF` | Deactivate smoke relay immediately |

These are also used in the reset sequence:
```cpp
CommandEvent::process(F("BMOFF\n"));  // Bad Motivator Off
```

### Alternative Pin Options

```cpp
// Use AUX3 (GPIO 5) if AUX5 is reserved for sound TX or RLD clock
BadMotivator badMotivator(5);   // AUX3 = GPIO 5

// Use AUX1 (GPIO 2) — see GPIO 2 boot caveat above
BadMotivator badMotivator(2);
```

If you need BadMotivator on a non-AUX pin, also update `platformio.ini`:
```ini
-DPIN_AUX5=<new_gpio_number>
```
Or pass the pin directly to the constructor and remove the `PIN_AUX5` dependency.

---

## 4. CBI — Charge Bay Indicator

### What it does

The Charge Bay Indicator (CBI) is an R2-D2 body panel prop containing:
- A **5×4 grid of LEDs** (controlled via a MAX7221 LED driver)
- **3 individual indicator LEDs** (Red, Yellow, Green — VCC level simulation)

Communication uses a **3-wire SPI-like protocol** (DIN / CLK / LOAD) via the
`LedControlMAX7221` driver, shared with the DataPanel on the same bus.

**Reeltwo source:** [`src/body/ChargeBayIndicator.h`](https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/body/ChargeBayIndicator.h)

### LED Layout (from Reeltwo source)

```
  CBI LED grid — MAX7221 matrix mapping:

  Column# 0 1 2 3 4      (top 5 bits of each row byte are significant)
  Bit     8 7 6 5 4
          ----------
          O O O O O   row #0
          O O O O O   row #1
          O O O O O   row #2
          O O O O O   row #3

  Individual indicator LEDs:
    Red LED    → row #4, col #5   Bit pattern B00000100
    Yellow LED → row #5, col #5   Bit pattern B00000100
    Green LED  → row #6, col #5   Bit pattern B00000100
```

### Pin Assignments

| Signal | AUX pin | GPIO | Notes |
|---|---|---|---|
| DIN (data) | AUX3 | GPIO 5 | SPI MOSI — connect to MAX7221 DIN |
| CLK (clock) | AUX2 | GPIO 4 | SPI CLK — connect to MAX7221 CLK |
| LOAD (chip-select) | AUX1 | GPIO 2 | SPI CS/LOAD — connect to MAX7221 LOAD |

### Alternative Pin Options

If AUX1/2/3 are not usable in your build (or you want to avoid the GPIO 2 boot-strap caveat),
you can move the CBI/DataPanel bus to any free GPIOs by editing these defines in
`AstroPixelsPlus.ino`:

```cpp
#define CBI_DATAIN_PIN PIN_AUX3
#define CBI_CLOCK_PIN  PIN_AUX2
#define CBI_LOAD_PIN   PIN_AUX1
```

Pick GPIOs that are digital-capable and not already used by I2C, Serial2, or WS2812 outputs.

### MAX7221 Wiring Diagram

```
AstroPixels Board                    MAX7221 IC (or module)
┌──────────────┐                     ┌───────────────────────┐
│              │                     │                       │
│  AUX3 (GP5)  ├──22 AWG────────────►DIN  (pin 1)           │
│              │                     │                       │
│  AUX2 (GP4)  ├──22 AWG────────────►CLK  (pin 13)          │
│              │                     │                       │
│  AUX1 (GP2)  ├──22 AWG────────────►LOAD (pin 12)          │
│              │                     │                       │
│  5V header   ├──20 AWG────────────►VCC  (pin 19, 24)      │
│              │                     │                       │
│  GND         ├──20 AWG────────────►GND  (pin 4, 9)        │
└──────────────┘                     │                       │
                                     │  ISET (pin 18) ──[R]──► VCC
                                     │                       │  (10 kΩ for ISET)
                                     │  DOUT (pin 24) ──────►► next MAX7221 DIN
                                     │                         (if DataPanel chained)
                                     └───────────────────────┘
                                                │
                                          LED connections
                                     ┌─────────┴──────────┐
                                     │  DIG0–DIG7 (rows)  │
                                     │  SEG A–G,DP (cols) │
                                     │  → LED matrix      │
                                     └────────────────────┘
```

### MAX7221 ISET Resistor (Current Limit)

The MAX7221 uses an external resistor connected between ISET (pin 18) and VCC to set
the maximum segment current. For standard LEDs rated at 20 mA per segment:

| ISET resistor | Segment current | Notes |
|---|---|---|
| 10 kΩ | ~40 mA | Maximum, suitable for high-brightness LEDs |
| 18 kΩ | ~22 mA | Standard 5 mm LED nominal current |
| 27 kΩ | ~15 mA | Reduced brightness, longer LED life |
| **47 kΩ** | ~9 mA | **Recommended for typical CBI/DataPanel LEDs** |

For CBI and DataPanel, a **47 kΩ ISET resistor** gives adequate brightness within
safe limits. Increase to 27 kΩ only if LEDs appear dim.

### Power Requirements

MAX7221 operating voltage: **4.0 – 5.5 V** (use the 5 V rail, not 3.3 V).  
Quiescent current (all LEDs on, ISET = 47 kΩ): ≈ **150–200 mA max** per device.

### Physical Connections Summary

| MAX7221 pin | Connects to | Wire gauge |
|---|---|---|
| DIN (1) | GPIO 5 / AUX3 | 22–26 AWG |
| CLK (13) | GPIO 4 / AUX2 | 22–26 AWG |
| LOAD (12) | GPIO 2 / AUX1 | 22–26 AWG |
| VCC (19, 24) | External 5 V | 22 AWG |
| GND (4, 9) | Common GND | 22 AWG |
| ISET (18) | VCC via 47 kΩ | 26 AWG |
| DOUT (24) | Next device DIN (if chained) | 22–26 AWG |

### Connector Recommendation

Use **JST-XH 2.54 mm** 5-pin connectors for the SPI bus connection from the AstroPixels
board to the CBI/DataPanel assembly. Label polarity; these connectors are not keyed
against reversal.

---

## 5. DataPanel — Data Panel

### What it does

The R2-D2 Data Panel is a body prop containing various colored LED arrays:
- **Blue bargraph** (vertical column, 4 LEDs)
- **Bottom LEDs** (horizontal, 6 LEDs)
- **Red/Yellow bargraph** (vertical, 2×6 LEDs)
- **Yellow block** LEDs (2×3 array)
- **Green block** LEDs (2×3 array)
- **Red indicator** LEDs ×2

All driven by a single MAX7221 device chained after the CBI on the same 3-wire bus.

**Reeltwo source:** [`src/body/DataPanel.h`](https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/body/DataPanel.h)

### LED Layout (from Reeltwo source)

```
  DataPanel LED mapping — MAX7221 rows:

  ROW #0  — Blue bargraph:  B11111100 fills all LEDs
  ROW #1  — Bottom LEDs:    top=B11100000, bottom=B00011100
  ROW #2  — Bargraph left:  B11111100 (R/Y/G column, left half)
  ROW #3  — Bargraph right: B11111100 (R/Y/G column, right half)
  ROW #4  — Yellow block:   top B00011100 (cols 3/4/5), bottom B11100000 (cols 0/1/2)
  ROW #5  — Green block:    top B00011100 (cols 3/4/5), bottom B11100000 (cols 0/1/2)

  Bargraph height levels (0–6):
    0 = off
    1 = 1×green
    2 = 2×green
    3 = 2×green + 1×yellow
    4 = 2×green + 2×yellow
    5 = 2×green + 2×yellow + 1×red
    6 = 2×green + 2×yellow + 2×red
```

### Wiring

The DataPanel shares the same 3-wire SPI bus as the CBI. It is **daisy-chained** after
CBI — the DataPanel's MAX7221 DIN connects to the CBI MAX7221's DOUT pin.

See §6 for the combined chain diagram.

### Firmware Declarations

```cpp
// Shared chain for both CBI and DataPanel
LedControlMAX7221<5> ledChain1(CBI_DATAIN_PIN, CBI_CLOCK_PIN, CBI_LOAD_PIN);
//                  ^  template parameter: max number of devices in chain

ChargeBayIndicator chargeBayIndicator(ledChain1);  // device 0
DataPanel          dataPanel(ledChain1);            // device 1 (chained)
```

The template parameter `<5>` reserves space for up to 5 chained MAX7221 devices.
Adjust if you add more devices.

### Marcduino Commands

| Command | Effect |
|---|---|
| `DP00000` | Normal animation (reset) |
| `DP10000` | Disabled (off) |
| `DP20000` | Flicker animation |

---

## 6. CBI + DataPanel Combined Chain

### Daisy-Chain Topology

When CBI and DataPanel are both enabled, two MAX7221 ICs are connected in series on
the same 3-wire bus:

```
AstroPixels Board
┌──────────────┐
│  AUX3 (GP5)  ├─DIN──────────────┐
│  AUX2 (GP4)  ├─CLK──────────────┼──────────────────┐
│  AUX1 (GP2)  ├─LOAD─────────────┼──────────────────┼─────────────┐
└──────────────┘                  │                  │             │
                                  ▼                  ▼             ▼
                            MAX7221 #1          MAX7221 #2       (all share CLK + LOAD)
                         ┌──────────────┐    ┌──────────────┐
                         │ CBI          │    │ DataPanel    │
                         │              │    │              │
                  ───────► DIN          │    │ DIN          │
                         │ DOUT ────────────►│             │
                         │ CLK ◄────────────── CLK         │
                         │ LOAD◄────────────── LOAD        │
                         │ VCC ◄──5V   │    │ VCC ◄──5V    │
                         │ GND ◄──GND  │    │ GND ◄──GND   │
                         └──────────────┘    └──────────────┘
```

### Wire Connections

| Signal | Board pin | CBI MAX7221 | DataPanel MAX7221 |
|---|---|---|---|
| DIN | AUX3 (GPIO 5) | DIN | — (from CBI DOUT) |
| CLK | AUX2 (GPIO 4) | CLK | CLK (parallel) |
| LOAD | AUX1 (GPIO 2) | LOAD | LOAD (parallel) |
| DOUT | — | DOUT → | DIN |
| VCC | External 5 V | VCC | VCC |
| GND | Common GND | GND | GND |

CLK and LOAD run in parallel to both ICs. DIN is a daisy-chain: board → CBI DIN,
CBI DOUT → DataPanel DIN.

### Decoupling Capacitors

Place a **100 nF ceramic capacitor** between VCC and GND **at each MAX7221** (as close
to the chip pins as possible). Optionally add a **10 µF electrolytic** in parallel for
bulk bypass.

---

## 7. Power Budget

> **⚠️ Do not power LEDs or relay loads from the AstroPixels board's 5 V pin.**
> The on-board 5 V regulator is sized for logic only. Use a dedicated external 5 V supply.

### Current Estimates

| Device | Typical | Peak | Notes |
|---|---|---|---|
| ESP32 (AstroPixels board) | 150 mA | 250 mA | WiFi active, all drivers running |
| FireStrip (8× WS2812B) | 60 mA | 480 mA | Peak = all LEDs white at max; typical ≈ 60–150 mA |
| CBI (MAX7221 + LEDs) | 80 mA | 180 mA | ISET 47 kΩ; 23 LEDs max on simultaneously |
| DataPanel (MAX7221 + LEDs) | 80 mA | 180 mA | ISET 47 kΩ; similar LED count |
| BadMotivator relay coil | 50 mA | 80 mA | Relay coil only; smoke machine drawn separately |
| Smoke machine | varies | 2–5 A | **Separate 12 V circuit; not via ESP32** |

WS2812B power rule-of-thumb:

```
I_max (amps) ~= LED_count * 0.060
I_est (amps) ~= LED_count * 0.060 * (brightness/255)
```

Example (default FireStrip): `8 * 0.060 * (100/255) ~= 0.19 A` estimated.

### Recommended Supply Configuration

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  Power Distribution                                                 │
  │                                                                     │
  │  5 V / 3 A supply ─────────────────────────────────────────────┐  │
  │                                                                  │  │
  │    ├── AstroPixels board (USB or VIN 5 V)    ≈ 250 mA max      │  │
  │    ├── FireStrip 5 V rail                    ≈ 480 mA max       │  │
  │    ├── CBI MAX7221 VCC                       ≈ 180 mA max       │  │
  │    ├── DataPanel MAX7221 VCC                 ≈ 180 mA max       │  │
  │    └── Relay module VCC                      ≈  80 mA           │  │
  │                                                                  │  │
  │    Total estimate: ≈ 0.5–0.8 A typical / ≈ 1.2 A peak            │  │
  │    → Use a 3 A / 5 V supply minimum (more if you add LEDs)       │  │
  │                                                                  │  │
  │  12 V / 5 A supply (smoke machine) — SEPARATE circuit           │  │
  │    └── Smoke machine (via relay NO contact)  ≈ 2–5 A            │  │
  └─────────────────────────────────────────────────────────────────┘
```

### Wire Gauge by Current Load

| Load | Max current | Minimum wire gauge | Notes |
|---|---|---|---|
| Signal lines (DIN/CLK/LOAD) | < 50 mA | 26 AWG | Short runs OK |
| NeoPixel data line | < 50 mA | 22–24 AWG | Add 330 Ω series resistor |
| 5 V power to LEDs / MAX7221 | ≤ 1.5 A | 20 AWG | Keep runs short |
| Relay coil supply | ≤ 100 mA | 22 AWG | |
| Relay output to smoke machine | ≤ 5 A | 18 AWG | Heat-resistant insulation |
| Smoke machine power (12 V) | ≤ 5 A | 16–18 AWG | Fuse at supply |

### Fusing

- Add a **3 A blade fuse** or polyfuse on the 5 V supply rail feeding LEDs and MAX7221 devices.
- Add a **5 A fuse** on the 12 V supply feeding the smoke machine relay output.

---

## 8. Pin Conflict Reference

### AUX4 (GPIO 18) and AUX5 (GPIO 19)

These pins are shared between gadget functions and the onboard sound serial port (Serial1).

| AUX pin | GPIO | Gadget use | Sound serial use | RSeries RLD use |
|---|---|---|---|---|
| AUX4 | 18 | FireStrip data | Sound RX (Serial1) | — |
| AUX5 | 19 | BadMotivator relay | Sound TX (Serial1) | RLD Clock (curved) |

**Mutual exclusions:**

| Combination | Compatible? | Notes |
|---|---|---|
| FireStrip + BadMotivator | ✅ Yes | Normal use case (this guide) |
| FireStrip + Sound Serial | ❌ No | AUX4 conflict |
| BadMotivator + Sound Serial | ❌ No | AUX5 conflict |
| BadMotivator + RSeries Curved RLD | ❌ No | AUX5 conflict |
| CBI/DataPanel + Sound Serial | ✅ Yes | CBI uses AUX1/2/3, different pins |

### GPIO 2 (AUX1) Boot Caveat

GPIO 2 (AUX1 / CBI LOAD) is the **boot-mode strap pin** on ESP32. It must be LOW
during power-on reset for normal boot. The MAX7221 LOAD line is normally HIGH when idle.

**Mitigation:** Most MAX7221 modules have a pull-down on LOAD internally. Verify that
the LOAD line is not externally pulled HIGH before power-on, or add a 10 kΩ pull-down
resistor between LOAD and GND.

Alternatively, reassign CBI LOAD to a safe GPIO:
```cpp
// In AstroPixelsPlus.ino — change if GPIO 2 is problematic
#define CBI_LOAD_PIN PIN_AUX1  // default: GPIO 2 — may conflict with boot strap
// Alternative: use any free GPIO not listed in §1
```

### Full Pin Assignment Summary

```
GPIO  2  (AUX1)  — CBI LOAD [boot strap, add pull-down]
GPIO  4  (AUX2)  — CBI CLK
GPIO  5  (AUX3)  — CBI DIN (MOSI)
GPIO 15           — PIN_FRONT_LOGIC (FLD WS2812B)
GPIO 16           — SERIAL2_RX (Marcduino in)
GPIO 17           — SERIAL2_TX (Marcduino out)
GPIO 18  (AUX4)  — FireStrip data  [conflicts: Sound RX]
GPIO 19  (AUX5)  — BadMotivator relay  [conflicts: Sound TX, RLD clock]
GPIO 21           — I2C SDA (PCA9685 servo)
GPIO 22           — I2C SCL (PCA9685 servo)
GPIO 23           — PIN_REAR_PSI
GPIO 25           — PIN_FRONT_HOLO
GPIO 26           — PIN_REAR_HOLO
GPIO 27           — PIN_TOP_HOLO
GPIO 32           — PIN_FRONT_PSI
GPIO 33           — PIN_REAR_LOGIC (RLD WS2812B)
```

---

## 9. Enabling Gadgets in Firmware

### platformio.ini Build Flags

Each gadget is disabled by default. Enable by changing the corresponding build flag
in `platformio.ini`:

```ini
build_flags =
    ; ... other flags ...
    -DAP_ENABLE_BADMOTIVATOR=1    ; was 0 — enable smoke relay
    -DAP_ENABLE_FIRESTRIP=1       ; was 0 — enable LED fire strip
    -DAP_ENABLE_CBI=1             ; was 0 — enable Charge Bay Indicator
    -DAP_ENABLE_DATAPANEL=1       ; was 0 — enable Data Panel
```

### Enabling Sound Serial vs FireStrip/BadMotivator

If you have a DFPlayer or other serial sound module on AUX4/AUX5 (Serial1), you cannot
simultaneously use FireStrip (AUX4) or BadMotivator (AUX5). Choose one:

**Option A — Gadgets only (no sound serial):**
```ini
-DAP_ENABLE_FIRESTRIP=1
-DAP_ENABLE_BADMOTIVATOR=1
; Leave sound player as kDisabled or use I2S/other pin
```

**Option B — Sound serial only (no gadgets on AUX4/AUX5):**
```ini
-DAP_ENABLE_FIRESTRIP=0    ; or use alternative pin
-DAP_ENABLE_BADMOTIVATOR=0 ; or use alternative pin
```

**Option C — Sound via alternative hardware, gadgets on AUX4/AUX5:**
Rewire the sound board to Serial2 (pins 16/17, after verifying Marcduino is not needed),
or use a software serial on free GPIOs.

### Runtime Commands to Test After Wiring

After flashing with gadgets enabled, verify function over serial or web API:

```
# Test FireStrip — 500 ms spark
~RT FSOFF
~RT FS1500

# Test FireStrip — 2.5 s fire
~RT FS22500

# Test BadMotivator — smoke on (6.5 s max, 1-min cooldown)
~RT BMON
~RT BMOFF

# Test CBI — normal animation
~RT CB00000

# Test CBI — flicker
~RT CB20000

# Test DataPanel — normal animation
~RT DP00000

# Test DataPanel — disabled (off)
~RT DP10000
```

(`~RT` prefix routes via the direct command handler in this firmware.)

---

## 10. Troubleshooting

### FireStrip shows no output

1. Verify `AP_ENABLE_FIRESTRIP=1` in `platformio.ini` and reflash.
2. Check GPIO 18 continuity to strip DIN.
3. Verify 5 V and GND reach the strip — measure at the strip connector with a multimeter.
4. The 300 Ω series resistor should be present; if missing, the signal may be damaged.
5. Send `FS22500` (fire for 2.5 s) and observe the strip LEDs.
6. Confirm no LEDLIB conflict: `USE_LEDLIB` should be unset or `0` (FastLED mode).

### BadMotivator relay does not click

1. Verify `AP_ENABLE_BADMOTIVATOR=1` in `platformio.ini` and reflash.
2. Check GPIO 19 voltage with a multimeter: should go HIGH (≈ 3.3 V) when `BMON` is received.
3. Verify relay module VCC is 5 V (not 3.3 V).
4. If relay does not click on signal: confirm relay module accepts 3.3 V input. Some modules require 5 V logic — add a level-shifting transistor or use an opto-isolated module.
5. Check the 1-minute cooldown: if `BMON` was recently sent, it is suppressed. Wait 60 s.
6. Maximum activation: 6.5 s — `BMON` will auto-cut after this period.

### CBI or DataPanel — no LED output

1. Verify `AP_ENABLE_CBI=1` / `AP_ENABLE_DATAPANEL=1` in `platformio.ini`.
2. Check MAX7221 VCC (must be 5 V — measure at the IC).
3. Verify signal connections:
   - DIN → AUX3 (GPIO 5)
   - CLK → AUX2 (GPIO 4)
   - LOAD → AUX1 (GPIO 2)
4. GPIO 2 boot caveat: if the board fails to boot with CBI connected, add a 10 kΩ
   pull-down on the LOAD line.
5. Send `CB00000` (CBI normal) and `DP00000` (DataPanel normal) to trigger animation.
6. ISET resistor: if LEDs are dim, reduce from 47 kΩ to 27 kΩ.
7. If CBI works but DataPanel is dark — check the DOUT → DIN daisy-chain connection.

### Board boots but gadgets behave erratically

1. Add 100 nF decoupling capacitors at each MAX7221 VCC pin.
2. Add 1000 µF / 6.3 V bulk cap at the FireStrip power injection point.
3. Check for shared GND: all devices (board, MAX7221 chips, LED strips) must share a
   common GND reference.
4. Keep signal wires under 30 cm where possible; use twisted pairs for CLK/DIN runs over 30 cm.

---

## References

### Reeltwo Library

| Source file | Component | GitHub link |
|---|---|---|
| `src/dome/FireStrip.h` | FireStrip LED animation | https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/dome/FireStrip.h |
| `src/dome/BadMotivator.h` | BadMotivator relay control | https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/dome/BadMotivator.h |
| `src/body/ChargeBayIndicator.h` | CBI LED sequences | https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/body/ChargeBayIndicator.h |
| `src/body/DataPanel.h` | DataPanel LED sequences | https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/body/DataPanel.h |
| `src/core/LedControlMAX7221.h` | MAX7221 SPI driver | https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/core/LedControlMAX7221.h |
| `src/core/RelaySwitch.h` | Relay timing/protection | https://github.com/reeltwo/Reeltwo/blob/23.5.3/src/core/RelaySwitch.h |

### External Resources

- **AstroPixels board:** https://we-make-things.co.uk/product/astropixels/
- **Reeltwo documentation:** https://reeltwo.github.io/Reeltwo/html/index.html
- **MAX7221 datasheet:** https://datasheets.maximintegrated.com/en/ds/MAX7219-MAX7221.pdf
- **R2 Builders forums:** https://astromech.net/forums/
- **WS2812B datasheet:** https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf

---

*Generated from firmware analysis of AstroPixelsPlus + Reeltwo `23.5.3` source.*
