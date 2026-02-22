#ifdef USE_WIFI_WEB
// ReelTwo WifiWebServer library: https://reeltwo.github.io/Reeltwo/html/
// Custom web server library for ESP32 with static page initialization
//
// CRITICAL: ESP32 Static Initialization Memory Limits
// ====================================================
// Root Cause: WButton constructor calls appendBodyf() → FormatString() → malloc()
//             String array initialization also allocates heap memory
//             ESP32 heap is NOT initialized during global static object construction
//             Exceeding ~12-15KB cumulative allocation causes boot crash:
//             assert failed: vApplicationGetIdleTaskMemory port_common.c:195
//
// PROVEN LIMITS (tested Dec 2025):
// - Maximum ~50 WButton instances in static arrays (each ≈200 bytes heap)
// - Maximum 6-7 String arrays (cumulative size matters, not count alone)
// - Combined limit: WButtons + String arrays must stay under ~12-15KB total heap
//
// Current Config (STABLE):
// - 44 WButton instances
// - 7 String arrays (panelSequences=21, holoEffects=25, logicsSeq=24,
//   logicsColors=10, swBaudRates, soundPlayer, soundSerial)
//
// SAFE PATTERNS:
// - DO use: W1, WLabel, WHR, WSlider, WTextField, WSelect, WHorizontalAlign, WVerticalAlign
// - DO use: const char* arrays (no heap allocation)
// - DO use: int/bool variables without = initialization (int var; NOT int var = 0;)
// - DO use: Unicode arrows (↖↑↗←●→↙↓↘) - these are NOT emojis
//
// UNSAFE PATTERNS (cause crashes):
// - DO NOT use: Too many WButton instances (heap via appendBodyf)
// - DO NOT use: Too many String arrays (heap allocation during brace init)
// - DO NOT use: WStyle (uses appendCSS → heap allocation)
// - DO NOT use: Emojis in strings (causes WiFi crashes)
// - DO NOT use: String var = "value"; in global scope (use String var; instead)
//
// If boot crashes after changes:
// 1. Count WButtons: (Get-Content WebPages.h | Select-String -Pattern '\bWButton\(' -AllMatches).Matches.Count
// 2. Count String arrays: grep -c '^String .*\[\]' WebPages.h
// 3. Remove WButtons or consolidate into WSelect dropdowns
// 4. Alternative: Use dynamic allocation (see WebPages.h.backup initWebPages() pattern)

#include "web-images.h"

////////////////////////////////
// List of available sequences by name and matching id
enum
{
    kMAX_FADE = 15,
    kMAX_DELAY = 500,
    kMIN_DELAY = 10,
    kMIN_BRI = 10,

    kMAX_ADJLOOP = 90000,
    kMIN_ADJLOOP = 500,
};

WMenuData mainMenu[] = {
    {"Logics", "/logics"},
    {"Panels", "/panels"},
    {"Holos", "/holos"},
    {"Setup", "/setup"}};

WMenuData setupMenu[] = {
    {"Home", "/"},
    {"Serial", "/serial"},
    {"Sound", "/sound"},
    {"WiFi", "/wifi"},
    {"Remote", "/remote"},
    {"Firmware", "/firmware"},
    {"Back", "/"}};

////////////////////////////////
// Dome Panels Control Page
// Organized by physical dome location for easier control:
// - Front Lower: P1, P2 (flanking front logic displays)
// - Side Lower: P3, P4 (left and right lower sides)
// - Upper/Rear: P7, P10 (upper sides and rear)
// Note: Non-moving panels (P5, P6, P8, P9, P13) removed to save memory

String panelSequences[] = {
    ":SE00 Stop",
    ":SE01 Scream",
    ":SE02 Wave",
    ":SE03 Smirk/Wave",
    ":SE04 Open/Close Wave",
    ":SE05 Beep Cantina",
    ":SE06 Short Circuit",
    ":SE07 Cantina Dance",
    ":SE08 Leia Message",
    ":SE09 Disco",
    ":SE10 Faint/Recover",
    ":SE13 Quiet Mode",
    ":SE14 Mid-Awake Mode",
    ":SE15 Full-Awake Mode",
    ":SE51 Scream (RC)",
    ":SE52 Wave (RC)",
    ":SE53 Smirk/Wave (RC)",
    ":SE54 Open/Wave (RC)",
    ":SE55 Marching Ants (RC)",
    ":SE56 Faint (RC)",
    ":SE57 Rhythmic (RC)"};

const char *panelSeqCommands[] = {
    ":SE00", ":SE01", ":SE02", ":SE03", ":SE04", ":SE05", ":SE06",
    ":SE07", ":SE08", ":SE09", ":SE10", ":SE13", ":SE14", ":SE15",
    ":SE51", ":SE52", ":SE53", ":SE54", ":SE55", ":SE56", ":SE57"};

int sPanelSeqSelected;

WElement panelsContents[] = {
    W1("Dome Panels Control"),

    WStyle(".panel_btn { width: 140px; margin: 2px; } .seq_btn { width: 140px; margin: 2px; } .section_label { font-weight: bold; margin-top: 15px; }"),

    // Predefined sequences dropdown - choreographed panel animations
    // :SE## commands trigger sequences that may involve timing/choreography
    WLabel("Predefined Sequences:", "seqlabel"),
    WSelect("Select Sequence:", "panelseq", panelSequences, SizeOfArray(panelSequences), []()
            { return sPanelSeqSelected; }, [](int val)
            { 
                sPanelSeqSelected = val;
                if (val >= 0 && val < SizeOfArray(panelSeqCommands))
                {
                    Marcduino::processCommand(player, panelSeqCommands[val]);
                } }),
    WVerticalAlign(), WHR(),

    // Front Lower Panels (P1-P2) - flanking front logic displays
    WLabel("Front Lower Panels:", "frontlowerlabel"), WButton("P1 Open", "p1o", []()
                                                              { Marcduino::processCommand(player, ":OP01"); }),
    WHorizontalAlign(), WButton("P1 Close", "p1c", []()
                                { Marcduino::processCommand(player, ":CL01"); }),
    WHorizontalAlign(), WButton("P2 Open", "p2o", []()
                                { Marcduino::processCommand(player, ":OP02"); }),
    WHorizontalAlign(), WButton("P2 Close", "p2c", []()
                                { Marcduino::processCommand(player, ":CL02"); }),
    WVerticalAlign(), WHR(),

    // Side Lower Panels (P3-P4) - left and right sides
    WLabel("Side Lower Panels:", "sidelowerlabel"), WButton("P3 Open", "p3o", []()
                                                            { Marcduino::processCommand(player, ":OP03"); }),
    WHorizontalAlign(), WButton("P3 Close", "p3c", []()
                                { Marcduino::processCommand(player, ":CL03"); }),
    WHorizontalAlign(), WButton("P4 Open", "p4o", []()
                                { Marcduino::processCommand(player, ":OP04"); }),
    WHorizontalAlign(), WButton("P4 Close", "p4c", []()
                                { Marcduino::processCommand(player, ":CL04"); }),
    WVerticalAlign(), WHR(),

    // Upper/Rear Panels (P7, P10) - sides and rear
    WLabel("Upper/Rear Panels:", "upperrearlabel"), WButton("P7 Open", "p7o", []()
                                                            { Marcduino::processCommand(player, ":OP09"); }),
    WHorizontalAlign(), WButton("P7 Close", "p7c", []()
                                { Marcduino::processCommand(player, ":CL09"); }),
    WHorizontalAlign(), WButton("P10 Open", "p10o", []()
                                { Marcduino::processCommand(player, ":OP12"); }),
    WHorizontalAlign(), WButton("P10 Close", "p10c", []()
                                { Marcduino::processCommand(player, ":CL12"); }),
    WVerticalAlign(), WHR(),

    // Quick actions - immediate panel commands (no choreography)
    // :OP/:CL commands open/close panels directly without sequences
    // *ST00 stops all servo movements immediately
    WLabel("Quick Actions:", "quicklabel"), WButton("Open All", "openall", []()
                                                    { Marcduino::processCommand(player, ":OP00"); }),
    WHorizontalAlign(), WButton("Close All", "closeall", []()
                                { Marcduino::processCommand(player, ":CL00"); }),
    WHorizontalAlign(), WButton("Stop", "panelstop", []()
                                { Marcduino::processCommand(player, "*ST00"); }),
    WVerticalAlign(), rseriesSVG};

////////////////////////////////
// Holo Projector Control Page

// IMPORTANT: Static initialization limitation discovered:
// WButton uses appendBodyf() internally which requires heap allocation via FormatString().
// During static global object construction (before setup() runs), the ESP32 heap is not
// fully initialized. Having too many WButton instances in static arrays (we crashed with 97)
// causes StoreProhibited exceptions during boot as each WButton constructor attempts memory
// allocation before the heap is ready.
//
// DETAILED TECHNICAL ANALYSIS:
// =============================
// Stack trace from crashes:
//   WButton::WButton() → appendBodyf(format, ...) → FormatString() → malloc()
//   String array[] = {...} → String copy constructors → heap allocation
//   Both happen during __static_initialization_and_destruction_0 (before main)
//   ESP32 FreeRTOS heap not ready → assert in vApplicationGetIdleTaskMemory
//
// Crash signatures:
//   - assert failed: esp_startup_start_app_common port_common.c:81 (res == pdTRUE)
//   - assert failed: vApplicationGetIdleTaskMemory port_common.c:195 (pxStackBufferTemp != NULL)
//   - StoreProhibited (0x1d) exception at FormatString.h:806
//
// Test Results (Dec 2025):
//   ✅ 44 WButtons + 6 String arrays = BOOTS
//   ✅ 52 WButtons + 6 String arrays = BOOTS
//   ❌ 65 WButtons + 6 String arrays = CRASH
//   ❌ 54 WButtons + 7 String arrays (large) = CRASH
//   ✅ 44 WButtons + 7 String arrays (smaller total) = BOOTS
//
// SOLUTIONS:
// 1. Keep WButton count under ~50 instances in static arrays
// 2. Limit String arrays to 6-7 total (watch cumulative size, not just count)
// 3. Convert WButtons to WSelect dropdowns (no heap allocation)
// 4. Use const char* arrays instead of String arrays when possible
// 5. Dynamic allocation pattern (allocate in setup() after heap ready):
//    static WElement *pageContents = nullptr;
//    void initWebPages() { pageContents = new WElement[100]{...}; }
//    Call from setup() - allows unlimited WButtons post-initialization
//
// Safe widgets for static init: W1, WLabel, WHR, WSlider, WTextField, WSelect,
//                               WHorizontalAlign, WVerticalAlign
// Unsafe (heap allocation): WButton, WStyle (uses appendBodyf/appendCSS internally)

////////////////////////////////
// Holo Projector Control Page
// Commands separated into two categories for clarity:
// 1. Light Effects - Control LEDs (On/Off, Pulse, Rainbow)
//    - *ON##/*OF## = Turn LED on/off
//    - *HP00-03 = Pulse effect (dim pulse random color)
//    - *HP04-07 = Rainbow effect (LED color cycling)
// 2. Movements - Control servos (Random positions, Nod animation)
//    - *RD## = Random servo positions
//    - *HN## = Nod servo animation
//    - *ST00 = Reset all to default

// Holo light effects dropdown (On/Off + LED effects like Pulse and Rainbow)
String holoLights[] = {
    "All On", "All Off",
    "Front On", "Front Off",
    "Rear On", "Rear Off",
    "Top On", "Top Off",
    "Pulse (All)", "Pulse (Front)", "Pulse (Rear)", "Pulse (Top)",
    "Rainbow (All)", "Rainbow (Front)", "Rainbow (Rear)", "Rainbow (Top)"};

const char *holoLightCommands[] = {
    "*ON00", "*OF00",
    "*ON01", "*OF01",
    "*ON02", "*OF02",
    "*ON03", "*OF03",
    "*HP00", "*HP01", "*HP02", "*HP03",
    "*HP04", "*HP05", "*HP06", "*HP07"};

int sHoloLightSelected;

// Holo movement sequences dropdown (Physical servo movements only)
String holoMovements[] = {
    "Random (All)", "Random (Front)", "Random (Rear)", "Random (Top)",
    "Nod (All)", "Nod (Front)", "Nod (Rear)", "Nod (Top)",
    "Reset All"};

const char *holoMovementCommands[] = {
    "*RD00", "*RD01", "*RD02", "*RD03",
    "*HN00", "*HN01", "*HN02", "*HN03",
    "*ST00"};

int sHoloMovementSelected;

WElement holosContents[] = {
    W1("Holo Projector Control"),

    // Light Effects dropdown - LED on/off and color effects (Pulse, Rainbow)
    // Note: Pulse and Rainbow affect LEDs only, they do NOT move the servos
    WLabel("Holo Light Effects:", "holo_light_label"),
    WSelect("Select:", "hololight", holoLights, SizeOfArray(holoLights), []()
            { return sHoloLightSelected; }, [](int val)
            { 
                sHoloLightSelected = val;
                if (val >= 0 && val < SizeOfArray(holoLightCommands))
                {
                    Marcduino::processCommand(player, holoLightCommands[val]);
                } }),
    WHR(),

    // Movement Sequences dropdown - Physical servo movements only
    // Random and Nod commands physically move the holo projector servos
    WLabel("Holo Movements:", "holo_movement_label"), WSelect("Select:", "holomovement", holoMovements, SizeOfArray(holoMovements), []()
                                                              { return sHoloMovementSelected; }, [](int val)
                                                              { 
                sHoloMovementSelected = val;
                if (val >= 0 && val < SizeOfArray(holoMovementCommands))
                {
                    Marcduino::processCommand(player, holoMovementCommands[val]);
                } }),
    WHR(),

    // Auto HP Twitch Control - Enable/disable automatic random holo movements
    // D198 = Disable auto twitch, D199 = Enable auto twitch
    WLabel("Auto HP Twitch:", "holo_twitch_label"), WButton("Disable", "holo_d198", []()
                                                            { Marcduino::processCommand(player, "D198"); }),
    WHorizontalAlign(), WButton("Enable", "holo_d199", []()
                                { Marcduino::processCommand(player, "D199"); }),
    WVerticalAlign(), rseriesSVG};

////////////////////////////////

WElement mainContents[] = {
    WVerticalMenu("menu", mainMenu, SizeOfArray(mainMenu)),
    rseriesSVG};

WElement setupContents[] = {
    WVerticalMenu("setup", setupMenu, SizeOfArray(setupMenu)),
    rseriesSVG};

String logicsSeq[] = {
#define LOGICENGINE_SEQ(nam, val) \
    BUILTIN_SEQ(nam, LogicEngineDefaults::val)
#define BUILTIN_SEQ(nam, val) \
    nam,

#include "logic-sequences.h"

#undef BUILTIN_SEQ
#undef LOGICENGINE_SEQ
};

unsigned logicsSeqNumber[] = {
#define LOGICENGINE_SEQ(nam, val) \
    BUILTIN_SEQ(nam, LogicEngineDefaults::val)
#define BUILTIN_SEQ(nam, val) \
    val,

#include "logic-sequences.h"

#undef BUILTIN_SEQ
#undef LOGICENGINE_SEQ
};

String logicsColors[] = {
    "Default",
    "Red",
    "Orange",
    "Yellow",
    "Green",
    "Cyan",
    "Blue",
    "Purple",
    "Magenta",
    "Pink"};

bool sFLDChanged = true;
bool sRLDChanged = true;

int sFLDSequence;
int sRLDSequence;

String sFLDText;
String sRLDText;
String sFLDDisplayText;
String sRLDDisplayText;

int sFLDColor = LogicEngineRenderer::kDefault;
int sRLDColor = LogicEngineRenderer::kDefault;

int sFLDSpeedScale;
int sRLDSpeedScale;

int sFLDNumSeconds;
int sRLDNumSeconds;

/////////////////////////////////////////////////////////////////////////
// Logic Displays Control Page
// Organized by display location for clarity:
// - Front Logic Displays (FLDs): Two front-facing displays
// - Rear Logic Display (RLD): Single rear display
// Each section has: Sequence, Color, Speed, Duration, Text Message
// All changes applied together via single "Apply Settings" button

WElement logicsContents[] = {
    W1("Logic Display Control"),

    // Front Logic Displays section
    WLabel("Front Logic Displays (FLDs):", "fld_section_label"),
    WSelect("Sequence:", "frontseq", logicsSeq, SizeOfArray(logicsSeq), []()
            { return sFLDSequence; }, [](int val)
            { sFLDSequence = val; sFLDChanged = true; }),
    WSelect("Color:", "frontcolor", logicsColors, SizeOfArray(logicsColors), []()
            { return sFLDColor; }, [](int val)
            { sFLDColor = val; sFLDChanged = true; }),
    WSlider("Speed:", "fldspeed", 0, 9, []() -> int
            { return sFLDSpeedScale; }, [](int val)
            { sFLDSpeedScale = val; sFLDChanged = true; }),
    WSlider("Duration (seconds):", "fldseconds", 0, 99, []() -> int
            { return sFLDNumSeconds; }, [](int val)
            { sFLDNumSeconds = val; sFLDChanged = true; }),
    WTextField("Text Message:", "fronttext", []() -> String
               { return sFLDText; }, [](String val)
               { sFLDText = val; sFLDChanged = true; }),
    WHR(),

    // Rear Logic Display section
    WLabel("Rear Logic Display (RLD):", "rld_section_label"), WSelect("Sequence:", "rearseq", logicsSeq, SizeOfArray(logicsSeq), []()
                                                                      { return sRLDSequence; }, [](int val)
                                                                      { sRLDSequence = val; sRLDChanged = true; }),
    WSelect("Color:", "rearcolor", logicsColors, SizeOfArray(logicsColors), []()
            { return sRLDColor; }, [](int val)
            { sRLDColor = val; sRLDChanged = true; }),
    WSlider("Speed:", "rldspeed", 0, 9, []() -> int
            { return sRLDSpeedScale; }, [](int val)
            { sRLDSpeedScale = val; sRLDChanged = true; }),
    WSlider("Duration (seconds):", "rldseconds", 0, 99, []() -> int
            { return sRLDNumSeconds; }, [](int val)
            { sRLDNumSeconds = val; sRLDChanged = true; }),
    WTextField("Text Message:", "reartext", []() -> String
               { return sRLDText; }, [](String val)
               { sRLDText = val; sRLDChanged = true; }),
    WHR(),

    // Apply button
    WButton("Apply Settings", "run", []()
            {
        if (sFLDChanged)
        {
            sFLDDisplayText = sFLDText;
            sFLDDisplayText.replace("\\n", "\n");
            FLD.selectSequence(logicsSeqNumber[sFLDSequence], (LogicEngineRenderer::ColorVal)sFLDColor, sFLDSpeedScale, sFLDNumSeconds);
            FLD.setTextMessage(sFLDDisplayText.c_str());
            sFLDChanged = false;
        }
        if (sRLDChanged)
        {
            sRLDDisplayText = sRLDText;
            sRLDDisplayText.replace("\\n", "\n");
            RLD.selectSequence(logicsSeqNumber[sRLDSequence], (LogicEngineRenderer::ColorVal)sRLDColor, sRLDSpeedScale, sRLDNumSeconds);
            RLD.setTextMessage(sRLDDisplayText.c_str());
            sRLDChanged = false;
        } }),
    rseriesSVG};

////////////////////////////////

String swBaudRates[] = {
    "2400",
    "9600",
};

int marcSerial1Baud;
int marcSerial2Baud;
bool marcSerialPass;
bool marcSerialEnabled;
bool marcWifiEnabled;
bool marcWifiSerialPass;

WElement serialContents[] = {
    WSelect("Serial2 Baud Rate", "serial2baud", swBaudRates, SizeOfArray(swBaudRates), []()
            { return (marcSerial2Baud = (preferences.getInt(PREFERENCE_MARCSERIAL2, MARC_SERIAL2_BAUD_RATE)) == 2400) ? 0 : 1; }, [](int val)
            { marcSerial2Baud = (val == 0) ? 2400 : 9600; }),
    WVerticalAlign(), WCheckbox("Serial pass-through to Serial2", "serialpass", []()
                                { return (marcSerialPass = (preferences.getBool(PREFERENCE_MARCSERIAL_PASS, MARC_SERIAL_PASS))); }, [](bool val)
                                { marcSerialPass = val; }),
    WVerticalAlign(), WCheckbox("JawaLite on Serial2", "enabled", []()
                                { return (marcSerialEnabled = (preferences.getBool(PREFERENCE_MARCSERIAL_ENABLED, MARC_SERIAL_ENABLED))); }, [](bool val)
                                { marcSerialEnabled = val; }),
    WVerticalAlign(), WCheckbox("JawaLite on Wifi (port 2000)", "wifienabled", []()
                                { return (marcWifiEnabled = (preferences.getBool(PREFERENCE_MARCWIFI_ENABLED, MARC_WIFI_ENABLED))); }, [](bool val)
                                { marcWifiEnabled = val; }),
    WVerticalAlign(), WCheckbox("JawaLite Wifi pass-through to Serial2", "wifipass", []()
                                { return (marcWifiSerialPass = (preferences.getBool(PREFERENCE_MARCWIFI_SERIAL_PASS, MARC_WIFI_SERIAL_PASS))); }, [](bool val)
                                { marcWifiSerialPass = val; }),
    WVerticalAlign(), WButton("Save", "save", []()
                              {
        preferences.putInt(PREFERENCE_MARCSERIAL1, marcSerial1Baud);
        preferences.putInt(PREFERENCE_MARCSERIAL2, marcSerial2Baud);
        preferences.putBool(PREFERENCE_MARCSERIAL_PASS, marcSerialPass);
        preferences.putBool(PREFERENCE_MARCSERIAL_ENABLED, marcSerialEnabled);
        preferences.putBool(PREFERENCE_MARCWIFI_ENABLED, marcWifiEnabled);
        preferences.putBool(PREFERENCE_MARCWIFI_SERIAL_PASS, marcWifiSerialPass); }),
    WVerticalAlign(), rseriesSVG};

////////////////////////////////

String soundPlayer[] = {
    "Disabled",
    "MP3 Trigger",
    "DFMiniPlayer",
    "HCR"};

String soundSerial[] = {
    "AUX4/AUX5",
    "Serial2"};

int marcSoundPlayer;
int marcSoundSerial;
int marcSoundVolume;
int marcSoundStartup;
bool marcSoundRandom;
int marcSoundRandomMin;
int marcSoundRandomMax;

WElement soundContents[] = {
    WSelect("Sound Player", "soundPlayer", soundPlayer, SizeOfArray(soundPlayer), []()
            { return (marcSoundPlayer = preferences.getInt(PREFERENCE_MARCSOUND, MARC_SOUND_PLAYER)); }, [](int val)
            { marcSoundPlayer = val; }),
    WVerticalAlign(), WSelect("Sound Serial", "soundSerial", soundSerial, SizeOfArray(soundSerial), []()
                              { return (marcSoundSerial = preferences.getInt(PREFERENCE_MARCSOUND_SERIAL, MARC_SOUND_SERIAL)); }, [](int val)
                              { marcSoundSerial = val; }),
    WVerticalAlign(), WSlider("Sound Volume", "soundVolume", 0, 1000, []()
                              { return (marcSoundVolume = preferences.getInt(PREFERENCE_MARCSOUND_VOLUME, MARC_SOUND_VOLUME)); }, [](int val)
                              {
            marcSoundVolume = val;
            sMarcSound.setVolume(marcSoundVolume / 1000.0); }),
    WVerticalAlign(), WTextFieldInteger("Sound Startup", "soundStartup", []() -> String
                                        { return String(marcSoundStartup = preferences.getInt(PREFERENCE_MARCSOUND_STARTUP, MARC_SOUND_STARTUP)); }, [](String val)
                                        { marcSoundStartup = val.toInt(); }),
    WVerticalAlign(), WCheckbox("Random Sound", "soundRandom", []()
                                { return (marcSoundRandom = (preferences.getBool(PREFERENCE_MARCSOUND_RANDOM, MARC_SOUND_RANDOM))); }, [](bool val)
                                { marcSoundRandom = val; }),
    WVerticalAlign(), WTextFieldInteger("Random Min Millis", "soundRandomMin", []() -> String
                                        { return String(marcSoundRandomMin = preferences.getInt(PREFERENCE_MARCSOUND_RANDOM_MIN, MARC_SOUND_RANDOM_MIN)); }, [](String val)
                                        { marcSoundRandomMin = val.toInt(); }),
    WVerticalAlign(), WTextFieldInteger("Random Max Millis", "soundRandomMax", []() -> String
                                        { return String(marcSoundRandomMax = preferences.getInt(PREFERENCE_MARCSOUND_RANDOM_MAX, MARC_SOUND_RANDOM_MAX)); }, [](String val)
                                        { marcSoundRandomMax = val.toInt(); }),
    WVerticalAlign(), WButton("Save", "save", []()
                              {
        preferences.putInt(PREFERENCE_MARCSOUND, marcSoundPlayer);
        preferences.putInt(PREFERENCE_MARCSOUND_SERIAL, marcSoundSerial);
        preferences.putInt(PREFERENCE_MARCSOUND_VOLUME, marcSoundVolume);
        preferences.putInt(PREFERENCE_MARCSOUND_STARTUP, marcSoundStartup);
        preferences.putBool(PREFERENCE_MARCSOUND_RANDOM, marcSoundRandom);
        preferences.putInt(PREFERENCE_MARCSOUND_RANDOM_MIN, marcSoundRandomMin);
        preferences.putInt(PREFERENCE_MARCSOUND_RANDOM_MAX, marcSoundRandomMax);
        if (marcSoundRandom)
        {
            sMarcSound.startRandom();
        }
        else
        {
            sMarcSound.stopRandom();
        }
        sMarcSound.setVolume(marcSoundVolume / 1000.0);
        // Flip values around if min is greater than max
        if (marcSoundRandomMin > marcSoundRandomMax)
        {
            int t = marcSoundRandomMin;
            marcSoundRandomMin = marcSoundRandomMax;
            marcSoundRandomMax = t;
        }
        sMarcSound.setRandomMin(marcSoundRandomMin);
        sMarcSound.setRandomMax(marcSoundRandomMax); }),
    WVerticalAlign(), rseriesSVG};

////////////////////////////////

String wifiSSID;
String wifiPass;
bool wifiAP;

WElement wifiContents[] = {
    W1("WiFi Setup"),
    WCheckbox("WiFi Enabled", "enabled", []()
              { return wifiEnabled; }, [](bool val)
              { wifiEnabled = val; }),
    WHR(), WCheckbox("Access Point", "apmode", []()
                     { return (wifiAP = preferences.getBool(PREFERENCE_WIFI_AP, WIFI_ACCESS_POINT)); }, [](bool val)
                     { wifiAP = val; }),
    WTextField("WiFi:", "wifi", []() -> String
               { return (wifiSSID = preferences.getString(PREFERENCE_WIFI_SSID, WIFI_AP_NAME)); }, [](String val)
               { wifiSSID = val; }),
    WPassword("Password:", "password", []() -> String
              { return (wifiPass = preferences.getString(PREFERENCE_WIFI_PASS, WIFI_AP_PASSPHRASE)); }, [](String val)
              { wifiPass = val; }),
    WLabel("WiFi Disables Droid Remote", "label2"), WHR(), WButton("Save", "save", []()
                                                                   {
        DEBUG_PRINTLN("WiFi Changed");
        preferences.putBool(PREFERENCE_REMOTE_ENABLED, remoteEnabled);
        preferences.putBool(PREFERENCE_WIFI_ENABLED, wifiEnabled);
        preferences.putBool(PREFERENCE_WIFI_AP, wifiAP);
        preferences.putString(PREFERENCE_WIFI_SSID, wifiSSID);
        preferences.putString(PREFERENCE_WIFI_PASS, wifiPass);
        reboot(); }),
    WHorizontalAlign(), WButton("Home", "home", "/"), WVerticalAlign(), rseriesSVG};

////////////////////////////////

String remoteHostName;
String remoteSecret;

WElement remoteContents[] = {
    W1("Droid Remote Setup"),
    WCheckbox("Droid Remote Enabled", "remoteenabled", []()
              { return remoteEnabled; }, [](bool val)
              { remoteEnabled = val; }),
    WHR(), WTextField("Device Name:", "hostname", []() -> String
                      { return (remoteHostName = preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME)); }, [](String val)
                      { remoteHostName = val; }),
    WPassword("Secret:", "secret", []() -> String
              { return (remoteSecret = preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET)); }, [](String val)
              { remoteSecret = val; }),
    WButton("Save", "save", []()
            {
        DEBUG_PRINTLN("Remote Changed");
        preferences.putBool(PREFERENCE_REMOTE_ENABLED, remoteEnabled);
        preferences.putString(PREFERENCE_REMOTE_HOSTNAME, remoteHostName);
        preferences.putString(PREFERENCE_REMOTE_SECRET, remoteSecret);
        reboot(); }),
    WHorizontalAlign(), WButton("Home", "home", "/"), WVerticalAlign(), rseriesSVG};

////////////////////////////////

WElement firmwareContents[] = {
    W1("Firmware Setup"),
    WFirmwareFile("Firmware:", "firmware"),
    WFirmwareUpload("Reflash", "firmware"),
    WLabel("Current Firmware Build Date:", "label"),
    WLabel(__DATE__, "date"),
#ifdef BUILD_VERSION
    WHRef(BUILD_VERSION, "Sources"),
#endif
    WButton("Clear Prefs", "clear", []()
            {
        DEBUG_PRINTLN("Clear all preference settings");
        preferences.clear();
        reboot(); }),
    WHorizontalAlign(),
    WButton("Reboot", "reboot", []()
            { reboot(); }),
    WVerticalAlign(),
    rseriesSVG};

//////////////////////////////////////////////////////////////////

WPage pages[] = {
    WPage("/", mainContents, SizeOfArray(mainContents)),
    WPage("/logics", logicsContents, SizeOfArray(logicsContents)),
    WPage("/panels", panelsContents, SizeOfArray(panelsContents)),
    WPage("/holos", holosContents, SizeOfArray(holosContents)),
    WPage("/setup", setupContents, SizeOfArray(setupContents)),
    WPage("/serial", serialContents, SizeOfArray(serialContents)),
    WPage("/sound", soundContents, SizeOfArray(soundContents)),
    WPage("/wifi", wifiContents, SizeOfArray(wifiContents)),
    WPage("/remote", remoteContents, SizeOfArray(remoteContents)),
    WPage("/firmware", firmwareContents, SizeOfArray(firmwareContents)),
    WUpload("/upload/firmware", [](Client &client)
            {
                if (Update.hasError())
                    client.println("HTTP/1.0 200 FAIL");
                else
                    client.println("HTTP/1.0 200 OK");
                client.println("Content-type:text/html");
                client.println("Vary: Accept-Encoding");
                client.println();
                client.println();
                client.stop();
                if (!Update.hasError())
                {
                    delay(1000);
                    preferences.end();
                    ESP.restart();
                }
                FLD.selectSequence(LogicEngineDefaults::FAILURE);
                FLD.setTextMessage("Flash Fail");
                FLD.selectSequence(LogicEngineDefaults::TEXTSCROLLLEFT, LogicEngineRenderer::kRed, 1, 0);
                FLD.setEffectWidthRange(1.0);
                FLD.setEffectWidthRange(1.0);
                otaInProgress = false; }, [](WUploader &upload)
            {
                if (upload.status == UPLOAD_FILE_START)
                {
                    otaInProgress = true;
                    unmountFileSystems();
                    FLD.selectSequence(LogicEngineDefaults::NORMAL);
                    RLD.selectSequence(LogicEngineDefaults::NORMAL);
                    FLD.setEffectWidthRange(0);
                    RLD.setEffectWidthRange(0);
                    Serial.printf("Update: %s\n", upload.filename.c_str());
                    if (!Update.begin(upload.fileSize))
                    {
                        //start with max available size
                        Update.printError(Serial);
                    }
                }
                else if (upload.status == UPLOAD_FILE_WRITE)
                {
                    float range = (float)upload.receivedSize / (float)upload.fileSize;
                    DEBUG_PRINTLN("Received: "+String(range*100)+"%");
                   /* flashing firmware to ESP*/
                    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                    {
                        Update.printError(Serial);
                    }
                    FLD.setEffectWidthRange(range);
                    RLD.setEffectWidthRange(range);
                }
                else if (upload.status == UPLOAD_FILE_END)
                {
                    DEBUG_PRINTLN("GAME OVER");
                    if (Update.end(true))
                    {
                        //true to set the size to the current progress
                        Serial.printf("Update Success: %u\nRebooting...\n", upload.receivedSize);
                    }
                    else
                    {
                        Update.printError(Serial);
                    }
                } })};

WifiWebServer<10, SizeOfArray(pages)> webServer(pages, wifiAccess);
#endif
