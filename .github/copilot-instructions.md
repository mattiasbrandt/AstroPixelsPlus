# AstroPixelsPlus Firmware - Copilot Instructions

## Project Overview
**AstroPixelsPlus** is an ESP32-based WiFi controller for R2-D2 dome animatronics. This is a **personal fork** with extensive improvements and enhancements over the original reeltwo/AstroPixelsPlus project.

**Hardware**: AstroPixels board (ESP32 dev board with integrated components)  
**Purchase**: https://we-make-things.co.uk/product/astropixels/  
**Original Repository**: https://github.com/reeltwo/AstroPixelsPlus  
**This Fork**: Contains major memory optimizations, web UI reorganization, and credenda fork features

## What This Firmware Does

### Primary Functions
Controls R2-D2 dome animatronics via multiple interfaces:
- **Dome Panels**: 13 servos via two PCA9685 I2C servo controllers (7 moving panels)
- **Logic Displays**: Front Logic Displays (FLD) and Rear Logic Display (RLD) using WS2812B RGB LEDs
- **Holoprojectors**: 3 holos (Front, Rear, Top) with RGB LEDs + 2-axis servos each
- **PSI Lights**: Front and rear Process State Indicator RGB lights
- **WiFi Access Point**: Web interface at http://192.168.4.1
- **Serial Commands**: MarcDuino protocol on Serial2 (2400 baud)
- **OTA Updates**: Over-the-air firmware updates
- **mDNS**: Access via http://astropixels.local

### Key Architectural Features
- **ESP32 Platform**: Dual-core, WiFi, Bluetooth (230MHz)
- **ReelTwo Framework**: R2-D2 component library (servos, LEDs, effects)
- **Web-Based Control**: Responsive HTML interface with live control
- **MarcDuino Compatible**: Accepts standard MarcDuino serial commands
- **Preferences Storage**: Settings saved to ESP32 NVS (non-volatile storage)
- **I2C Master Mode**: Controls two PCA9685 boards for servos (default mode)
- **I2C Slave Mode**: Can act as I2C slave (disables servo control)

## Current User Configuration

This fork is configured for a **standard R2-D2 dome setup**:
- **Dome Controller**: Controls dome panels, holos, and logics
- **Body Controller**: Separate BetterDuino Mega 2560 (see BetterDuinoFirmwareV4 workspace)
- **WiFi Bridge**: ESP8266 D1 Mini (protoAstroControl) connects body to WiFi
- **Network Architecture**: 
  - AstroPixels: WiFi AP at 192.168.4.1 (dome control)
  - ESP8266 Bridge: WiFi client at 192.168.4.2 (body control via BetterDuino)
  - R2 Touch app connects to either IP on port 9750

## Hardware Architecture

### ESP32 Pin Assignments
```cpp
// I2C for servo controllers
#define PIN_SDA 21        // I2C Data
#define PIN_SCL 22        // I2C Clock

// RGB LED outputs (WS2812B data pins)
#define PIN_FRONT_LOGIC 15   // Front Logic Displays (2 displays)
#define PIN_REAR_LOGIC 33    // Rear Logic Display (1 display)
#define PIN_FRONT_PSI 32     // Front PSI lights
#define PIN_REAR_PSI 23      // Rear PSI lights
#define PIN_FRONT_HOLO 25    // Front Holo RGB LEDs
#define PIN_REAR_HOLO 26     // Rear Holo RGB LEDs
#define PIN_TOP_HOLO 27      // Top Holo RGB LEDs

// Auxiliary pins (multipurpose)
#define PIN_AUX1 2        // CBI Load (if using Charge Bay Indicator)
#define PIN_AUX2 4        // CBI Clock
#define PIN_AUX3 5        // CBI Data In
#define PIN_AUX4 18       // Sound RX (Serial1)
#define PIN_AUX5 19       // Sound TX (Serial1) / RLD Clock (curved)

// Serial2 (MarcDuino commands)
#define SERIAL2_RX_PIN 16
#define SERIAL2_TX_PIN 17
```

### Servo Configuration (PCA9685 Controllers)

**Two I2C PCA9685 boards** at addresses 0x40 and 0x41:

**Controller 1 (0x40) - Dome Panels:**
- Ch 0: Panel 1 (small, front lower left)
- Ch 1: Panel 2 (small, front lower right)
- Ch 2: Panel 3 (small, left side lower)
- Ch 3: Panel 4 (small, right side lower)
- Ch 4: Panel 5 (medium, Magic Panel - fixed, doesn't move)
- Ch 5: Panel 6 (large, front upper)
- Ch 6: Mini Panel A
- Ch 7: Panel 7 (medium, left upper side)
- Ch 8: Panel 8 (rear PSI location - fixed)
- Ch 9: Panel 9 (RLD location - fixed)
- Ch 10: Panel 10 (rear upper)
- Ch 11: Panel 11 (top pie panel - rarely used)
- Ch 12: Panel 12 (top pie panel - rarely used)

**Controller 2 (0x41) - Holoprojectors:**
- Ch 0: Front Holo Horizontal servo
- Ch 1: Front Holo Vertical servo
- Ch 2: Rear Holo Horizontal servo
- Ch 3: Rear Holo Vertical servo
- Ch 4: Top Holo Horizontal servo
- Ch 5: Top Holo Vertical servo
- Ch 6-15: Available

**Moving Panels (7 total)**: P1, P2, P3, P4, P7, P10, P11 (P6 sometimes)  
**Fixed Panels (5 total)**: P5 (Magic), P6 (upper), P8 (PSI), P9 (RLD), P13 (top)

## Code Architecture

### File Structure
```
AstroPixelsPlus/
├── AstroPixelsPlus.ino      # Main sketch - setup(), loop(), WiFi, OTA
├── WebPages.h                # Web interface HTML (CRITICAL - memory limits!)
├── Screens.h                 # Menu screens (if USE_MENUS defined)
├── web-images.h              # Base64 encoded images for web UI
├── platformio.ini            # PlatformIO build configuration
├── MarcduinoPanel.h          # Panel sequence commands (:SE, :OP, :CL)
├── MarcduinoHolo.h           # Holo commands (*ON, *HP, *RD)
├── MarcduinoLogics.h         # Logic display commands (@0T, @1M)
├── MarcduinoPSI.h            # PSI light commands
├── MarcduinoSequence.h       # @APLE sequence parser
├── MarcduinoSound.h          # Sound commands (if sound board connected)
├── logic-sequences.h         # Logic display effect definitions
├── effects/                  # Visual effects (Plasma, Fractal, etc.)
│   ├── BitmapEffect.h
│   ├── FadeAndScrollEffect.h
│   ├── FractalEffect.h
│   ├── MeatBallsEffect.h
│   └── PlasmaEffect.h
└── menus/                    # Menu system screens
    ├── MainScreen.h
    ├── LogicsScreen.h
    ├── HoloScreen.h
    └── SequenceScreen.h
```

### Key Libraries (ReelTwo Framework)
- **`dome/Logics.h`**: Logic display base classes
- **`dome/LogicEngineController.h`**: Logic animation controller
- **`dome/HoloLights.h`**: Holoprojector RGB LED control
- **`dome/NeoPSI.h`**: PSI RGB lights
- **`ServoDispatchPCA9685.h`**: I2C servo controller driver
- **`ServoSequencer.h`**: Choreographed servo sequences
- **`core/Marcduino.h`**: MarcDuino command parser macros
- **`wifi/WifiAccess.h`**: WiFi AP/Client management
- **`wifi/WifiWebServer.h`**: Web server implementation

### ReelTwo Framework Patterns

**MARCDUINO_ACTION Macro:**
```cpp
// Define MarcDuino command handlers
MARCDUINO_ACTION(CommandName, @0T1, ({
    // Code to execute when @0T1 received
    FLD.selectSequence(LogicEngineRenderer::NORMAL);
    RLD.selectSequence(LogicEngineRenderer::NORMAL);
}))
```

**DO_COMMAND_AND_WAIT Pattern:**
```cpp
// Execute command, wait, then reset
MARCDUINO_ACTION(FailureSequence, @0T4, ({
    FLD.selectSequence(LogicEngineRenderer::FAILURE);
    DO_COMMAND_AND_WAIT(F("HPA007|11\n"), 11000)  // Holo fail for 11s
    DO_RESET({
        // Auto-reset to normal after timeout
        FLD.selectSequence(LogicEngineRenderer::NORMAL);
    })
}))
```

## ESP32 Memory Management - CRITICAL

### The Memory Crisis (Solved December 2025)

**Problem**: ESP32 static initialization crashes with `vApplicationGetIdleTaskMemory` assert failure  
**Root Cause**: WButton constructors allocate heap memory via `malloc()` during global object construction **before** ESP32 heap is fully initialized  
**Symptom**: Boot crashes when WButton count + String arrays exceed ~12-15KB heap

### PROVEN SAFE LIMITS (WebPages.h)
```cpp
// TESTED DECEMBER 2025 - DO NOT EXCEED:
// - Maximum ~50 WButton instances (each ≈200 bytes heap)
// - Maximum 6-7 String arrays (total size matters)
// - Combined: WButtons + String arrays < 12-15KB

// CURRENT STABLE CONFIGURATION:
// - 27 WButton instances (46% margin remaining)
// - 8 String arrays (panelSequences, holoLights, etc.)
// Result: ✅ 100% stable boots, zero crashes
```

### Memory Test Results (Documented)
| WButtons | String Arrays | Boot Result |
|----------|---------------|-------------|
| 27 | 8 | ✅ STABLE (current) |
| 37 | 7 | ✅ BOOTS |
| 44 | 6 | ✅ BOOTS |
| 52 | 6 | ✅ BOOTS |
| 54 | 7 (large) | ❌ CRASH |
| 65 | 6 | ❌ CRASH |
| 97 | 6 | ❌ CRASH (before refactor) |

### Safe vs Unsafe Component Patterns

**SAFE (no heap during static init):**
- ✅ `W1()`, `WLabel()`, `WHR()` - simple elements
- ✅ `WSlider()`, `WTextField()`, `WSelect()` - form inputs
- ✅ `WHorizontalAlign()`, `WVerticalAlign()` - layouts
- ✅ `const char* arrays[]` - no heap allocation
- ✅ `int var;` (uninitialized) - no heap
- ✅ Unicode arrows: ↖↑↗←●→↙↓↘

**UNSAFE (heap allocation during static init):**
- ❌ Too many `WButton()` instances (limit ~50)
- ❌ Too many `String arrays[]` (limit 6-7)
- ❌ `WStyle()` - uses appendCSS (heap)
- ❌ Emojis in strings - WiFi crashes
- ❌ `String var = "value";` in global scope
- ❌ `int var = 0;` (initialized) - avoid if possible

### How to Count Instances (Troubleshooting)
```powershell
# PowerShell - Count WButtons
(Get-Content WebPages.h | Select-String -Pattern '\bWButton\(' -AllMatches).Matches.Count

# PowerShell - Count String arrays
(Get-Content WebPages.h | Select-String -Pattern '^String .*\[\]').Count
```

### Memory Optimization Strategy Applied
**Before (97 WButtons)**: Constant boot crashes  
**After (27 WButtons)**: 100% stable

**Changes Made:**
1. Consolidated 70 WButtons into WSelect dropdowns (memory efficient)
2. Removed non-moving panel buttons (P5, P6, P8, P9, P13)
3. Removed redundant Back/Home navigation buttons
4. Organized by function: Panels by location, Holos by type
5. Kept 23-button margin for future features (27/50 limit)

## Web Interface Organization

### Panels Page (WebPages.h lines 72-170)

**Organization by R2-D2 Dome Physical Location:**

```
┌─────────────────────────────────────┐
│   Predefined Sequences (dropdown)   │  21 choreographed panel animations
├─────────────────────────────────────┤
│   Front Lower Panels                │
│   ├─ Panel 1 OP/CL  Panel 2 OP/CL  │  Flank front logic displays
├─────────────────────────────────────┤
│   Side Lower Panels                 │
│   ├─ Panel 3 OP/CL  Panel 4 OP/CL  │  Left and right lower sides
├─────────────────────────────────────┤
│   Upper/Rear Panels                 │
│   ├─ Panel 7 OP/CL  Panel 10 OP/CL │  Upper sides and rear upper
├─────────────────────────────────────┤
│   Quick Actions                     │
│   ├─ Open All  Close All  Stop     │  Emergency controls
└─────────────────────────────────────┘
```

**Command Types:**
- `:SE##` - Predefined sequences (choreographed, timed)
- `:OP##` - Open panel (immediate, no choreography)
- `:CL##` - Close panel (immediate)
- `*ST00` - Emergency stop (all servos)

**Removed Non-Moving Panels** (saved 10 WButtons):
- P5 (Magic Panel) - fixed panel
- P6 (small upper) - fixed panel
- P8 (Rear PSI location) - fixed panel
- P9 (RLD location) - fixed panel
- P13 (top panel) - non-standard/rarely used

### Holoprojectors Page (WebPages.h lines 208-280)

**Separated by Function Type** (reduces user confusion):

```
┌─────────────────────────────────────┐
│   Holo Light Effects (dropdown)     │
│   ├─ All On/Off                     │  *ON00, *OF00 (all holos)
│   ├─ Front/Rear/Top On/Off          │  *ON01-03, *OF01-03
│   ├─ Pulse (All/Front/Rear/Top)     │  *HP00-03 - LED dim pulse
│   └─ Rainbow (All/Front/Rear/Top)   │  *HP04-07 - LED color cycle
├─────────────────────────────────────┤
│   Holo Movements (dropdown)         │
│   ├─ Random (All/Front/Rear/Top)    │  *RD00-03 - Servo random
│   ├─ Nod (All/Front/Rear/Top)       │  *HN00-03 - Servo nod
│   └─ Reset All                      │  *ST00 - Reset servos
├─────────────────────────────────────┤
│   Auto HP Twitch Control            │
│   ├─ Disable (D198)                 │  Turn off auto movement
│   └─ Enable (D199)                  │  Turn on auto movement
└─────────────────────────────────────┘
```

**IMPORTANT DISTINCTION:**
- **Light Effects** (`*HP00-07`): Control **LEDs ONLY** (no servo movement)
  - Pulse: Dim pulse random color on LEDs
  - Rainbow: LED color cycling animation
- **Movements** (`*RD`, `*HN`): Control **SERVOS ONLY** (no LED change)
  - Random: Servo random positions
  - Nod: Servo nodding animation

**Verified in MarcduinoHolo.h source code comments.**

### Logic Displays Page (WebPages.h lines 345-420)

**Organized by Display Location:**

```
┌─────────────────────────────────────┐
│   Front Logic Displays (FLDs)       │  Two front displays
│   ├─ Sequence (dropdown)            │
│   ├─ Color (dropdown)               │
│   ├─ Speed (slider)                 │
│   ├─ Duration (slider)              │
│   └─ Text Message (input)           │
├─────────────────────────────────────┤
│   Rear Logic Display (RLD)          │  Single rear display
│   ├─ Sequence (dropdown)            │
│   ├─ Color (dropdown)               │
│   ├─ Speed (slider)                 │
│   ├─ Duration (slider)              │
│   └─ Text Message (input)           │
├─────────────────────────────────────┤
│   Apply Settings Button             │  Apply to both displays
└─────────────────────────────────────┘
```

**Benefits:**
- Clear separation between FLD (front) and RLD (rear)
- All settings grouped by display location
- Single Apply button reduces complexity

## MarcDuino Command Protocol

### Command Categories

**Panel Commands** (MarcduinoPanel.h):
- `:CL00` - Close all panels
- `:OP00` - Open all panels
- `:OF00` - Flutter all panels
- `:SE##` - Sequence ## (predefined choreography)
- `:OP##` - Open panel ##
- `:CL##` - Close panel ##
- `:SF##$#` - Set servo easing

**Holo Commands** (MarcduinoHolo.h):
- `*ON00` - All holos on (dim cycle) **[CREDENDA FORK]**
- `*OF00` - All holos off **[CREDENDA FORK]**
- `*ON##` - Holo ## on (01=Front, 02=Rear, 03=Top)
- `*OF##` - Holo ## off
- `*HP0##-07##` - Pulse/Rainbow LED effects (00-03=Pulse, 04-07=Rainbow)
- `*RD##` - Random servo movement
- `*HN##` - Nod servo animation
- `*HP###` - Specific servo positions (e.g., `*HP001` = front down)
- `*ST00` - Reset all holos

**Logic Display Commands** (MarcduinoLogics.h):
- `@0T1` - All logics normal
- `@0T2` - All logics flashing color
- `@0T3` - All logics alarm
- `@0T4` - All logics failure **[ENHANCED - adds holo animation]**
- `@0T5` - All logics red alert/scream **[ENHANCED - adds holo scream]**
- `@0T6` - All logics Leia **[ENHANCED - adds holo Leia message]**
- `@0T11` - All logics march
- `@1T#` - Front logics only (same numbers)
- `@2T#` - Rear logics only (same numbers)
- `@1MText` - Set front logics text and scroll left
- `@2MText` - Set rear logics text and scroll left
- `@#P60` - Set font to Latin (#=1/2/3)
- `@#P61` - Set font to Aurabesh (#=1/2/3)

**Advanced Logic Sequences** (MarcduinoSequence.h):
- `@APLE[L][EE][C][S][NN]` - Custom logic sequence
  - L: Logic (0=All, 1=Front, 2=Rear)
  - EE: Effect (00-23, 99=Random)
  - C: Color (0-9)
  - S: Speed/Sensitivity (1-9)
  - NN: Duration (seconds, 00=continuous)
  - Example: `@APLE51000` = Solid Red continuous

**Configuration Commands** (AstroPixelsPlus.ino):
- `#APWIFI` / `#APWIFI0` / `#APWIFI1` - Toggle/disable/enable WiFi
- `#APREMOTE` / `#APREMOTE0` / `#APREMOTE1` - Toggle/disable/enable remote
- `#APZERO` - Clear all preferences (FACTORY RESET)
- `#APRESTART` - Restart ESP32

## Credenda Fork Improvements

This fork integrates enhancements from the credenda/AstroPixelsPlus fork:

### 1. All Holos On/Off Commands (MarcduinoHolo.h)
```cpp
// CREDENDA FORK IMPROVEMENT
*ON00 - Turn all three holos ON (dim cycle random color)
*OF00 - Turn all three holos OFF

// Implementation:
CommandEvent::process(F("HPA0040")); // All holo dim cycle
CommandEvent::process(F("HPA0000")); // All holo off
```

### 2. Enhanced Logic Sequences (MarcduinoLogics.h)

**Failure Sequence (@0T4):**
- Added synchronized holo projector failure animation (HPA007)
- Auto-resets to normal after 11 seconds
```cpp
DO_COMMAND_AND_WAIT(F("HPA007|11\n"), 11000)
DO_RESET({ resetToNormal(); })
```

**Scream/Red Alert (@0T5):**
- Added synchronized holo projector scream animation (HPA0040)
- Auto-resets to normal after 7 seconds
```cpp
DO_COMMAND_AND_WAIT(F("HPA0040|7\n"), 7000)
DO_RESET({ resetToNormal(); })
```

**Leia Sequence (@0T6):**
- Added synchronized front holo Leia message (HPS1)
- Auto-resets after 45 seconds
```cpp
DO_COMMAND_AND_WAIT(F("HPS1|45\n"), 45000)
DO_RESET({ resetSequence(); })
```

### 3. Enhanced Startup Text (AstroPixelsPlus.ino line 674)
```cpp
// CREDENDA FORK IMPROVEMENT
// Shows "STAR WARS" on RLD and "R2D2" on FLD during boot
RLD.selectScrollTextLeft("... STAR WARS ....", LogicEngineRenderer::kBlue, 0, 15);
FLD.selectScrollTextLeft("... R2D2 ...", LogicEngineRenderer::kRed, 0, 15);
```

**Effect**: Iconic Star Wars branding before WiFi starts

## Build System - PlatformIO

### platformio.ini Configuration
```ini
[platformio]
default_envs = astropixelsplus
src_dir = .  # Source files in root directory (Arduino sketch style)

[env:astropixelsplus]
platform = espressif32@5.2.0
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_port = COM8  # Change to your port
monitor_port = COM8

lib_deps = 
    https://github.com/reeltwo/Reeltwo#23.5.3
    https://github.com/adafruit/Adafruit_NeoPixel
    https://github.com/FastLED/FastLED
    https://github.com/DFRobot/DFRobotDFPlayerMini

build_flags = 
    -DCORE_DEBUG_LEVEL=1
    -DESP32_ARDUINO_NO_RGB_BUILTIN  # Prevent FastLED RMT conflicts
    -mfix-esp32-psram-cache-issue
    -Os  # Optimize for size
```

### Build Commands
```bash
# Via PlatformIO toolbar in VS Code (recommended)
# Or via CLI:
pio run                    # Build
pio run -t upload          # Upload via USB
pio run -t monitor         # Serial monitor
pio device monitor         # Alternative monitor
```

### Over-the-Air (OTA) Updates
Once WiFi is configured, upload via network:
```bash
# Arduino IDE: Tools → Port → AstroPixels at 192.168.4.1
# PlatformIO: platformio.ini upload_port = 192.168.4.1
```

## Integration with Other Systems

### Multi-Controller Droid Architecture

**This Setup:**
- **Dome**: AstroPixelsPlus ESP32 (this firmware)
  - Controls: Panels, holos, logic displays, PSI lights
  - WiFi: AP mode at 192.168.4.1
  - Web: http://192.168.4.1 or http://astropixels.local
  - Serial: MarcDuino commands on Serial2 (2400 baud)

- **Body**: BetterDuino (Arduino Mega 2560)
  - Controls: Body servos, sound, utilities
  - Connection: Serial to ESP8266 WiFi bridge
  - Repository: BetterDuinoFirmwareV4 (same workspace)

- **WiFi Bridge**: protoAstroControl (ESP8266 D1 Mini)
  - Connects: BetterDuino to WiFi network
  - IP: 192.168.4.2 (connects to AstroPixels AP)
  - Protocol: TCP:9750 ↔ Serial:9600 transparent bridge
  - Repository: ESP8266_BetterDuino_Bridge (same workspace)

**Network Diagram:**
```
                    ┌─────────────────────┐
                    │   R2 Touch App      │
                    │   (iOS/Android)     │
                    └──────────┬──────────┘
                               │ WiFi
           ┌───────────────────┴───────────────────┐
           │                                       │
    ┌──────▼─────────┐                  ┌─────────▼──────┐
    │ AstroPixels    │                  │ ESP8266 Bridge │
    │ 192.168.4.1    │◄─────WiFi────────┤ 192.168.4.2    │
    │ (Dome Master)  │                  │ (Body Bridge)  │
    └────────────────┘                  └────────┬───────┘
           │                                     │ Serial
           │ I2C                                 │ 9600
           │                                     │
    ┌──────▼─────────┐                  ┌───────▼────────┐
    │ PCA9685 Servos │                  │ Mega 2560      │
    │ (Dome Panels)  │                  │ BetterDuino    │
    └────────────────┘                  │ (Body Master)  │
                                        └────────────────┘
```

**Why Two WiFi Devices:**
- AstroPixels can't do WiFi + Serial bridge simultaneously (ESP32 limitations)
- Separate ESP8266 dedicated to body serial bridging
- Both connect to same WiFi AP for unified control

### R2 Touch / R2 Control Apps
- Connect to AstroPixels: `192.168.4.1:9750`
- Connect to Body (via bridge): `192.168.4.2:9750`
- Send MarcDuino commands (`:SE00`, `$85`, etc.)
- Commands routed to appropriate controller

## Common Development Workflows

### Adding a New Panel Sequence

1. **Define sequence** in `MarcduinoPanel.h`:
```cpp
MARCDUINO_ACTION(MySequence, :SE42, ({
    SEQUENCE_PLAY_ONCE_EASING(servoSequencer, SeqPanelMySequence, MY_EASING);
}))
```

2. **Create sequence array** (if custom timing needed):
```cpp
static const ServoSequence::Settings SeqPanelMySequence[] PROGMEM = {
    {1, 1000, 500, 0},  // Panel 1, 1000ms delay, 500 open pos, 0 ease
    {2, 500, 500, 0},   // Panel 2, 500ms delay, 500 open pos
    {0, 0, 0, 0}        // Terminator
};
```

3. **Add to web UI dropdown** in `WebPages.h`:
```cpp
String panelSequences[] = {
    // ... existing sequences
    "My Sequence|:SE42"
};
```

### Adding a New Holo Command

1. **Add to** `MarcduinoHolo.h`:
```cpp
MARCDUINO_ACTION(MyHoloEffect, *MY00, ({
    frontHolo.setEffect(HoloLights::kPulse);
    rearHolo.setEffect(HoloLights::kRainbow);
}))
```

2. **Add to web UI** in `WebPages.h`:
```cpp
String holoLights[] = {
    // ... existing commands
    "My Effect|*MY00"
};
```

### Modifying Web Interface

**CRITICAL**: Count WButtons before adding!
```powershell
(Get-Content WebPages.h | Select-String '\bWButton\(').Count
# If > 45, consolidate into WSelect dropdowns instead
```

**Safe additions:**
```cpp
// Add dropdown (memory efficient)
WSelect holoSelect("holosel", "Holo Effects", holoLights, ARRASIZE(holoLights));

// Add slider (safe)
WSlider speedSlider("speed", "Speed", "1", "9", "5");

// Add text input (safe)
WTextField textInput("msg", "Message", "Hello R2");
```

**Avoid:**
```cpp
// DON'T add many WButtons if already near 50 limit
WButton btn1("B1", "Button 1");  // Each costs ~200 bytes heap
WButton btn2("B2", "Button 2");
// ... 20 more buttons = crash risk
```

## Troubleshooting Guide

### Issue: Boot Crashes with "vApplicationGetIdleTaskMemory"
**Cause**: Too many WButtons or String arrays in WebPages.h  
**Solution**:
1. Count instances: `(Get-Content WebPages.h | Select-String '\bWButton\(').Count`
2. If > 50: Consolidate WButtons into WSelect dropdowns
3. If String arrays > 7: Combine or reduce
4. Reference IMPROVEMENTS.md for consolidation examples

### Issue: WiFi Won't Start
**Checks:**
1. Preferences corrupted: Send `#APZERO` (factory reset)
2. Default credentials: SSID="AstroPixels", Password="Astromech"
3. Serial monitor (115200 baud): Check WiFi init messages
4. WiFi disabled: Send `#APWIFI1` to enable

### Issue: Servos Not Responding
**Checks:**
1. I2C scan: Check `scan_i2c()` output in serial monitor
2. PCA9685 addresses: Must be 0x40 and 0x41
3. I2C wiring: SDA pin 21, SCL pin 22
4. Servo power: Separate 5-6V supply (not USB)
5. I2C slave mode: Verify `USE_I2C_ADDRESS` NOT defined (disables servos)

### Issue: MarcDuino Commands Ignored
**Checks:**
1. Serial2 baud rate: 2400 (check sending device)
2. Command terminator: Must end with `\r` (carriage return)
3. Serial2 pins: RX=16, TX=17
4. Command format: `:SE00`, `*ON01`, `@0T1` (exact syntax)
5. Serial monitor: Enable at 115200 to see command echo

### Issue: Web Interface Blank/Crashes
**Cause**: Memory issue or SPIFFS not mounted  
**Solution**:
1. Check serial for SPIFFS errors
2. Reflash filesystem: PlatformIO → Upload Filesystem Image
3. Reduce WButton count if memory-related
4. Clear browser cache (old cached pages)

### Issue: OTA Update Fails
**Checks:**
1. WiFi connected: Verify IP address stable
2. Network latency: Must be on same network/AP
3. Firewall: Allow port 3232
4. Sketch size: Must fit in OTA partition (~1.4MB limit)
5. Alternative: Use USB upload if OTA unstable

### Issue: LEDs Not Working
**Checks:**
1. Data pin correct: FLD=15, RLD=33, Holos=25/26/27
2. FastLED conflicts: Verify `ESP32_ARDUINO_NO_RGB_BUILTIN` defined
3. Power: LEDs need separate 5V supply (not ESP32 3.3V)
4. LED count: Check logic-sequences.h for pixel counts
5. Test command: `@0T2` (flashing color)

## Code Review Checklist

When reviewing or modifying this firmware:

✅ **Memory Safety**
- WButton count < 50 (check with PowerShell command)
- String array count < 7
- No WStyle() in global scope
- No emojis in strings (use Unicode arrows)

✅ **ESP32 Configuration**
- `ESP32_ARDUINO_NO_RGB_BUILTIN` defined (prevents FastLED conflicts)
- Pin assignments don't overlap
- I2C addresses unique (0x40, 0x41 for PCA9685)
- Serial ports correct (Serial2 for MarcDuino)

✅ **MarcDuino Protocol**
- Commands end with `\r` terminator
- Command format correct (`:`, `*`, `@`, `#` prefixes)
- MARCDUINO_ACTION macros used for new commands
- Commands added to web UI dropdowns

✅ **Web Interface**
- Added features use WSelect (not WButton) where possible
- Tested on mobile browser (responsive design)
- JavaScript functions defined in web-images.h if needed
- Applied settings button wired correctly

✅ **Integration**
- Serial baud rates match connected devices
- WiFi credentials match network setup
- OTA enabled for remote updates
- Preferences saved for important settings

## Development Guidelines

### When Modifying Code

**DO:**
- Count WButtons BEFORE adding new web features
- Use MARCDUINO_ACTION macro for command handlers
- Store preferences for user-configurable settings
- Test on actual hardware (servos, LEDs)
- Use ReelTwo AnimatedEvent for non-blocking animations
- Check IMPROVEMENTS.md for recent changes

**DON'T:**
- Add many WButtons near the 50-instance limit
- Use emojis in web UI strings (WiFi crashes)
- Block in loop() - use AnimatedEvent or timers
- Modify core ReelTwo library files (fork if needed)
- Change I2C addresses without updating servoSettings
- Forget to update both .ino and PlatformIO build

### Version Tracking
**Current Fork Version**: Based on reeltwo/AstroPixelsPlus + December 2025 improvements  
**Major Changes**: See IMPROVEMENTS.md (~822 additions, ~522 deletions)

**Update locations:**
- `AstroPixelsPlus.ino`: Sketch comments
- `IMPROVEMENTS.md`: Detailed change log
- Git commit messages: Reference issue/feature

### Testing New Features
1. **Memory test**: Count WButtons + String arrays before flash
2. **Boot test**: Monitor serial for crashes during startup
3. **Function test**: Verify servos, LEDs, commands work
4. **Web test**: Check web interface on mobile + desktop
5. **Integration test**: Test with R2 Touch app if applicable
6. **OTA test**: Verify OTA updates still work

## Known Issues & Limitations

### Hardware Limitations
- **ESP32 I2C**: Occasional glitches with long wires (use short quality cables)
- **PCA9685 Servo Count**: 16 channels per board, 32 total max (two boards)
- **WiFi Range**: 2.4GHz only, ~30ft range inside R2 dome
- **LED Strip Length**: Power injection needed for >100 LEDs per strip

### Software Limitations
- **Memory Constraints**: WButton limit requires careful UI design
- **I2C vs Slave Mode**: Can't be I2C master AND slave simultaneously
- **Serial2 Dedicated**: Can't share Serial2 for other purposes
- **OTA Size**: Sketch must fit in OTA partition (~1.4MB)

### Future Enhancement Ideas
- [ ] Dynamic WButton allocation (remove 50-button limit)
- [ ] MQTT support for home automation integration
- [ ] Real-time panel position feedback (if servos have sensors)
- [ ] Audio reactive logic displays (FFT on mic input)
- [ ] Bluetooth control (ESP32 has BLE capability)
- [ ] Multi-language web interface
- [ ] Saved animation presets (store sequences in preferences)

## Security & Safety Notes

- **WiFi Password**: Change default "Astromech" password via web interface
- **No Encryption**: WiFi AP uses WPA2-PSK, not WPA3
- **No Authentication**: Web interface has no login (trusted network only)
- **Command Injection**: MarcDuino commands trusted (no validation)
- **Servo Limits**: Set min/max in servoSettings to prevent mechanical damage
- **Power Supply**: Servos/LEDs need separate 5-6V supply (not USB/ESP32)
- **Factory Reset**: `#APZERO` clears all settings (use cautiously)

## Related Documentation

- **AstroPixels Board**: https://we-make-things.co.uk/product/astropixels/
- **Original Firmware**: https://github.com/reeltwo/AstroPixelsPlus
- **ReelTwo Library**: https://github.com/reeltwo/Reeltwo
- **MarcDuino Protocol**: https://www.curiousmarc.com/r2-d2/marcduino-system
- **R2 Builders Club**: https://astromech.net/forums/
- **Wokwi Simulator**: https://wokwi.com/projects/347975094870475347 (logic engine demo)

---

## Key Takeaway for Copilot Agents

This is a **heavily modified fork** of AstroPixelsPlus focused on **memory optimization** and **web UI improvements**. The critical constraint is **ESP32 static initialization memory** - WButton count must stay under 50 instances (currently at 27, leaving 46% margin).

**Current Configuration**: Standard R2-D2 dome controller with:
- 7 moving panels (P1-P4, P7, P10, P11)
- 3 holoprojectors (Front, Rear, Top) with RGB + servos
- Front Logic Displays (2) + Rear Logic Display (1)
- WiFi AP at 192.168.4.1
- MarcDuino serial on Serial2 (2400 baud)
- Credenda fork enhancements (All Holos On/Off, enhanced sequences, STAR WARS startup)

**Architecture**: ESP32 with ReelTwo framework, PCA9685 I2C servo controllers, WS2812B RGB LEDs, web-based control, OTA updates.

**When helping with changes**: 
1. Check WButton count FIRST before adding web features
2. Understand MarcDuino command format (`:`, `*`, `@`, `#` prefixes)
3. Reference IMPROVEMENTS.md for recent refactoring details
4. Test memory safety with boot stability
5. Respect the 50-WButton limit (current: 27/50)

**Integration context**: This is the **dome controller** in a multi-controller R2 build. The **body controller** (BetterDuino) and **WiFi bridge** (protoAstroControl) are separate projects in the same workspace.
