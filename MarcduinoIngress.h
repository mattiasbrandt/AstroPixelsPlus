#ifndef MARCDUINO_INGRESS_H
#define MARCDUINO_INGRESS_H

enum MarcduinoIngressTransportKind
{
    MARCDUINO_INGRESS_WEB_API,
    MARCDUINO_INGRESS_WEB_SOCKET,
    MARCDUINO_INGRESS_USB_SERIAL,
    MARCDUINO_INGRESS_BODY_LINK_UART,
    MARCDUINO_INGRESS_BODY_LINK_WIFI,
    MARCDUINO_INGRESS_WIFI_MARCDUINO,
    MARCDUINO_INGRESS_I2C_SLAVE,
    MARCDUINO_INGRESS_INTERNAL
};

struct MarcduinoIngressSource
{
    const char *label;
    MarcduinoIngressTransportKind transport;
    bool suppressBodyLinkEgress;
    bool fromRemotePeer;
};

static const MarcduinoIngressSource kMarcduinoIngressWebApi = {
    "astropixel-web-api",
    MARCDUINO_INGRESS_WEB_API,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressWebSocket = {
    "astropixel-web-ws",
    MARCDUINO_INGRESS_WEB_SOCKET,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressUsbSerial = {
    "usb-serial",
    MARCDUINO_INGRESS_USB_SERIAL,
    false,
    false
};

static const MarcduinoIngressSource kMarcduinoIngressBodyLinkUart = {
    "body-link-uart",
    MARCDUINO_INGRESS_BODY_LINK_UART,
    true,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressBodyLinkWifi = {
    "body-link-wifi",
    MARCDUINO_INGRESS_BODY_LINK_WIFI,
    true,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressWifiMarcduino = {
    "wifi-marcduino",
    MARCDUINO_INGRESS_WIFI_MARCDUINO,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressI2CSlave = {
    "i2c-slave",
    MARCDUINO_INGRESS_I2C_SLAVE,
    false,
    true
};

static const MarcduinoIngressSource kMarcduinoIngressInternal = {
    "internal",
    MARCDUINO_INGRESS_INTERNAL,
    false,
    false
};

static const char *marcduinoIngressSourceLabel(const MarcduinoIngressSource &source)
{
    return source.label ? source.label : "unknown";
}

static bool marcduinoIngressSuppressesBodyLinkEgress(const MarcduinoIngressSource &source)
{
    return source.suppressBodyLinkEgress;
}

static void marcduinoIngressAdmit(const MarcduinoIngressSource &source, const char *cmd);
static bool enqueueMarcduinoCommand(const char *source, const char *cmd, bool suppressBodyLinkEgress = false);
static void drainMarcduinoCommandQueue();

#endif // MARCDUINO_INGRESS_H

#ifdef MARCDUINO_INGRESS_IMPLEMENTATION
#ifndef MARCDUINO_INGRESS_IMPLEMENTATION_INCLUDED
#define MARCDUINO_INGRESS_IMPLEMENTATION_INCLUDED

struct PendingMarcduinoCommand
{
    char source[24];
    char cmd[CONSOLE_BUFFER_SIZE];
    bool suppressBodyLinkEgress;
};

static portMUX_TYPE sMarcduinoQueueMux = portMUX_INITIALIZER_UNLOCKED;
static PendingMarcduinoCommand sMarcduinoQueue[8];
static volatile uint8_t sMarcduinoQueueHead = 0;
static volatile uint8_t sMarcduinoQueueTail = 0;
static volatile uint8_t sMarcduinoQueueCount = 0;
static uint32_t sMarcduinoQueueFullCount = 0;

static bool enqueueMarcduinoCommand(const char *source, const char *cmd, bool suppressBodyLinkEgress)
{
    if (cmd == nullptr || cmd[0] == '\0') return false;

    bool queued = false;
    portENTER_CRITICAL(&sMarcduinoQueueMux);
    if (sMarcduinoQueueCount < SizeOfArray(sMarcduinoQueue))
    {
        PendingMarcduinoCommand &entry = sMarcduinoQueue[sMarcduinoQueueTail];
        strlcpy(entry.source, source ? source : "unknown", sizeof(entry.source));
        strlcpy(entry.cmd, cmd, sizeof(entry.cmd));
        entry.suppressBodyLinkEgress = suppressBodyLinkEgress;
        sMarcduinoQueueTail = (sMarcduinoQueueTail + 1) % SizeOfArray(sMarcduinoQueue);
        sMarcduinoQueueCount++;
        queued = true;
    }
    portEXIT_CRITICAL(&sMarcduinoQueueMux);

    if (!queued)
    {
        sMarcduinoQueueFullCount++;
        logCapture.printf("[CMD][%s][queue-full] %s\n", source ? source : "unknown", cmd);
    }
    return queued;
}

static bool dequeueMarcduinoCommand(char *source, size_t sourceSize, char *cmd, size_t cmdSize, bool *suppressBodyLinkEgress)
{
    bool found = false;
    portENTER_CRITICAL(&sMarcduinoQueueMux);
    if (sMarcduinoQueueCount > 0)
    {
        PendingMarcduinoCommand &entry = sMarcduinoQueue[sMarcduinoQueueHead];
        strlcpy(source, entry.source, sourceSize);
        strlcpy(cmd, entry.cmd, cmdSize);
        *suppressBodyLinkEgress = entry.suppressBodyLinkEgress;
        sMarcduinoQueueHead = (sMarcduinoQueueHead + 1) % SizeOfArray(sMarcduinoQueue);
        sMarcduinoQueueCount--;
        found = true;
    }
    portEXIT_CRITICAL(&sMarcduinoQueueMux);
    return found;
}

static bool marcduinoIngressHandlePanelCalibrationCommand(const char *cmd)
{
    if (cmd == nullptr) return false;

    // These commands parse their suffix from the command buffer. Handling them
    // before queueing keeps all transports safe from deferred getCommand()
    // lifetime hazards in the MARCDUINO_ACTION fallback handlers.
    if (strncmp(cmd, ":MV", 3) == 0)
    {
        const char *args = cmd + 3;
        uint8_t tgt = 0;
        uint16_t val = 0;
        uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
        {
            if (!movePanelMaskToValue(msk, val))
                logCapture.println("[PANEL CAL] :MV unsupported target/value for this build");
        }
        else
        {
            logCapture.println("[PANEL CAL] Invalid :MV command");
        }
        return true;
    }

    if (strncmp(cmd, "#SO", 3) == 0)
    {
        const char *args = cmd + 3;
        uint8_t tgt = 0;
        uint16_t val = 0;
        uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
        {
            if (!applyPanelCalibrationToMask(msk, true, false, val))
                logCapture.println("[PANEL CAL] #SO unsupported target/value for this build");
        }
        else
        {
            logCapture.println("[PANEL CAL] Invalid #SO command");
        }
        return true;
    }

    if (strncmp(cmd, "#SC", 3) == 0)
    {
        const char *args = cmd + 3;
        uint8_t tgt = 0;
        uint16_t val = 0;
        uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
        {
            if (!applyPanelCalibrationToMask(msk, false, true, val))
                logCapture.println("[PANEL CAL] #SC unsupported target/value for this build");
        }
        else
        {
            logCapture.println("[PANEL CAL] Invalid #SC command");
        }
        return true;
    }

    if (strncmp(cmd, "#SW", 3) == 0)
    {
        const char *args = cmd + 3;
        uint8_t tgt = 0;
        uint32_t msk = 0;
        if (parseTwoDigitTarget(args, tgt) && panelTargetToMask(tgt, msk))
        {
            if (!swapPanelCalibrationInMask(msk))
                logCapture.println("[PANEL CAL] #SW unsupported target for this build");
        }
        else
        {
            logCapture.println("[PANEL CAL] Invalid #SW command");
        }
        return true;
    }

    return false;
}

static void marcduinoIngressAdmit(const MarcduinoIngressSource &source, const char *cmd)
{
    if (cmd == nullptr || cmd[0] == '\0') return;

    const char *label = marcduinoIngressSourceLabel(source);
    if (shouldBlockCommandDuringSleep(cmd))
    {
        logCapture.printf("[CMD][%s][sleep-blocked] %s\n", label, cmd);
        return;
    }
    if (shouldDropDuplicateMoodReset(cmd))
    {
        logCapture.printf("[CMD][%s][mood-duplicate-dropped] %s\n", label, cmd);
        return;
    }
    logCapture.printf("[CMD][%s] %s\n", label, cmd);
    if (handleImmediateServoMoveCommand(label, cmd))
        return;
    if (applyDomeVisualPresetCommand(label, cmd))
        return;
    if (applyDomeVisualAuthoringCommand(label, cmd))
        return;
    if (marcduinoIngressHandlePanelCalibrationCommand(cmd))
        return;

    enqueueMarcduinoCommand(label, cmd, marcduinoIngressSuppressesBodyLinkEgress(source));
}

static void drainMarcduinoCommandQueue()
{
    char source[24];
    char cmd[CONSOLE_BUFFER_SIZE];
    bool suppressBodyLinkEgress = false;
    while (dequeueMarcduinoCommand(source, sizeof(source), cmd, sizeof(cmd), &suppressBodyLinkEgress))
    {
        logCapture.printf("[CMD][%s][dispatch] %s\n", source, cmd);
        bool previousSuppressBodyLinkEgress = sSuppressBodyLinkEgress;
        if (suppressBodyLinkEgress)
            sSuppressBodyLinkEgress = true;
        Marcduino::processCommand(player, cmd);
        sSuppressBodyLinkEgress = previousSuppressBodyLinkEgress;
    }
}

#endif // MARCDUINO_INGRESS_IMPLEMENTATION_INCLUDED
#endif // MARCDUINO_INGRESS_IMPLEMENTATION
