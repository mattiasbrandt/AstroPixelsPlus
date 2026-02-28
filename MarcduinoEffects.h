#ifndef MARCDUINO_EFFECTS_H
#define MARCDUINO_EFFECTS_H

/***********************************************************
 * MarcduinoEffects.h
 * MarcDuino interface for smoke, fire, CBI, and DataPanel gadgets
 * 
 * Command handlers for:
 * - BadMotivator (smoke effects)
 * - FireStrip (fire effects)  
 * - ChargeBayIndicator (CBI patterns)
 * - DataPanel (data display patterns)
 * 
 * Commands follow Marcduino protocol with # prefix
 * 
 * All gadget features are gated by build flags and must be explicitly
 * enabled in platformio.ini:
 *   -DAP_ENABLE_BADMOTIVATOR=1
 *   -DAP_ENABLE_FIRESTRIP=1
 *   -DAP_ENABLE_CBI=1
 *   -DAP_ENABLE_DATAPANEL=1
 ***********************************************************/

// Include this file only if at least one gadget feature is enabled
#if AP_ENABLE_BADMOTIVATOR || AP_ENABLE_FIRESTRIP || AP_ENABLE_CBI || AP_ENABLE_DATAPANEL

// External gadget objects (declared in AstroPixelsPlus.ino) - only available when build flags enabled
#if AP_ENABLE_BADMOTIVATOR
extern BadMotivator badMotivator;
#endif
#if AP_ENABLE_FIRESTRIP
extern FireStrip fireStrip;
#endif
#if AP_ENABLE_CBI
extern ChargeBayIndicator chargeBayIndicator;
#endif
#if AP_ENABLE_DATAPANEL
extern DataPanel dataPanel;
#endif

// Safety constants
#if AP_ENABLE_BADMOTIVATOR
static const uint16_t SMOKE_MAX_CONTINUOUS_MS = 30000;  // 30 seconds max continuous smoke
static const uint16_t SMOKE_COOLDOWN_MS = 5000;         // 5 seconds minimum cooldown
#endif
#if AP_ENABLE_FIRESTRIP
static const uint8_t FIRE_MAX_BRIGHTNESS = 200;         // Max brightness cap for fire effects
#endif

// Helper function to parse numeric parameters
static bool parseNumericParameter(const char *cmd, uint16_t &value, uint8_t maxDigits = 4)
{
    if (cmd == nullptr || cmd[0] == '\0') return false;
    
    value = 0;
    uint8_t digits = 0;
    
    while (cmd[digits] != '\0' && digits < maxDigits)
    {
        if (cmd[digits] < '0' || cmd[digits] > '9') return false;
        value = value * 10 + (cmd[digits] - '0');
        digits++;
    }
    
    return digits > 0;
}

// Helper function to parse two parameters separated by comma
static bool parseTwoParameters(const char *cmd, uint16_t &param1, uint16_t &param2)
{
    if (cmd == nullptr || cmd[0] == '\0') return false;
    
    // Find comma separator
    const char *comma = strchr(cmd, ',');
    if (comma == nullptr) return false;
    
    // Parse first parameter
    uint16_t temp1 = 0;
    uint8_t i = 0;
    while (&cmd[i] < comma)
    {
        if (cmd[i] < '0' || cmd[i] > '9') return false;
        temp1 = temp1 * 10 + (cmd[i] - '0');
        i++;
    }
    
    // Parse second parameter after comma
    const char *secondParam = comma + 1;
    uint16_t temp2 = 0;
    i = 0;
    while (secondParam[i] != '\0')
    {
        if (secondParam[i] < '0' || secondParam[i] > '9') return false;
        temp2 = temp2 * 10 + (secondParam[i] - '0');
        i++;
    }
    
    param1 = temp1;
    param2 = temp2;
    return i > 0;
}

/////////////////////////////
// BadMotivator (Smoke) Commands
/////////////////////////////
#if AP_ENABLE_BADMOTIVATOR

MARCDUINO_ACTION(BadMotivatorSmoke, #BMSMOKE, ({
    DEBUG_PRINTLN("[EFFECTS] BadMotivator smoke start");
    badMotivator.startSmoke();
}))

MARCDUINO_ACTION(BadMotivatorStop, #BMSTOP, ({
    DEBUG_PRINTLN("[EFFECTS] BadMotivator smoke stop");
    badMotivator.stopSmoke();
}))

MARCDUINO_ACTION(BadMotivatorPulse, #BMPULSE, ({
    const char *cmd = Marcduino::getCommand();
    uint16_t seconds = 0;
    if (parseNumericParameter(cmd, seconds) && seconds > 0 && seconds <= 30)
    {
        DEBUG_PRINT("[EFFECTS] BadMotivator pulse for ");
        DEBUG_PRINT(seconds);
        DEBUG_PRINTLN(" seconds");
        badMotivator.pulseSmoke(seconds * 1000); // Convert to milliseconds
    }
    else
    {
        DEBUG_PRINTLN("[EFFECTS] Invalid BMPULSE parameter (must be 1-30 seconds)");
    }
}))

MARCDUINO_ACTION(BadMotivatorAuto, #BMAUTO, ({
    const char *cmd = Marcduino::getCommand();
    uint16_t minSeconds = 0, maxSeconds = 0;
    if (parseTwoParameters(cmd, minSeconds, maxSeconds))
    {
        if (minSeconds >= 1 && minSeconds <= 60 && maxSeconds >= minSeconds && maxSeconds <= 60)
        {
            DEBUG_PRINT("[EFFECTS] BadMotivator auto mode: ");
            DEBUG_PRINT(minSeconds);
            DEBUG_PRINT("-");
            DEBUG_PRINT(maxSeconds);
            DEBUG_PRINTLN(" seconds");
            badMotivator.setAutoMode(minSeconds * 1000, maxSeconds * 1000);
        }
        else
        {
            DEBUG_PRINTLN("[EFFECTS] Invalid BMAUTO parameters (must be 1-60 seconds, min <= max)");
        }
    }
    else
    {
        DEBUG_PRINTLN("[EFFECTS] Invalid BMAUTO format (expected: #BMAUTO,<min>,<max>)");
    }
}))

#endif // AP_ENABLE_BADMOTIVATOR

/////////////////////////////
// FireStrip (Fire) Commands
/////////////////////////////
#if AP_ENABLE_FIRESTRIP

MARCDUINO_ACTION(FireSpark, #FIRESPARK, ({
    const char *cmd = Marcduino::getCommand();
    uint16_t milliseconds = 0;
    if (parseNumericParameter(cmd, milliseconds) && milliseconds > 0 && milliseconds <= 5000)
    {
        DEBUG_PRINT("[EFFECTS] Fire spark for ");
        DEBUG_PRINT(milliseconds);
        DEBUG_PRINTLN(" ms");
        fireStrip.spark(milliseconds);
    }
    else
    {
        DEBUG_PRINTLN("[EFFECTS] Invalid FIRESPARK parameter (must be 1-5000 ms)");
    }
}))

MARCDUINO_ACTION(FireBurn, #FIREBURN, ({
    const char *cmd = Marcduino::getCommand();
    uint16_t milliseconds = 0;
    if (parseNumericParameter(cmd, milliseconds) && milliseconds > 0 && milliseconds <= 10000)
    {
        DEBUG_PRINT("[EFFECTS] Fire burn for ");
        DEBUG_PRINT(milliseconds);
        DEBUG_PRINTLN(" ms");
        fireStrip.burn(milliseconds, FIRE_MAX_BRIGHTNESS);
    }
    else
    {
        DEBUG_PRINTLN("[EFFECTS] Invalid FIREBURN parameter (must be 1-10000 ms)");
    }
}))

MARCDUINO_ACTION(FireStop, #FIRESTOP, ({
    DEBUG_PRINTLN("[EFFECTS] Fire stop");
    fireStrip.stop();
}))

#endif // AP_ENABLE_FIRESTRIP

/////////////////////////////
// ChargeBayIndicator (CBI) Commands
/////////////////////////////
#if AP_ENABLE_CBI

MARCDUINO_ACTION(ChargeBayOn, #CBION, ({
    DEBUG_PRINTLN("[EFFECTS] Charge Bay Indicator on");
    chargeBayIndicator.enable();
}))

MARCDUINO_ACTION(ChargeBayOff, #CBIOFF, ({
    DEBUG_PRINTLN("[EFFECTS] Charge Bay Indicator off");
    chargeBayIndicator.disable();
}))

MARCDUINO_ACTION(ChargeBaySet, #CBISET, ({
    const char *cmd = Marcduino::getCommand();
    uint16_t pattern = 0;
    if (parseNumericParameter(cmd, pattern) && pattern <= 9)
    {
        DEBUG_PRINT("[EFFECTS] Charge Bay Indicator pattern: ");
        DEBUG_PRINTLN(pattern);
        chargeBayIndicator.setPattern(pattern);
    }
    else
    {
        DEBUG_PRINTLN("[EFFECTS] Invalid CBISET parameter (must be 0-9)");
    }
}))

#endif // AP_ENABLE_CBI

/////////////////////////////
// DataPanel Commands
/////////////////////////////
#if AP_ENABLE_DATAPANEL

MARCDUINO_ACTION(DataPanelOn, #DPON, ({
    DEBUG_PRINTLN("[EFFECTS] Data Panel on");
    dataPanel.enable();
}))

MARCDUINO_ACTION(DataPanelOff, #DPOFF, ({
    DEBUG_PRINTLN("[EFFECTS] Data Panel off");
    dataPanel.disable();
}))

MARCDUINO_ACTION(DataPanelSet, #DPSET, ({
    const char *cmd = Marcduino::getCommand();
    uint16_t pattern = 0;
    if (parseNumericParameter(cmd, pattern) && pattern <= 9)
    {
        DEBUG_PRINT("[EFFECTS] Data Panel pattern: ");
        DEBUG_PRINTLN(pattern);
        dataPanel.setPattern(pattern);
    }
    else
    {
        DEBUG_PRINTLN("[EFFECTS] Invalid DPSET parameter (must be 0-9)");
    }
}))

#endif // AP_ENABLE_DATAPANEL

#endif // AP_ENABLE_BADMOTIVATOR || AP_ENABLE_FIRESTRIP || AP_ENABLE_CBI || AP_ENABLE_DATAPANEL

#endif // MARCDUINO_EFFECTS_H
