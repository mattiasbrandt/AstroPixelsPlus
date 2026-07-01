#ifndef ASYNC_WEB_INTERFACE_H
#define ASYNC_WEB_INTERFACE_H

// AsyncWebInterface.h — Replaces WebPages.h
// Registers ESPAsyncWebServer routes, REST API, WebSocket, and OTA upload.
// All ReelTwo core (Marcduino, ServoDispatch, LogicEngine, etc.) stays unchanged.

#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <Wire.h>
#include <WiFi.h>
#include <ctype.h>
#include <string.h>
#include "SPIFFS.h"
#include "LogCapture.h"
#include "WiringConfig.h"
#include "GeneratedDomeLayout.h"
#include "DomeElementStatus.h"
#include "DomeLayoutTemplateStore.h"

// Gadget includes for extern declarations
#if AP_ENABLE_FIRESTRIP
#include "dome/FireStrip.h"
#endif
#if AP_ENABLE_BADMOTIVATOR
#include "dome/BadMotivator.h"
#endif
#if AP_ENABLE_CBI
#include "body/ChargeBayIndicator.h"
#endif
#if AP_ENABLE_DATAPANEL
#include "body/DataPanel.h"
#endif

// Forward declarations — these are defined in AstroPixelsPlus.ino
extern void reboot();
extern void unmountFileSystems();

// Globals defined in .ino that we need access to
extern AnimationPlayer player;
extern Preferences preferences;
extern bool wifiEnabled;
extern bool remoteEnabled;
extern bool otaInProgress;
extern bool soundLocalEnabled;
extern bool sSleepModeActive;
extern uint32_t sSleepModeSinceMs;
extern uint32_t sMinFreeHeap;
extern uint32_t sBodyLastSeenMs;
extern uint32_t sBodyHeartbeatRx;
extern uint32_t sBodyLastTxMs;
extern bool bodyLinkConnected();
extern BodyLinkTransport bodyLinkActiveTransport();
extern const char *bodyLinkGetTransportName();
extern bool bodyLinkWifiEnabled();
extern String bodyLinkGetPeerIP();
extern const char *bodyLinkGetPeerSource();
extern uint32_t bodyLinkUartHeartbeatAgeMs();
extern uint32_t bodyLinkWifiHeartbeatAgeMs();
extern bool shouldBlockCommandDuringSleep(const char *cmd);
extern bool enterSoftSleepMode(bool fromPeer = false);
extern bool exitSoftSleepMode(bool fromPeer = false);
extern String getConfiguredDroidName();

// Gadget preference constants (must match AstroPixelsPlus.ino)
#define PREFERENCE_BADMOTIVATOR_ENABLED "badmot"
#define PREFERENCE_FIRESTRIP_ENABLED "firest"
#define PREFERENCE_CBI_ENABLED "cbienb"
#define PREFERENCE_DATAPANEL_ENABLED "dpenab"

#ifdef USE_DROID_REMOTE
extern bool sRemoteConnected;
#endif

// Gadget extern declarations
#if AP_ENABLE_FIRESTRIP
extern FireStrip fireStrip;
#endif
#if AP_ENABLE_BADMOTIVATOR
extern BadMotivator badMotivator;
#endif
#if AP_ENABLE_CBI
extern ChargeBayIndicator chargeBayIndicator;
#endif
#if AP_ENABLE_DATAPANEL
extern DataPanel dataPanel;
#endif

// ESPAsyncWebServer objects — allocated once, no static init heap issues
static AsyncWebServer asyncServer(80);
static AsyncWebSocket ws("/ws");

// Log capture — wraps Serial, tees output to ring buffer for WS broadcast
// logCapture instance is declared in AstroPixelsPlus.ino (before panelConfigLoad)
// so boot-time wiring-config logs reach the same ring buffer as runtime API logs.
// Same translation unit — no extern needed.
static uint32_t lastLogCount = 0;

// Broadcast timers
static uint32_t lastStateBroadcast = 0;
static uint32_t lastHealthBroadcast = 0;
static bool rebootScheduled = false;
static uint32_t rebootAtMs = 0;
static bool otaUploadFailed = false;
static int otaUploadHttpStatus = 500;
static String otaUploadError;
static uint32_t lastI2CScanMs = 0;
static bool cachedPanelsOk = false;
static bool cachedHolosOk = false;
static uint8_t cachedPanelsCode = 255;
static uint8_t cachedHolosCode = 255;
static uint32_t cachedPanelsLastOkMs = 0;
static uint32_t cachedPanelsLastFailMs = 0;
static uint32_t cachedHolosLastOkMs = 0;
static uint32_t cachedHolosLastFailMs = 0;
static uint32_t cachedPanelsConsecutiveFailures = 0;
static uint32_t cachedHolosConsecutiveFailures = 0;
static uint32_t cachedI2CScanDurationUs = 0;
static uint32_t cachedI2CDeviceCount = 0;
static uint32_t lastI2CDeepScanMs = 0;
static bool cachedLastScanWasDeep = false;
static uint32_t i2cCodeHistogram[6] = {0, 0, 0, 0, 0, 0};
static String cachedI2CDevicesJson = "[]";
static uint32_t i2cProbeFailures = 0;

// Forward declarations
static void broadcastState();
static void broadcastOtaProgress(float progress);

static String otaJson(bool ok, const String &error = "")
{
    if (ok)
        return "{\"ok\":true}";
    return "{\"ok\":false,\"error\":\"" + error + "\"}";
}

static void markOtaUploadFailed(int status, const char *error)
{
    otaUploadFailed = true;
    otaUploadHttpStatus = status;
    otaUploadError = error;
}

static bool isAsciiPrintable(char c)
{
    return c >= 32 && c <= 126;
}

static bool isValidCommandString(const String &cmd)
{
    if (cmd.length() == 0 || cmd.length() > 63) return false;
    for (size_t i = 0; i < cmd.length(); i++)
    {
        if (!isAsciiPrintable(cmd[i])) return false;
    }
    return true;
}

static bool parseWsCommand(const uint8_t *data, size_t len, char *out, size_t outSize)
{
    if (len == 0 || len >= outSize) return false;
    for (size_t i = 0; i < len; i++)
    {
        char c = (char)data[i];
        if (!isAsciiPrintable(c)) return false;
    }
    memcpy(out, data, len);
    out[len] = '\0'; // Ensure null termination
    return true;
}

static bool parseDurationSeconds(const String &raw, uint8_t minValue, uint8_t maxValue, uint8_t &out)
{
    String value = raw;
    value.trim();
    if (value.length() == 0 || value.length() > 2) return false;
    for (size_t i = 0; i < value.length(); i++)
    {
        if (!isdigit((unsigned char)value[i])) return false;
    }
    long parsed = value.toInt();
    if (parsed < minValue || parsed > maxValue) return false;
    out = (uint8_t)parsed;
    return true;
}

// Read a required POST string param, trim it, write to out.
// Sends 400 and returns false if the param is absent.
static bool parsePostParam(AsyncWebServerRequest *request, const char *name, String &out)
{
    if (!request->hasParam(name, true))
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "{\"error\":\"missing %s param\"}", name);
        request->send(400, "application/json", msg);
        return false;
    }
    out = request->getParam(name, true)->value();
    out.trim();
    return true;
}

// Validate a Marcduino command string. Sends 400 and returns false if invalid.
static bool validateMarcduinoCmd(AsyncWebServerRequest *request, const String &cmd)
{
    if (!isValidCommandString(cmd))
    {
        request->send(400, "application/json", "{\"error\":\"invalid cmd\"}");
        return false;
    }
    return true;
}

// Return true (and send 423) if the command should be blocked while in sleep mode.
static bool guardSleep(AsyncWebServerRequest *request, const char *cmd)
{
    if (shouldBlockCommandDuringSleep(cmd))
    {
        request->send(423, "application/json", "{\"error\":\"sleeping\",\"hint\":\"POST /api/wake\"}");
        return true;
    }
    return false;
}

static void processMarcduinoCommandWithSource(const char *source, const char *cmd)
{
    if (cmd == nullptr || cmd[0] == '\0') return;
    if (shouldBlockCommandDuringSleep(cmd))
    {
        logCapture.printf("[CMD][%s][sleep-blocked] %s\n", source, cmd);
        return;
    }
    logCapture.printf("[CMD][%s] %s\n", source, cmd);
    if (handleImmediateServoMoveCommand(source, cmd))
        return;
    if (applyDomeVisualPresetCommand(source, cmd))
        return;
    if (applyDomeVisualAuthoringCommand(source, cmd))
        return;

    // Panel calibration commands are handled here synchronously rather than
    // via MARCDUINO_ACTION. The reason: MARCDUINO_ACTION wraps the body in
    // DO_ONCE() inside animateOnce(), which defers execution to the next
    // AnimatedEvent::process() loop iteration. By that point the incoming
    // cmd buffer (a temporary String in the async web handler) has been
    // freed, so getCommand() inside the macro body returns a dangling
    // pointer and all parsing silently fails. Handling them here while cmd
    // is still on the stack avoids the issue entirely.
    // MarcduinoPanel.h now also implements these commands for stable command
    // buffers (e.g. MarcduinoSerial fBuffer), preserving serial parity while
    // this synchronous path keeps async-web ingress safe.

    // :MV<pp><vvvv> — move panel <pp> to pulse width <vvvv> us (temporary, not saved)
    if (strncmp(cmd, ":MV", 3) == 0)
    {
        const char *args = cmd + 3;          // skip ":MV", args = "<pp><vvvv>"
        uint8_t tgt = 0; uint16_t val = 0; uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
        {
            if (!movePanelMaskToValue(msk, val))
                logCapture.println("[PANEL CAL] :MV unsupported target/value for this build");
        }
        else { logCapture.println("[PANEL CAL] Invalid :MV command"); }
        return;
    }
    // #SO<pp><vvvv> — save open position for panel <pp> as pulse width <vvvv> us
    if (strncmp(cmd, "#SO", 3) == 0)
    {
        const char *args = cmd + 3;
        uint8_t tgt = 0; uint16_t val = 0; uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
        {
            if (!applyPanelCalibrationToMask(msk, true, false, val))
                logCapture.println("[PANEL CAL] #SO unsupported target/value for this build");
        }
        else { logCapture.println("[PANEL CAL] Invalid #SO command"); }
        return;
    }
    // #SC<pp><vvvv> — save closed position for panel <pp> as pulse width <vvvv> us
    if (strncmp(cmd, "#SC", 3) == 0)
    {
        const char *args = cmd + 3;
        uint8_t tgt = 0; uint16_t val = 0; uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
        {
            if (!applyPanelCalibrationToMask(msk, false, true, val))
                logCapture.println("[PANEL CAL] #SC unsupported target/value for this build");
        }
        else { logCapture.println("[PANEL CAL] Invalid #SC command"); }
        return;
    }
    // #SW<pp> — swap open/closed calibration values for panel <pp>
    if (strncmp(cmd, "#SW", 3) == 0)
    {
        const char *args = cmd + 3;
        uint8_t tgt = 0; uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && panelTargetToMask(tgt, msk))
        {
            if (!swapPanelCalibrationInMask(msk))
                logCapture.println("[PANEL CAL] #SW unsupported target for this build");
        }
        else { logCapture.println("[PANEL CAL] Invalid #SW command"); }
        return;
    }

    enqueueMarcduinoCommand(source, cmd);
}

static bool isSensitivePrefKey(const String &key)
{
    return key == "pass" || key == "rsecret";
}

static bool isAllowedPrefKey(const String &key)
{
    return key == "wifi" || key == "ap" || key == "ssid" || key == "pass" ||
           key == "remote" || key == "rhost" || key == "rsecret" ||
           key == "mserial2" || key == "mserialpass" || key == "mserial" ||
           key == "mwifi" || key == "mwifipass" ||
           key == "msound" || key == "msoundser" || key == "mvolume" ||
           key == "msoundstart" || key == "mrandom" || key == "mrandommin" ||
           key == "mrandommax" || key == "msoundlocal" ||
           key == "dname" ||
           key == "mbodylink" || key == "mbodywifi" || key == "bodypeerip" ||
           key == PREFERENCE_BADMOTIVATOR_ENABLED || key == PREFERENCE_FIRESTRIP_ENABLED ||
           key == PREFERENCE_CBI_ENABLED || key == PREFERENCE_DATAPANEL_ENABLED ||
           key == "holo_boot_loop" || key == "dm_happy_sound";
}

static size_t maxPrefValueLen(const String &key)
{
    if (key == "ssid" || key == "rhost") return 32;
    if (key == "pass" || key == "rsecret") return 64;
    if (key == "dname") return 24;
    if (key == "bodypeerip") return 15;
    return 16;
}

static bool defaultBoolForPrefKey(const String &key)
{
    if (key == "wifi") return WIFI_ENABLED;
    if (key == "ap") return WIFI_ACCESS_POINT;
    if (key == "remote") return REMOTE_ENABLED;
    if (key == "msoundlocal") return MARC_SOUND_LOCAL_ENABLED;
    if (key == "mserialpass") return MARC_SERIAL_PASS;
    if (key == "mserial") return MARC_SERIAL_ENABLED;
    if (key == "mwifi") return MARC_WIFI_ENABLED;
    if (key == "mwifipass") return MARC_WIFI_SERIAL_PASS;
    if (key == "mbodylink") return BODY_LINK_ENABLED;
    if (key == "mbodywifi") return BODY_WIFI_ENABLED;
    if (key == PREFERENCE_BADMOTIVATOR_ENABLED) return AP_ENABLE_BADMOTIVATOR;
    if (key == PREFERENCE_FIRESTRIP_ENABLED) return AP_ENABLE_FIRESTRIP;
    if (key == PREFERENCE_CBI_ENABLED) return AP_ENABLE_CBI;
    if (key == PREFERENCE_DATAPANEL_ENABLED) return AP_ENABLE_DATAPANEL;
    if (key == "holo_boot_loop") return true;
    if (key == "dm_happy_sound") return true;
    return false;
}

static bool isIntegerPrefKey(const String &key)
{
    return key == "mserial2" || key == "msound" || key == "msoundser" ||
           key == "mvolume" || key == "msoundstart" || key == "mrandom" ||
           key == "mrandommin" || key == "mrandommax";
}

static int defaultIntForPrefKey(const String &key)
{
    if (key == "mserial2") return MARC_SERIAL2_BAUD_RATE;
    if (key == "msound") return MARC_SOUND_PLAYER;
    if (key == "msoundser") return MARC_SOUND_SERIAL;
    if (key == "mvolume") return MARC_SOUND_VOLUME;
    if (key == "msoundstart") return MARC_SOUND_STARTUP;
    if (key == "mrandom") return MARC_SOUND_RANDOM;
    if (key == "mrandommin") return MARC_SOUND_RANDOM_MIN;
    if (key == "mrandommax") return MARC_SOUND_RANDOM_MAX;
    return 0;
}

static bool parseIntegerPrefValue(const String &val, int &out)
{
    if (val.length() == 0) return false;
    for (size_t i = 0; i < val.length(); i++)
    {
        if (!isdigit((unsigned char)val[i])) return false;
    }
    out = val.toInt();
    return true;
}

static String jsonEscape(const String &in)
{
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++)
    {
        char c = in[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 0x20)
        {
            const char *hex = "0123456789ABCDEF";
            out += "\\u00";
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
        else out += c;
    }
    return out;
}

// ---------------------------------------------------------------
// WebSocket event handler
// ---------------------------------------------------------------
static String buildStateJson();

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        logCapture.printf("[WS] Client #%u connected from %s\n", client->id(),
                         client->remoteIP().toString().c_str());
        client->text("{\"type\":\"state\",\"data\":" + buildStateJson() + "}");
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        logCapture.printf("[WS] Client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
        {
            char cmd[64];
            if (parseWsCommand(data, len, cmd, sizeof(cmd)))
            {
                processMarcduinoCommandWithSource("astropixel-web-ws", cmd);
                broadcastState();
            }
        }
    }
}

// ---------------------------------------------------------------
// Build state JSON string
// ---------------------------------------------------------------
static String buildStateJson()
{

    String json = "{";
    json.reserve(512);
    String droidName = getConfiguredDroidName();
    json += "\"wifiEnabled\":" + String(wifiEnabled ? "true" : "false");
    json += ",\"remoteEnabled\":" + String(remoteEnabled ? "true" : "false");
    json += ",\"soundLocalEnabled\":" + String(soundLocalEnabled ? "true" : "false");
    json += ",\"sleepMode\":" + String(sSleepModeActive ? "true" : "false");
    json += ",\"sleepSinceMs\":" + String(sSleepModeSinceMs);
    json += ",\"mood\":{\"command\":\"" + String(sCurrentMoodCmd) + "\"";
    json += ",\"name\":\"" + String(currentMoodName()) + "\"}";
    int soundPref = preferences.getInt("msound", MARC_SOUND_PLAYER);
    bool soundModuleEnabled = (soundLocalEnabled && soundPref != 0);
    json += ",\"soundModuleEnabled\":" + String(soundModuleEnabled ? "true" : "false");
#ifdef USE_DROID_REMOTE
    json += ",\"remoteConnected\":" + String(sRemoteConnected ? "true" : "false");
    json += ",\"remoteSupported\":true";
#else
    json += ",\"remoteConnected\":false";
    json += ",\"remoteSupported\":false";
#endif
    json += ",\"otaInProgress\":" + String(otaInProgress ? "true" : "false");
    json += ",\"uptime\":" + String(millis() / 1000);
    json += ",\"freeHeap\":" + String(ESP.getFreeHeap());
    json += ",\"minFreeHeap\":" + String(sMinFreeHeap);
    json += ",\"i2c_probe_failures\":" + String(i2cProbeFailures);
    // Body link status (for real-time WebSocket updates)
    bool bodyLinkPrefEnabled = preferences.getBool("mbodylink", BODY_LINK_ENABLED);
    json += ",\"body_link\":{\"enabled\":" + String(bodyLinkPrefEnabled ? "true" : "false");
    json += ",\"connected\":" + String(bodyLinkConnected() ? "true" : "false");
    json += ",\"transport\":\"" + String(bodyLinkGetTransportName()) + "\"";
    json += ",\"wifi_enabled\":" + String(bodyLinkWifiEnabled() ? "true" : "false");
    json += ",\"peer_ip\":\"" + jsonEscape(bodyLinkGetPeerIP()) + "\"}";
    json += ",\"droidName\":\"" + jsonEscape(droidName) + "\"";

    // WiFi details
    json += ",\"wifiAP\":" + String((WiFi.getMode() & WIFI_MODE_AP) ? "true" : "false");
    if (WiFi.getMode() & WIFI_MODE_STA)
    {
        json += ",\"wifiIP\":\"" + WiFi.localIP().toString() + "\"";
        json += ",\"wifiRSSI\":" + String(WiFi.RSSI());
    }
    else
    {
        json += ",\"wifiIP\":\"" + WiFi.softAPIP().toString() + "\"";
        json += ",\"wifiRSSI\":0";
    }

    json += "}";
    return json;
}

// ---------------------------------------------------------------
// Dynamic wiring config — JSON helpers, validation, NVS save
//
// Pulls per-slot channel assignments from NVS namespace "panels" / "holos",
// falling back to the MK4 defaults in AstroPixelsPlus.ino. Used by the
// GET/POST /api/{panels,holos}/config endpoints. NVS reads here mirror the
// firmware-side panelConfigLoad() / holoConfigLoad() but DO NOT touch the
// servoDispatch — saving via POST requires a reboot to take effect (returned
// as reboot_required:true). See ADR 0001 / 0002 / 0004 for context.
// ---------------------------------------------------------------

// Panel labels for slot indexes 0–12 (MK4 printed-droid identifiers).
// Slot 7 (PP5) and slot 12 (PP3) are unserviced on standard MK4 but their
// identities are surfaced so a builder who wires a servo can activate them.
static const char *panelSlotLabel(int slot)
{
    return (slot >= 0 && slot < NUM_PANEL_SLOTS) ? kPanelSlotLabels[slot] : "?";
}

// Marcduino open command per slot. Slots 7 (PP5) and 12 (PP3) are unserviced
// on a standard MK4 but still carry a command so a builder who wires a servo
// can activate them. Pie panels use the :OPP* fork namespace (ADR 0007).
// Ring panels P7, P11, P13 use their identity number, not the old slot index.
// MUST STAY IN SYNC WITH servoSettings[] in AstroPixelsPlus.ino (ADR 0006 slot
// order). Slot 0-6 = P1/P2/P3/P4/P7/P11/P13; slot 7-12 = PP5/PP1/PP2/PP4/PP6/PP3.
static const char *panelSlotCommand(int slot)
{
    static const char *cmds[NUM_PANEL_SLOTS] = {
        //  P1       P2       P3       P4       P7       P11      P13
        ":OP01", ":OP02", ":OP03", ":OP04", ":OP07", ":OP11", ":OP13",
        //  PP5      PP1      PP2      PP4      PP6      PP3
        ":OPP5", ":OPP1", ":OPP2", ":OPP4", ":OPP6", ":OPP3",
    };
    return (slot >= 0 && slot < NUM_PANEL_SLOTS) ? cmds[slot] : "";
}

// Fixed holo labels — "<projector> (<community equivalent>) — <axis>".
static const char *holoSlotLabel(int slot)
{
    static const char *labels[NUM_HOLO_SLOTS] = {
        "FHP (HP1) — H", "FHP (HP1) — V",
        "THP (HP3) — H", "THP (HP3) — V",
        "RHP (HP2) — V", "RHP (HP2) — H",
    };
    return (slot >= 0 && slot < NUM_HOLO_SLOTS) ? labels[slot] : "?";
}


// Build the GET response JSON for a wiring-config endpoint. Caller supplies
// slot labels and (optionally) command strings — pass labelFn=nullptr to omit
// the "label" field, cmdFn=nullptr to omit the "cmd" field.
static String wiringConfigBuildJson(const char *board, int slotCount,
                                     const uint8_t *channels, const bool *actives,
                                     const char *(*labelFn)(int),
                                     const char *(*cmdFn)(int))
{
    String json = "{";
    json.reserve(64 + slotCount * 80);
    json += "\"board\":\"";
    json += board;
    json += "\",\"slot_count\":";
    json += slotCount;
    json += ",\"slots\":[";
    for (int i = 0; i < slotCount; i++)
    {
        if (i > 0) json += ',';
        json += "{\"index\":";
        json += i;
        if (labelFn)
        {
            json += ",\"label\":\"";
            json += jsonEscape(labelFn(i));
            json += '"';
        }
        json += ",\"channel\":";
        json += channels[i];
        json += ",\"active\":";
        json += actives[i] ? "true" : "false";
        if (cmdFn)
        {
            json += ",\"cmd\":\"";
            json += jsonEscape(cmdFn(i));
            json += '"';
        }
        json += '}';
    }
    json += "]}";
    return json;
}

static int domeLayoutPanelSlotForId(const char *id)
{
    if (!id) return -1;
    for (int i = 0; i < NUM_PANEL_SLOTS; i++)
    {
        if (strcmp(id, kPanelSlotLabels[i]) == 0) return i;
    }
    return -1;
}

static bool domeLayoutPanelActive(const char *id)
{
    int slot = domeLayoutPanelSlotForId(id);
    if (slot < 0) return false;
    return servoDispatch.getPin(slot) != 0 && servoDispatch.getGroup(slot) != 0;
}

static void domeLayoutAppendStringArray(String &json, const char *field,
                                        const char *const *values, size_t count)
{
    json += ",\"";
    json += field;
    json += "\":[";
    for (size_t i = 0; i < count; i++)
    {
        if (i > 0) json += ',';
        json += '"';
        json += jsonEscape(String(values[i]));
        json += '"';
    }
    json += ']';
}

static void domeLayoutAppendPoint(String &json, const char *field,
                                  const DomeLayout::DomeLayoutPoint &point)
{
    if (!point.present) return;
    json += ",\"";
    json += field;
    json += "\":{\"x\":";
    json += String(point.x, 1);
    json += ",\"y\":";
    json += String(point.y, 1);
    json += '}';
}

static void domeLayoutAppendGeometry(String &json,
                                     const DomeLayout::DomeLayoutElement &element)
{
    if (!element.inLayout) return;
    json += ",\"geometry\":{\"type\":\"";
    switch (element.geometryType)
    {
        case DomeLayout::DomeLayoutGeometryType::SvgPath:
            json += "svg_path\",\"d\":\"";
            json += jsonEscape(String(element.svgPath ? element.svgPath : ""));
            json += '"';
            break;
        case DomeLayout::DomeLayoutGeometryType::Circle:
            json += "circle\",\"cx\":";
            json += String(element.cx, 1);
            json += ",\"cy\":";
            json += String(element.cy, 1);
            json += ",\"r\":";
            json += String(element.r, 1);
            break;
        case DomeLayout::DomeLayoutGeometryType::Ellipse:
            json += "ellipse\",\"cx\":";
            json += String(element.cx, 1);
            json += ",\"cy\":";
            json += String(element.cy, 1);
            json += ",\"rx\":";
            json += String(element.rx, 1);
            json += ",\"ry\":";
            json += String(element.ry, 1);
            json += ",\"rotation\":";
            json += String(element.rotation, 1);
            break;
        case DomeLayout::DomeLayoutGeometryType::Point:
            json += "point\",\"cx\":";
            json += String(element.cx, 1);
            json += ",\"cy\":";
            json += String(element.cy, 1);
            json += ",\"r\":";
            json += String(element.r, 1);
            break;
    }
    json += '}';
}

static void domeLayoutAppendCallout(String &json,
                                    const DomeLayout::DomeLayoutCallout &callout)
{
    if (!callout.present) return;
    json += ",\"callout\":{\"x\":";
    json += String(callout.x, 1);
    json += ",\"y\":";
    json += String(callout.y, 1);
    json += ",\"r\":";
    json += String(callout.r, 1);
    if (callout.connectorPresent)
    {
        json += ",\"connector_to\":{\"x\":";
        json += String(callout.connectorX, 1);
        json += ",\"y\":";
        json += String(callout.connectorY, 1);
        json += '}';
    }
    json += '}';
}

static bool domeLayoutReadStatusForId(const String &id,
                                      const DomeElementStatusSnapshot *statuses,
                                      bool statusOk,
                                      bool &disabled,
                                      String &reason)
{
    disabled = false;
    reason = "";
    if (!statusOk) return false;
    int index = domeElementStatusIndexOf(id);
    if (index < 0 || index >= DOME_ELEMENT_STATUS_MAX_ELEMENTS) return false;
    disabled = statuses[index].disabled;
    reason = statuses[index].reason;
    return true;
}

static bool domeLayoutTemplateFindElementFieldBool(const String &objectJson,
                                                   const char *wantedKey,
                                                   bool &out)
{
    String needle = String("\"") + wantedKey + "\"";
    int keyAt = objectJson.indexOf(needle);
    if (keyAt < 0) return false;
    int colonAt = objectJson.indexOf(':', keyAt + needle.length());
    if (colonAt < 0) return false;
    const char *p = objectJson.c_str() + colonAt + 1;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (strncmp(p, "true", 4) == 0)
    {
        out = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0)
    {
        out = false;
        return true;
    }
    return false;
}

static int domeLayoutTemplateFindElementsKey(const String &json)
{
    const char *p = json.c_str();
    String errMsg;
    if (!domeLayoutTemplateExpectChar(p, '{', errMsg)) return -1;
    while (true)
    {
        domeLayoutTemplateSkipWs(p);
        if (*p == '}') return -1;
        const char *keyStart = p;
        String key;
        if (!domeLayoutTemplateParseJsonString(p, key, errMsg)) return -1;
        if (!domeLayoutTemplateExpectChar(p, ':', errMsg)) return -1;
        if (key == "elements") return keyStart - json.c_str();
        if (!domeLayoutTemplateSkipJsonValue(p, errMsg)) return -1;
        domeLayoutTemplateSkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') return -1;
        return -1;
    }
}

static bool domeLayoutTemplateFindArrayEnd(const String &json, int arrayStart,
                                           int &arrayEnd)
{
    if (arrayStart < 0 || arrayStart >= (int)json.length() ||
        json[arrayStart] != '[') return false;
    const char *p = json.c_str() + arrayStart;
    String errMsg;
    if (!domeLayoutTemplateSkipJsonArray(p, errMsg)) return false;
    arrayEnd = p - json.c_str();
    return true;
}

static bool domeLayoutTemplateAppendComposedElement(String &out,
                                                    const String &elementJson,
                                                    const DomeElementStatusSnapshot *statuses,
                                                    bool statusOk)
{
    String errMsg;
    String id;
    if (!domeLayoutTemplateFindRootString(elementJson, "id", id, errMsg)) return false;
    bool commandable = false;
    domeLayoutTemplateFindElementFieldBool(elementJson, "commandable", commandable);

    int closeAt = elementJson.lastIndexOf('}');
    if (closeAt < 0) return false;
    out += elementJson.substring(0, closeAt);
    if (commandable && domeLayoutTemplateIsCommandableId(id))
    {
        out += ",\"active\":";
        out += domeLayoutPanelActive(id.c_str()) ? "true" : "false";
    }

    bool disabled = false;
    String reason;
    domeLayoutReadStatusForId(id, statuses, statusOk, disabled, reason);
    out += ",\"disabled\":";
    out += disabled ? "true" : "false";
    out += ",\"disabled_reason\":";
    if (disabled && reason.length() > 0)
    {
        out += '"';
        out += jsonEscape(reason);
        out += '"';
    }
    else
    {
        out += "null";
    }
    out += '}';
    return true;
}

static bool domeLayoutBuildCustomLayoutJson(const String &templateJson,
                                            String &out,
                                            String &errMsg)
{
    DomeLayoutTemplateInfo info = {};
    if (!domeLayoutTemplateValidateJson(templateJson, info, errMsg)) return false;

    int keyAt = domeLayoutTemplateFindElementsKey(templateJson);
    if (keyAt < 0)
    {
        errMsg = "custom template missing elements array";
        return false;
    }
    int colonAt = templateJson.indexOf(':', keyAt);
    if (colonAt < 0)
    {
        errMsg = "custom template elements array is malformed";
        return false;
    }
    int arrayStart = colonAt + 1;
    while (arrayStart < (int)templateJson.length() &&
           isspace((unsigned char)templateJson[arrayStart])) arrayStart++;
    int arrayEnd = -1;
    if (!domeLayoutTemplateFindArrayEnd(templateJson, arrayStart, arrayEnd))
    {
        errMsg = "custom template elements array is malformed";
        return false;
    }

    DomeElementStatusSnapshot statuses[DOME_ELEMENT_STATUS_MAX_ELEMENTS];
    bool statusOk = domeElementStatusReadAll(statuses, DOME_ELEMENT_STATUS_MAX_ELEMENTS);

    out = "";
    out.reserve(templateJson.length() + 2048);
    out += templateJson.substring(0, keyAt);
    out += "\"layout_source\":\"custom\",\"runtime_state_ts\":";
    out += millis();
    out += ",\"elements\":[";

    const char *p = templateJson.c_str() + arrayStart + 1;
    bool first = true;
    while (true)
    {
        domeLayoutTemplateSkipWs(p);
        if (*p == ']') break;
        const char *elementStart = p;
        if (!domeLayoutTemplateSkipJsonObject(p, errMsg)) return false;
        String elementJson = templateJson.substring(elementStart - templateJson.c_str(),
                                                    p - templateJson.c_str());
        if (!first) out += ',';
        if (!domeLayoutTemplateAppendComposedElement(out, elementJson,
                                                     statuses, statusOk))
        {
            errMsg = "custom template element composition failed";
            return false;
        }
        first = false;
        domeLayoutTemplateSkipWs(p);
        if (*p == ',')
        {
            p++;
            continue;
        }
        if (*p == ']') break;
        errMsg = "expected ',' or ']' in custom elements array";
        return false;
    }

    out += "]";
    out += templateJson.substring(arrayEnd);
    return true;
}

static String buildBundledDomeLayoutJson()
{
    DomeElementStatusSnapshot statuses[DOME_ELEMENT_STATUS_MAX_ELEMENTS];
    bool statusOk = domeElementStatusReadAll(statuses, DOME_ELEMENT_STATUS_MAX_ELEMENTS);

    String json = "{";
    json.reserve(2048 + (DomeLayout::kElementCount * 360));
    json += "\"schema_revision\":";
    json += DomeLayout::kSchemaRevision;
    json += ",\"template_id\":\"";
    json += jsonEscape(String(DomeLayout::kTemplateId));
    json += "\",\"template_name\":\"";
    json += jsonEscape(String(DomeLayout::kTemplateName));
    json += "\",\"template_revision\":";
    json += DomeLayout::kTemplateRevision;
    json += ",\"model\":\"";
    json += jsonEscape(String(DomeLayout::kModel));
    json += "\",\"source\":\"";
    json += jsonEscape(String(DomeLayout::kSource));
    json += "\",\"layout_source\":\"bundled\",\"coordinate_space\":{\"viewBox\":\"";
    json += jsonEscape(String(DomeLayout::kCoordinateSpaceViewBox));
    json += "\"},\"runtime_state_ts\":";
    json += millis();
    json += ",\"elements\":[";

    for (size_t i = 0; i < DomeLayout::kElementCount; i++)
    {
        const DomeLayout::DomeLayoutElement &element = DomeLayout::kElements[i];
        if (i > 0) json += ',';
        json += "{\"id\":\"";
        json += jsonEscape(String(element.id));
        json += "\",\"label\":\"";
        json += jsonEscape(String(element.label));
        json += "\",\"element_type\":\"";
        json += jsonEscape(String(element.elementType));
        json += "\",\"panel_kind\":";
        if (element.panelKind)
        {
            json += '"';
            json += jsonEscape(String(element.panelKind));
            json += '"';
        }
        else
        {
            json += "null";
        }
        json += ",\"mounted_on\":";
        if (element.mountedOn)
        {
            json += '"';
            json += jsonEscape(String(element.mountedOn));
            json += '"';
        }
        else
        {
            json += "null";
        }
        json += ",\"in_layout\":";
        json += element.inLayout ? "true" : "false";
        json += ",\"commandable\":";
        json += element.commandable ? "true" : "false";
        if (element.commandable && element.panelKind && strcmp(element.panelKind, "fixed") != 0)
        {
            json += ",\"active\":";
            json += domeLayoutPanelActive(element.id) ? "true" : "false";
        }
        json += ",\"disabled\":";
        bool disabled = statusOk ? statuses[i].disabled : false;
        String reason = statusOk ? statuses[i].reason : "";
        json += disabled ? "true" : "false";
        json += ",\"disabled_reason\":";
        if (disabled && reason.length() > 0)
        {
            json += '"';
            json += jsonEscape(reason);
            json += '"';
        }
        else
        {
            json += "null";
        }
        domeLayoutAppendStringArray(json, "aliases", element.aliases, element.aliasCount);
        domeLayoutAppendStringArray(json, "capabilities", element.capabilities,
                                    element.capabilityCount);
        json += ",\"render_order\":";
        json += element.renderOrder;
        domeLayoutAppendGeometry(json, element);
        domeLayoutAppendPoint(json, "label_anchor", element.labelAnchor);
        domeLayoutAppendCallout(json, element.callout);
        json += '}';
    }

    json += "]}";
    return json;
}

static String buildDomeLayoutJson()
{
    if (domeLayoutTemplateIsCustomSelected())
    {
        String customJson;
        String errMsg;
        if (domeLayoutTemplateReadFile(customJson, errMsg))
        {
            String composed;
            if (domeLayoutBuildCustomLayoutJson(customJson, composed, errMsg))
            {
                return composed;
            }
            logCapture.printf("[API] custom dome layout rejected at serve time: %s\n",
                              errMsg.c_str());
        }
        else
        {
            logCapture.printf("[API] custom dome layout unavailable: %s\n",
                              errMsg.c_str());
        }
    }
    return buildBundledDomeLayoutJson();
}


// ---------------------------------------------------------------
// Raw servo test — direct PCA9685 PWM writes for the wiring config UI
//
// These endpoints let an operator pulse a single physical channel on either
// PCA9685 board without involving ServoDispatch / slot routing. That's the
// point: the test is for identifying which silkscreen channel a wired servo
// actually connects to, which has to work regardless of whether the slot is
// configured or active. Direct I2C is also stateless and slot-independent.
//
// At rest the ReelTwo AnimationPlayer only writes I2C when a slot is actively
// animating; a raw test pulse therefore persists on the channel until either
// (a) /api/servo/stop is called, (b) another test is started on the same board
// (server-side auto-stop), or (c) a Marcduino command moves that slot.
// ---------------------------------------------------------------

// PCA9685 board addresses match the I2C jumpers documented in HARDWARE_WIRING.md.
static const uint8_t PCA9685_PANELS_ADDR = 0x40;
static const uint8_t PCA9685_HOLOS_ADDR  = 0x41;

// PWM count = round(pulse_µs × 4096 × 50 / 1_000_000) = round(µs × 0.2048).
// Approximate values are fine — the goal is visible movement for identification,
// not precision positioning. Per-slot calibration lives elsewhere and is unrelated.
static const uint16_t PWM_PANEL_OPEN   = 369;  // 1800 µs
static const uint16_t PWM_PANEL_CLOSED = 246;  // 1200 µs
static const uint16_t PWM_HOLO_A       = 410;  // 2000 µs (one extreme)
static const uint16_t PWM_HOLO_B       = 205;  // 1000 µs (other extreme)
static const uint16_t PWM_NEUTRAL      = 307;  // 1500 µs (used as holo stop pulse)

// Server-side test state. -1 means no test active on that board. We track one
// channel per board so a second /api/servo/test on the same board auto-stops
// the previous channel (mirroring the UI expectation that only one row can be
// in test mode at a time).
static int8_t   sPanelTestChannel = -1;
static int8_t   sHoloTestChannel  = -1;

// Async holo sweep state machine — polled from mainLoop() (matches the existing
// sPanelReleaseAtMs pattern, no FreeRTOS task). Sweep alternates between two
// extremes with a 1 s hold per phase.
static volatile bool sHoloSweepActive   = false;
static uint8_t       sHoloSweepChannel  = 0;
static uint8_t       sHoloSweepPhase    = 0;        // 0 = at A, 1 = at B
static uint32_t      sHoloSweepDeadline = 0;
static const uint32_t HOLO_SWEEP_HOLD_MS = 1000;

// Drive one PCA9685 channel directly. Standard 4-byte LED_ON_L / LED_ON_H /
// LED_OFF_L / LED_OFF_H block starting at register 6 + channel * 4 (the
// PCA9685 datasheet's LED0_ON_L base offset). LED_ON is 0 so the pulse always
// starts at the beginning of the PWM period; LED_OFF is the count value.
static void writePwm(uint8_t boardAddr, uint8_t channel, uint16_t count)
{
    if (channel > 15) return;  // out-of-range guard; UI clamps but be defensive
    Wire.beginTransmission(boardAddr);
    Wire.write(6 + channel * 4);
    Wire.write(0);                // LED_ON_L
    Wire.write(0);                // LED_ON_H
    Wire.write(lowByte(count));   // LED_OFF_L
    Wire.write(highByte(count));  // LED_OFF_H
    Wire.endTransmission();
}

// Called once per mainLoop() tick to advance the holo sweep. Cheap when idle:
// a single comparison short-circuits when no sweep is active.
static void holoSweepPoll()
{
    if (!sHoloSweepActive) return;
    if ((int32_t)(millis() - sHoloSweepDeadline) < 0) return;

    // Time to flip phase. Phase 0 ⇄ 1; write the corresponding extreme.
    sHoloSweepPhase = (sHoloSweepPhase == 0) ? 1 : 0;
    writePwm(PCA9685_HOLOS_ADDR, sHoloSweepChannel,
             (sHoloSweepPhase == 0) ? PWM_HOLO_A : PWM_HOLO_B);
    sHoloSweepDeadline = millis() + HOLO_SWEEP_HOLD_MS;
}

// Stop whatever test is running on the given board (if any). Writes a
// neutral/closed pulse to leave the servo at a known position, then clears
// the tracked test channel and sweep state. Safe to call when nothing is
// active — returns silently.
static void servoTestStop(bool isPanels)
{
    if (isPanels)
    {
        if (sPanelTestChannel >= 0)
        {
            writePwm(PCA9685_PANELS_ADDR, sPanelTestChannel, PWM_PANEL_CLOSED);
            sPanelTestChannel = -1;
        }
    }
    else
    {
        // Clear the sweep flag BEFORE writing the stop pulse so the poll
        // function cannot race in and flip phase between our writes.
        sHoloSweepActive = false;
        if (sHoloTestChannel >= 0)
        {
            writePwm(PCA9685_HOLOS_ADDR, sHoloTestChannel, PWM_NEUTRAL);
            sHoloTestChannel = -1;
        }
    }
}

// Parse the JSON body for /api/servo/test (wantChannel=true) and /api/servo/stop
// (wantChannel=false). Tiny, fixed-shape scanner mirroring wiringConfigParseBody.
static bool servoTestParseBody(const String &body, bool &outIsPanels, int &outChannel,
                                bool wantChannel, String &errMsg)
{
    int boardKey = body.indexOf("\"board\"");
    if (boardKey < 0) { errMsg = "missing board"; return false; }
    int boardColon = body.indexOf(':', boardKey);
    int quoteStart = body.indexOf('"', boardColon + 1);
    int quoteEnd   = (quoteStart >= 0) ? body.indexOf('"', quoteStart + 1) : -1;
    if (quoteStart < 0 || quoteEnd < 0) { errMsg = "board must be a string"; return false; }
    String boardStr = body.substring(quoteStart + 1, quoteEnd);
    if (boardStr == "panels")      outIsPanels = true;
    else if (boardStr == "holos")  outIsPanels = false;
    else { errMsg = "board must be panels or holos"; return false; }

    if (wantChannel)
    {
        int chKey = body.indexOf("\"channel\"");
        if (chKey < 0) { errMsg = "missing channel"; return false; }
        int chColon = body.indexOf(':', chKey);
        // End-pointer strtol + jsonValueEndsCleanly to reject non-numeric
        // ("bad", true, "5" as string) AND numeric-prefix-with-trailing-
        // garbage ("5bad", "5 garbage"). Whitespace is skipped THEN a
        // structural separator is required — whitespace alone is not a
        // valid terminator.
        const char *chStart = body.c_str() + chColon + 1;
        char *chEnd = nullptr;
        long chVal = strtol(chStart, &chEnd, 10);
        if (chEnd == chStart || !jsonValueEndsCleanly(chEnd))
        { errMsg = "channel must be a number"; return false; }
        if (chVal < 0 || chVal > 15) { errMsg = "channel out of range (0-15)"; return false; }
        outChannel = (int)chVal;
    }
    return true;
}

// ---------------------------------------------------------------
// Probe an I2C address — returns true if device ACKs
// ---------------------------------------------------------------
static uint8_t probeI2CCode(uint8_t addr, uint8_t attempts = 2)
{
    uint8_t code = 4;
    for (uint8_t i = 0; i < attempts; i++)
    {
        Wire.beginTransmission(addr);
        code = (uint8_t)Wire.endTransmission();
        if (code == 0) break;
        delayMicroseconds(200);
    }
    if (code > 5) code = 5;
    i2cCodeHistogram[code]++;
    if (code != 0) i2cProbeFailures++;
    return code;
}

static void refreshI2CHealthCache(bool force = false)
{
    uint32_t now = millis();
    if (!force && (now - lastI2CScanMs) < 30000u)
        return;

    uint32_t scanStartUs = micros();

    cachedPanelsCode = probeI2CCode(0x40);
    cachedHolosCode = probeI2CCode(0x41);

    cachedPanelsOk = (cachedPanelsCode == 0);
    cachedHolosOk = (cachedHolosCode == 0);

    if (cachedPanelsOk)
    {
        cachedPanelsLastOkMs = now;
        cachedPanelsConsecutiveFailures = 0;
    }
    else
    {
        cachedPanelsLastFailMs = now;
        cachedPanelsConsecutiveFailures++;
    }

    if (cachedHolosOk)
    {
        cachedHolosLastOkMs = now;
        cachedHolosConsecutiveFailures = 0;
    }
    else
    {
        cachedHolosLastFailMs = now;
        cachedHolosConsecutiveFailures++;
    }

    bool deepScan = force;
    String devices = "[";
    bool firstDev = true;
    uint32_t deviceCount = 0;

    if (deepScan)
    {
        for (uint8_t addr = 1; addr < 127; addr++)
        {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0)
            {
                if (!firstDev) devices += ",";
                firstDev = false;
                deviceCount++;
                devices += "\"0x" + String(addr, HEX) + "\"";
            }
        }
        cachedLastScanWasDeep = true;
        lastI2CDeepScanMs = now;
    }
    else
    {
        if (cachedPanelsOk)
        {
            devices += "\"0x40\"";
            firstDev = false;
            deviceCount++;
        }
        if (cachedHolosOk)
        {
            if (!firstDev) devices += ",";
            devices += "\"0x41\"";
            deviceCount++;
        }
        cachedLastScanWasDeep = false;
    }
    devices += "]";
    cachedI2CDevicesJson = devices;
    cachedI2CDeviceCount = deviceCount;
    cachedI2CScanDurationUs = micros() - scanStartUs;
    lastI2CScanMs = now;
}

static String buildI2CDiagnosticsJson(bool forceScan = false)
{
    refreshI2CHealthCache(forceScan);
    uint32_t now = millis();
    uint32_t scanAgeMs = (now >= lastI2CScanMs) ? (now - lastI2CScanMs) : 0;
    uint32_t deepScanAgeMs = (lastI2CDeepScanMs > 0 && now >= lastI2CDeepScanMs) ? (now - lastI2CDeepScanMs) : 0;
    bool has40 = (cachedI2CDevicesJson.indexOf("\"0x40\"") >= 0);
    bool has41 = (cachedI2CDevicesJson.indexOf("\"0x41\"") >= 0);

    String faults = "[";
    String hints = "[";
    bool firstFault = true;
    bool firstHint = true;
    auto addFault = [&](const char *fault, const char *hint)
    {
        if (!firstFault) faults += ",";
        faults += "\"" + String(fault) + "\"";
        firstFault = false;
        if (hint && hint[0] != '\0')
        {
            if (!firstHint) hints += ",";
            hints += "\"" + String(hint) + "\"";
            firstHint = false;
        }
    };

    if (!cachedPanelsOk && !cachedHolosOk)
    {
        addFault("both_expected_missing", "Both 0x40 and 0x41 are missing: check SDA/SCL wiring, common ground, and controller power.");
    }
    else
    {
        if (!cachedPanelsOk)
        {
            addFault("panels_0x40_missing", "0x40 missing: check panel controller power, address straps, and wiring to panel PCA9685.");
        }
        if (!cachedHolosOk)
        {
            addFault("holos_0x41_missing", "0x41 missing: check holo controller power, address straps, and wiring to holo PCA9685.");
        }
    }
    if (!has40 && cachedPanelsOk)
    {
        addFault("scan_missed_0x40", "Probe to 0x40 ACKed but bus scan missed it. This can indicate transient bus instability.");
    }
    if (!has41 && cachedHolosOk)
    {
        addFault("scan_missed_0x41", "Probe to 0x41 ACKed but bus scan missed it. This can indicate transient bus instability.");
    }
    if (cachedPanelsConsecutiveFailures >= 2 || cachedHolosConsecutiveFailures >= 2)
    {
        addFault("intermittent_failures", "Repeated probe failures detected. Check for loose connections, bus noise, or servo rail brownout conditions.");
    }
    if (firstFault)
    {
        addFault("none", "I2C diagnostic checks are stable for expected PCA9685 addresses.");
    }
    faults += "]";
    hints += "]";

    String json = "{";
    json.reserve(1024);
    json += "\"scan_age_ms\":" + String(scanAgeMs);
    json += ",\"scan_mode\":\"" + String(cachedLastScanWasDeep ? "deep" : "quick") + "\"";
    json += ",\"deep_scan_age_ms\":" + String(deepScanAgeMs);
    json += ",\"scan_duration_us\":" + String(cachedI2CScanDurationUs);
    json += ",\"device_count\":" + String(cachedI2CDeviceCount);
    json += ",\"devices\":" + cachedI2CDevicesJson;
    json += ",\"probe_failures\":" + String(i2cProbeFailures);

    json += ",\"code_histogram\":{";
    json += "\"0\":" + String(i2cCodeHistogram[0]);
    json += ",\"1\":" + String(i2cCodeHistogram[1]);
    json += ",\"2\":" + String(i2cCodeHistogram[2]);
    json += ",\"3\":" + String(i2cCodeHistogram[3]);
    json += ",\"4\":" + String(i2cCodeHistogram[4]);
    json += ",\"other\":" + String(i2cCodeHistogram[5]);
    json += "}";

    json += ",\"panels\":{";
    json += "\"addr\":\"0x40\"";
    json += ",\"ok\":" + String(cachedPanelsOk ? "true" : "false");
    json += ",\"last_code\":" + String(cachedPanelsCode);
    json += ",\"last_ok_ms\":" + String(cachedPanelsLastOkMs);
    json += ",\"last_fail_ms\":" + String(cachedPanelsLastFailMs);
    json += ",\"consecutive_failures\":" + String(cachedPanelsConsecutiveFailures);
    json += "}";

    json += ",\"holos\":{";
    json += "\"addr\":\"0x41\"";
    json += ",\"ok\":" + String(cachedHolosOk ? "true" : "false");
    json += ",\"last_code\":" + String(cachedHolosCode);
    json += ",\"last_ok_ms\":" + String(cachedHolosLastOkMs);
    json += ",\"last_fail_ms\":" + String(cachedHolosLastFailMs);
    json += ",\"consecutive_failures\":" + String(cachedHolosConsecutiveFailures);
    json += "}";

    json += ",\"operator\":{";
    json += "\"faults\":" + faults;
    json += ",\"hints\":" + hints;
    json += ",\"code_meaning\":{";
    json += "\"0\":\"ok\"";
    json += ",\"1\":\"buffer_overflow\"";
    json += ",\"2\":\"address_nack\"";
    json += ",\"3\":\"data_nack\"";
    json += ",\"4\":\"other_error\"";
    json += ",\"5\":\"timeout\"";
    json += "}";
    json += "}";
    return json;
}

static void scheduleReboot(uint32_t delayMs)
{
    rebootScheduled = true;
    rebootAtMs = millis() + delayMs;
}

// ---------------------------------------------------------------
// Build health JSON string
// ---------------------------------------------------------------
static String buildHealthJson()
{
    String json = "{";
    json.reserve(1024);

    // I2C device probes
    refreshI2CHealthCache();
    bool panelsOk = cachedPanelsOk;
    bool holosOk  = cachedHolosOk;
    json += "\"i2c_panels\":" + String(panelsOk ? "true" : "false");
    json += ",\"i2c_holos\":" + String(holosOk ? "true" : "false");
    json += ",\"i2c_panels_code\":" + String(cachedPanelsCode);
    json += ",\"i2c_holos_code\":" + String(cachedHolosCode);
    json += ",\"i2c_panels_fail_streak\":" + String(cachedPanelsConsecutiveFailures);
    json += ",\"i2c_holos_fail_streak\":" + String(cachedHolosConsecutiveFailures);

    // Sound module — check if not disabled
    // sMarcSound is the global MarcSound instance in .ino
    // We can't easily check module state from here without another extern,
    // so we report it based on preference config
    int soundPref = preferences.getInt("msound", MARC_SOUND_PLAYER);
    bool soundEnabled = (soundLocalEnabled && soundPref != 0);
    json += ",\"sound_module\":" + String(soundEnabled ? "true" : "false");
    json += ",\"sound_local_enabled\":" + String(soundLocalEnabled ? "true" : "false");
    json += ",\"sleep_mode\":" + String(sSleepModeActive ? "true" : "false");
    json += ",\"sleep_since_ms\":" + String(sSleepModeSinceMs);

    // WiFi
    json += ",\"wifi\":" + String(wifiEnabled ? "true" : "false");

    uint32_t nowMs = millis();

    // Droid Remote
#ifdef USE_DROID_REMOTE
    json += ",\"remote\":" + String(sRemoteConnected ? "true" : "false");
    json += ",\"remote_enabled\":" + String(remoteEnabled ? "true" : "false");
#else
    json += ",\"remote\":false";
    json += ",\"remote_enabled\":false";
#endif

    // SPIFFS
    json += ",\"spiffs\":true"; // If we got this far, SPIFFS is mounted

    // Free heap
    json += ",\"freeHeap\":" + String(ESP.getFreeHeap());
    json += ",\"uptime\":" + String(millis() / 1000);
    json += ",\"reset_reason\":\"" + String(resetReasonName(sBootResetReason)) + "\"";
    json += ",\"reset_reason_code\":" + String((int)sBootResetReason);
    json += ",\"coredump_present\":" + String(sBootCoreDumpPresent ? "true" : "false");
    json += ",\"visual_preset\":{";
    json += "\"current\":\"" + jsonEscape(String(sCurrentVisualPreset)) + "\"";
    json += ",\"last_cmd\":\"" + jsonEscape(String(sLastVisualPresetCmd)) + "\"";
    json += ",\"apply_count\":" + String(sVisualPresetApplyCount);
    json += ",\"unknown_count\":" + String(sVisualPresetUnknownCount);
    json += ",\"last_applied_ms\":" + String(sVisualPresetLastAppliedMs);
    json += ",\"age_ms\":" + String(sVisualPresetLastAppliedMs > 0 ? (uint32_t)(nowMs - sVisualPresetLastAppliedMs) : 0);
    json += "}";

    json += ",\"visual_authoring\":{";
    json += "\"logic\":{";
    json += "\"last_cmd\":\"" + jsonEscape(String(sVisualAuthoringLogic.lastCmd)) + "\"";
    json += ",\"target\":\"" + jsonEscape(String(sVisualAuthoringLogic.target)) + "\"";
    json += ",\"mode\":\"" + jsonEscape(String(sVisualAuthoringLogic.mode)) + "\"";
    json += ",\"color\":\"" + jsonEscape(String(sVisualAuthoringLogic.color)) + "\"";
    json += ",\"duration\":" + String(sVisualAuthoringLogic.duration);
    json += ",\"apply_count\":" + String(sVisualAuthoringLogic.applyCount);
    json += ",\"reject_count\":" + String(sVisualAuthoringLogic.rejectCount);
    json += ",\"last_applied_ms\":" + String(sVisualAuthoringLogic.lastAppliedMs);
    json += ",\"age_ms\":" + String(sVisualAuthoringLogic.lastAppliedMs > 0 ? (uint32_t)(nowMs - sVisualAuthoringLogic.lastAppliedMs) : 0);
    json += "},\"text\":{";
    json += "\"last_cmd\":\"" + jsonEscape(String(sVisualAuthoringText.lastCmd)) + "\"";
    json += ",\"target\":\"" + jsonEscape(String(sVisualAuthoringText.target)) + "\"";
    json += ",\"color\":\"" + jsonEscape(String(sVisualAuthoringText.color)) + "\"";
    json += ",\"duration\":" + String(sVisualAuthoringText.duration);
    json += ",\"speed\":" + String(sVisualAuthoringText.speed);
    json += ",\"decoded_length\":" + String(sVisualAuthoringText.decodedLength);
    json += ",\"apply_count\":" + String(sVisualAuthoringText.applyCount);
    json += ",\"reject_count\":" + String(sVisualAuthoringText.rejectCount);
    json += ",\"last_applied_ms\":" + String(sVisualAuthoringText.lastAppliedMs);
    json += ",\"age_ms\":" + String(sVisualAuthoringText.lastAppliedMs > 0 ? (uint32_t)(nowMs - sVisualAuthoringText.lastAppliedMs) : 0);
    json += "},\"holo\":{";
    json += "\"last_cmd\":\"" + jsonEscape(String(sVisualAuthoringHolo.lastCmd)) + "\"";
    json += ",\"target\":\"" + jsonEscape(String(sVisualAuthoringHolo.target)) + "\"";
    json += ",\"effect\":\"" + jsonEscape(String(sVisualAuthoringHolo.effect)) + "\"";
    json += ",\"color\":\"" + jsonEscape(String(sVisualAuthoringHolo.color)) + "\"";
    json += ",\"duration_or_count\":" + String(sVisualAuthoringHolo.durationOrCount);
    json += ",\"apply_count\":" + String(sVisualAuthoringHolo.applyCount);
    json += ",\"reject_count\":" + String(sVisualAuthoringHolo.rejectCount);
    json += ",\"last_applied_ms\":" + String(sVisualAuthoringHolo.lastAppliedMs);
    json += ",\"age_ms\":" + String(sVisualAuthoringHolo.lastAppliedMs > 0 ? (uint32_t)(nowMs - sVisualAuthoringHolo.lastAppliedMs) : 0);
    json += "}}";

    json += ",\"i2c_devices\":" + cachedI2CDevicesJson;
    json += ",\"min_free_heap\":" + String(sMinFreeHeap);
    json += ",\"i2c_probe_failures\":" + String(i2cProbeFailures);
    json += ",\"cmd_queue\":{";
    json += "\"depth\":" + String(sMarcduinoQueueCount);
    json += ",\"capacity\":" + String(SizeOfArray(sMarcduinoQueue));
    json += ",\"queue_full_count\":" + String(sMarcduinoQueueFullCount);
    json += "}";
    // Body link status
    bool bodyLinkPrefEnabled = preferences.getBool("mbodylink", BODY_LINK_ENABLED);
    json += ",\"body_link\":{";
    json += "\"enabled\":" + String(bodyLinkPrefEnabled ? "true" : "false");
    json += ",\"connected\":" + String(bodyLinkConnected() ? "true" : "false");
    json += ",\"transport\":\"" + String(bodyLinkGetTransportName()) + "\"";
    json += ",\"wifi_enabled\":" + String(bodyLinkWifiEnabled() ? "true" : "false");
    json += ",\"peer_ip\":\"" + jsonEscape(bodyLinkGetPeerIP()) + "\"";
    json += ",\"last_rx_ms\":" + String(sBodyLastSeenMs > 0 ? (int32_t)(millis() - sBodyLastSeenMs) : 0);
    json += ",\"hb_rx\":" + String(sBodyHeartbeatRx);
    json += ",\"uart_hb_age_ms\":" + String(bodyLinkUartHeartbeatAgeMs());
    json += ",\"wifi_hb_age_ms\":" + String(bodyLinkWifiHeartbeatAgeMs());
    json += ",\"peer_source\":\"" + String(bodyLinkGetPeerSource()) + "\"";
    json += "}";

    // Gadget status
    json += ",\"gadgets\":{";
#if AP_ENABLE_BADMOTIVATOR
    bool badmotEnabled = preferences.getBool(PREFERENCE_BADMOTIVATOR_ENABLED, AP_ENABLE_BADMOTIVATOR);
    json += "\"badmotivator\":{\"enabled\":" + String(badmotEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += "\"badmotivator\":{\"enabled\":false,\"present\":false}";
#endif
#if AP_ENABLE_FIRESTRIP
    bool firestripEnabled = preferences.getBool(PREFERENCE_FIRESTRIP_ENABLED, AP_ENABLE_FIRESTRIP);
    json += ",\"firestrip\":{\"enabled\":" + String(firestripEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += ",\"firestrip\":{\"enabled\":false,\"present\":false}";
#endif
#if AP_ENABLE_CBI
    bool cbiEnabled = preferences.getBool(PREFERENCE_CBI_ENABLED, AP_ENABLE_CBI);
    json += ",\"cbi\":{\"enabled\":" + String(cbiEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += ",\"cbi\":{\"enabled\":false,\"present\":false}";
#endif
#if AP_ENABLE_DATAPANEL
    bool datapanelEnabled = preferences.getBool(PREFERENCE_DATAPANEL_ENABLED, AP_ENABLE_DATAPANEL);
    json += ",\"datapanel\":{\"enabled\":" + String(datapanelEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += ",\"datapanel\":{\"enabled\":false,\"present\":false}";
#endif
    json += "}";

    json += "}";
    return json;
}

// ---------------------------------------------------------------
// Initialize the async web server
// Call from setup() after WiFi is configured
// ---------------------------------------------------------------
static void initAsyncWeb()
{
    // Attach WebSocket
    ws.onEvent(onWsEvent);
    asyncServer.addHandler(&ws);

    // ---- Static files from SPIFFS ----
    asyncServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // ---- REST API: Send Marcduino command ----
    asyncServer.on("/api/cmd", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        String cmd;
        if (!parsePostParam(request, "cmd", cmd)) return;
        if (!validateMarcduinoCmd(request, cmd)) return;
        if (guardSleep(request, cmd.c_str())) return;
        logCapture.printf("[API] cmd=%s len=%u\n", cmd.c_str(), (unsigned int)cmd.length());
        processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
        broadcastState();
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // ---- REST API: Get state ----
    asyncServer.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", buildStateJson());
    });

    // ---- REST API: Get health ----
    asyncServer.on("/api/health", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", buildHealthJson());
    });

    asyncServer.on("/api/diag/i2c", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        bool forceScan = request->hasParam("force") && request->getParam("force")->value() == "1";
        request->send(200, "application/json", buildI2CDiagnosticsJson(forceScan));
    });

    // ---- REST API: Get log lines ----
    asyncServer.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String json = "{\"lines\":[";
        int count = logCapture.lineCount();
        for (int i = 0; i < count; i++)
        {
            if (i > 0) json += ",";
            // Escape special chars in log line
            String line = logCapture.getLine(i);
            json += "\"" + jsonEscape(line) + "\"";
        }
        json += "]}";
        request->send(200, "application/json", json);
    });

    // ---- REST API: Read preferences ----
    // Boolean preference keys — stored with putBool, read with getBool
    // These must be handled separately from string preferences
    asyncServer.on("/api/pref", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        if (!request->hasParam("keys"))
        {
            request->send(400, "application/json", "{\"error\":\"missing keys param\"}");
            return;
        }
        String keys = request->getParam("keys")->value();
        String json = "{";
        bool first = true;
        int start = 0;
        while (start <= (int)keys.length())
        {
            int comma = keys.indexOf(',', start);
            if (comma < 0) comma = keys.length();
            String key = keys.substring(start, comma);
            key.trim();
            if (key.length() > 0)
            {
                if (!isAllowedPrefKey(key))
                {
                    request->send(400, "application/json", "{\"error\":\"invalid key\"}");
                    return;
                }
                if (!first) json += ",";
                first = false;
                // Boolean keys: firmware uses getBool/putBool for these
                if (key == "wifi" || key == "ap" || key == "remote" || key == "msoundlocal" ||
                    key == "mserialpass" || key == "mserial" || key == "mwifi" || key == "mwifipass" ||
                    key == PREFERENCE_BADMOTIVATOR_ENABLED || key == PREFERENCE_FIRESTRIP_ENABLED ||
                    key == PREFERENCE_CBI_ENABLED || key == PREFERENCE_DATAPANEL_ENABLED ||
                    key == "mbodylink" || key == "mbodywifi")
                {
                    bool val = preferences.getBool(key.c_str(), defaultBoolForPrefKey(key));
                    json += "\"" + jsonEscape(key) + "\":" + (val ? "true" : "false");
                }
                else if (isIntegerPrefKey(key))
                {
                    int val = preferences.getInt(key.c_str(), defaultIntForPrefKey(key));
                    json += "\"" + jsonEscape(key) + "\":" + String(val);
                }
                else
                {
                    String val = preferences.getString(key.c_str(), "");
                    json += "\"" + jsonEscape(key) + "\":\"" + jsonEscape(val) + "\"";
                }
            }
            start = comma + 1;
        }
        json += "}";
        request->send(200, "application/json", json);
    });

    // ---- REST API: Set preference ----
    asyncServer.on("/api/pref", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        if (request->hasParam("key", true) && request->hasParam("val", true))
        {
            String key = request->getParam("key", true)->value();
            String val = request->getParam("val", true)->value();
            key.trim();

            if (key.length() == 0 || key.length() > 16)
            {
                request->send(400, "application/json", "{\"error\":\"invalid key\"}");
                return;
            }

            // Special key: factory reset
            if (key == "_clear")
            {
                logCapture.println("[API] Factory reset — clearing all preferences");
                preferences.clear();
            }
            else if (!isAllowedPrefKey(key))
            {
                request->send(400, "application/json", "{\"error\":\"key not allowed\"}");
                return;
            }
            else if (val.length() > maxPrefValueLen(key))
            {
                request->send(400, "application/json", "{\"error\":\"value too long\"}");
                return;
            }
            // Boolean keys — firmware uses getBool/putBool
            else if (key == "wifi" || key == "ap" || key == "remote" || key == "msoundlocal" ||
                     key == "mserialpass" || key == "mserial" || key == "mwifi" || key == "mwifipass" ||
                     key == "mbodylink" || key == "mbodywifi" ||
                     key == PREFERENCE_BADMOTIVATOR_ENABLED || key == PREFERENCE_FIRESTRIP_ENABLED ||
                     key == PREFERENCE_CBI_ENABLED || key == PREFERENCE_DATAPANEL_ENABLED)
            {
                bool bval = (val == "1" || val == "true");
                preferences.putBool(key.c_str(), bval);
                if (key == "msoundlocal")
                {
                    soundLocalEnabled = bval;
                }
                if (key == PREFERENCE_BADMOTIVATOR_ENABLED || key == PREFERENCE_FIRESTRIP_ENABLED ||
                    key == PREFERENCE_CBI_ENABLED || key == PREFERENCE_DATAPANEL_ENABLED)
                {
                    // Just store the preference value, actual enable/disable happens on next reboot
                    // or could be handled dynamically if needed
                    logCapture.printf("[API] pref (gadget): %s = %s\n", key.c_str(), bval ? "true" : "false");
                }
                logCapture.printf("[API] pref (bool): %s = %s\n", key.c_str(), bval ? "true" : "false");
            }
            else if (isIntegerPrefKey(key))
            {
                int ival = 0;
                if (!parseIntegerPrefValue(val, ival))
                {
                    request->send(400, "application/json", "{\"error\":\"invalid integer value\"}");
                    return;
                }
                preferences.putInt(key.c_str(), ival);
                logCapture.printf("[API] pref (int): %s = %d\n", key.c_str(), ival);
            }
            else
            {
                preferences.putString(key.c_str(), val);
                if (isSensitivePrefKey(key))
                    logCapture.printf("[API] pref: %s = <redacted>\n", key.c_str());
                else
                    logCapture.printf("[API] pref: %s = %s\n", key.c_str(), val.c_str());
            }

            bool needsReboot = request->hasParam("reboot", true) &&
                               request->getParam("reboot", true)->value() == "1";
            request->send(200, "application/json", "{\"ok\":true}");
            if (needsReboot)
            {
                scheduleReboot(500);
            }
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"missing key/val params\"}");
        }
    });

    // ---- REST API: Dynamic wiring config (panels) ----
    // GET returns current channel/active per slot (NVS or MK4 defaults).
    // POST saves a new config to NVS; reboot required to apply (panelConfigLoad
    // runs in setup() before SetupEvent::ready()).
    asyncServer.on("/api/panels/config", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        uint8_t channels[NUM_PANEL_SLOTS];
        bool    actives[NUM_PANEL_SLOTS];
        wiringConfigRead(PREFERENCE_PANELS_NS, PREFERENCE_PANELS_CH_FMT,
                         PREFERENCE_PANELS_ACT_FMT, NUM_PANEL_SLOTS,
                         defaultPanelCh, defaultPanelActive, channels, actives);
        request->send(200, "application/json",
            wiringConfigBuildJson("panels", NUM_PANEL_SLOTS, channels, actives,
                                   panelSlotLabel, panelSlotCommand));
    });

    asyncServer.on("/api/panels/config", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            // onRequest — fires after the body is fully buffered into _tempObject.
            String *body = (String *)request->_tempObject;
            if (!body || body->length() == 0)
            {
                logCapture.println("[API] panels/config rejected: empty body");
                request->send(400, "application/json", "{\"error\":\"empty body\"}");
                if (body) { delete body; request->_tempObject = nullptr; }
                return;
            }

            uint8_t channels[NUM_PANEL_SLOTS];
            bool    actives[NUM_PANEL_SLOTS];
            String  errMsg;
            bool ok = wiringConfigParseBody(*body, NUM_PANEL_SLOTS, channels, actives, errMsg);
            delete body;
            request->_tempObject = nullptr;
            if (!ok)
            {
                logCapture.printf("[API] panels/config rejected: %s\n", errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }
            if (!wiringConfigCheckConflicts(NUM_PANEL_SLOTS, channels, actives, errMsg))
            {
                logCapture.printf("[API] panels/config rejected: %s\n", errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }
            if (!wiringConfigSave(PREFERENCE_PANELS_NS, PREFERENCE_PANELS_CH_FMT,
                                   PREFERENCE_PANELS_ACT_FMT, NUM_PANEL_SLOTS,
                                   channels, actives))
            {
                logCapture.println("[API] Error: panels/config NVS save failed "
                                    "(flash full or NVS corrupt — investigate)");
                request->send(500, "application/json", "{\"error\":\"NVS save failed\"}");
                return;
            }
            // Success path — count actives so the log carries enough detail to
            // confirm at a glance what was saved without dumping all 13 slots.
            int activeCount = 0;
            for (int i = 0; i < NUM_PANEL_SLOTS; i++) if (actives[i]) activeCount++;
            logCapture.printf("[API] panels/config saved: %d active, %d inactive "
                               "— reboot required to apply\n",
                               activeCount, NUM_PANEL_SLOTS - activeCount);
            request->send(200, "application/json", "{\"ok\":true,\"reboot_required\":true}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            // onBody — accumulate raw bytes; cap to guard against runaway payloads.
            // Expected size is ~600 bytes for 13 slots; 4 KiB ceiling is generous.
            if (total > 4096) return;
            if (index == 0)
            {
                request->_tempObject = new String();
                ((String *)request->_tempObject)->reserve(total + 1);
            }
            String *body = (String *)request->_tempObject;
            if (body) body->concat((const char *)data, len);
        });

    // ---- REST API: Dynamic wiring config (holos) ----
    asyncServer.on("/api/holos/config", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        uint8_t channels[NUM_HOLO_SLOTS];
        bool    actives[NUM_HOLO_SLOTS];
        wiringConfigRead(PREFERENCE_HOLOS_NS, PREFERENCE_HOLOS_CH_FMT,
                         PREFERENCE_HOLOS_ACT_FMT, NUM_HOLO_SLOTS,
                         defaultHoloCh, defaultHoloActive, channels, actives);
        // Holo endpoint omits the "cmd" field — holos don't have :OPnn equivalents.
        request->send(200, "application/json",
            wiringConfigBuildJson("holos", NUM_HOLO_SLOTS, channels, actives,
                                   holoSlotLabel, nullptr));
    });

    asyncServer.on("/api/holos/config", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            String *body = (String *)request->_tempObject;
            if (!body || body->length() == 0)
            {
                logCapture.println("[API] holos/config rejected: empty body");
                request->send(400, "application/json", "{\"error\":\"empty body\"}");
                if (body) { delete body; request->_tempObject = nullptr; }
                return;
            }
            uint8_t channels[NUM_HOLO_SLOTS];
            bool    actives[NUM_HOLO_SLOTS];
            String  errMsg;
            bool ok = wiringConfigParseBody(*body, NUM_HOLO_SLOTS, channels, actives, errMsg);
            delete body;
            request->_tempObject = nullptr;
            if (!ok)
            {
                logCapture.printf("[API] holos/config rejected: %s\n", errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }
            if (!wiringConfigCheckConflicts(NUM_HOLO_SLOTS, channels, actives, errMsg))
            {
                logCapture.printf("[API] holos/config rejected: %s\n", errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }
            if (!wiringConfigSave(PREFERENCE_HOLOS_NS, PREFERENCE_HOLOS_CH_FMT,
                                   PREFERENCE_HOLOS_ACT_FMT, NUM_HOLO_SLOTS,
                                   channels, actives))
            {
                logCapture.println("[API] Error: holos/config NVS save failed "
                                    "(flash full or NVS corrupt — investigate)");
                request->send(500, "application/json", "{\"error\":\"NVS save failed\"}");
                return;
            }
            int activeCount = 0;
            for (int i = 0; i < NUM_HOLO_SLOTS; i++) if (actives[i]) activeCount++;
            logCapture.printf("[API] holos/config saved: %d active, %d inactive "
                               "— reboot required to apply\n",
                               activeCount, NUM_HOLO_SLOTS - activeCount);
            request->send(200, "application/json", "{\"ok\":true,\"reboot_required\":true}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            if (total > 4096) return;
            if (index == 0)
            {
                request->_tempObject = new String();
                ((String *)request->_tempObject)->reserve(total + 1);
            }
            String *body = (String *)request->_tempObject;
            if (body) body->concat((const char *)data, len);
        });

    // ---- REST API: Dome layout read model ----
    // External editors consume this composed model instead of stitching
    // together template geometry, wiring state, and maintenance status.
    asyncServer.on("/api/dome/layout", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", buildDomeLayoutJson());
    });

    // ---- REST API: Dome layout template management ----
    // Custom templates live on SPIFFS and stay display-only: validation rejects
    // command/slot/channel fields before a template can be activated. Bundled
    // MK4 remains the immutable fallback and rollback target.
    asyncServer.on("/api/dome/layout-template", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", domeLayoutTemplateInfoJson(jsonEscape));
    });

    asyncServer.on("/api/dome/layout-template", HTTP_DELETE, [](AsyncWebServerRequest *request)
    {
        bool removed = true;
        if (SPIFFS.exists(DOME_LAYOUT_TEMPLATE_PATH))
        {
            removed = SPIFFS.remove(DOME_LAYOUT_TEMPLATE_PATH);
        }
        if (SPIFFS.exists(DOME_LAYOUT_TEMPLATE_TMP_PATH))
        {
            SPIFFS.remove(DOME_LAYOUT_TEMPLATE_TMP_PATH);
        }
        bool selectedOk = domeLayoutTemplateSetCustomSelected(false);
        if (!removed || !selectedOk)
        {
            request->send(500, "application/json", "{\"error\":\"template rollback failed\"}");
            return;
        }
        request->send(200, "application/json",
                      "{\"ok\":true,\"selected_source\":\"bundled\"}");
    });

    asyncServer.on("/api/dome/layout-template/select", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            String *body = (String *)request->_tempObject;
            if (!body || body->length() == 0)
            {
                request->send(400, "application/json", "{\"error\":\"empty body\"}");
                if (body) { delete body; request->_tempObject = nullptr; }
                return;
            }

            String errMsg;
            String source;
            bool parsed = domeLayoutTemplateFindRootString(*body, "source",
                                                           source, errMsg);
            delete body;
            request->_tempObject = nullptr;
            if (!parsed || (source != "custom" && source != "bundled"))
            {
                request->send(400, "application/json",
                              "{\"error\":\"source must be custom or bundled\"}");
                return;
            }
            bool selectCustom = source == "custom";
            if (selectCustom)
            {
                String customJson;
                DomeLayoutTemplateInfo info = {};
                if (!domeLayoutTemplateReadFile(customJson, errMsg) ||
                    !domeLayoutTemplateValidateJson(customJson, info, errMsg))
                {
                    request->send(400, "application/json",
                        String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                    return;
                }
            }
            if (!domeLayoutTemplateSetCustomSelected(selectCustom))
            {
                request->send(500, "application/json",
                              "{\"error\":\"template selection save failed\"}");
                return;
            }
            request->send(200, "application/json",
                String("{\"ok\":true,\"selected_source\":\"") +
                (selectCustom ? "custom" : "bundled") + "\"}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
           size_t index, size_t total)
        {
            if (total > DOME_LAYOUT_TEMPLATE_MAX_BYTES) return;
            if (index == 0)
            {
                request->_tempObject = new String();
                ((String *)request->_tempObject)->reserve(total + 1);
            }
            String *body = (String *)request->_tempObject;
            if (body) body->concat((const char *)data, len);
        });

    asyncServer.on("/api/dome/layout-template", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            String *body = (String *)request->_tempObject;
            if (!body || body->length() == 0)
            {
                logCapture.println("[API] dome/layout-template rejected: empty body");
                request->send(400, "application/json", "{\"error\":\"empty body\"}");
                if (body) { delete body; request->_tempObject = nullptr; }
                return;
            }

            DomeLayoutTemplateInfo info = {};
            String errMsg;
            bool ok = domeLayoutTemplateValidateJson(*body, info, errMsg);
            if (ok) ok = domeLayoutTemplateWriteCustom(*body, errMsg);
            delete body;
            request->_tempObject = nullptr;
            if (!ok)
            {
                logCapture.printf("[API] dome/layout-template rejected: %s\n",
                                  errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }
            if (!domeLayoutTemplateSetCustomSelected(true))
            {
                request->send(500, "application/json",
                              "{\"error\":\"template selection save failed\"}");
                return;
            }
            logCapture.printf("[API] dome/layout-template installed: %s rev %d\n",
                              info.templateId.c_str(), info.templateRevision);
            request->send(200, "application/json",
                String("{\"ok\":true,\"selected_source\":\"custom\",") +
                "\"template_id\":\"" + jsonEscape(info.templateId) + "\"," +
                "\"template_revision\":" + info.templateRevision + "}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
           size_t index, size_t total)
        {
            if (total > DOME_LAYOUT_TEMPLATE_MAX_BYTES) return;
            if (index == 0)
            {
                request->_tempObject = new String();
                ((String *)request->_tempObject)->reserve(total + 1);
            }
            String *body = (String *)request->_tempObject;
            if (body) body->concat((const char *)data, len);
        });

    // ---- REST API: Dome element status ----
    // Operator maintenance flags for the layout contract. This endpoint only
    // persists advisory element availability; it never blocks commands or
    // changes servo/runtime routing.
    asyncServer.on("/api/dome/element-status", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", domeElementStatusBuildJson(jsonEscape));
    });

    asyncServer.on("/api/dome/element-status", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            String *body = (String *)request->_tempObject;
            if (!body || body->length() == 0)
            {
                logCapture.println("[API] dome/element-status rejected: empty body");
                request->send(400, "application/json", "{\"error\":\"empty body\"}");
                if (body) { delete body; request->_tempObject = nullptr; }
                return;
            }

            DomeElementStatusUpdate updates[DOME_ELEMENT_STATUS_MAX_ELEMENTS];
            int updateCount = 0;
            String errMsg;
            bool ok = domeElementStatusParseBody(*body, updates,
                                                 DOME_ELEMENT_STATUS_MAX_ELEMENTS,
                                                 updateCount, errMsg);
            delete body;
            request->_tempObject = nullptr;
            if (!ok)
            {
                logCapture.printf("[API] dome/element-status rejected: %s\n", errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }
            if (!domeElementStatusSaveUpdates(updates, updateCount))
            {
                logCapture.println("[API] Error: dome/element-status NVS save failed "
                                    "(flash full or NVS corrupt — investigate)");
                request->send(500, "application/json", "{\"error\":\"NVS save failed\"}");
                return;
            }
            logCapture.printf("[API] dome/element-status saved: %d update(s)\n",
                              updateCount);
            request->send(200, "application/json",
                String("{\"ok\":true,\"updated\":") + updateCount + "}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            // Status updates are small; cap at 4 KiB to match the wiring config
            // endpoints while still allowing several annotated elements at once.
            if (total > 4096) return;
            if (index == 0)
            {
                request->_tempObject = new String();
                ((String *)request->_tempObject)->reserve(total + 1);
            }
            String *body = (String *)request->_tempObject;
            if (body) body->concat((const char *)data, len);
        });

    // ---- REST API: Raw servo test (no slot routing, direct PCA9685 write) ----
    // Panel test: opens the requested channel and holds it until /api/servo/stop
    // or another test on the same board (server-side auto-stop). Holo test: starts
    // a sweep state machine polled in mainLoop(). Both endpoints accept a JSON
    // body of the shape { "board": "panels"|"holos", "channel": 0-15 }.
    asyncServer.on("/api/servo/test", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            String *body = (String *)request->_tempObject;
            if (!body || body->length() == 0)
            {
                logCapture.println("[API] /servo/test rejected: empty body");
                request->send(400, "application/json", "{\"error\":\"empty body\"}");
                if (body) { delete body; request->_tempObject = nullptr; }
                return;
            }
            bool   isPanels = false;
            int    channel  = -1;
            String errMsg;
            bool ok = servoTestParseBody(*body, isPanels, channel, /*wantChannel*/true, errMsg);
            delete body;
            request->_tempObject = nullptr;
            if (!ok)
            {
                logCapture.printf("[API] /servo/test rejected: %s\n", errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }

            // Server-side auto-stop: if a test is already running on this board,
            // close it cleanly before driving the new channel. UI mirrors this
            // by resetting the previous row's button — but the server enforces.
            const char *boardName = isPanels ? "panels" : "holos";
            int8_t prevChannel = isPanels ? sPanelTestChannel : sHoloTestChannel;
            if (prevChannel >= 0)
            {
                logCapture.printf("[Wiring] Auto-stopping previous test on %s CH%d "
                                   "before starting new pulse\n",
                                   boardName, (int)prevChannel);
            }
            servoTestStop(isPanels);

            if (isPanels)
            {
                writePwm(PCA9685_PANELS_ADDR, (uint8_t)channel, PWM_PANEL_OPEN);
                sPanelTestChannel = (int8_t)channel;
                logCapture.printf("[Wiring] Test pulse: panels CH%d held open "
                                   "(stays open until /servo/stop or another test "
                                   "on this board)\n", channel);
            }
            else
            {
                // Prime the first phase at extreme A; mainLoop() will flip after
                // HOLO_SWEEP_HOLD_MS. Set state in this exact order — channel and
                // phase must be valid before sHoloSweepActive becomes true, since
                // holoSweepPoll() reads them once it sees the active flag.
                sHoloTestChannel    = (int8_t)channel;
                sHoloSweepChannel   = (uint8_t)channel;
                sHoloSweepPhase     = 0;
                writePwm(PCA9685_HOLOS_ADDR, (uint8_t)channel, PWM_HOLO_A);
                sHoloSweepDeadline  = millis() + HOLO_SWEEP_HOLD_MS;
                sHoloSweepActive    = true;
                logCapture.printf("[Wiring] Test sweep started: holos CH%d "
                                   "(alternating extremes with 1 s holds — "
                                   "stays running until /servo/stop)\n", channel);
            }
            String resp = String("{\"ok\":true,\"board\":\"") + boardName +
                          "\",\"channel\":" + channel + "}";
            request->send(200, "application/json", resp);
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            if (total > 256) return;  // /api/servo/test body is tiny (~40 bytes)
            if (index == 0)
            {
                request->_tempObject = new String();
                ((String *)request->_tempObject)->reserve(total + 1);
            }
            String *body = (String *)request->_tempObject;
            if (body) body->concat((const char *)data, len);
        });

    asyncServer.on("/api/servo/stop", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            String *body = (String *)request->_tempObject;
            if (!body || body->length() == 0)
            {
                logCapture.println("[API] /servo/stop rejected: empty body");
                request->send(400, "application/json", "{\"error\":\"empty body\"}");
                if (body) { delete body; request->_tempObject = nullptr; }
                return;
            }
            bool   isPanels = false;
            int    unusedChannel = -1;
            String errMsg;
            // Stop only needs the board; channel (if sent) is ignored — server
            // already tracks which channel is active.
            bool ok = servoTestParseBody(*body, isPanels, unusedChannel, /*wantChannel*/false, errMsg);
            delete body;
            request->_tempObject = nullptr;
            if (!ok)
            {
                logCapture.printf("[API] /servo/stop rejected: %s\n", errMsg.c_str());
                request->send(400, "application/json",
                    String("{\"error\":\"") + jsonEscape(errMsg) + "\"}");
                return;
            }
            // Snapshot which channel was active so the log can name it; we
            // still report success even if nothing was running (idempotent stop).
            const char *boardName = isPanels ? "panels" : "holos";
            int8_t prevChannel = isPanels ? sPanelTestChannel : sHoloTestChannel;
            servoTestStop(isPanels);  // no-op if nothing is active on that board
            if (prevChannel >= 0)
            {
                logCapture.printf("[Wiring] Test stopped: %s CH%d (servo returned "
                                   "to %s)\n", boardName, (int)prevChannel,
                                   isPanels ? "closed" : "neutral");
            }
            else
            {
                logCapture.printf("[Wiring] /servo/stop: no active test on %s "
                                   "(no-op)\n", boardName);
            }
            request->send(200, "application/json",
                String("{\"ok\":true,\"board\":\"") + boardName + "\"}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            if (total > 256) return;
            if (index == 0)
            {
                request->_tempObject = new String();
                ((String *)request->_tempObject)->reserve(total + 1);
            }
            String *body = (String *)request->_tempObject;
            if (body) body->concat((const char *)data, len);
        });

    // ---- REST API: Reboot ----
    asyncServer.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", "{\"ok\":true,\"msg\":\"rebooting\"}");
        scheduleReboot(500);
    });

    asyncServer.on("/api/sleep", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        bool changed = enterSoftSleepMode();
        request->send(200, "application/json", changed
            ? "{\"ok\":true,\"sleepMode\":true,\"changed\":true}"
            : "{\"ok\":true,\"sleepMode\":true,\"changed\":false}");
        broadcastState();
    });

    asyncServer.on("/api/wake", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        bool changed = exitSoftSleepMode();
        request->send(200, "application/json", changed
            ? "{\"ok\":true,\"sleepMode\":false,\"changed\":true}"
            : "{\"ok\":true,\"sleepMode\":false,\"changed\":false}");
        broadcastState();
    });

    // ---- REST API: Smoke control ----
    asyncServer.on("/api/smoke", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        String state;
        if (!parsePostParam(request, "state", state)) return;
        if (state == "on")
        {
            processMarcduinoCommandWithSource("astropixel-web-api", "BMON");
            request->send(200, "application/json", "{\"ok\":true,\"state\":\"on\"}");
        }
        else if (state == "off")
        {
            processMarcduinoCommandWithSource("astropixel-web-api", "BMOFF");
            request->send(200, "application/json", "{\"ok\":true,\"state\":\"off\"}");
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"invalid state, use 'on' or 'off'\"}");
        }
    });

    // ---- REST API: Fire effects control ----
    asyncServer.on("/api/fire", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        String state;
        if (!parsePostParam(request, "state", state)) return;
        if (state == "on")
        {
            processMarcduinoCommandWithSource("astropixel-web-api", "FS11000");
            request->send(200, "application/json", "{\"ok\":true,\"state\":\"on\"}");
        }
        else if (state == "off")
        {
            processMarcduinoCommandWithSource("astropixel-web-api", "FSOFF");
            request->send(200, "application/json", "{\"ok\":true,\"state\":\"off\"}");
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"invalid state, use 'on' or 'off'\"}");
        }
    });

    // ---- REST API: CBI (Charge Bay Indicator) control ----
    asyncServer.on("/api/cbi", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        // Return current CBI state (simplified - no direct state tracking)
        request->send(200, "application/json", "{\"ok\":true,\"state\":\"unknown\"}");
    });

    asyncServer.on("/api/cbi", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        String action;
        if (!parsePostParam(request, "action", action)) return;
        if (action == "flicker")
        {
            uint8_t durationSec = 6;
            if (request->hasParam("duration", true) &&
                !parseDurationSeconds(request->getParam("duration", true)->value(), 1, 99, durationSec))
            {
                request->send(400, "application/json", "{\"error\":\"invalid duration, use 1-99 seconds\"}");
                return;
            }
            char duration[3];
            snprintf(duration, sizeof(duration), "%02u", durationSec);
            String cmd = "CB2" + String(duration) + "006";
            processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
            request->send(200, "application/json", "{\"ok\":true,\"action\":\"flicker\",\"duration\":" + String(durationSec) + "}");
        }
        else if (action == "disable")
        {
            uint8_t durationSec = 8;
            if (request->hasParam("duration", true) &&
                !parseDurationSeconds(request->getParam("duration", true)->value(), 1, 99, durationSec))
            {
                request->send(400, "application/json", "{\"error\":\"invalid duration, use 1-99 seconds\"}");
                return;
            }
            char duration[3];
            snprintf(duration, sizeof(duration), "%02u", durationSec);
            String cmd = "CB1" + String(duration) + "008";
            processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
            request->send(200, "application/json", "{\"ok\":true,\"action\":\"disable\",\"duration\":" + String(durationSec) + "}");
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"invalid action, use 'flicker' or 'disable'\"}");
        }
    });

    // ---- REST API: DataPanel control ----
    asyncServer.on("/api/datapanel", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        // Return current DataPanel state (simplified - no direct state tracking)
        request->send(200, "application/json", "{\"ok\":true,\"state\":\"unknown\"}");
    });

    asyncServer.on("/api/datapanel", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        String action;
        if (!parsePostParam(request, "action", action)) return;
        if (action == "flicker")
        {
            uint8_t durationSec = 6;
            if (request->hasParam("duration", true) &&
                !parseDurationSeconds(request->getParam("duration", true)->value(), 1, 99, durationSec))
            {
                request->send(400, "application/json", "{\"error\":\"invalid duration, use 1-99 seconds\"}");
                return;
            }
            char duration[3];
            snprintf(duration, sizeof(duration), "%02u", durationSec);
            String cmd = "DP2" + String(duration) + "006";
            processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
            request->send(200, "application/json", "{\"ok\":true,\"action\":\"flicker\",\"duration\":" + String(durationSec) + "}");
        }
        else if (action == "disable")
        {
            uint8_t durationSec = 8;
            if (request->hasParam("duration", true) &&
                !parseDurationSeconds(request->getParam("duration", true)->value(), 1, 99, durationSec))
            {
                request->send(400, "application/json", "{\"error\":\"invalid duration, use 1-99 seconds\"}");
                return;
            }
            char duration[3];
            snprintf(duration, sizeof(duration), "%02u", durationSec);
            String cmd = "DP1" + String(duration) + "008";
            processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
            request->send(200, "application/json", "{\"ok\":true,\"action\":\"disable\",\"duration\":" + String(durationSec) + "}");
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"invalid action, use 'flicker' or 'disable'\"}");
        }
    });

    // ---- REST API: Panel calibration reset ----
    asyncServer.on("/api/panelcal/reset", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        String target;
        if (!parsePostParam(request, "target", target)) return;
        uint8_t tgt = 0;
        uint32_t mask = 0;
        if (!parseTwoDigitTarget(target.c_str(), tgt) || target.length() != 2 || !panelTargetToMask(tgt, mask))
        {
            request->send(400, "application/json", "{\"error\":\"invalid target (use 00-15)\"}");
            return;
        }

        uint16_t matchedServos = 0;
        uint16_t removedKeys = 0;
        for (uint16_t i = 0; i < servoDispatch.getNumServos(); i++)
        {
            uint32_t group = servoDispatch.getGroup(i);
            bool panelServo = (group & (SMALL_PANEL | MEDIUM_PANEL | BIG_PANEL | PIE_PANEL | TOP_PIE_PANEL | MINI_PANEL)) != 0;
            if (!panelServo || (group & mask) == 0) continue;

            matchedServos++;

            char openKey[8];
            char closeKey[8];
            snprintf(openKey, sizeof(openKey), "so%02u", i);
            snprintf(closeKey, sizeof(closeKey), "sc%02u", i);

            if (preferences.remove(openKey)) removedKeys++;
            if (preferences.remove(closeKey)) removedKeys++;
        }

        bool doReboot = true;
        if (request->hasParam("reboot", true))
        {
            String rebootVal = request->getParam("reboot", true)->value();
            rebootVal.toLowerCase();
            doReboot = (rebootVal == "1" || rebootVal == "true");
        }

        String json = "{\"ok\":true";
        json += ",\"target\":\"" + target + "\"";
        json += ",\"matched_servos\":" + String(matchedServos);
        json += ",\"removed_keys\":" + String(removedKeys);
        json += ",\"rebooting\":" + String(doReboot ? "true" : "false") + "}";
        request->send(200, "application/json", json);

        if (doReboot)
        {
            scheduleReboot(800);
        }
    });

    // ---- Firmware upload (OTA via web) ----
    asyncServer.on("/upload/firmware", HTTP_POST,
        // Response handler (called after upload completes)
        [](AsyncWebServerRequest *request)
        {
            otaInProgress = false;
            if (otaUploadFailed || Update.hasError())
            {
                if (otaUploadError.length() == 0)
                    otaUploadError = "firmware update failed";
                request->send(otaUploadHttpStatus, "application/json", otaJson(false, otaUploadError));
                FLD.selectSequence(LogicEngineDefaults::FAILURE);
                FLD.setTextMessage("Flash Fail");
                FLD.selectSequence(LogicEngineDefaults::TEXTSCROLLLEFT,
                                   LogicEngineRenderer::kRed, 1, 0);
            }
            else
            {
                request->send(200, "application/json", otaJson(true));
                scheduleReboot(1000);
            }
        },
        // Upload handler (called for each chunk)
        [](AsyncWebServerRequest *request, const String &filename,
           size_t index, uint8_t *data, size_t len, bool final)
        {
            if (index == 0)
            {
                // First chunk — start the update
                otaInProgress = true;
                otaUploadFailed = false;
                otaUploadHttpStatus = 500;
                otaUploadError = "";
                unmountFileSystems();
                FLD.selectSequence(LogicEngineDefaults::NORMAL);
                RLD.selectSequence(LogicEngineDefaults::NORMAL);
                FLD.setEffectWidthRange(0);
                RLD.setEffectWidthRange(0);
                logCapture.printf("Update: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
                {
                    Update.printError(Serial);
                    logCapture.println("Update begin failed");
                    markOtaUploadFailed(413, "firmware image rejected or too large for OTA slot");
                    return;
                }
            }
            if (otaUploadFailed)
            {
                return;
            }
            if (len)
            {
                if (Update.write(data, len) != len)
                {
                    Update.printError(Serial);
                    logCapture.println("Update write failed");
                    markOtaUploadFailed(413, "firmware image write failed or exceeded OTA slot");
                    return;
                }
                // Show progress on logic displays + broadcast to WS clients
                if (request->contentLength() > 0)
                {
                    float range = (float)(index + len) / (float)request->contentLength();
                    FLD.setEffectWidthRange(range);
                    RLD.setEffectWidthRange(range);
                    broadcastOtaProgress(range);
                }
            }
            if (final)
            {
                logCapture.printf("Update complete: %u bytes\n", index + len);
                if (otaUploadFailed)
                {
                    logCapture.println("Update aborted after previous write error");
                    return;
                }
                if (Update.end(true))
                {
                    logCapture.println("Update Success. Rebooting...");
                }
                else
                {
                    Update.printError(Serial);
                    markOtaUploadFailed(500, "firmware update finalize failed");
                }
            }
        });

    // ---- Filesystem upload (SPIFFS OTA via web) ----
    asyncServer.on("/upload/filesystem", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            otaInProgress = false;
            if (otaUploadFailed || Update.hasError())
            {
                if (otaUploadError.length() == 0)
                    otaUploadError = "filesystem update failed";
                request->send(otaUploadHttpStatus, "application/json", otaJson(false, otaUploadError));
                FLD.selectSequence(LogicEngineDefaults::FAILURE);
                FLD.setTextMessage("FS Flash Fail");
                FLD.selectSequence(LogicEngineDefaults::TEXTSCROLLLEFT,
                                   LogicEngineRenderer::kRed, 1, 0);
            }
            else
            {
                request->send(200, "application/json", otaJson(true));
                scheduleReboot(1000);
            }
        },
        [](AsyncWebServerRequest *request, const String &filename,
           size_t index, uint8_t *data, size_t len, bool final)
        {
            if (index == 0)
            {
                otaInProgress = true;
                otaUploadFailed = false;
                otaUploadHttpStatus = 500;
                otaUploadError = "";
                unmountFileSystems();
                FLD.selectSequence(LogicEngineDefaults::NORMAL);
                RLD.selectSequence(LogicEngineDefaults::NORMAL);
                FLD.setEffectWidthRange(0);
                RLD.setEffectWidthRange(0);
                logCapture.printf("Filesystem update: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS))
                {
                    Update.printError(Serial);
                    logCapture.println("Filesystem update begin failed");
                    markOtaUploadFailed(413, "filesystem image rejected or too large for SPIFFS partition");
                    return;
                }
            }
            if (otaUploadFailed)
            {
                return;
            }
            if (len)
            {
                if (Update.write(data, len) != len)
                {
                    Update.printError(Serial);
                    logCapture.println("Filesystem update write failed");
                    markOtaUploadFailed(413, "filesystem image write failed or exceeded SPIFFS partition");
                    return;
                }
                if (request->contentLength() > 0)
                {
                    float range = (float)(index + len) / (float)request->contentLength();
                    FLD.setEffectWidthRange(range);
                    RLD.setEffectWidthRange(range);
                    broadcastOtaProgress(range);
                }
            }
            if (final)
            {
                logCapture.printf("Filesystem update complete: %u bytes\n", index + len);
                if (otaUploadFailed)
                {
                    logCapture.println("Filesystem update aborted after previous write error");
                    return;
                }
                if (Update.end(true))
                {
                    logCapture.println("Filesystem update success. Rebooting...");
                }
                else
                {
                    Update.printError(Serial);
                    markOtaUploadFailed(500, "filesystem update finalize failed");
                }
            }
        });

    // Start the server
    asyncServer.begin();
    logCapture.println("[AsyncWeb] Server started on port 80");
}

// ---------------------------------------------------------------
// Call from eventLoopTask to clean up dead WS clients and
// broadcast new log lines
// ---------------------------------------------------------------
static void asyncWebLoop()
{
    ws.cleanupClients();

    if (rebootScheduled && (int32_t)(millis() - rebootAtMs) >= 0)
    {
        rebootScheduled = false;
        reboot();
        return;
    }

    if (ws.count() == 0)
        return;

    // Broadcast new log lines to WebSocket clients
    uint32_t totalLogs = logCapture.totalCount();
    if (totalLogs > lastLogCount)
    {
        uint32_t start = lastLogCount + 1;
        if (totalLogs - start + 1 > LOG_CAPTURE_MAX_LINES)
            start = totalLogs - LOG_CAPTURE_MAX_LINES + 1;

        for (uint32_t idx = start; idx <= totalLogs; idx++)
        {
            const char *line = logCapture.getLineByCount(idx);
            if (line[0] == '\0') continue;
            String json = "{\"type\":\"log\",\"line\":\"" + jsonEscape(String(line)) + "\"}";
            ws.textAll(json);
        }
        lastLogCount = totalLogs;
    }

    // Periodic state broadcast every 5 seconds
    uint32_t now = millis();
    if (now - lastStateBroadcast >= 5000)
    {
        broadcastState();
        lastStateBroadcast = now;
    }

    // Periodic health broadcast every 30 seconds
    // (I2C scan takes ~100ms so keep interval long)
    if (now - lastHealthBroadcast >= 30000)
    {
        refreshI2CHealthCache(false);
        String json = "{\"type\":\"health\",\"data\":" + buildHealthJson() + "}";
        ws.textAll(json);
        lastHealthBroadcast = now;
    }
}

// ---------------------------------------------------------------
// Broadcast state to all connected WebSocket clients
// ---------------------------------------------------------------
static void broadcastState()
{
    if (ws.count() > 0)
    {
        String json = "{\"type\":\"state\",\"data\":" + buildStateJson() + "}";
        ws.textAll(json);
    }
}

// ---------------------------------------------------------------
// Broadcast OTA progress to all connected WebSocket clients
// ---------------------------------------------------------------
static void broadcastOtaProgress(float progress)
{
    if (ws.count() > 0)
    {
        String json = "{\"type\":\"ota\",\"progress\":" + String(progress, 2) + "}";
        ws.textAll(json);
    }
}

// ---------------------------------------------------------------
// Get the LogCapture instance — used in .ino to redirect Serial
// ---------------------------------------------------------------
static LogCapture &getLogCapture()
{
    return logCapture;
}

#endif // ASYNC_WEB_INTERFACE_H
