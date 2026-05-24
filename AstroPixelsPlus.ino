/**
 *
 * AstroPixelsPlus sketch operates as a I2C master that can optionally be connected to one or more
 * Adafruit PCA9685 servo controllers to control dome panels. The sketch also provides serial commands
 * on Serial2.
 *
 */

// Support for RSeries Logic Engine FLD and/or RLD lights (requires FastLED library)
// Define USE_RSERIES_FLD to enable support for RSeries FLD
// #define USE_RSERIES_FLD
// Define USE_RSERIES_RLD to enable support for RSeries RLD
// #define USE_RSERIES_RLD
// Define USE_RSERIES_RLD_CURVED to enable support for RSeries RLD curved (AUX5 becomes clock pin)
// #define USE_RSERIES_RLD_CURVED

#if defined(USE_RSERIES_FLD) || defined(USE_RSERIES_RLD) || defined(USE_RSERIES_RLD_CURVED)
// RSeries logics require FastLED
#define USE_LEDLIB 0
#endif

// Prevent conflicts with FastLED's RMT driver on ESP32 (also set in platformio.ini)
#ifndef ESP32_ARDUINO_NO_RGB_BUILTIN
#define ESP32_ARDUINO_NO_RGB_BUILTIN
#endif

// Define USE_I2C_ADDRESS to enable slave mode. This will disable servo support
// #define USE_I2C_ADDRESS 0x0a
#define USE_DEBUG // Define to enable debug diagnostic
#define USE_WIFI  // Define to enable Wifi support
#define USE_SPIFFS

#ifndef AP_ENABLE_DROID_REMOTE
#define AP_ENABLE_DROID_REMOTE 0
#endif

#ifndef AP_DROID_NAME
#define AP_DROID_NAME "AstroPixels"
#endif


#ifndef AP_ENABLE_BADMOTIVATOR
#define AP_ENABLE_BADMOTIVATOR 0
#endif

#ifndef AP_ENABLE_FIRESTRIP
#define AP_ENABLE_FIRESTRIP 0
#endif

#ifndef AP_ENABLE_CBI
#define AP_ENABLE_CBI 0
#endif

#ifndef AP_ENABLE_DATAPANEL
#define AP_ENABLE_DATAPANEL 0
#endif
#ifdef USE_WIFI
#if AP_ENABLE_DROID_REMOTE
#define USE_DROID_REMOTE // Define for droid remote support
#endif
#define USE_MDNS
#define USE_OTA
#define USE_WIFI_WEB
#define USE_WIFI_MARCDUINO
// #define LIVE_STREAM
#endif

////////////////////////////////

// Replace with your network credentials
#ifdef USE_WIFI
#define REMOTE_ENABLED true // default disabled
#define WIFI_ENABLED true   // default enabled
// Set these to your desired WiFi credentials.
#define WIFI_AP_NAME "AstroPixels"
#define WIFI_AP_PASSPHRASE "Astromech"
#define WIFI_ACCESS_POINT true /* true if access point: false if joining existing wifi */
#endif

// SMQ device name for ESPNOW
#define SMQ_HOSTNAME "Astro"
#define SMQ_SECRET "Astromech"

///////////////////////////////////

#if __has_include("build_version.h")
#include "build_version.h"
#endif

#if __has_include("reeltwo_build_version.h")
#include "reeltwo_build_version.h"
#endif

////////////////////////////////

#define PREFERENCE_REMOTE_ENABLED "remote"
#define PREFERENCE_REMOTE_HOSTNAME "rhost"
#define PREFERENCE_REMOTE_SECRET "rsecret"
#define PREFERENCE_REMOTE_PAIRED "rpaired"
#define PREFERENCE_REMOTE_LMK "rlmk"

#define PREFERENCE_WIFI_ENABLED "wifi"
#define PREFERENCE_WIFI_SSID "ssid"
#define PREFERENCE_WIFI_PASS "pass"
#define PREFERENCE_WIFI_AP "ap"

#define PREFERENCE_MARCSERIAL1 "mserial1"
#define PREFERENCE_MARCSERIAL2 "mserial2"
#define PREFERENCE_MARCSERIAL_PASS "mserialpass"
#define PREFERENCE_MARCSERIAL_ENABLED "mserial"

#define PREFERENCE_MARCWIFI_ENABLED "mwifi"
#define PREFERENCE_MARCWIFI_SERIAL_PASS "mwifipass"
#define PREFERENCE_BODY_LINK_ENABLED  "mbodylink"
#define PREFERENCE_BODY_WIFI_ENABLED  "mbodywifi"
#define PREFERENCE_BODY_PEER_IP      "bodypeerip"
#define BODY_LINK_ENABLED             true   // on by default in this fork
#define BODY_WIFI_ENABLED             true   // WiFi fallback enabled by default

// Dynamic wiring config — slot counts and offsets for servoSettings[].
// NUM_PANEL_SLOTS + NUM_HOLO_SLOTS must equal SizeOfArray(servoSettings); a
// static_assert below the array declaration enforces that. HOLO_SLOT_OFFSET is
// the servoSettings[] index of the first holo slot, used by holoConfigLoad()
// to convert between slot index and NVS key index (slotIdx = i - HOLO_SLOT_OFFSET).
#define NUM_PANEL_SLOTS   13
#define NUM_HOLO_SLOTS     6
#define HOLO_SLOT_OFFSET  13

// NVS namespaces and key prefixes for the dynamic wiring config. Keys are
// kept short (<= 15 chars) to satisfy ESP32 NVS's key-length limit.
#define PREFERENCE_PANELS_NS       "panels"
#define PREFERENCE_PANELS_CH_FMT   "pc_ch%d"   // pc_ch0..pc_ch12 — physical silkscreen channel
#define PREFERENCE_PANELS_ACT_FMT  "pc_act%d"  // pc_act0..pc_act12 — bool active
#define PREFERENCE_HOLOS_NS        "holos"
#define PREFERENCE_HOLOS_CH_FMT    "hc_ch%d"   // hc_ch0..hc_ch5
#define PREFERENCE_HOLOS_ACT_FMT   "hc_act%d"  // hc_act0..hc_act5


#define PREFERENCE_MARCSOUND "msound"
#define PREFERENCE_MARCSOUND_SERIAL "msoundser"
#define PREFERENCE_MARCSOUND_VOLUME "mvolume"
#define PREFERENCE_MARCSOUND_STARTUP "msoundstart"
#define PREFERENCE_MARCSOUND_RANDOM "mrandom"
#define PREFERENCE_MARCSOUND_RANDOM_MIN "mrandommin"
#define PREFERENCE_MARCSOUND_RANDOM_MAX "mrandommax"
#define PREFERENCE_MARCSOUND_LOCAL_ENABLED "msoundlocal"
#define PREFERENCE_DROID_NAME "dname"
#define PREFERENCE_DROID_NAME "dname"

#define PREFERENCE_BADMOTIVATOR_ENABLED "badmot"
#define PREFERENCE_FIRESTRIP_ENABLED "firest"
#define PREFERENCE_CBI_ENABLED "cbienb"
#define PREFERENCE_DATAPANEL_ENABLED "dpenab"
////////////////////////////////

#define CONSOLE_BUFFER_SIZE 300

////////////////////////////////

#if defined(USE_LCD_SCREEN) || defined(USE_DROID_REMOTE)
#define USE_MENUS // Define if using menu system
#endif

////////////////////////////////

#ifdef USE_DROID_REMOTE
#include "ReelTwoSMQ32.h"
#else
#include "ReelTwo.h"
#endif
#include "dome/Logics.h"
#include "dome/LogicEngineController.h"
#include "dome/HoloLights.h"
#include "dome/NeoPSI.h"
#include "dome/FireStrip.h"
#include "dome/BadMotivator.h"
#include "dome/TeecesPSI.h"
#include "dome/TeecesLogics.h"
#include "body/DataPanel.h"
#include "body/ChargeBayIndicator.h"

#ifdef USE_I2C_ADDRESS
#include "i2c/I2CReceiver.h"
#include "ServoDispatchDirect.h"
#else
#include "ServoDispatchPCA9685.h"
#endif
#include "ServoSequencer.h"
#include "core/Marcduino.h"

#include <Preferences.h>

////////////////////////////////

#define SERIAL2_RX_PIN 16
#define SERIAL2_TX_PIN 17
#define COMMAND_SERIAL Serial2

////////////////////////////////

#define MARC_SERIAL2_BAUD_RATE 2400
#define MARC_SERIAL_PASS true
#define MARC_SERIAL_ENABLED true
#define MARC_WIFI_ENABLED true
#define MARC_WIFI_SERIAL_PASS true
#define MARC_SOUND_PLAYER MarcSound::kDisabled
#define MARC_SOUND_SERIAL 0
#define MARC_SOUND_VOLUME 500 // 0 - 1000
#define MARC_SOUND_STARTUP 255
#define MARC_SOUND_RANDOM true
#define MARC_SOUND_RANDOM_MIN 5000
#define MARC_SOUND_RANDOM_MAX 30000
#define MARC_SOUND_LOCAL_ENABLED true

#include "wifi/WifiAccess.h"

////////////////////////////////

#ifdef USE_MDNS
#include <ESPmDNS.h>
#endif
#ifdef USE_WIFI_WEB
#include <ESPAsyncWebServer.h>
#endif
#ifdef USE_WIFI_MARCDUINO
#include "wifi/WifiMarcduinoReceiver.h"
#endif
#ifdef USE_OTA
#include <ArduinoOTA.h>
#endif
#ifdef USE_SPIFFS
#include "SPIFFS.h"
#elif defined(USE_FATFS)
#include "FFat.h"
#elif defined(USE_LITTLEFS)
#include "LITTLEFS.h"
#endif
#include "FS.h"

////////////////////////////////

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_FRONT_LOGIC 15
#define PIN_REAR_LOGIC 33
#define PIN_FRONT_PSI 32
#define PIN_REAR_PSI 23
#define PIN_FRONT_HOLO 25
#define PIN_REAR_HOLO 26
#define PIN_TOP_HOLO 27
#ifndef PIN_AUX1
#define PIN_AUX1 2
#endif
#ifndef PIN_AUX2
#define PIN_AUX2 4
#endif
#ifndef PIN_AUX3
#define PIN_AUX3 5
#endif
#ifndef PIN_AUX4
#define PIN_AUX4 18
#endif
#ifndef PIN_AUX5
#define PIN_AUX5 19
#endif

#ifdef USE_RSERIES_RLD_CURVED
// Define RSeries RLD clock pin to be AUX5 (could just as well be AUX1, AUX2, AUX3, or AUX4)
#define PIN_REAR_LOGIC_CLOCK PIN_AUX5
#endif

#ifndef CBI_DATAIN_PIN
#define CBI_DATAIN_PIN PIN_AUX3
#endif
#ifndef CBI_CLOCK_PIN
#define CBI_CLOCK_PIN PIN_AUX2
#endif
#ifndef CBI_LOAD_PIN
#define CBI_LOAD_PIN PIN_AUX1
#endif

////////////////////////////////

#define SOUND_SERIAL Serial1
#define SOUND_RX_PIN PIN_AUX4
#define SOUND_TX_PIN PIN_AUX5
#define SOUND_BAUD 9600

////////////////////////////////

#if defined(USE_RSERIES_RLD_CURVED)
LogicEngineCurvedRLD<PIN_REAR_LOGIC, PIN_REAR_LOGIC_CLOCK> RLD(LogicEngineRLDDefault, 3);
#elif defined(USE_RSERIES_RLD)
LogicEngineDeathStarRLD<PIN_REAR_LOGIC> RLD(LogicEngineRLDDefault, 3);
#else
AstroPixelRLD<PIN_REAR_LOGIC> RLD(LogicEngineRLDDefault, 3);
#endif

#ifdef USE_RSERIES_FLD
LogicEngineDeathStarFLD<PIN_FRONT_LOGIC> FLD(LogicEngineFLDDefault, 1);
#else
AstroPixelFLD<PIN_FRONT_LOGIC> FLD(LogicEngineFLDDefault, 1);
#endif

AstroPixelFrontPSI<PIN_FRONT_PSI> frontPSI(LogicEngineFrontPSIDefault, 4);
AstroPixelRearPSI<PIN_REAR_PSI> rearPSI(LogicEngineRearPSIDefault, 5);

#if USE_HOLO_TEMPLATE
HoloLights<PIN_FRONT_HOLO, NEO_GRB> frontHolo(1);
HoloLights<PIN_REAR_HOLO, NEO_GRB> rearHolo(2);
HoloLights<PIN_TOP_HOLO, NEO_GRB> topHolo(3);
#else
HoloLights frontHolo(PIN_FRONT_HOLO, HoloLights::kRGB, 1);
HoloLights rearHolo(PIN_REAR_HOLO, HoloLights::kRGB, 2);
HoloLights topHolo(PIN_TOP_HOLO, HoloLights::kRGB, 3);
#endif

#if AP_ENABLE_FIRESTRIP
#if USE_FIRESTRIP_TEMPLATE
 FireStrip<PIN_AUX4> fireStrip;
#else
 FireStrip fireStrip(PIN_AUX4);
#endif
#endif

#if AP_ENABLE_BADMOTIVATOR
 BadMotivator badMotivator(PIN_AUX5);
#endif
#if AP_ENABLE_CBI || AP_ENABLE_DATAPANEL
LedControlMAX7221<5> ledChain1(CBI_DATAIN_PIN, CBI_CLOCK_PIN, CBI_LOAD_PIN);
#if AP_ENABLE_CBI
ChargeBayIndicator chargeBayIndicator(ledChain1);
#endif
#if AP_ENABLE_DATAPANEL
DataPanel dataPanel(ledChain1);
#endif
#endif






////////////////////////////////

#define SMALL_PANEL 0x0001
#define MEDIUM_PANEL 0x0002
#define BIG_PANEL 0x0004
#define PIE_PANEL 0x0008
#define TOP_PIE_PANEL 0x0010
#define MINI_PANEL 0x0020

#define HOLO_HSERVO 0x1000
#define HOLO_VSERVO 0x2000

#define DOME_PANELS_MASK (SMALL_PANEL | MEDIUM_PANEL | BIG_PANEL)
#define PIE_PANELS_MASK (PIE_PANEL)
#define ALL_DOME_PANELS_MASK (MINI_PANEL | DOME_PANELS_MASK | PIE_PANELS_MASK | TOP_PIE_PANEL)
#define DOME_DANCE_PANELS_MASK (DOME_PANELS_MASK | PIE_PANELS_MASK)
#define HOLO_SERVOS_MASK (HOLO_HSERVO | HOLO_VSERVO)

#define PANEL_GROUP_1 (1L << 14)
#define PANEL_GROUP_2 (1L << 15)
#define PANEL_GROUP_3 (1L << 16)
#define PANEL_GROUP_4 (1L << 17)
#define PANEL_GROUP_5 (1L << 18)
#define PANEL_GROUP_6 (1L << 19)
#define PANEL_GROUP_7 (1L << 20)
#define PANEL_GROUP_8 (1L << 21)
#define PANEL_GROUP_9 (1L << 22)
#define PANEL_GROUP_10 (1L << 23)

////////////////////////////////
// Servo panel mapping for Mr. Baddeley MK4 complex dome (printed-droid labels).
// Ring panels with servos:    P1, P2, P3, P4, P7, P11, P13
// Pie panels with servos:     PP1, PP2, PP4, PP6
// Unserviced panels (slots exist, no servo wired on MK4): PP5 (slot 7), PP3 (slot 12).
// Fixed (no slot, no servo):  P5 (Magic Panel/frame), P6, P8 (Rear PSI), P9 (Rear
//                             Logic), P10, P12 (Front Logic), P14 (Front PSI).
//
// Channel addressing — IMPORTANT:
//   Each entry's first field is the ReelTwo "firmware pin", a 1-indexed value that
//   ServoDispatchPCA9685 converts to (board, silkscreen-channel) at runtime via:
//       pin = (n × 16) + physCh + 1
//   where n = 0 for the 0x40 panel board, n = 1 for the 0x41 holo board, and physCh
//   is the silkscreen number (0–15) printed on the PCA9685. So firmware pin 1 drives
//   silkscreen CH0, pin 2 drives CH1, pin 17 drives 0x41 CH0, pin 22 drives 0x41 CH5.
//   Pin 16 still addresses 0x40 CH15 — the holo board does NOT begin at pin 16.
//   (See docs/adr/0004-holo-servosettings-starts-at-pin-17.md.)
//
//   PROGMEM safety: a pin value of 0 underflows fLastLength[channel-1] in the
//   ServoDispatchPCA9685 constructor. Inactive slots must therefore carry a
//   non-zero PROGMEM pin and be zeroed post-construction via setServo(i, 0, …, 0)
//   in panelConfigLoad()/holoConfigLoad().
//
// PCA9685 @ 0x40 silkscreen channel → printed-droid panel → Marcduino command:
//   CH 0-3  = P1-P4    :OP01-:OP04   (4 small ring panels)
//   CH 4    = P7       :OP05          (small upper ring panel)
//   CH 5    = P11      :OP06          (lower-left ring panel)
//   CH 6    = P13      :OP07          (lower-front ring panel, near FLD)
//   CH 7    = unused   —              (PP5 slot exists in firmware but no servo on MK4)
//   CH 8-10 = PP1/PP2/PP4  :OP08-:OP10  (3 individually-addressed pie panels)
//   CH 11   = PP6      :OP11 group only  (4th pie panel, opened with all-top cmd)
//   CH 12   = unused   —              (PP3 slot exists in firmware but no servo on MK4)
//
// Group commands (no dedicated channel):
//   :OP11 = all pie panels (PP1+PP2+PP4+PP6)
//   :OP12 = all ring panels (P1-P4, P7, P11, P13)
//   :OP00 = all panels
//
// See docs/HARDWARE_WIRING.md for the full PCA9685 wiring table.
// Pulse widths (800-2200 µs) are defaults; per-panel calibration stored in NVS via web UI.
// Channel assignments (firmware pins) are MK4 defaults; per-builder overrides live in
// NVS namespaces "panels" / "holos" and are applied by panelConfigLoad() and
// holoConfigLoad() before SetupEvent::ready().
const ServoSettings servoSettings[] PROGMEM = {
#ifndef USE_I2C_ADDRESS
    // Panel Controller (PCA9685 @ 0x40) — slots 0–12; firmware pin = silkscreen + 1.
    // The Mr Baddeley MK4 Complex Dome IS a standard for WHICH panels exist and which
    // have servo mounts (slots 0–6 ring, slots 8–11 pie, slots 7+12 unserviced
    // by default). The CHANNEL-TO-SLOT mapping below is NOT a standard —
    // builders wire each servo to whichever PCA9685 silkscreen channel is
    // physically convenient. The defaults pack panels compactly from CH0 as a
    // sensible starting point; builders whose wiring differs override per-slot
    // via the Wiring Config UI (panels.html), saved to NVS namespace "panels",
    // applied at boot by panelConfigLoad().
    {1,  800, 2200, PANEL_GROUP_1  | SMALL_PANEL}, /* slot 0:  P1  (ring)       CH0   :OP01 */
    {2,  800, 2200, PANEL_GROUP_2  | SMALL_PANEL}, /* slot 1:  P2  (ring)       CH1   :OP02 */
    {3,  800, 2200, PANEL_GROUP_3  | SMALL_PANEL}, /* slot 2:  P3  (ring)       CH2   :OP03 */
    {4,  800, 2200, PANEL_GROUP_4  | SMALL_PANEL}, /* slot 3:  P4  (ring)       CH3   :OP04 */
    {5,  800, 2200, PANEL_GROUP_5  | SMALL_PANEL}, /* slot 4:  P7  (ring upper) CH4   :OP05 */
    {6,  800, 2200, PANEL_GROUP_6  | SMALL_PANEL}, /* slot 5:  P11 (ring lower) CH5   :OP06 */
    {7,  800, 2200, PANEL_GROUP_7  | SMALL_PANEL}, /* slot 6:  P13 (ring front) CH6   :OP07 */
    {8,  800, 2200, MINI_PANEL},                   /* slot 7:  PP5 (unserviced) CH7   —     panelConfigLoad() zeroes this slot at boot; PROGMEM pin is non-zero only to satisfy the constructor's fLastLength[pin-1] write. */
    {9,  800, 2200, PANEL_GROUP_8  | PIE_PANEL},   /* slot 8:  PP1 (pie)        CH8   :OP08 */
    {10, 800, 2200, PANEL_GROUP_9  | PIE_PANEL},   /* slot 9:  PP2 (pie)        CH9   :OP09 */
    {11, 800, 2200, PANEL_GROUP_10 | PIE_PANEL},   /* slot 10: PP4 (pie)        CH10  :OP10 */
    {12, 800, 2200, PIE_PANEL},                    /* slot 11: PP6 (pie)        CH11  :OP11 group only */
    {13, 800, 2200, TOP_PIE_PANEL},                /* slot 12: PP3 (unserviced) CH12  —     panelConfigLoad() zeroes this slot at boot; PROGMEM pin is non-zero only to satisfy the constructor's fLastLength[pin-1] write. */

    // Holo Controller (PCA9685 @ 0x41, A0 bridged) — slots 13–18; firmware pin = 16 + silkscreen + 1.
    // Pin 16 still addresses 0x40 CH15 — see ADR 0004. Holo board begins at pin 17.
    {17, 800, 2200, HOLO_HSERVO}, /* slot 13: FHP — front holo horizontal CH0  */
    {18, 800, 2200, HOLO_VSERVO}, /* slot 14: FHP — front holo vertical   CH1  */
    {19, 800, 2200, HOLO_HSERVO}, /* slot 15: THP — top holo horizontal   CH2  */
    {20, 800, 2200, HOLO_VSERVO}, /* slot 16: THP — top holo vertical     CH3  */
    {21, 800, 2200, HOLO_VSERVO}, /* slot 17: RHP — rear holo vertical    CH4  */
    {22, 800, 2200, HOLO_HSERVO}, /* slot 18: RHP — rear holo horizontal  CH5  */
#endif
};

#ifndef USE_I2C_ADDRESS
// Compile-time guard: if anyone adds or removes a slot in servoSettings[] without
// updating NUM_PANEL_SLOTS / NUM_HOLO_SLOTS / HOLO_SLOT_OFFSET, this fires loudly
// instead of letting holoConfigLoad() silently walk off the end of the array.
static_assert(
    SizeOfArray(servoSettings) == (NUM_PANEL_SLOTS + NUM_HOLO_SLOTS),
    "servoSettings[] size drift — update NUM_PANEL_SLOTS / NUM_HOLO_SLOTS to match");
static_assert(
    HOLO_SLOT_OFFSET == NUM_PANEL_SLOTS,
    "HOLO_SLOT_OFFSET must equal NUM_PANEL_SLOTS — holos sit immediately after panels");
#endif

// Default channel assignments used when NVS is absent (fresh boot, factory reset).
// Channels are physical silkscreen numbers (0–15) on each PCA9685 board; firmware pin
// is computed at runtime by panelConfigLoad() / holoConfigLoad() with the formula
// pin = (n × 16) + physCh + 1 (n = 0 for 0x40 panel board, n = 1 for 0x41 holo board).
// Active=false marks a slot as having no servo wired — its setServo() call then uses
// pin = 0, group = 0 to keep it out of all I2C writes and command-mask routing.
// These are just defaults — there is no single canonical "MK4 wiring"; builders
// whose physical wiring differs override per-slot via the Wiring Config UI on
// panels.html, persisted to NVS namespace "panels".
static const uint8_t defaultPanelCh[NUM_PANEL_SLOTS] = {
    0,   // slot 0:  P1   (ring)
    1,   // slot 1:  P2   (ring)
    2,   // slot 2:  P3   (ring)
    3,   // slot 3:  P4   (ring)
    4,   // slot 4:  P7   (ring upper)
    5,   // slot 5:  P11  (ring lower)
    6,   // slot 6:  P13  (ring front)
    0,   // slot 7:  PP5  — unserviced by default; channel value ignored when active=false
    8,   // slot 8:  PP1  (pie)
    9,   // slot 9:  PP2  (pie)
    10,  // slot 10: PP4  (pie)
    11,  // slot 11: PP6  (pie, :OP11 group only)
    0,   // slot 12: PP3  — unserviced by default; channel value ignored when active=false
};
static const bool defaultPanelActive[NUM_PANEL_SLOTS] = {
    true,  // P1
    true,  // P2
    true,  // P3
    true,  // P4
    true,  // P7
    true,  // P11
    true,  // P13
    false, // PP5 — inactive by default; flip to true via Wiring Config UI if a servo is wired
    true,  // PP1
    true,  // PP2
    true,  // PP4
    true,  // PP6
    false, // PP3 — inactive by default; flip to true via Wiring Config UI if a servo is wired
};

// Holo defaults — physical silkscreen channels on the 0x41 board.
static const uint8_t defaultHoloCh[NUM_HOLO_SLOTS] = {
    0,  // slot 13 (holo 0): FHP — front holo horizontal
    1,  // slot 14 (holo 1): FHP — front holo vertical
    2,  // slot 15 (holo 2): THP — top holo horizontal
    3,  // slot 16 (holo 3): THP — top holo vertical
    4,  // slot 17 (holo 4): RHP — rear holo vertical
    5,  // slot 18 (holo 5): RHP — rear holo horizontal
};
static const bool defaultHoloActive[NUM_HOLO_SLOTS] = {
    true, true, true, true, true, true,
};

#ifdef USE_I2C_ADDRESS
ServoDispatchDirect<SizeOfArray(servoSettings)> servoDispatch(servoSettings);
#else
ServoDispatchPCA9685<SizeOfArray(servoSettings)> servoDispatch(servoSettings);
#endif
ServoSequencer servoSequencer(servoDispatch);
AnimationPlayer player(servoSequencer);

// Dynamic wiring config — apply per-slot PCA9685 channel assignments and
// active/inactive state from NVS, overriding the PROGMEM defaults eagerly copied
// into fServos[] by the ServoDispatchPCA9685 constructor.
//
// Both functions MUST be called in setup() after Wire.begin() and BEFORE
// SetupEvent::ready() — that ordering is load-bearing because SetupEvent::ready()
// triggers the first PCA9685 I2C write, after which any slot still pointing at
// the wrong channel would briefly drive that physical output. See ADR 0002 for
// why setServo() is the right hook (vs construction reordering or PROGMEM editing).

// LogCapture instance declared here (rather than in AsyncWebInterface.h, which is
// included later in this TU) so that panelConfigLoad() / holoConfigLoad() — which
// run during setup() before the web layer is brought up — can write to the same
// captured ring buffer the web log viewer reads from. Tees to hardware Serial AND
// the buffer; raw Serial.print() bypasses the buffer and won't reach the web UI.
#include "LogCapture.h"
static LogCapture logCapture(Serial);

#ifndef USE_I2C_ADDRESS
// Operator-visible labels for the wiring-config boot logs. Mirrored in
// AsyncWebInterface.h's panelSlotLabel/holoSlotLabel — keep in sync.
static const char *kPanelSlotLabels[NUM_PANEL_SLOTS] = {
    "P1", "P2", "P3", "P4", "P7", "P11", "P13",
    "PP5", "PP1", "PP2", "PP4", "PP6", "PP3",
};
static const char *kHoloSlotLabels[NUM_HOLO_SLOTS] = {
    "FHP horizontal", "FHP vertical",
    "THP horizontal", "THP vertical",
    "RHP vertical",   "RHP horizontal",
};

static void panelConfigLoad()
{
    Preferences prefs;
    // Read-write open creates the namespace silently if it doesn't exist —
    // avoids the noisy "[E][Preferences.cpp:50] nvs_open failed: NOT_FOUND"
    // log that read-only mode produces on first ever boot. We never call
    // put*() in this function, so no flash writes happen after the one-time
    // namespace-entry creation on first boot. Subsequent boots: pure read.
    prefs.begin(PREFERENCE_PANELS_NS, false);
    int activeCount = 0;
    int inactiveCount = 0;
    int chOverrides = 0;
    int actOverrides = 0;
    for (int i = 0; i < NUM_PANEL_SLOTS; i++)
    {
        char keyC[12];
        char keyA[12];
        snprintf(keyC, sizeof(keyC), PREFERENCE_PANELS_CH_FMT, i);
        snprintf(keyA, sizeof(keyA), PREFERENCE_PANELS_ACT_FMT, i);

        // active = NVS bool if present, else MK4 default. The active flag is the
        // sole source of truth for whether a servo is wired — channel value is
        // ignored when inactive (we don't use a sentinel like channel=255).
        bool hasActOverride = prefs.isKey(keyA);
        bool hasChOverride  = prefs.isKey(keyC);
        bool active = hasActOverride ? prefs.getBool(keyA, defaultPanelActive[i])
                                      : defaultPanelActive[i];
        uint8_t physCh = hasChOverride ? prefs.getUChar(keyC, defaultPanelCh[i])
                                        : defaultPanelCh[i];

        // Log per-slot overrides that actually differ from the MK4 default so
        // operators can verify their saved mapping took effect. Skip silent
        // overrides where the saved value matches the default.
        if (hasChOverride && physCh != defaultPanelCh[i])
        {
            logCapture.printf("[Wiring] Panel %s channel override: CH%u (default CH%u)\n",
                              kPanelSlotLabels[i], (unsigned)physCh,
                              (unsigned)defaultPanelCh[i]);
            chOverrides++;
        }
        if (hasActOverride && active != defaultPanelActive[i])
        {
            logCapture.printf("[Wiring] Panel %s %s by saved config (default %s)\n",
                              kPanelSlotLabels[i],
                              active ? "enabled" : "disabled",
                              defaultPanelActive[i] ? "enabled" : "disabled");
            actOverrides++;
        }

        // Preserve the PROGMEM group/pulse so command routing, mask membership,
        // and per-slot calibration are unchanged. We override only the pin and,
        // for inactive slots, also zero the group bits to remove them from every
        // mask comparison (ALL_DOME_PANELS_MASK, PIE_PANELS_MASK, the per-slot
        // PANEL_GROUP_n bit drives :OPnn routing). pin=0 + group=0 is the
        // ReelTwo convention for "no servo on this slot".
        uint32_t group   = active ? servoDispatch.getGroup(i) : 0;
        uint8_t  pin     = active ? (physCh + 1) : 0;          // 0x40: silkscreen → firmware pin
        uint16_t start   = servoDispatch.getStart(i);
        uint16_t end     = servoDispatch.getEnd(i);
        uint16_t neutral = servoDispatch.getNeutral(i);

        // Operator-visible safety check: an enabled panel with no command
        // routing won't move when any Marcduino panel command is sent. Only
        // fires if a future firmware edit drops the group bits — should never
        // trigger on the shipped MK4 defaults.
        if (active && group == 0)
        {
            logCapture.printf(
                "[Wiring] Warning: panel %s is enabled in the wiring config "
                "but won't respond to any open/close commands. Either uncheck "
                "it in the Servo Wiring Config (if no servo is wired) or ask "
                "the firmware maintainer to restore its command group bits.\n",
                kPanelSlotLabels[i]);
        }

        if (active) activeCount++; else inactiveCount++;
        servoDispatch.setServo(i, pin, start, end, neutral, group);
    }
    prefs.end();

    // Boot-time summary — always logged so an operator who connects later can
    // confirm at a glance that the load ran and how many channels are wired.
    logCapture.printf("[Wiring] Panel config loaded: %d active, %d inactive "
                       "(NVS overrides: %d channel, %d active flag)\n",
                       activeCount, inactiveCount, chOverrides, actOverrides);
}

static void holoConfigLoad()
{
    Preferences prefs;
    // Read-write to suppress first-boot NOT_FOUND log — same reasoning as
    // panelConfigLoad above. No put*() calls = no flash writes after the
    // one-time namespace creation.
    prefs.begin(PREFERENCE_HOLOS_NS, false);
    int activeCount = 0;
    int inactiveCount = 0;
    int chOverrides = 0;
    int actOverrides = 0;
    for (int i = HOLO_SLOT_OFFSET; i < HOLO_SLOT_OFFSET + NUM_HOLO_SLOTS; i++)
    {
        // slotIdx (0–5) is what we use for NVS keys and the default arrays;
        // the dispatch itself addresses by absolute servoSettings[] index i.
        int slotIdx = i - HOLO_SLOT_OFFSET;
        char keyC[12];
        char keyA[12];
        snprintf(keyC, sizeof(keyC), PREFERENCE_HOLOS_CH_FMT, slotIdx);
        snprintf(keyA, sizeof(keyA), PREFERENCE_HOLOS_ACT_FMT, slotIdx);

        bool hasActOverride = prefs.isKey(keyA);
        bool hasChOverride  = prefs.isKey(keyC);
        bool active = hasActOverride ? prefs.getBool(keyA, defaultHoloActive[slotIdx])
                                      : defaultHoloActive[slotIdx];
        uint8_t physCh = hasChOverride ? prefs.getUChar(keyC, defaultHoloCh[slotIdx])
                                        : defaultHoloCh[slotIdx];

        if (hasChOverride && physCh != defaultHoloCh[slotIdx])
        {
            logCapture.printf("[Wiring] Holo %s channel override: CH%u (default CH%u)\n",
                              kHoloSlotLabels[slotIdx], (unsigned)physCh,
                              (unsigned)defaultHoloCh[slotIdx]);
            chOverrides++;
        }
        if (hasActOverride && active != defaultHoloActive[slotIdx])
        {
            logCapture.printf("[Wiring] Holo %s %s by saved config (default %s)\n",
                              kHoloSlotLabels[slotIdx],
                              active ? "enabled" : "disabled",
                              defaultHoloActive[slotIdx] ? "enabled" : "disabled");
            actOverrides++;
        }

        // 0x41 board pin formula. Firmware pin 16 still addresses 0x40 CH15 —
        // the holo board starts at pin 17 (silkscreen CH0). See ADR 0004 for
        // the chip-boundary math.
        uint32_t group   = active ? servoDispatch.getGroup(i) : 0;
        uint8_t  pin     = active ? (uint8_t)(16 + physCh + 1) : 0;
        uint16_t start   = servoDispatch.getStart(i);
        uint16_t end     = servoDispatch.getEnd(i);
        uint16_t neutral = servoDispatch.getNeutral(i);

        if (active && group == 0)
        {
            logCapture.printf(
                "[Wiring] Warning: holo %s is enabled in the wiring config "
                "but won't respond to any holo commands. Either uncheck it "
                "in the Holo Wiring Config (if no servo is wired) or ask the "
                "firmware maintainer to restore its command group bits.\n",
                kHoloSlotLabels[slotIdx]);
        }

        if (active) activeCount++; else inactiveCount++;
        servoDispatch.setServo(i, pin, start, end, neutral, group);
    }
    prefs.end();

    logCapture.printf("[Wiring] Holo config loaded: %d active, %d inactive "
                       "(NVS overrides: %d channel, %d active flag)\n",
                       activeCount, inactiveCount, chOverrides, actOverrides);
}
#endif // USE_I2C_ADDRESS

// Panel servo auto-release — cut PWM after a close sequence so a stalled/
// misconnected servo cannot grind indefinitely against its mechanical stop.
// sPanelReleaseMask tracks which groups are pending; only those servos are cut,
// so an open panel in a different group is never disturbed by another group's close.
static uint32_t sPanelReleaseAtMs   = 0;
static uint32_t sPanelReleaseMask   = 0;
static void schedulePanelRelease(uint32_t mask, uint32_t delayMs = 1500);
static void cancelPanelRelease(uint32_t mask = ALL_DOME_PANELS_MASK);
MarcduinoSerial<> marcduinoSerial(player);

/////////////////////////////////////////////////////////////////////////

#include "MarcduinoSound.h"
MarcSound::Module sSoundPlayer;

/////////////////////////////////////////////////////////////////////////

#define NUM_LEDS 28 * 4
uint32_t lastEvent;
CRGB leds[NUM_LEDS];
#ifdef LIVE_STREAM
AsyncUDP udp;
#endif

enum
{
    SDBITMAP = 100,
    PLASMA,
    METABALLS,
    FRACTAL,
    FADEANDSCROLL
};

#include "effects/BitmapEffect.h"
#include "effects/FadeAndScrollEffect.h"
#include "effects/FractalEffect.h"
#include "effects/MeatBallsEffect.h"
#include "effects/PlasmaEffect.h"

////////////////////////////////
// Standard LogicEngine sequences are in the range 0-99. Custom sequences start at 100
static const LogicEffect sCustomLogicEffects[] = {
    LogicEffectBitmap,
    LogicEffectPlasma,
    LogicEffectMetaBalls,
    LogicEffectFractal,
    LogicEffectFadeAndScroll};

LogicEffect CustomLogicEffectSelector(unsigned selectSequence)
{
    if (selectSequence >= 100 && selectSequence - 100 < SizeOfArray(sCustomLogicEffects))
    {
        return LogicEffect(sCustomLogicEffects[selectSequence - 100]);
    }
    return LogicEffectDefaultSelector(selectSequence);
}

////////////////////////////////

Preferences preferences;

////////////////////////////////

bool mountReadOnlyFileSystem()
{
#ifdef USE_SPIFFS
    return (SPIFFS.begin(true));
#endif
    return false;
}

void unmountFileSystems()
{
#ifdef USE_SPIFFS
    SPIFFS.end();
#endif
}

////////////////////////////////
// This function is called when settings have been changed and needs a reboot
void reboot()
{
    DEBUG_PRINTLN(F("Restarting..."));
#ifdef USE_DROID_REMOTE
    DisconnectRemote();
#endif
    unmountFileSystems();
    preferences.end();
    ESP.restart();
}

////////////////////////////////
// This function is called when aborting or ending Marcduino sequences. It should reset all droid devices to Normal
void resetSequence()
{
    Marcduino::send(F("$s"));
    CommandEvent::process(F(
        "LE000000|0\n" // LogicEngine devices to normal
        "FSOFF\n"      // Fire Stripe Off
        "BMOFF\n"      // Bad Motiviator Off
        "HPA000|0\n"   // Holo Projectors to Normal
        "CB00000\n"    // Charge Bay to Normal
        "DP00000\n")); // Data Panel to Normal
}

////////////////////////////////

int32_t strtol(const char *cmd, const char **endptr)
{
    bool sign = false;
    int32_t result = 0;
    if (*cmd == '-')
    {
        cmd++;
        sign = true;
    }
    while (isdigit(*cmd))
    {
        result = result * 10L + (*cmd - '0');
        cmd++;
    }
    *endptr = cmd;
    return (sign) ? -result : result;
}

////////////////////////////////

bool numberparams(const char *cmd, uint8_t &argcount, int32_t *args, uint8_t maxcount)
{
    for (argcount = 0; argcount < maxcount; argcount++)
    {
        args[argcount] = strtol(cmd, &cmd);
        if (*cmd == '\0')
        {
            argcount++;
            return true;
        }
        else if (*cmd != ',')
        {
            return false;
        }
        cmd++;
    }
    return true;
}

////////////////////////////////

#include "MarcduinoHolo.h"
#include "FlthyHoloExtras.h"
#include "MarcduinoLogics.h"
#include "MarcduinoSequence.h"
#include "MarcduinoPanel.h"
#include "MarcduinoPSI.h"

////////////////////////////////

#ifdef USE_WIFI
WifiAccess wifiAccess;
bool wifiEnabled;
bool wifiActive;
bool remoteEnabled;
bool remoteActive;
TaskHandle_t eventTask;
bool otaInProgress;
#endif

#ifdef USE_WIFI_WEB
static bool sAsyncWebStarted;
#endif

bool soundLocalEnabled;
bool sSleepModeActive;
uint32_t sSleepModeSinceMs;
uint32_t sSleepEnforceAtMs;
static const uint8_t kStatusScrollSpeedScale = 4;
static const uint32_t kSleepTransitionScrollMs = 5200;
bool sWakeTransitionPending;
uint32_t sWakeTransitionAtMs;
uint32_t sMinFreeHeap = 0;
static bool sSoundInitPending;
static uint32_t sSoundInitAtMs;
static uint8_t sSoundInitAttempts;
static MarcSound::Module sSoundInitModule;
static int sSoundInitStartup;
static float sSoundInitVolume;

// Body link state — not persisted, reset on reboot
static uint32_t sBodyLastSeenMs  = 0;   // millis() when last #PAHB received (0=never)
static uint32_t sBodyHeartbeatRx = 0;   // count of #PAHB frames received from body
static uint32_t sBodyLastTxMs    = 0;   // millis() of last #APHB sent
static bool sSuppressBodyLinkEgress = false;
static uint32_t sLastMoodResetMs = 0;
static char sLastMoodResetCmd[6] = "";
static char sCurrentMoodCmd[6] = "";
#include "BodyLinkWiFi.h"
#include "DomeSequences.h"
bool dome_PiesOpen   = false;
bool dome_AllOpen    = false;
bool dome_LowOpen    = false;
bool dome_seqRunning = false;

static bool bodyLinkConnected()
{
    return sBodyLastSeenMs > 0 && (millis() - sBodyLastSeenMs) < 5000;
}

static bool isBodyLinkSource(const char *source)
{
    return source != nullptr && strncmp(source, "body-link-", 10) == 0;
}

static bool isMoodResetCommand(const char *cmd)
{
    return cmd != nullptr &&
        (strcmp(cmd, ":SE10") == 0 ||
         strcmp(cmd, ":SE11") == 0 ||
         strcmp(cmd, ":SE13") == 0 ||
         strcmp(cmd, ":SE14") == 0);
}

static bool shouldDropDuplicateMoodReset(const char *cmd)
{
    if (!isMoodResetCommand(cmd))
        return false;

    uint32_t now = millis();
    if (strcmp(sLastMoodResetCmd, cmd) == 0 && (now - sLastMoodResetMs) < 2500)
        return true;

    strlcpy(sLastMoodResetCmd, cmd, sizeof(sLastMoodResetCmd));
    sLastMoodResetMs = now;
    return false;
}

static void setCurrentMoodCommand(const char *cmd)
{
    if (!isMoodResetCommand(cmd))
        return;
    strlcpy(sCurrentMoodCmd, cmd, sizeof(sCurrentMoodCmd));
}

static const char *currentMoodName()
{
    if (strcmp(sCurrentMoodCmd, ":SE10") == 0) return "Quiet";
    if (strcmp(sCurrentMoodCmd, ":SE11") == 0) return "Full-Awake";
    if (strcmp(sCurrentMoodCmd, ":SE13") == 0) return "Mid-Awake";
    if (strcmp(sCurrentMoodCmd, ":SE14") == 0) return "Awake+";
    return "";
}

static void sendBodyCommand(const char* cmd)
{
    if (cmd == nullptr || *cmd == '\0') return;

    if (sSuppressBodyLinkEgress)
    {
        DEBUG_PRINT(F("[BodyLink] Suppressed egress while processing inbound command: "));
        DEBUG_PRINTLN(cmd);
        return;
    }

    BodyLinkTransport transport = bodyLinkActiveTransport();
    if (transport == BODY_LINK_UART && COMMAND_SERIAL)
    {
        COMMAND_SERIAL.print(cmd);
        COMMAND_SERIAL.print('\r');
    }
    else if (transport == BODY_LINK_WIFI)
    {
        bodyLinkWiFiSendUDP(cmd);
    }
}

static void handleBodySerial()
{
    static bool sBodyLinkEnabled = false;
    static bool sBodyLinkInitDone = false;
    if (!sBodyLinkInitDone)
    {
        sBodyLinkEnabled = preferences.getBool(PREFERENCE_BODY_LINK_ENABLED, BODY_LINK_ENABLED);
        sBodyLinkInitDone = true;
    }
    if (!sBodyLinkEnabled) return;
    static char sBuf[65];  // 64 data + 1 null terminator
    static uint8_t sBufLen = 0;
    while (COMMAND_SERIAL.available())
    {
        char c = (char)COMMAND_SERIAL.read();
        if (c == '\r' || c == '\n')
        {
            if (sBufLen > 0)
            {
                sBuf[sBufLen] = '\0';
                uint32_t now = millis();
                if (strcmp(sBuf, "#PAHB") == 0)
                {
                    bodyLinkMarkUartHeartbeat(now);
                }
                else
                {
                    bodyLinkMarkUartActivity(now);
                    processMarcduinoCommandWithSourceMain("body-link-uart", sBuf);
                }
                sBufLen = 0;
            }
        }
        else if (sBufLen < sizeof(sBuf) - 1)
        {
            sBuf[sBufLen++] = c;
        }
        else
        {
            // Buffer overflow — discard and reset
            DEBUG_PRINTLN(F("[BodyLink] Buffer overflow - command discarded"));
            sBuf[0] = '\0';
            sBufLen = 0;
        }
    }
}

static void handleBodyLinkHeartbeat()
{
    static bool sBodyLinkEnabled = false;
    static bool sBodyLinkInitDone = false;
    if (!sBodyLinkInitDone)
    {
        sBodyLinkEnabled = preferences.getBool(PREFERENCE_BODY_LINK_ENABLED, BODY_LINK_ENABLED);
        sBodyLinkInitDone = true;
    }
    if (!sBodyLinkEnabled) return;

    // Connection state tracking for logging
    static bool sPrevConnected = false;
    bool nowConnected = bodyLinkConnected();
    if (nowConnected != sPrevConnected)
    {
        if (nowConnected)
            DEBUG_PRINTLN(F("[BodyLink] Body controller connected"));
        else
            DEBUG_PRINTLN(F("[BodyLink] Body controller LOST"));
        sPrevConnected = nowConnected;
    }

    static BodyLinkTransport sPrevTransport = BODY_LINK_DISCONNECTED;
    BodyLinkTransport transport = bodyLinkActiveTransport();
    if (transport != sPrevTransport)
    {
        DEBUG_PRINT(F("[BodyLink] Active transport: "));
        DEBUG_PRINTLN(bodyLinkGetTransportName());
        sPrevTransport = transport;
    }

    uint32_t now = millis();
    if (now - sBodyLastTxMs >= 1000)
    {
        if (transport == BODY_LINK_UART && COMMAND_SERIAL)
        {
            COMMAND_SERIAL.print("#APHB\r");
            sBodyLastTxMs = now;
        }
        else if (transport == BODY_LINK_WIFI)
        {
            if (bodyLinkWiFiSendUDP("#APHB"))
            {
                sBodyLastTxMs = now;
            }
        }
    }
}

#ifdef USE_WIFI_MARCDUINO
WifiMarcduinoReceiver wifiMarcduinoReceiver(wifiAccess);
#endif

////////////////////////////////

#ifdef USE_MENUS

#include "Screens.h"
#include "menus/CommandScreen.h"

#ifdef USE_LCD_SCREEN

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define SCREEN_ADDRESS 0x3C

#include "menus/CommandScreenHandlerSSD1306.h"
CommandScreenHandlerSSD1306 sDisplay(sPinManager);

#else

#include "menus/CommandScreenHandlerSMQ.h"
CommandScreenHandlerSMQ sDisplay;

#endif

#include "menus/utility/ChoiceIntArrayScreen.h"
#include "menus/utility/ChoiceStrArrayScreen.h"
#include "menus/utility/UnsignedValueScreen.h"
#include "menus/utility/MenuScreen.h"

#include "menus/MainScreen.h"
#include "menus/SplashScreen.h"
#include "menus/SequenceScreen.h"
#include "menus/LogicsScreen.h"
#include "menus/HoloScreen.h"

#endif

////////////////////////////////

#ifdef USE_WIFI_WEB
#include "AsyncWebInterface.h"
#endif

#ifdef USE_DROID_REMOTE
bool sRemoteConnected;
bool sRemoteConnecting;
SMQAddress sRemoteAddress;
#endif


////////////////////////////////

void scan_i2c()
{
    unsigned nDevices = 0;
    Serial.println(F("==========================================="));
    Serial.println(F("Scanning I2C addresses 0x01-0x7E..."));
    Serial.println(F("Expected: 0x40 (Panels), 0x41 (Holos)"));
    Serial.println(F("==========================================="));
    
    for (byte address = 1; address < 127; address++)
    {
        String name = "<unknown>";
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        if (address == 0x70)
        {
            // All call address for PCA9685
            name = "PCA9685:all";
        }
        if (address == 0x40)
        {
            // Adafruit PCA9685 - Panels Controller
            name = "PCA9685 (Panels) ← EXPECTED";
        }
        if (address == 0x41)
        {
            // Adafruit PCA9685 - Holos Controller
            name = "PCA9685 (Holos) ← EXPECTED";
        }
        if (address == 0x14)
        {
            // IA-Parts magic panel
            name = "IA-Parts Magic Panel";
        }
        if (address == 0x20)
        {
            // IA-Parts periscope
            name = "IA-Parts Periscope";
        }
        if (address == 0x16)
        {
            // PSIPro
            name = "PSIPro";
        }

        if (error == 0)
        {
            Serial.print(F("✓ I2C device found at address 0x"));
            if (address < 16)
                Serial.print(F("0"));
            Serial.print(address, HEX);
            Serial.print(F(" "));
            Serial.println(name);
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print(F("✗ Unknown error at address 0x"));
            if (address < 16)
                Serial.print(F("0"));
            Serial.println(address, HEX);
        }
    }
    Serial.println(F("==========================================="));
    if (nDevices == 0)
        Serial.println(F("❌ NO I2C DEVICES FOUND!"));
    else
    {
        Serial.print(F("✓ Found "));
        Serial.print(nDevices);
        Serial.println(F(" I2C device(s)"));
    }
    Serial.println(F("==========================================\n"));
}

////////////////////////////////

void setup()
{
    REELTWO_READY();

    if (!preferences.begin("astro", false))
    {
        DEBUG_PRINTLN(F("Failed to init prefs"));
    }
#ifdef USE_WIFI
    wifiEnabled = preferences.getBool(PREFERENCE_WIFI_ENABLED, WIFI_ENABLED);
    wifiActive = false;
#ifdef USE_DROID_REMOTE
    remoteEnabled = remoteActive = preferences.getBool(PREFERENCE_REMOTE_ENABLED, REMOTE_ENABLED);
#else
    remoteEnabled = remoteActive = false;
#endif
#endif
    soundLocalEnabled = preferences.getBool(PREFERENCE_MARCSOUND_LOCAL_ENABLED, MARC_SOUND_LOCAL_ENABLED);
    sSleepModeActive = false;
    sSleepModeSinceMs = 0;
    sSleepEnforceAtMs = 0;
    sWakeTransitionPending = false;
    sWakeTransitionAtMs = 0;
    sMinFreeHeap = ESP.getFreeHeap();
    String droidName = getConfiguredDroidName();
    PrintReelTwoInfo(Serial, droidName.c_str());

    bool serial2Enabled = preferences.getBool(PREFERENCE_MARCSERIAL_ENABLED, MARC_SERIAL_ENABLED);
    bool bodyLinkEnabled = preferences.getBool(PREFERENCE_BODY_LINK_ENABLED, BODY_LINK_ENABLED);

    // Body link depends on Serial2 transport. If user disabled mserial but kept
    // body link enabled, keep Serial2 active to avoid a broken heartbeat link.
    if (bodyLinkEnabled && !serial2Enabled)
    {
        DEBUG_PRINTLN(F("[BodyLink] Forcing Serial2 active because body link is enabled"));
        serial2Enabled = true;
    }

    if (serial2Enabled)
    {
        COMMAND_SERIAL.begin(preferences.getInt(PREFERENCE_MARCSERIAL2, MARC_SERIAL2_BAUD_RATE), SERIAL_8N1, SERIAL2_RX_PIN, SERIAL2_TX_PIN);
        if (bodyLinkEnabled)
        {
            // Body link enabled — disable Reeltwo stream handling to prevent race conditions
            marcduinoSerial.setStream(nullptr, nullptr);
            // handleBodySerial() in mainLoop() reads manually
        }
        else
        {
            // Body link disabled — use legacy Reeltwo stream handler
            // if (preferences.getBool(PREFERENCE_MARCSERIAL_PASS, MARC_SERIAL_PASS))
            marcduinoSerial.setStream(&COMMAND_SERIAL, &Serial);
        }
    }
    if (!mountReadOnlyFileSystem())
    {
        DEBUG_PRINTLN(F("Failed to mount read only filesystem"));
    }

#ifndef USE_I2C_ADDRESS
    Wire.begin();
    Serial.println(F("\n=== I2C DIAGNOSTICS ==="));
    Serial.println(F("Initializing I2C on SDA=21, SCL=22"));
    delay(100); // Give I2C time to settle
    scan_i2c();
    Serial.println(F("=== END I2C DIAGNOSTICS ===\n"));

    // Apply per-slot wiring overrides BEFORE SetupEvent::ready() — that call
    // triggers the first PCA9685 I2C write, so any setServo() updates must be
    // in place beforehand or the wrong channels would briefly drive. See ADR 0002.
    panelConfigLoad();
    holoConfigLoad();
#endif
    SetupEvent::ready();
    loadPersistedPanelCalibration();

    #if AP_ENABLE_DATAPANEL
    dataPanel.setSequence(DataPanel::kDisabled);
#endif
#if AP_ENABLE_CBI
    chargeBayIndicator.setSequence(ChargeBayIndicator::kDisabled);
#endif


#ifdef USE_LCD_SCREEN
    sDisplay.setEnabled(sDisplay.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS));
    if (sDisplay.isEnabled())
    {
        sDisplay.invertDisplay(false);
        sDisplay.clearDisplay();
        sDisplay.setRotation(2);
    }
#endif
    MarcSound::Module soundPlayer = (MarcSound::Module)preferences.getInt(PREFERENCE_MARCSOUND, MARC_SOUND_PLAYER);
    int soundStartup = preferences.getInt(PREFERENCE_MARCSOUND_STARTUP, MARC_SOUND_STARTUP);
    sSoundInitPending = false;
    sSoundInitAttempts = 0;
    if (!soundLocalEnabled)
    {
        DEBUG_PRINTLN(F("Local sound execution disabled by preference"));
    }
    else if (soundPlayer != MarcSound::kDisabled)
    {
        SOUND_SERIAL.begin(SOUND_BAUD, SERIAL_8N1, SOUND_RX_PIN, SOUND_TX_PIN);
        sSoundInitPending = true;
        sSoundInitAtMs = millis() + 3000;
        sSoundInitModule = soundPlayer;
        sSoundInitStartup = soundStartup;
        sSoundInitVolume = preferences.getInt(PREFERENCE_MARCSOUND_VOLUME, MARC_SOUND_VOLUME) / 1000.0f;
        DEBUG_PRINTLN(F("Sound module initialization scheduled (deferred)"));
    }
    // Assign servos to holo projectors
    frontHolo.assignServos(&servoDispatch, 13, 14);
    // Second PCA9685 controller
    // { 16, 800, 2200, HOLO_HSERVO },                /* 13: horizontal front holo */
    // { 17, 800, 2200, HOLO_VSERVO },                /* 14: vertical front holo */

    topHolo.assignServos(&servoDispatch, 15, 16);
    // { 18, 800, 2200, HOLO_HSERVO },                /* 15: horizontal top holo */
    // { 19, 800, 2200, HOLO_VSERVO },                /* 16: vertical top holo */

    rearHolo.assignServos(&servoDispatch, 17, 18);
    // { 20, 800, 2200, HOLO_VSERVO },                /* 17: vertical rear holo */
    // { 21, 800, 2200, HOLO_HSERVO },                /* 18: horizontal rear holo */

    // CREDENDA FORK IMPROVEMENT: Enhanced startup text display
    // Shows droid name on both logic displays during boot
    // Initialize LED effects before WiFi starts
    String bootScroll = "... " + droidName + " ...";
    RLD.selectScrollTextLeft(bootScroll.c_str(), LogicEngineRenderer::kBlue, kStatusScrollSpeedScale, 15);
    FLD.selectScrollTextLeft(bootScroll.c_str(), LogicEngineRenderer::kBlue, kStatusScrollSpeedScale, 15);
    RLD.setLogicEffectSelector(CustomLogicEffectSelector);
    FLD.setLogicEffectSelector(CustomLogicEffectSelector);
    frontPSI.setLogicEffectSelector(CustomLogicEffectSelector);
    rearPSI.setLogicEffectSelector(CustomLogicEffectSelector);

#ifdef USE_WIFI
    if (remoteEnabled)
    {
#ifdef USE_SMQ
        WiFi.mode(WIFI_MODE_APSTA);
        if (SMQ::init(preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME),
                      preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET)))
        {
            SMQLMK key;
            if (preferences.getBytes(PREFERENCE_REMOTE_LMK, &key, sizeof(SMQLMK)) == sizeof(SMQLMK))
            {
                SMQ::setLocalMasterKey(&key);
            }

            SMQAddressKey pairedHosts[SMQ_MAX_PAIRED_HOSTS];
            size_t pairedHostsSize = preferences.getBytesLength(PREFERENCE_REMOTE_PAIRED);
            unsigned numHosts = pairedHostsSize / sizeof(pairedHosts[0]);
            printf("numHosts: %d\n", numHosts);
            Serial.print(F("WiFi.macAddress() : "));
            Serial.println(WiFi.macAddress());
            if (numHosts != 0)
            {
                if (preferences.getBytes(PREFERENCE_REMOTE_PAIRED, pairedHosts, pairedHostsSize) == pairedHostsSize)
                {
                    SMQ::addPairedHosts(numHosts, pairedHosts);
                }
            }
            printf("Droid Remote Enabled %s:%s\n",
                   preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME).c_str(),
                   preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET).c_str());
            SMQ::setHostPairingCallback([](SMQHost *host)
                                        {
                if (host == nullptr)
                {
                    printf("Pairing timed out\n");
                }
                else //if (host->hasTopic("LCD"))
                {
                    switch (SMQ::masterKeyExchange(&host->fLMK))
                    {
                        case -1:
                            printf("Pairing Stopped\n");
                            SMQ::stopPairing();
                            return;
                        case 1:
                            // Save new master key
                            SMQLMK lmk;
                            SMQ::getLocalMasterKey(&lmk);
                            printf("Saved new master key\n");
                            preferences.putBytes(PREFERENCE_REMOTE_LMK, &lmk, sizeof(lmk));
                            break;
                        case 0:
                            // We had the master key
                            break;
                    }
                    printf("Pairing: %s [%s]\n", host->getHostName().c_str(), host->fLMK.toString().c_str());
                    if (SMQ::addPairedHost(&host->fAddr, &host->fLMK))
                    {
                        SMQAddressKey pairedHosts[SMQ_MAX_PAIRED_HOSTS];
                        unsigned numHosts = SMQ::getPairedHostCount();
                        if (SMQ::getPairedHosts(pairedHosts, numHosts) == numHosts)
                        {
                            preferences.putBytes(PREFERENCE_REMOTE_PAIRED,
                                pairedHosts, numHosts*sizeof(pairedHosts[0]));
                            printf("Pairing Success\n");
                        }
                    }
                    printf("Pairing Stopped\n");
                    SMQ::stopPairing();
                } });

            SMQ::setHostDiscoveryCallback([](SMQHost *host)
                                          {
                if (host->hasTopic("LCD"))
                {
                    printf("Remote Discovered: %s\n", host->getHostName().c_str());
                } });

            SMQ::setHostLostCallback([](SMQHost *host)
                                     {
                printf("Lost: %s [%s] [%s]\n", host->getHostName().c_str(), host->getHostAddress().c_str(),
                    sRemoteAddress.toString().c_str());
                if (sRemoteAddress.equals(host->fAddr.fData))
                {
                    printf("DISABLING REMOTE\n");
                    sDisplay.setEnabled(false);
                } });
        }
        else
        {
            printf("Failed to activate Droid Remote\n");
        }
#endif
    }
    if (wifiEnabled)
    {
#ifdef USE_WIFI_WEB
        // In preparation for adding WiFi settings web page
        wifiAccess.setNetworkCredentials(
            preferences.getString(PREFERENCE_WIFI_SSID, WIFI_AP_NAME),
            preferences.getString(PREFERENCE_WIFI_PASS, WIFI_AP_PASSPHRASE),
            preferences.getBool(PREFERENCE_WIFI_AP, WIFI_ACCESS_POINT),
            preferences.getBool(PREFERENCE_WIFI_ENABLED, WIFI_ENABLED));
        // Keep WiFi fully awake to avoid multi-second UI/API latency spikes
        // seen with default ESP32 modem sleep in STA mode.
        WiFi.setSleep(false);
        // CRITICAL: setNetworkCredentials changes WiFi mode to STA
        // If remote is enabled, we need APSTA mode for ESP-NOW, so override it here
        if (remoteEnabled)
        {
            WiFi.mode(WIFI_MODE_APSTA);
        }
#ifdef USE_WIFI_MARCDUINO
        wifiMarcduinoReceiver.setEnabled(preferences.getBool(PREFERENCE_MARCWIFI_ENABLED, MARC_WIFI_ENABLED));
        if (wifiMarcduinoReceiver.enabled())
        {
            wifiMarcduinoReceiver.setCommandHandler([](const char *cmd)
                                                    {
                printf("cmd: %s\n", cmd);
                processMarcduinoCommandWithSourceMain("wifi-marcduino", cmd);
                if (preferences.getBool(PREFERENCE_MARCWIFI_SERIAL_PASS, MARC_WIFI_SERIAL_PASS))
                {
                    COMMAND_SERIAL.print(cmd); COMMAND_SERIAL.print('\r');
                } });
        }
#endif
        wifiAccess.notifyWifiConnected([](WifiAccess &wifi)
                                       {
                                           wifiActive = true;
                                           WiFi.setSleep(false);
                                           Serial.print("Connect to http://");
                                           Serial.println(wifi.getIPAddress());
#ifdef USE_WIFI_WEB
                                           if (!sAsyncWebStarted)
                                           {
                                               initAsyncWeb();
                                               sAsyncWebStarted = true;
                                           }
#endif
                                           bodyLinkWiFiInit();
                                           bodyLinkSetupMDNS();
                                       });
        wifiAccess.notifyWifiDisconnected([](WifiAccess &)
                                          {
                                              wifiActive = false;
                                          });
        bodyLinkWiFiInit();
#endif
#ifdef USE_OTA
        ArduinoOTA.onStart([]()
                           {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
            {
                type = "sketch";
            }
            else // U_SPIFFS
            {
                type = "filesystem";
            }
            DEBUG_PRINTLN(F("OTA START")); })
            .onEnd([]()
                   { DEBUG_PRINTLN(F("OTA END")); })
            .onProgress([](unsigned int progress, unsigned int total)
                        {
                            // float range = (float)progress / (float)total;
                        })
            .onError([](ota_error_t error)
                     {
            String desc;
            if (error == OTA_AUTH_ERROR) desc = "Auth Failed";
            else if (error == OTA_BEGIN_ERROR) desc = "Begin Failed";
            else if (error == OTA_CONNECT_ERROR) desc = "Connect Failed";
            else if (error == OTA_RECEIVE_ERROR) desc = "Receive Failed";
            else if (error == OTA_END_ERROR) desc = "End Failed";
            else desc = "Error: "+String(error);
            DEBUG_PRINTLN(desc); });
#endif
    }
#endif
#ifdef USE_WIFI_WEB
    sAsyncWebStarted = false;
#endif

    // DEBUG TEMP: Deferring web server creation until WiFi connected
    // Logic effect selectors initialized after WiFi connects (see callback above)
#ifdef USE_WIFI
    xTaskCreatePinnedToCore(
        eventLoopTask,
        "Events",
        10000, // shrink stack size?
        NULL,
        1,
        &eventTask,
        0);
#endif
    DEBUG_PRINTLN(F("Ready"));
    if (preferences.getBool("holo_boot_loop", true))
        CommandEvent::process(F("HPS9"));
    if (soundLocalEnabled && !sSoundInitPending)
    {
        sMarcSound.playStartSound();
        sMarcSound.setRandomMin(preferences.getInt(PREFERENCE_MARCSOUND_RANDOM_MIN, MARC_SOUND_RANDOM_MIN));
        sMarcSound.setRandomMax(preferences.getInt(PREFERENCE_MARCSOUND_RANDOM_MAX, MARC_SOUND_RANDOM_MAX));
        if (preferences.getInt(PREFERENCE_MARCSOUND_RANDOM, MARC_SOUND_RANDOM))
            sMarcSound.startRandomInSeconds(13);
    }
}

////////////////

MARCDUINO_ACTION(DirectCommand, ~RT, ({
                     // Direct ReelTwo command
                     CommandEvent::process(Marcduino::getCommand());
                 }))

////////////////

MARCDUINO_ACTION(MDDirectCommand, @AP, ({
                     // Direct ReelTwo command
                     CommandEvent::process(Marcduino::getCommand());
                 }))

////////////////

MARCDUINO_ACTION(WifiToggle, #APWIFI, ({
#ifdef USE_WIFI
                     bool wifiSetting = wifiEnabled;
                     switch (*Marcduino::getCommand())
                     {
                     case '0':
                         wifiSetting = false;
                         break;
                     case '1':
                         wifiSetting = true;
                         break;
                     case '\0':
                         // Toggle WiFi
                         wifiSetting = !wifiSetting;
                         break;
                     }
                     if (wifiEnabled != wifiSetting)
                     {
                         if (wifiSetting)
                         {
                             preferences.putBool(PREFERENCE_WIFI_ENABLED, true);
                             DEBUG_PRINTLN(F("WiFi Enabled"));
                         }
                         else
                         {
                             preferences.putBool(PREFERENCE_WIFI_ENABLED, false);
                             DEBUG_PRINTLN(F("WiFi Disabled"));
                         }
                         reboot();
                     }
#endif
                 }))

////////////////

MARCDUINO_ACTION(RemoteToggle, #APREMOTE, ({
#ifdef USE_DROID_REMOTE
                     bool remoteSetting = remoteEnabled;
                     switch (*Marcduino::getCommand())
                     {
                     case '0':
                         remoteSetting = false;
                         break;
                     case '1':
                         remoteSetting = true;
                         break;
                     case '\0':
                         // Toggle remote
                         remoteSetting = !remoteSetting;
                         break;
                     }
                     if (remoteEnabled != remoteSetting)
                     {
                         if (remoteSetting)
                         {
                             preferences.putBool(PREFERENCE_REMOTE_ENABLED, true);
                             DEBUG_PRINTLN(F("Remote Enabled"));
                         }
                         else
                         {
                             preferences.putBool(PREFERENCE_REMOTE_ENABLED, false);
                             DEBUG_PRINTLN(F("Remote Disabled"));
                         }
                         reboot();
                     }
#endif
                 }))

////////////////

MARCDUINO_ACTION(RemoteName, #APRNAME, ({
                     String newHostname = String(Marcduino::getCommand());
                     if (preferences.getString(PREFERENCE_REMOTE_HOSTNAME, SMQ_HOSTNAME) != newHostname)
                     {
                         preferences.putString(PREFERENCE_REMOTE_HOSTNAME, newHostname);
                         printf("Changed.\n");
                         reboot();
                     }
                 }))

////////////////

MARCDUINO_ACTION(RemoteSecret, #APRSECRET, ({
                     String newSecret = String(Marcduino::getCommand());
                     if (preferences.getString(PREFERENCE_REMOTE_SECRET, SMQ_SECRET) != newSecret)
                     {
                         preferences.putString(PREFERENCE_REMOTE_SECRET, newSecret);
                         printf("Changed.\n");
                         reboot();
                     }
                 }))

////////////////

MARCDUINO_ACTION(RemotePair, #APPAIR, ({
#ifdef USE_DROID_REMOTE
                     printf("Pairing Started ...\n");
                     SMQ::startPairing();
#endif
                 }))

////////////////

MARCDUINO_ACTION(RemoteUnpair, #APUNPAIR, ({
                     if (preferences.remove(PREFERENCE_REMOTE_PAIRED))
                     {
                         printf("Unpairing Success...\n");
                         reboot();
                     }
                     else
                     {
                         printf("Not Paired...\n");
                     }
                 }))

////////////////

MARCDUINO_ACTION(ClearPrefs, #APZERO, ({
                     preferences.clear();
                     DEBUG_PRINT(F("Clearing preferences. "));
                     reboot();
                 }))

////////////////

MARCDUINO_ACTION(Restart, #APRESTART, ({
                     reboot();
                 }))

////////////////

MARCDUINO_ACTION(BodySleepSync, #PASL, ({
                     // Body entered sleep — mirror locally, suppress echo back
                     enterSoftSleepMode(true);
                 }))

////////////////

MARCDUINO_ACTION(BodyWakeSync, #PAWU, ({
                     // Body exited sleep — mirror locally, suppress echo back
                     exitSoftSleepMode(true);
                 }))

////////////////

#ifdef USE_SMQ
// SMQ messages are received via ESPNOW.
SMQMESSAGE(DIAL, {
    long newValue = msg.get_int32("new");
    long oldValue = msg.get_int32("old");
    sDisplay.remoteDialEvent(newValue, oldValue);
})

///////////////////////////////////////////////////////////////////////////////

SMQMESSAGE(BUTTON, {
    uint8_t id = msg.get_uint8("id");
    bool pressed = msg.get_uint8("pressed");
    bool repeat = msg.get_uint8("repeat");
    sDisplay.remoteButtonEvent(id, pressed, repeat);
})

///////////////////////////////////////////////////////////////////////////////

SMQMESSAGE(SELECT, {
    DEBUG_PRINTLN(F("REMOTE ACTIVE"));
    sDisplay.setEnabled(true);
    sDisplay.switchToScreen(kMainScreen);
    sMainScreen.init();
    sRemoteConnected = true;
    sRemoteConnecting = true;
    sRemoteAddress = SMQ::messageSender();
})
#endif

#ifdef USE_DROID_REMOTE
static void DisconnectRemote()
{
#ifdef USE_SMQ
    printf("DisconnectRemote : %d\n", sRemoteConnected);
    if (sRemoteConnected)
    {
        if (SMQ::sendTopic("EXIT", "Remote"))
        {
            SMQ::sendString("addr", SMQ::getAddress());
            SMQ::sendEnd();
            printf("SENT EXIT\n");
            sRemoteConnected = false;
            sDisplay.setEnabled(false);
        }
    }
#endif
}
#endif

////////////////

static unsigned sPos;
static char sBuffer[CONSOLE_BUFFER_SIZE];

static bool isCalibrationPanelServoGroup(uint32_t group)
{
    return (group & (SMALL_PANEL | MEDIUM_PANEL | BIG_PANEL | PIE_PANEL | TOP_PIE_PANEL | MINI_PANEL)) != 0;
}

static void loadPersistedPanelCalibration()
{
    for (uint16_t i = 0; i < servoDispatch.getNumServos(); i++)
    {
        uint32_t group = servoDispatch.getGroup(i);
        if (!isCalibrationPanelServoGroup(group)) continue;

        uint16_t startPulse = servoDispatch.getStart(i);
        uint16_t endPulse = servoDispatch.getEnd(i);

        char openKey[8];
        char closeKey[8];
        snprintf(openKey, sizeof(openKey), "so%02u", i);
        snprintf(closeKey, sizeof(closeKey), "sc%02u", i);

        uint16_t persistedOpen = preferences.getUShort(openKey, startPulse);
        uint16_t persistedClose = preferences.getUShort(closeKey, endPulse);

        if (persistedOpen != startPulse)
        {
            servoDispatch.setStart(i, persistedOpen);
        }
        if (persistedClose != endPulse)
        {
            servoDispatch.setEnd(i, persistedClose);
        }
    }
}

String getConfiguredDroidName()
{
    String name = preferences.getString(PREFERENCE_DROID_NAME, AP_DROID_NAME);
    name.trim();
    if (name.length() == 0)
    {
        name = AP_DROID_NAME;
    }
    if (name.length() > 24)
    {
        name = name.substring(0, 24);
    }
    return name;
}

static bool isWakeProfileCommand(const char *cmd)
{
    return strcmp(cmd, ":SE11") == 0 || strcmp(cmd, ":SE13") == 0 || strcmp(cmd, ":SE14") == 0
        || strcmp(cmd, "#PAWU") == 0;
}

bool shouldBlockCommandDuringSleep(const char *cmd)
{
    if (!sSleepModeActive || cmd == nullptr || cmd[0] == '\0') return false;
    if (isWakeProfileCommand(cmd)) return false;
    // Emergency all-stop (:SE00) and all-close (:CL00) always pass through —
    // they must halt a grinding or runaway sequence even during sleep.
    // Individual group closes (:CL01-:CL12) remain blocked intentionally.
    if (strcmp(cmd, ":SE00") == 0 || strcmp(cmd, ":CL00") == 0) return false;
    return true;
}

static void applySoftSleepOutputs()
{
    sMarcSound.suspendRandom();
    sMarcSound.stop();

    FLD.selectSequence(LogicEngineRenderer::LIGHTSOUT);
    RLD.selectSequence(LogicEngineRenderer::LIGHTSOUT);
    frontPSI.selectSequence(LogicEngineRenderer::LIGHTSOUT);
    rearPSI.selectSequence(LogicEngineRenderer::LIGHTSOUT);

    frontHolo.selectSequence(7, 0);
    rearHolo.selectSequence(7, 0);
    topHolo.selectSequence(7, 0);
    frontHolo.off();
    rearHolo.off();
    topHolo.off();
}

static void applySoftWakeOutputs()
{
    sMarcSound.resumeRandomInSeconds(2);
    Marcduino::processCommand(player, ":SE14");
    Marcduino::processCommand(player, "@0P1");
}

bool enterSoftSleepMode(bool fromPeer)
{
    if (sSleepModeActive) return false;

    sWakeTransitionPending = false;
    sWakeTransitionAtMs = 0;
    Marcduino::processCommand(player, ":SE10");
    Marcduino::processCommand(player, "*ST00");
    FLD.selectScrollTextLeft("GOING TO SLEEP...", LogicEngineRenderer::kBlue, kStatusScrollSpeedScale, 6);
    RLD.selectScrollTextLeft("GOING TO SLEEP...", LogicEngineRenderer::kBlue, kStatusScrollSpeedScale, 6);

    sSleepModeActive = true;
    sSleepModeSinceMs = millis();
    sSleepEnforceAtMs = sSleepModeSinceMs + kSleepTransitionScrollMs;
    DEBUG_PRINTLN(F("Soft sleep mode enabled"));
    if (!fromPeer)
        sendBodyCommand("#APSL");
    return true;
}

bool exitSoftSleepMode(bool fromPeer)
{
    if (!sSleepModeActive) return false;

    sSleepEnforceAtMs = 0;
    FLD.selectScrollTextLeft("WAKING UP...", LogicEngineRenderer::kGreen, kStatusScrollSpeedScale, 6);
    RLD.selectScrollTextLeft("WAKING UP...", LogicEngineRenderer::kGreen, kStatusScrollSpeedScale, 6);
    sWakeTransitionPending = true;
    sWakeTransitionAtMs = millis() + kSleepTransitionScrollMs;

    sSleepModeActive = false;
    sSleepModeSinceMs = 0;
    DEBUG_PRINTLN(F("Soft sleep mode disabled"));
    if (!fromPeer)
        sendBodyCommand("#APWU");
    return true;
}

static void processMarcduinoCommandWithSourceMain(const char *source, const char *cmd)
{
    if (cmd == nullptr || cmd[0] == '\0') return;
    if (shouldBlockCommandDuringSleep(cmd))
    {
        Serial.printf("[CMD][%s][sleep-blocked] %s\n", source, cmd);
        return;
    }
    if (shouldDropDuplicateMoodReset(cmd))
    {
        Serial.printf("[CMD][%s][mood-duplicate-dropped] %s\n", source, cmd);
        return;
    }
    Serial.printf("[CMD][%s] %s\n", source, cmd);
    bool fromBodyLink = isBodyLinkSource(source);
    bool previousSuppressBodyLinkEgress = sSuppressBodyLinkEgress;
    if (fromBodyLink)
        sSuppressBodyLinkEgress = true;
    Marcduino::processCommand(player, cmd);
    sSuppressBodyLinkEgress = previousSuppressBodyLinkEgress;
}

////////////////

#ifdef USE_I2C_ADDRESS
I2CReceiverBase<CONSOLE_BUFFER_SIZE> i2cReceiver(USE_I2C_ADDRESS, [](char *cmd)
                                                 {
    DEBUG_PRINT(F("[I2C] RECEIVED=\""));
    DEBUG_PRINT(cmd);
    DEBUG_PRINTLN(F("\""));
    processMarcduinoCommandWithSourceMain("i2c-slave", cmd); });
#endif

////////////////

static void releasePanelServos()
{
    for (uint16_t i = 0; i < servoDispatch.getNumServos(); i++)
    {
        if (servoDispatch.getGroup(i) & sPanelReleaseMask)
        {
            uint8_t pin = servoDispatch.getPin(i);
#ifndef USE_I2C_ADDRESS
            // Write full-off to the PCA9685 channel so the motor is actually
            // de-energised. disable() alone only clears firmware state; the
            // chip keeps driving the last PWM value until told otherwise.
            if (pin != 0)
                servoDispatch.setOutput(pin, false);
#endif
            servoDispatch.disable(i);
        }
    }
    sPanelReleaseMask = 0;
    DEBUG_PRINTLN(F("[Panel] Auto-release: servo PWM cut after close"));
}

static void schedulePanelRelease(uint32_t mask, uint32_t delayMs)
{
    sPanelReleaseMask |= mask;
    uint32_t newDeadline = millis() + delayMs;
    // Never shorten an existing deadline — a slow-close in progress needs its full time.
    if (sPanelReleaseAtMs == 0 || (int32_t)(newDeadline - sPanelReleaseAtMs) > 0)
        sPanelReleaseAtMs = newDeadline;
}

static void cancelPanelRelease(uint32_t mask)
{
    sPanelReleaseMask &= ~mask;
    if (sPanelReleaseMask == 0)
        sPanelReleaseAtMs = 0;
}

////////////////

void mainLoop()
{
    AnimatedEvent::process();

    handleBodySerial();
    bodyLinkWiFiRx();
    handleBodyLinkHeartbeat();

    if (sSleepModeActive && sSleepEnforceAtMs != 0 && (int32_t)(millis() - sSleepEnforceAtMs) >= 0)
    {
        applySoftSleepOutputs();
        sSleepEnforceAtMs = 0;
    }

    if (sPanelReleaseAtMs != 0 && (int32_t)(millis() - sPanelReleaseAtMs) >= 0)
    {
        releasePanelServos();
        sPanelReleaseAtMs = 0;
    }

    // Advance the holo wiring-test sweep state machine. Cheap when idle —
    // returns immediately if sHoloSweepActive is false. See AsyncWebInterface.h.
    holoSweepPoll();

    if (sWakeTransitionPending && sWakeTransitionAtMs != 0 && (int32_t)(millis() - sWakeTransitionAtMs) >= 0)
    {
        applySoftWakeOutputs();
        sWakeTransitionPending = false;
        sWakeTransitionAtMs = 0;
    }

    uint32_t freeHeapNow = ESP.getFreeHeap();
    if (sMinFreeHeap == 0 || freeHeapNow < sMinFreeHeap)
    {
        sMinFreeHeap = freeHeapNow;
    }

    if (sSoundInitPending && (int32_t)(millis() - sSoundInitAtMs) >= 0)
    {
        if (sMarcSound.begin(sSoundInitModule, SOUND_SERIAL, sSoundInitStartup))
        {
            sMarcSound.setVolume(sSoundInitVolume);
            sMarcSound.playStartSound();
            sMarcSound.setRandomMin(preferences.getInt(PREFERENCE_MARCSOUND_RANDOM_MIN, MARC_SOUND_RANDOM_MIN));
            sMarcSound.setRandomMax(preferences.getInt(PREFERENCE_MARCSOUND_RANDOM_MAX, MARC_SOUND_RANDOM_MAX));
            if (preferences.getInt(PREFERENCE_MARCSOUND_RANDOM, MARC_SOUND_RANDOM))
                sMarcSound.startRandomInSeconds(13);
            sSoundInitPending = false;
            DEBUG_PRINTLN(F("Sound module initialized (deferred)"));
        }
        else
        {
            sSoundInitAttempts++;
            if (sSoundInitAttempts >= 5)
            {
                sSoundInitPending = false;
                DEBUG_PRINTLN(F("FAILED TO INITALIZE SOUND MODULE"));
            }
            else
            {
                sSoundInitAtMs = millis() + 1000;
            }
        }
    }

    sMarcSound.idle();
#ifdef USE_MENUS
    sDisplay.process();
#endif


    if (Serial.available())
    {

        int ch = Serial.read();
        // ================================================================
        if (preferences.getBool(PREFERENCE_MARCSERIAL_PASS, MARC_SERIAL_PASS))
        {
            COMMAND_SERIAL.write(ch); // send it out COMMAND_SERIAL
        }
        // ================================================================
        if (ch == 0x0A || ch == 0x0D)
        {
            processMarcduinoCommandWithSourceMain("usb-serial", sBuffer);
            sPos = 0;
        }
        else if (sPos < SizeOfArray(sBuffer) - 1)
        {
            sBuffer[sPos++] = ch;
            sBuffer[sPos] = '\0';
        }
    }
}
////////////////

#ifdef USE_WIFI
void eventLoopTask(void *)
{
    for (;;)
    {
        if (wifiActive)
        {
#ifdef USE_OTA
            ArduinoOTA.handle();
#endif
#ifdef USE_WIFI_WEB
            asyncWebLoop();
#endif
            bodyLinkResolvePeer();
        }
        if (remoteActive)
        {
#ifdef USE_SMQ
            SMQ::process();
#endif
        }
#ifdef USE_LVGL_DISPLAY
        statusDisplay.refresh();
#endif
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif

////////////////

void loop()
{
    mainLoop();
}
