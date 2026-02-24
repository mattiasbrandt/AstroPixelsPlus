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
#include "LogCapture.h"

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
extern volatile uint32_t sArtooLastSignalMs;
extern volatile uint32_t sArtooSignalBursts;
extern portMUX_TYPE sArtooTelemetryMux;

#ifdef USE_DROID_REMOTE
extern bool sRemoteConnected;
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
static String cachedI2CDevicesJson = "[]";

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
           key == "mrandommax" || key == "msoundlocal" || key == "apitoken";
}

static size_t maxPrefValueLen(const String &key)
{
    if (key == "artoobaud") return 8;
    if (key == "ssid" || key == "rhost") return 32;
    if (key == "pass" || key == "rsecret" || key == "apitoken") return 64;
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
    String json = "{";
    json += "\"wifiEnabled\":" + String(wifiEnabled ? "true" : "false");
    json += ",\"remoteEnabled\":" + String(remoteEnabled ? "true" : "false");
    json += ",\"artooEnabled\":" + String(artooEnabled ? "true" : "false");
    json += ",\"artooBaud\":" + String(artooBaud);
    json += ",\"soundLocalEnabled\":" + String(soundLocalEnabled ? "true" : "false");
#ifdef USE_DROID_REMOTE
    json += ",\"remoteConnected\":" + String(sRemoteConnected ? "true" : "false");
#endif
    json += ",\"otaInProgress\":" + String(otaInProgress ? "true" : "false");
    json += ",\"uptime\":" + String(millis() / 1000);
    json += ",\"freeHeap\":" + String(ESP.getFreeHeap());

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
static bool probeI2C(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}

static void refreshI2CHealthCache(bool force = false)
{
    uint32_t now = millis();
    if (!force && (now - lastI2CScanMs) < 30000u)
        return;

    cachedPanelsOk = probeI2C(0x40);
    cachedHolosOk = probeI2C(0x41);

    String devices = "[";
    bool firstDev = true;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            if (!firstDev) devices += ",";
            firstDev = false;
            devices += "\"0x" + String(addr, HEX) + "\"";
        }
    }
    devices += "]";
    cachedI2CDevicesJson = devices;
    lastI2CScanMs = now;
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

    // I2C device probes
    refreshI2CHealthCache();
    bool panelsOk = cachedPanelsOk;
    bool holosOk  = cachedHolosOk;
    json += "\"i2c_panels\":" + String(panelsOk ? "true" : "false");
    json += ",\"i2c_holos\":" + String(holosOk ? "true" : "false");

    // Sound module — check if not disabled
    // sMarcSound is the global MarcSound instance in .ino
    // We can't easily check module state from here without another extern,
    // so we report it based on preference config
    String soundPref = preferences.getString("msound", "0");
    bool soundEnabled = (soundLocalEnabled && soundPref != "0");
    json += ",\"sound_module\":" + String(soundEnabled ? "true" : "false");
    json += ",\"sound_local_enabled\":" + String(soundLocalEnabled ? "true" : "false");

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
                if (key == "wifi" || key == "ap" || key == "remote" || key == "artoo" || key == "msoundlocal")
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
        refreshI2CHealthCache(true);
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
