# AstroPixelsPlus Fork Improvements Documentation

This document summarizes all improvements made to the AstroPixelsPlus project to enhance usability, organization, and stability.

---

## 🎯 Summary of Changes

**Total Lines Modified:** ~822 additions, ~522 deletions across 7 files  
**Focus Areas:** Web interface reorganization, memory optimization, credenda fork integration, code documentation

---

## 0. 2026 Async Web Migration (Current Architecture)

### Why We Switched From ReelTwo WebPages

The old ReelTwo web UI (`WebPages.h` + `WifiWebServer` + `WButton`) allocated heap during static initialization. On ESP32 this led to a practical button/UI size ceiling and boot instability as the page set grew.

The migration replaced only the web transport/presentation layer with `ESPAsyncWebServer` + SPIFFS UI files:

- `AsyncWebInterface.h` now serves REST + WebSocket endpoints
- `data/*.html`, `data/style.css`, `data/app.js` now hold UI structure/behavior
- `WebPages.h` is now legacy/archived (`WebPages.h.bak`)

### Important Clarification: Core Robot Behavior Was Not Rewritten

This migration did **not** replace ReelTwo command handling. It wrapped the existing control path.

All control paths still converge on the same dispatcher:

```
Serial2 (Marcduino hardware)
WiFi socket (port 2000)
REST /api/cmd + WebSocket /ws
  -> Marcduino::processCommand(player, cmd)
  -> existing MARCDUINO_ACTION handlers
  -> existing ReelTwo hardware drivers
```

So REST/WebSocket is a new input layer on top of existing ReelTwo-era behavior.

### Endpoint Mapping to Existing Runtime/Core Calls

| Endpoint | Method | Runtime/Core Integration |
|----------|--------|--------------------------|
| `/api/cmd` | POST | `Marcduino::processCommand(player, cmd)` |
| `/ws` | WS text | `Marcduino::processCommand(player, cmd)` |
| `/api/pref` | GET/POST | existing `Preferences` read/write (`get*`, `put*`) |
| `/api/reboot` | POST | existing `reboot()` path |
| `/upload/firmware` | POST | existing `Update.*` OTA flow + logic progress rendering |
| `/api/state` `/api/health` `/api/logs` | GET | snapshot/broadcast wrappers over current runtime state |

### OTA vs SPIFFS Update Scope (Important)

The firmware page OTA upload (`/upload/firmware`) updates the ESP32 application binary only.

- OTA updates: `.bin` firmware image in app partition
- Not updated by OTA: SPIFFS web files (`data/*.html`, `data/style.css`, `data/app.js`, image assets)

If web UI files change, deploy them with SPIFFS upload over USB:

```bash
pio run -e astropixelsplus -t uploadfs --upload-port /dev/ttyUSB0
```

This distinction is now shown on the firmware page to reduce confusion during bench testing.

### OTA UX Improvement (2026-02)

Firmware-page upload UX was improved for real-world reboot behavior:

- Handles connection reset during reboot without falsely showing a failed/stuck state
- Waits for `/api/state` reconnect before final "device back online" success message
- Added collapsible Live Log Console on firmware page for OTA/debug visibility
- Home-page OTA indicator changed from full-screen dim overlay to non-blocking status panel

### What This Enables

- Removes compile-time widget count limits from firmware code
- Scales UI by editing SPIFFS web files rather than adding static `WButton` objects
- Keeps command behavior consistent across Serial2, socket, and web clients
- Simplifies future UI iteration/testing (`python3 -m http.server 8080 --directory data`)

---

## 1. ESP32 Memory Optimization & Crash Prevention

### Problem Solved
- **Root Cause:** WButton constructors call `appendBodyf()` → `FormatString()` → `malloc()` during static initialization
- **Symptoms:** Boot crashes with error: `assert failed: vApplicationGetIdleTaskMemory port_common.c:195`
- **Impact:** ESP32 heap not fully initialized during global object construction; exceeding ~12-15KB causes failure

### Solution Implemented
- Reduced WButton count from **97 → 27 instances** (70 buttons eliminated)
- Consolidated controls into WSelect dropdowns (more memory efficient)
- Removed non-moving panel buttons (P5, P6, P8, P9, P13)
- Removed redundant Back/Home navigation buttons from all pages

### Memory Limits Documented (WebPages.h lines 1-47)
```cpp
// PROVEN LIMITS (tested Dec 2025):
// - Maximum ~50 WButton instances in static arrays (each ≈200 bytes heap)
// - Maximum 6-7 String arrays (cumulative size matters)
// - Combined limit: WButtons + String arrays must stay under ~12-15KB total heap
//
// Current Config (STABLE):
// - 27 WButton instances
// - 8 String arrays (panelSequences, holoLights, holoMovements, logicsSeq, 
//   logicsColors, swBaudRates, soundPlayer, soundSerial)
```

### Test Results Documented
| WButtons | String Arrays | Result |
|----------|---------------|--------|
| 44 | 6 | ✅ BOOTS |
| 52 | 6 | ✅ BOOTS |
| 37 | 7 | ✅ BOOTS |
| 27 | 8 | ✅ BOOTS (current) |
| 65 | 6 | ❌ CRASH |
| 54 | 7 (large) | ❌ CRASH |

### Safe vs Unsafe Patterns Documented
**SAFE:**
- W1, WLabel, WHR, WSlider, WTextField, WSelect, WHorizontalAlign, WVerticalAlign
- const char* arrays (no heap allocation)
- int/bool variables without initialization (`int var;` NOT `int var = 0;`)
- Unicode arrows (↖↑↗←●→↙↓↘) - NOT emojis

**UNSAFE:**
- Too many WButton instances
- Too many String arrays
- WStyle (uses appendCSS → heap allocation)
- Emojis in strings (causes WiFi crashes)
- String var = "value"; in global scope

---

## 2. Web Interface Reorganization

### 2.1 Panels Page Improvements (WebPages.h lines 72-170)

#### Organization by Physical Dome Location
**Before:** All 13 panels listed without organization  
**After:** Grouped by R2-D2 dome physical locations

```
Organized Structure:
├── Predefined Sequences (dropdown with 21 choreographed animations)
├── Front Lower Panels: P1, P2 (flanking front logic displays)
├── Side Lower Panels: P3, P4 (left and right lower sides) 
├── Upper/Rear Panels: P7, P10 (upper sides and rear)
└── Quick Actions: Open All, Close All, Stop
```

#### Removed Non-Moving Panels
- **P5** (Magic Panel) - fixed panel, doesn't open
- **P6** (small upper panel) - fixed panel
- **P8** (Rear PSI location) - fixed panel
- **P9** (RLD location) - fixed panel
- **P13** (top panel) - non-standard/rarely used

**Benefit:** Saved 10 WButton instances, cleaner interface focused on moving panels

#### Command Type Distinction
- **:SE## commands:** Predefined sequences with timing/choreography
- **:OP/:CL commands:** Direct open/close (immediate, no choreography)
- **\*ST00 command:** Emergency stop for all servos

---

### 2.2 Holo Projectors Page Improvements (WebPages.h lines 208-280)

#### Separated Light Effects from Movements
**Problem:** Users confused between LED effects and servo movements  
**Solution:** Split into two distinct dropdowns with clear categorization

```
Holo Light Effects Dropdown (16 items):
├── All On/Off, Front On/Off, Rear On/Off, Top On/Off (8 commands)
├── Pulse (All/Front/Rear/Top) - LED dim pulse random color (4 commands)
└── Rainbow (All/Front/Rear/Top) - LED color cycling (4 commands)

Holo Movements Dropdown (9 items):
├── Random (All/Front/Rear/Top) - Servo random positions (4 commands)
├── Nod (All/Front/Rear/Top) - Servo nodding animation (4 commands)
└── Reset All - Reset to default state (1 command)

Auto HP Twitch Control:
├── Disable (D198) - Turn off automatic movements
└── Enable (D199) - Turn on automatic movements
```

#### Command Reference Table
| Command | Type | Description |
|---------|------|-------------|
| \*ON##, \*OF## | Light Control | Turn holo LED on/off |
| \*HP00-03 | Light Effect | Pulse - dim pulse random color |
| \*HP04-07 | Light Effect | Rainbow - LED color cycling |
| \*RD## | Movement | Random servo positions |
| \*HN## | Movement | Servo nod animation |
| \*ST00 | Control | Reset all to default |
| D198/D199 | Control | Disable/Enable auto twitch |

**Key Insight:** Pulse and Rainbow commands affect **LEDs only**, they do NOT move servos. This was verified in MarcduinoHolo.h source code comments: "Dim pulse random color" and "rainbow".

---

### 2.3 Logic Displays Page Improvements (WebPages.h lines 345-420)

#### Organization by Display Location
**Before:** Mixed controls for front and rear displays  
**After:** Clear sections for each display type

```
Front Logic Displays (FLDs) Section:
├── Sequence dropdown
├── Color dropdown
├── Speed slider
├── Duration slider
└── Text message input

Rear Logic Display (RLD) Section:
├── Sequence dropdown
├── Color dropdown  
├── Speed slider
├── Duration slider
└── Text message input

Apply Settings Button:
└── Applies all changes to both displays simultaneously
```

**Benefits:**
- Clear separation between FLD (two front displays) and RLD (single rear display)
- All settings grouped by display location
- Single Apply button reduces complexity

---

## 3. Credenda Fork Integration

### 3.1 All Holos On/Off Commands (MarcduinoHolo.h lines 3-13)

**Added Commands:**
- `*ON00` - Turn all three holo projectors ON (dim cycle random color)
- `*OF00` - Turn all three holo projectors OFF

**Implementation:**
```cpp
// CREDENDA FORK IMPROVEMENT: Added All Holos On/Off commands
// These commands control all three holo projectors (Front, Rear, Top) simultaneously
// *ON00 = All holos dim cycle random color
// *OF00 = All holos off

MARCDUINO_ACTION(AllHoloOn, *ON00, ({
    CommandEvent::process(F("HPA0040")); // All Holo Dim cycle random color
}))

MARCDUINO_ACTION(AllHoloOFF, *OF00, ({
    CommandEvent::process(F("HPA0000")); // All Holo off
}))
```

---

### 3.2 Enhanced Logic Sequences (MarcduinoLogics.h lines 108-160)

#### Failure Sequence (@0T4)
**Enhancement:** Added synchronized holo projector failure animation
```cpp
// CREDENDA FORK IMPROVEMENT: Enhanced Failure sequence
// Added holo projector failure animation (HPA007) synchronized with logic displays
// Auto-resets to normal after 11 seconds

MARCDUINO_ACTION(FailureSequence, @0T4, ({
    FLD.selectSequence(LogicEngineRenderer::FAILURE);
    RLD.selectSequence(LogicEngineRenderer::FAILURE);
    // Add All holo failure sequence
    DO_COMMAND_AND_WAIT(F("HPA007|11\n"), 11000)
    DO_RESET({
        resetSequence();
        // Reset to normal after failure
        FLD.selectSequence(LogicEngineRenderer::NORMAL);
        RLD.selectSequence(LogicEngineRenderer::NORMAL);
    })
}))
```

#### Scream/Red Alert Sequence (@0T5)
**Enhancement:** Added synchronized holo projector scream animation
```cpp
// CREDENDA FORK IMPROVEMENT: Enhanced Scream/Red Alert sequence
// Added holo projector scream animation (HPA0040) synchronized with logic displays
// Auto-resets to normal after 7 seconds

MARCDUINO_ACTION(ScreamLogicsSequence, @0T5, ({
    FLD.selectSequence(LogicEngineRenderer::REDALERT);
    RLD.selectSequence(LogicEngineRenderer::REDALERT);
    // Add All holo scream sequence
    DO_COMMAND_AND_WAIT(F("HPA0040|7\n"), 7000)
    DO_RESET({
        resetSequence();
        // Reset to normal after scream
        FLD.selectSequence(LogicEngineRenderer::NORMAL);
        RLD.selectSequence(LogicEngineRenderer::NORMAL);
    })
}))
```

#### Leia Sequence (@0T6)
**Enhancement:** Added synchronized front holo Leia message
```cpp
// CREDENDA FORK IMPROVEMENT: Enhanced Leia sequence
// Added front holo projector Leia message (HPS1) synchronized with logic displays
// Auto-resets after 45 seconds

MARCDUINO_ACTION(LeiaLogicsSequence, @0T6, ({
    FLD.selectSequence(LogicEngineRenderer::LEIA);
    RLD.selectSequence(LogicEngineRenderer::LEIA);
    // Add Front holo Leia sequence
    DO_COMMAND_AND_WAIT(F("HPS1|45\n"), 45000)
    DO_RESET({
        resetSequence();
    })
}))
```

---

### 3.3 Enhanced Startup Text (AstroPixelsPlus.ino line 674)

**Enhancement:** Displays iconic Star Wars text on boot
```cpp
// CREDENDA FORK IMPROVEMENT: Enhanced startup text display
// Shows "STAR WARS" on RLD and "R2D2" on FLD during boot
// Initialize LED effects before WiFi starts
RLD.selectScrollTextLeft("... STAR WARS ....", LogicEngineRenderer::kBlue, 0, 15);
FLD.selectScrollTextLeft("... R2D2 ...", LogicEngineRenderer::kRed, 0, 15);
```

**Effect:** 
- Rear Logic Display (RLD): Scrolls "... STAR WARS ...." in blue
- Front Logic Displays (FLD): Scrolls "... R2D2 ..." in red
- Executes before WiFi initialization for immediate visual feedback

---

## 4. Code Documentation Improvements

### 4.1 Comprehensive Comments Added

#### WebPages.h Documentation
- **Lines 1-47:** ESP32 static initialization memory limits
- **Lines 72-78:** Panels page organization by dome location
- **Lines 208-220:** Holo commands categorization (lights vs movements)
- **Lines 345-352:** Logic displays organization
- **Throughout:** Inline comments explaining command types and purposes

#### MarcduinoHolo.h Documentation
- **Lines 3-8:** Credenda fork All Holos On/Off feature
- **Lines 122-127:** Pulse commands clarified as LED effects (NOT movements)
- **Lines 143-148:** Rainbow commands clarified as LED effects (NOT movements)

#### MarcduinoLogics.h Documentation
- **Lines 108:** Failure sequence enhancement with holo animation
- **Lines 124:** Scream sequence enhancement with holo animation
- **Lines 140:** Leia sequence enhancement with holo message

#### AstroPixelsPlus.ino Documentation
- **Line 674:** Startup text enhancement from credenda fork

### 4.2 Troubleshooting Guide Added
```powershell
# Count WButtons (PowerShell):
(Get-Content WebPages.h | Select-String -Pattern '\bWButton\(' -AllMatches).Matches.Count

# Count String arrays (grep):
grep -c '^String .*\[\]' WebPages.h

# If crashes occur:
# 1. Count instances (commands above)
# 2. Compare to limits (50 WButtons, 6-7 String arrays)
# 3. Consolidate WButtons into WSelect dropdowns
# 4. Alternative: Use dynamic allocation pattern (see WebPages.h.backup)
```

---

## 5. File-by-File Change Summary

### WebPages.h (~634 lines changed)
- ✅ Added 47-line header documentation on ESP32 memory limits
- ✅ Added panels page with dropdown and organized by dome location
- ✅ Added holos page with separate light/movement dropdowns
- ✅ Reorganized logics page by display location
- ✅ Added inline comments throughout
- ✅ Removed 70+ WButton instances (97 → 27)

### MarcduinoHolo.h (~394 lines reformatted)
- ✅ Added All Holos On/Off commands (\*ON00, \*OF00)
- ✅ Added documentation for Pulse commands (LED effects)
- ✅ Added documentation for Rainbow commands (LED effects)
- ✅ Code reformatting for consistency

### MarcduinoLogics.h (~188 lines reformatted)
- ✅ Enhanced Failure sequence with holo animation + auto-reset
- ✅ Enhanced Scream sequence with holo animation + auto-reset
- ✅ Enhanced Leia sequence with holo message + auto-reset
- ✅ Code reformatting for consistency

### AstroPixelsPlus.ino (~114 lines changed)
- ✅ Added "STAR WARS" / "R2D2" startup text on logic displays
- ✅ Added documentation comment for startup enhancement

### Other Files
- **README.md:** Minor updates
- **effects/BitmapEffect.h:** Minor fixes
- **platformio.ini:** Configuration updates

---

## 6. Benefits & Impact

### Stability Improvements
- ✅ **Zero boot crashes** - Reduced from constant crashes to 100% stable boots
- ✅ **46% margin remaining** - 27/50 WButton limit (23 buttons of headroom)
- ✅ **Proven test results** - Documented safe configurations

### Usability Improvements
- ✅ **Clearer organization** - Panels by location, holos by function type
- ✅ **Reduced clutter** - Removed non-moving panels, redundant navigation
- ✅ **Better categorization** - Light effects vs movements clearly separated
- ✅ **Intuitive layout** - Physical dome locations mirror web interface

### Feature Additions
- ✅ **All Holos On/Off** - Control all three holos simultaneously
- ✅ **Enhanced sequences** - Failure, Scream, Leia with holo integration
- ✅ **Startup text** - "STAR WARS" branding on boot
- ✅ **Auto-reset** - Sequences automatically return to normal

### Maintainability Improvements
- ✅ **Comprehensive documentation** - Every major change explained
- ✅ **Troubleshooting guide** - Clear steps to debug memory issues
- ✅ **Test results** - Known good configurations documented
- ✅ **Inline comments** - Code intent clear for future developers

---

## 7. Future Recommendations

### Staying Within Limits
1. **Before adding features:** Count current WButtons and String arrays
2. **Prefer dropdowns:** Use WSelect instead of multiple WButtons
3. **Test incrementally:** Add features one at a time, test boot stability
4. **Monitor headroom:** Keep 10+ button margin for safety

### Alternative Approach (If Limits Exceeded)
If static initialization limits become constraining, consider the dynamic allocation pattern:
```cpp
// See WebPages.h.backup for full implementation
void initWebPages() {
    // Allocate arrays at runtime after heap is ready
    mainContentsArray = new WElement[20]{
        W1("Title"),
        WButton(...), // Unlimited buttons possible
        // ...
    };
}
```
**Tradeoff:** More complex initialization, but no static memory limits.

---

## 8. Version History

**December 2025** - Major refactoring
- ESP32 memory crisis resolved
- Web interface reorganized
- Credenda fork features integrated
- Comprehensive documentation added

---

## Conclusion

These improvements transform AstroPixelsPlus from a crash-prone prototype into a stable, well-organized, and user-friendly R2-D2 control system. The combination of memory optimization, interface reorganization, credenda fork integration, and thorough documentation provides a solid foundation for future development.

**Key Achievement:** Reduced WButton count by 72% (97 → 27) while **adding** features and improving usability.
