#ifndef ASYNC_WEB_INTERFACE_H
#define ASYNC_WEB_INTERFACE_H

// AsyncWebInterface.h — Replaces WebPages.h
// Registers ESPAsyncWebServer routes, REST API, WebSocket, and OTA upload.
// All ReelTwo core (Marcduino, ServoDispatch, LogicEngine, etc.) stays unchanged.

#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <Wire.h>
#include "SPIFFS.h"

// Forward declarations — these are defined in AstroPixelsPlus.ino
extern void reboot();
extern void unmountFileSystems();

// Globals defined in .ino that we need access to
extern AnimationPlayer player;
extern Preferences preferences;
extern bool wifiEnabled;
extern bool remoteEnabled;
extern bool otaInProgress;

#ifdef USE_DROID_REMOTE
extern bool sRemoteConnected;
#endif

// ESPAsyncWebServer objects — allocated once, no static init heap issues
static AsyncWebServer asyncServer(80);
static AsyncWebSocket ws("/ws");

// ---------------------------------------------------------------
// WebSocket event handler
// ---------------------------------------------------------------
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("[WS] Client #%u connected from %s\n", client->id(),
                      client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        // Could handle incoming WS messages (commands) here in future
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
        {
            data[len] = 0;
            // Treat incoming WS text as Marcduino command
            Marcduino::processCommand(player, (const char *)data);
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
#ifdef USE_DROID_REMOTE
    json += ",\"remoteConnected\":" + String(sRemoteConnected ? "true" : "false");
#endif
    json += ",\"uptime\":" + String(millis() / 1000);
    json += ",\"freeHeap\":" + String(ESP.getFreeHeap());
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

// ---------------------------------------------------------------
// Build health JSON string
// ---------------------------------------------------------------
static String buildHealthJson()
{
    String json = "{";

    // I2C device probes
    bool panelsOk = probeI2C(0x40);   // PCA9685 — dome panels
    bool holosOk  = probeI2C(0x41);   // PCA9685 — holo servos
    json += "\"i2c_panels\":" + String(panelsOk ? "true" : "false");
    json += ",\"i2c_holos\":" + String(holosOk ? "true" : "false");

    // Sound module — check if not disabled
    // sMarcSound is the global MarcSound instance in .ino
    // We can't easily check module state from here without another extern,
    // so we report it based on preference config
    String soundPref = preferences.getString("msound", "0");
    bool soundEnabled = (soundPref != "0");
    json += ",\"sound_module\":" + String(soundEnabled ? "true" : "false");

    // WiFi
    json += ",\"wifi\":" + String(wifiEnabled ? "true" : "false");

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

    // I2C scan — list all responding addresses
    json += ",\"i2c_devices\":[";
    bool firstDev = true;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            if (!firstDev) json += ",";
            firstDev = false;
            json += "\"0x" + String(addr, HEX) + "\"";
        }
    }
    json += "]";

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
        if (request->hasParam("cmd", true))
        {
            String cmd = request->getParam("cmd", true)->value();
            Serial.printf("[API] cmd: %s\n", cmd.c_str());
            Marcduino::processCommand(player, cmd.c_str());
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

    // ---- REST API: Read preferences ----
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
                if (!first) json += ",";
                first = false;
                String val = preferences.getString(key.c_str(), "");
                // Escape quotes in value
                val.replace("\"", "\\\"");
                json += "\"" + key + "\":\"" + val + "\"";
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

            // Special key: factory reset
            if (key == "_clear")
            {
                Serial.println("[API] Factory reset — clearing all preferences");
                preferences.clear();
            }
            else
            {
                preferences.putString(key.c_str(), val);
                Serial.printf("[API] pref: %s = %s\n", key.c_str(), val.c_str());
            }

            bool needsReboot = request->hasParam("reboot", true) &&
                               request->getParam("reboot", true)->value() == "1";
            request->send(200, "application/json", "{\"ok\":true}");
            if (needsReboot)
            {
                delay(500);
                reboot();
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
        request->send(200, "application/json", "{\"ok\":true,\"msg\":\"rebooting\"}");
        delay(500);
        reboot();
    });

    // ---- Firmware upload (OTA via web) ----
    asyncServer.on("/upload/firmware", HTTP_POST,
        // Response handler (called after upload completes)
        [](AsyncWebServerRequest *request)
        {
            otaInProgress = false;
            if (Update.hasError())
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
                delay(1000);
                preferences.end();
                ESP.restart();
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
                unmountFileSystems();
                FLD.selectSequence(LogicEngineDefaults::NORMAL);
                RLD.selectSequence(LogicEngineDefaults::NORMAL);
                FLD.setEffectWidthRange(0);
                RLD.setEffectWidthRange(0);
                Serial.printf("Update: %s\n", filename.c_str());
                if (!Update.begin(request->contentLength()))
                {
                    Update.printError(Serial);
                }
            }
            if (len)
            {
                if (Update.write(data, len) != len)
                {
                    Update.printError(Serial);
                }
                // Show progress on logic displays
                if (request->contentLength() > 0)
                {
                    float range = (float)(index + len) / (float)request->contentLength();
                    FLD.setEffectWidthRange(range);
                    RLD.setEffectWidthRange(range);
                }
            }
            if (final)
            {
                Serial.printf("Update complete: %u bytes\n", index + len);
                if (Update.end(true))
                {
                    Serial.println("Update Success. Rebooting...");
                }
                else
                {
                    Update.printError(Serial);
                }
            }
        });

    // Start the server
    asyncServer.begin();
    Serial.println("[AsyncWeb] Server started on port 80");
}

// ---------------------------------------------------------------
// Call from eventLoopTask to clean up dead WS clients periodically
// ---------------------------------------------------------------
static void asyncWebLoop()
{
    ws.cleanupClients();
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

#endif // ASYNC_WEB_INTERFACE_H
