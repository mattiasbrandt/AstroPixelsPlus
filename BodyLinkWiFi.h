#ifndef BODY_LINK_WIFI_H
#define BODY_LINK_WIFI_H

#include <WiFiUdp.h>
#ifdef USE_MDNS
#include <ESPmDNS.h>
#endif

enum BodyLinkTransport
{
    BODY_LINK_UART,
    BODY_LINK_WIFI,
    BODY_LINK_DISCONNECTED
};

enum BodyLinkPeerSource
{
    BODY_LINK_PEER_NONE,
    BODY_LINK_PEER_MANUAL,
    BODY_LINK_PEER_RX,
    BODY_LINK_PEER_MDNS
};

static const uint16_t kBodyLinkUdpPort = 4901;
static const uint16_t kBodyLinkRxBufLen = 64;
static const uint32_t kBodyLinkHeartbeatTimeoutMs = 5000;
static const uint32_t kBodyLinkMdnResolveCooldownMs = 10000;

static WiFiUDP sBodyUdp;
static bool sBodyWiFiEnabled = true;
static bool sBodyUdpBound = false;
static bool sBodyMdnsStarted = false;
static bool sArduinoOtaMdnsStarted = false;
static uint32_t sBodyLastSeenUartMs = 0;
static uint32_t sBodyLastSeenWifiMs = 0;
static uint32_t sBodyWifiHeartbeatRx = 0;
static uint32_t sBodyLastMdnsResolveMs = 0;
static IPAddress sBodyPeerIp(0, 0, 0, 0);
static BodyLinkPeerSource sBodyPeerSource = BODY_LINK_PEER_NONE;

static void processMarcduinoCommandWithSourceMain(const char *source, const char *cmd);

static bool bodyLinkIpKnown(const IPAddress &ip)
{
    return !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

static const char *bodyLinkPeerSourceName(BodyLinkPeerSource source)
{
    switch (source)
    {
    case BODY_LINK_PEER_MANUAL:
        return "manual";
    case BODY_LINK_PEER_RX:
        return "rx";
    case BODY_LINK_PEER_MDNS:
        return "mdns";
    default:
        return "none";
    }
}

static bool bodyLinkHasManualPeer()
{
    return sBodyPeerSource == BODY_LINK_PEER_MANUAL;
}

static void bodyLinkSetPeer(const IPAddress &ip, BodyLinkPeerSource source)
{
    if (!bodyLinkIpKnown(ip))
        return;

    if (bodyLinkHasManualPeer() && source != BODY_LINK_PEER_MANUAL)
        return;

    if (sBodyPeerIp == ip && sBodyPeerSource == source)
        return;

    sBodyPeerIp = ip;
    sBodyPeerSource = source;
    DEBUG_PRINT(F("[BodyLink] WiFi peer set to "));
    DEBUG_PRINT(ip);
    DEBUG_PRINT(F(" ("));
    DEBUG_PRINT(bodyLinkPeerSourceName(source));
    DEBUG_PRINTLN(F(")"));
}

static void bodyLinkMarkUartActivity(uint32_t now)
{
    sBodyLastSeenMs = now;
}

static void bodyLinkMarkUartHeartbeat(uint32_t now)
{
    sBodyLastSeenMs = now;
    sBodyLastSeenUartMs = now;
    sBodyHeartbeatRx++;
}

static void bodyLinkMarkWifiActivity(uint32_t now)
{
    sBodyLastSeenMs = now;
}

static void bodyLinkMarkWifiHeartbeat(uint32_t now)
{
    sBodyLastSeenMs = now;
    sBodyLastSeenWifiMs = now;
    sBodyHeartbeatRx++;
    sBodyWifiHeartbeatRx++;
}

static bool bodyLinkReadManualPeer()
{
    if (!preferences.isKey(PREFERENCE_BODY_PEER_IP))
    {
        if (bodyLinkHasManualPeer())
        {
            sBodyPeerIp = IPAddress(0, 0, 0, 0);
            sBodyPeerSource = BODY_LINK_PEER_NONE;
        }
        return false;
    }

    String manualIp = preferences.getString(PREFERENCE_BODY_PEER_IP, "");
    manualIp.trim();
    if (manualIp.length() == 0)
    {
        if (bodyLinkHasManualPeer())
        {
            sBodyPeerIp = IPAddress(0, 0, 0, 0);
            sBodyPeerSource = BODY_LINK_PEER_NONE;
        }
        return false;
    }

    IPAddress parsed;
    if (!parsed.fromString(manualIp))
    {
        DEBUG_PRINT(F("[BodyLink] Invalid manual peer IP: "));
        DEBUG_PRINTLN(manualIp);
        return false;
    }

    bodyLinkSetPeer(parsed, BODY_LINK_PEER_MANUAL);
    return true;
}

static void bodyLinkWiFiInit()
{
    if (!preferences.getBool(PREFERENCE_BODY_LINK_ENABLED, BODY_LINK_ENABLED))
        return;

    sBodyWiFiEnabled = preferences.getBool(PREFERENCE_BODY_WIFI_ENABLED, BODY_WIFI_ENABLED);
    if (!sBodyWiFiEnabled)
    {
        DEBUG_PRINTLN(F("[BodyLink] WiFi fallback disabled by preference"));
        return;
    }

    bodyLinkReadManualPeer();

    if (!wifiActive)
        return;

    if (!sBodyUdpBound)
    {
        if (sBodyUdp.begin(kBodyLinkUdpPort))
        {
            sBodyUdpBound = true;
            DEBUG_PRINT(F("[BodyLink] UDP listener started on port "));
            DEBUG_PRINTLN(kBodyLinkUdpPort);
        }
        else
        {
            DEBUG_PRINTLN(F("[BodyLink] Failed to start UDP listener"));
        }
    }
}

static void bodyLinkSetupMDNS()
{
#ifdef USE_MDNS
    if (!sBodyMdnsStarted)
    {
        if (!MDNS.begin("astropixelsplus"))
        {
            DEBUG_PRINTLN(F("[BodyLink] Error setting up MDNS responder"));
            return;
        }
        sBodyMdnsStarted = true;
        DEBUG_PRINTLN(F("[BodyLink] mDNS hostname: astropixelsplus"));
    }

    if (!sArduinoOtaMdnsStarted)
    {
        MDNS.enableArduino(3232, false);
        sArduinoOtaMdnsStarted = true;
        DEBUG_PRINTLN(F("[BodyLink] mDNS Arduino OTA service enabled"));
    }

    if (!preferences.getBool(PREFERENCE_BODY_LINK_ENABLED, BODY_LINK_ENABLED))
        return;

    MDNS.addService("marcduino", "udp", kBodyLinkUdpPort);
#endif
}

static void bodyLinkResolvePeer()
{
#ifdef USE_MDNS
    if (!sBodyWiFiEnabled || !wifiActive)
        return;
    if (!sBodyUdpBound)
        return;
    if (bodyLinkHasManualPeer())
        return;
    if (sBodyPeerSource == BODY_LINK_PEER_RX)
        return;

    uint32_t now = millis();
    if (sBodyLastMdnsResolveMs != 0 && (now - sBodyLastMdnsResolveMs) < kBodyLinkMdnResolveCooldownMs)
        return;

    sBodyLastMdnsResolveMs = now;
    IPAddress ip = MDNS.queryHost("protoartoo");
    if (bodyLinkIpKnown(ip))
    {
        bodyLinkSetPeer(ip, BODY_LINK_PEER_MDNS);
    }
#endif
}

static void bodyLinkWiFiRx()
{
    if (!sBodyWiFiEnabled || !sBodyUdpBound)
        return;

    for (;;)
    {
        int packetLen = sBodyUdp.parsePacket();
        if (packetLen <= 0)
            break;

        IPAddress remote = sBodyUdp.remoteIP();
        if (!bodyLinkHasManualPeer() && bodyLinkIpKnown(remote))
        {
            bodyLinkSetPeer(remote, BODY_LINK_PEER_RX);
        }

        char packetBuf[kBodyLinkRxBufLen + 1];
        int readLen = sBodyUdp.read(packetBuf, kBodyLinkRxBufLen);
        if (readLen < 0)
            continue;
        packetBuf[readLen] = '\0';

        while (sBodyUdp.available() > 0)
        {
            sBodyUdp.read();
        }

        char lineBuf[kBodyLinkRxBufLen + 1];
        uint8_t lineLen = 0;
        for (int i = 0; i <= readLen; i++)
        {
            char c = packetBuf[i];
            bool lineEnd = (c == '\0' || c == '\r' || c == '\n');
            if (lineEnd)
            {
                if (lineLen == 0)
                    continue;

                lineBuf[lineLen] = '\0';
                uint32_t now = millis();
                if (strcmp(lineBuf, "#PAHB") == 0)
                {
                    bodyLinkMarkWifiHeartbeat(now);
                }
                else
                {
                    bodyLinkMarkWifiActivity(now);
                    processMarcduinoCommandWithSourceMain("body-link-wifi", lineBuf);
                }
                lineLen = 0;
                continue;
            }

            if (lineLen >= kBodyLinkRxBufLen)
            {
                DEBUG_PRINTLN(F("[BodyLink] UDP command overflow; dropping line"));
                lineLen = 0;
                continue;
            }

            lineBuf[lineLen++] = c;
        }
    }
}

static bool bodyLinkWiFiSendUDP(const char *payload)
{
    if (!sBodyWiFiEnabled || !sBodyUdpBound || payload == nullptr || payload[0] == '\0')
        return false;
    if (!bodyLinkIpKnown(sBodyPeerIp))
        return false;

    size_t len = strlen(payload);
    bool hasTerminator = (len > 0 && (payload[len - 1] == '\r' || payload[len - 1] == '\n'));

    if (!sBodyUdp.beginPacket(sBodyPeerIp, kBodyLinkUdpPort))
        return false;

    sBodyUdp.write((const uint8_t *)payload, len);
    if (!hasTerminator)
        sBodyUdp.write('\r');

    return sBodyUdp.endPacket() == 1;
}

static BodyLinkTransport bodyLinkActiveTransport()
{
    if (!preferences.getBool(PREFERENCE_BODY_LINK_ENABLED, BODY_LINK_ENABLED))
        return BODY_LINK_DISCONNECTED;

    uint32_t now = millis();
    if (sBodyLastSeenUartMs > 0 && (now - sBodyLastSeenUartMs) < kBodyLinkHeartbeatTimeoutMs)
        return BODY_LINK_UART;

    if (sBodyWiFiEnabled && wifiActive && bodyLinkIpKnown(sBodyPeerIp))
        return BODY_LINK_WIFI;

    return BODY_LINK_DISCONNECTED;
}

static const char *bodyLinkGetTransportName()
{
    switch (bodyLinkActiveTransport())
    {
    case BODY_LINK_UART:
        return "uart";
    case BODY_LINK_WIFI:
        return "wifi";
    default:
        return "disconnected";
    }
}

static bool bodyLinkWifiEnabled()
{
    return sBodyWiFiEnabled;
}

static String bodyLinkGetPeerIP()
{
    return bodyLinkIpKnown(sBodyPeerIp) ? sBodyPeerIp.toString() : String("");
}

static const char *bodyLinkGetPeerSource()
{
    return bodyLinkPeerSourceName(sBodyPeerSource);
}

static uint32_t bodyLinkUartHeartbeatAgeMs()
{
    if (sBodyLastSeenUartMs == 0)
        return 0;
    return millis() - sBodyLastSeenUartMs;
}

static uint32_t bodyLinkWifiHeartbeatAgeMs()
{
    if (sBodyLastSeenWifiMs == 0)
        return 0;
    return millis() - sBodyLastSeenWifiMs;
}

#endif // BODY_LINK_WIFI_H
