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
#include "SPIFFS.h"
#include "SPIFFS.h"
#include "LogCapture.h"

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
extern bool artooEnabled;
extern int artooBaud;
extern bool soundLocalEnabled;
extern bool sSleepModeActive;
extern uint32_t sSleepModeSinceMs;
extern uint32_t sMinFreeHeap;
extern volatile uint32_t sArtooLastSignalMs;
extern volatile uint32_t sArtooSignalBursts;
extern portMUX_TYPE sArtooTelemetryMux;
extern bool shouldBlockCommandDuringSleep(const char *cmd);
extern bool enterSoftSleepMode();
extern bool exitSoftSleepMode();
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
static LogCapture logCapture(Serial);
static uint32_t lastLogCount = 0;

// Broadcast timers
static uint32_t lastStateBroadcast = 0;
static uint32_t lastHealthBroadcast = 0;
static bool authWarningLogged = false;
static bool rebootScheduled = false;
static uint32_t rebootAtMs = 0;
static bool otaUploadFailed = false;
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
    out[len] = '\0';
    return true;
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
    Marcduino::processCommand(player, cmd);
}

static bool isSensitivePrefKey(const String &key)
{
    return key == "pass" || key == "rsecret";
}

static bool isAllowedPrefKey(const String &key)
{
    return key == "wifi" || key == "ap" || key == "ssid" || key == "pass" ||
           key == "remote" || key == "rhost" || key == "rsecret" ||
           key == "artoo" || key == "artoobaud" ||
           key == "mserial2" || key == "mserialpass" || key == "mserial" ||
           key == "mwifi" || key == "mwifipass" ||
           key == "msound" || key == "msoundser" || key == "mvolume" ||
           key == "msoundstart" || key == "mrandom" || key == "mrandommin" ||
           key == "mrandommax" || key == "msoundlocal" || key == "apitoken" ||
           key == "dname";
}

static size_t maxPrefValueLen(const String &key)
{
    if (key == "artoobaud") return 8;
    if (key == "ssid" || key == "rhost") return 32;
    if (key == "pass" || key == "rsecret" || key == "apitoken") return 64;
    if (key == "dname") return 24;
    return 16;
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

static bool checkWriteAuth(AsyncWebServerRequest *request)
{
    String token = preferences.getString("apitoken", "");
    if (token.length() == 0)
    {
        if (!authWarningLogged)
        {
            logCapture.println("[Auth] Write API token not configured; write endpoints are open");
            authWarningLogged = true;
        }
        return true;
    }

    if (request->hasHeader("X-AP-Token") && request->getHeader("X-AP-Token")->value() == token)
        return true;

    if (request->hasParam("token", true) && request->getParam("token", true)->value() == token)
        return true;

    return false;
}

// ---------------------------------------------------------------
// WebSocket event handler
// ---------------------------------------------------------------
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        logCapture.printf("[WS] Client #%u connected from %s\n", client->id(),
                         client->remoteIP().toString().c_str());
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
    uint32_t serial2LastSeenMs;
    uint32_t serial2SignalBursts;
    portENTER_CRITICAL(&sArtooTelemetryMux);
    serial2LastSeenMs = sArtooLastSignalMs;
    serial2SignalBursts = sArtooSignalBursts;
    portEXIT_CRITICAL(&sArtooTelemetryMux);

    String json = "{";
    json.reserve(512);
    String droidName = getConfiguredDroidName();
    json += "\"wifiEnabled\":" + String(wifiEnabled ? "true" : "false");
    json += ",\"remoteEnabled\":" + String(remoteEnabled ? "true" : "false");
    json += ",\"artooEnabled\":" + String(artooEnabled ? "true" : "false");
    json += ",\"artooBaud\":" + String(artooBaud);
    json += ",\"soundLocalEnabled\":" + String(soundLocalEnabled ? "true" : "false");
    json += ",\"sleepMode\":" + String(sSleepModeActive ? "true" : "false");
    json += ",\"sleepSinceMs\":" + String(sSleepModeSinceMs);
    String soundPref = preferences.getString("msound", "0");
    bool soundModuleEnabled = (soundLocalEnabled && soundPref != "0");
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
    json += ",\"serial2LastSeenMs\":" + String(serial2LastSeenMs);
    json += ",\"serial2SignalBursts\":" + String(serial2SignalBursts);
    json += ",\"i2c_probe_failures\":" + String(i2cProbeFailures);
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
    json.reserve(768);

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
    String soundPref = preferences.getString("msound", "0");
    bool soundEnabled = (soundLocalEnabled && soundPref != "0");
    json += ",\"sound_module\":" + String(soundEnabled ? "true" : "false");
    json += ",\"sound_local_enabled\":" + String(soundLocalEnabled ? "true" : "false");
    json += ",\"sleep_mode\":" + String(sSleepModeActive ? "true" : "false");
    json += ",\"sleep_since_ms\":" + String(sSleepModeSinceMs);

    // WiFi
    json += ",\"wifi\":" + String(wifiEnabled ? "true" : "false");

    uint32_t nowMs = millis();
    uint32_t lastMs;
    uint32_t artooBursts;
    portENTER_CRITICAL(&sArtooTelemetryMux);
    lastMs = sArtooLastSignalMs;
    artooBursts = sArtooSignalBursts;
    portEXIT_CRITICAL(&sArtooTelemetryMux);
    uint32_t deltaMs = (lastMs > 0 && nowMs >= lastMs) ? (nowMs - lastMs) : 0xFFFFFFFFu;
    bool artooLink = artooEnabled && (deltaMs <= 5000u);
    json += ",\"artoo\":" + String(artooLink ? "true" : "false");
    json += ",\"artoo_enabled\":" + String(artooEnabled ? "true" : "false");
    json += ",\"artoo_baud\":" + String(artooBaud);
    json += ",\"artoo_last_seen_ms\":" + String(lastMs);
    json += ",\"artoo_signal_bursts\":" + String(artooBursts);

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

    json += ",\"i2c_devices\":" + cachedI2CDevicesJson;
    json += ",\"min_free_heap\":" + String(sMinFreeHeap);
    json += ",\"i2c_probe_failures\":" + String(i2cProbeFailures);

    // Gadget status
    json += ",\"gadgets\":{";
#if AP_ENABLE_BADMOTIVATOR
    bool badmotEnabled = preferences.getBool(PREFERENCE_BADMOTIVATOR_ENABLED, false);
    json += "\"badmotivator\":{\"enabled\":" + String(badmotEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += "\"badmotivator\":{\"enabled\":false,\"present\":false}";
#endif
#if AP_ENABLE_FIRESTRIP
    bool firestripEnabled = preferences.getBool(PREFERENCE_FIRESTRIP_ENABLED, false);
    json += ",\"firestrip\":{\"enabled\":" + String(firestripEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += ",\"firestrip\":{\"enabled\":false,\"present\":false}";
#endif
#if AP_ENABLE_CBI
    bool cbiEnabled = preferences.getBool(PREFERENCE_CBI_ENABLED, false);
    json += ",\"cbi\":{\"enabled\":" + String(cbiEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += ",\"cbi\":{\"enabled\":false,\"present\":false}";
#endif
#if AP_ENABLE_DATAPANEL
    bool datapanelEnabled = preferences.getBool(PREFERENCE_DATAPANEL_ENABLED, false);
    json += ",\"datapanel\":{\"enabled\":" + String(datapanelEnabled ? "true" : "false") + ",\"present\":true}";
#else
    json += ",\"datapanel\":{\"enabled\":false,\"present\":false}";
#endif
    json += "}";

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
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        if (request->hasParam("cmd", true))
        {
            String cmd = request->getParam("cmd", true)->value();
            cmd.trim();
            if (!isValidCommandString(cmd))
            {
                request->send(400, "application/json", "{\"error\":\"invalid cmd\"}");
                return;
            }
            if (shouldBlockCommandDuringSleep(cmd.c_str()))
            {
                request->send(423, "application/json", "{\"error\":\"sleeping\",\"hint\":\"POST /api/wake\"}");
                return;
            }
            logCapture.printf("[API] cmd=%s len=%u\n", cmd.c_str(), (unsigned int)cmd.length());
            processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
            request->send(200, "application/json", "{\"ok\":true}");
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"missing cmd param\"}");
        }
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
                // Boolean keys: firmware uses getBool/putBool for these
                if (key == "wifi" || key == "ap" || key == "remote" || key == "artoo" || key == "msoundlocal" ||
                    key == PREFERENCE_BADMOTIVATOR_ENABLED || key == PREFERENCE_FIRESTRIP_ENABLED ||
                    key == PREFERENCE_CBI_ENABLED || key == PREFERENCE_DATAPANEL_ENABLED)
                {
                    bool val = preferences.getBool(key.c_str(), false);
                    json += "\"" + jsonEscape(key) + "\":" + (val ? "true" : "false");
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
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
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
            else if (key == "wifi" || key == "ap" || key == "remote" || key == "artoo" || key == "msoundlocal")
            {
                bool bval = (val == "1" || val == "true");
                preferences.putBool(key.c_str(), bval);
                if (key == "msoundlocal")
                {
                    soundLocalEnabled = bval;
                }
                // Gadget preferences
                else if (key == PREFERENCE_BADMOTIVATOR_ENABLED || key == PREFERENCE_FIRESTRIP_ENABLED ||
                         key == PREFERENCE_CBI_ENABLED || key == PREFERENCE_DATAPANEL_ENABLED)
                {
                    // Just store the preference value, actual enable/disable happens on next reboot
                    // or could be handled dynamically if needed
                    logCapture.printf("[API] pref (gadget): %s = %s\n", key.c_str(), bval ? "true" : "false");
                }
                logCapture.printf("[API] pref (bool): %s = %s\n", key.c_str(), bval ? "true" : "false");
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

    // ---- REST API: Reboot ----
    asyncServer.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        request->send(200, "application/json", "{\"ok\":true,\"msg\":\"rebooting\"}");
        scheduleReboot(500);
    });

    asyncServer.on("/api/sleep", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        bool changed = enterSoftSleepMode();
        request->send(200, "application/json", changed
            ? "{\"ok\":true,\"sleepMode\":true,\"changed\":true}"
            : "{\"ok\":true,\"sleepMode\":true,\"changed\":false}");
        broadcastState();
    });

    asyncServer.on("/api/wake", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        bool changed = exitSoftSleepMode();
        request->send(200, "application/json", changed
            ? "{\"ok\":true,\"sleepMode\":false,\"changed\":true}"
            : "{\"ok\":true,\"sleepMode\":false,\"changed\":false}");
        broadcastState();
    });

    // ---- REST API: Smoke control ----
    asyncServer.on("/api/smoke", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        if (request->hasParam("state", true))
        {
            String state = request->getParam("state", true)->value();
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
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"missing state param\"}");
        }
    });

    // ---- REST API: Fire effects control ----
    asyncServer.on("/api/fire", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        if (request->hasParam("state", true))
        {
            String state = request->getParam("state", true)->value();
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
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"missing state param\"}");
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
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        if (request->hasParam("action", true))
        {
            String action = request->getParam("action", true)->value();
            if (action == "flicker")
            {
                String duration = request->hasParam("duration", true) ? 
                    request->getParam("duration", true)->value() : "6";
                String cmd = "CB2" + duration + "006";
                processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
                request->send(200, "application/json", "{\"ok\":true,\"action\":\"flicker\",\"duration\":\"" + duration + "\"}");
            }
            else if (action == "disable")
            {
                String duration = request->hasParam("duration", true) ? 
                    request->getParam("duration", true)->value() : "8";
                String cmd = "CB1" + duration + "008";
                processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
                request->send(200, "application/json", "{\"ok\":true,\"action\":\"disable\",\"duration\":\"" + duration + "\"}");
            }
            else
            {
                request->send(400, "application/json", "{\"error\":\"invalid action, use 'flicker' or 'disable'\"}");
            }
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"missing action param\"}");
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
        if (!checkWriteAuth(request))
        {
            request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
            return;
        }
        if (request->hasParam("action", true))
        {
            String action = request->getParam("action", true)->value();
            if (action == "flicker")
            {
                String duration = request->hasParam("duration", true) ? 
                    request->getParam("duration", true)->value() : "6";
                String cmd = "DP2" + duration + "006";
                processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
                request->send(200, "application/json", "{\"ok\":true,\"action\":\"flicker\",\"duration\":\"" + duration + "\"}");
            }
            else if (action == "disable")
            {
                String duration = request->hasParam("duration", true) ? 
                    request->getParam("duration", true)->value() : "8";
                String cmd = "DP1" + duration + "008";
                processMarcduinoCommandWithSource("astropixel-web-api", cmd.c_str());
                request->send(200, "application/json", "{\"ok\":true,\"action\":\"disable\",\"duration\":\"" + duration + "\"}");
            }
            else
            {
                request->send(400, "application/json", "{\"error\":\"invalid action, use 'flicker' or 'disable'\"}");
            }
        }
        else
        {
            request->send(400, "application/json", "{\"error\":\"missing action param\"}");
        }
    });

    // ---- Firmware upload (OTA via web) ----
    asyncServer.on("/upload/firmware", HTTP_POST,
        // Response handler (called after upload completes)
        [](AsyncWebServerRequest *request)
        {
            if (!checkWriteAuth(request))
            {
                request->send(401, "text/plain", "Unauthorized");
                return;
            }
            otaInProgress = false;
            if (otaUploadFailed || Update.hasError())
            {
                request->send(500, "text/plain", "Update FAILED");
                FLD.selectSequence(LogicEngineDefaults::FAILURE);
                FLD.setTextMessage("Flash Fail");
                FLD.selectSequence(LogicEngineDefaults::TEXTSCROLLLEFT,
                                   LogicEngineRenderer::kRed, 1, 0);
            }
            else
            {
                request->send(200, "text/plain", "Update OK - Rebooting...");
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
                unmountFileSystems();
                FLD.selectSequence(LogicEngineDefaults::NORMAL);
                RLD.selectSequence(LogicEngineDefaults::NORMAL);
                FLD.setEffectWidthRange(0);
                RLD.setEffectWidthRange(0);
                logCapture.printf("Update: %s\n", filename.c_str());
                if (!Update.begin(request->contentLength()))
                {
                    Update.printError(Serial);
                    logCapture.println("Update begin failed");
                    otaUploadFailed = true;
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
                    otaUploadFailed = true;
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
                    otaUploadFailed = true;
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
