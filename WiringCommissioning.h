#pragma once

// WiringCommissioning.h — deep module for builder-time servo wiring work.
// Owns board metadata, wiring config JSON, live apply, and raw servo tests.

#include <Arduino.h>
#include <Wire.h>
#include "WiringConfig.h"

#ifndef USE_I2C_ADDRESS

enum WiringBoardId
{
    kWiringBoardPanels,
    kWiringBoardHolos,
};

struct WiringBoardSpec
{
    WiringBoardId id;
    const char *board;
    const char *displayName;
    const char *configPathName;
    const char *nvsNamespace;
    const char *channelKeyFormat;
    const char *activeKeyFormat;
    int slotCount;
    int servoSlotOffset;
    uint8_t firmwareBoard;
    uint8_t pcaAddress;
    const uint8_t *defaultChannels;
    const bool *defaultActives;
    const char *const *bootLabels;
    const char *(*labelFn)(int);
    const char *(*commandFn)(int);
};

static String wiringCommissioningJsonEscape(const String &in)
{
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++)
    {
        char c = in[i];
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((uint8_t)c < 0x20)
                {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
                    out += buf;
                }
                else
                {
                    out += c;
                }
                break;
        }
    }
    return out;
}

static const char *wiringPanelSlotLabel(int slot)
{
    return (slot >= 0 && slot < NUM_PANEL_SLOTS) ? kPanelSlotLabels[slot] : "?";
}

static const char *wiringPanelSlotCommand(int slot)
{
    static const char *cmds[NUM_PANEL_SLOTS] = {
        ":OP01", ":OP02", ":OP03", ":OP04", ":OP07", ":OP11", ":OP13",
        ":OPP5", ":OPP1", ":OPP2", ":OPP4", ":OPP6", ":OPP3",
    };
    return (slot >= 0 && slot < NUM_PANEL_SLOTS) ? cmds[slot] : "";
}

static const char *wiringHoloSlotLabel(int slot)
{
    static const char *labels[NUM_HOLO_SLOTS] = {
        "FHP (HP1) — H", "FHP (HP1) — V",
        "THP (HP3) — H", "THP (HP3) — V",
        "RHP (HP2) — V", "RHP (HP2) — H",
    };
    return (slot >= 0 && slot < NUM_HOLO_SLOTS) ? labels[slot] : "?";
}

static const WiringBoardSpec &wiringBoardSpec(WiringBoardId id)
{
    static const WiringBoardSpec panels = {
        kWiringBoardPanels,
        "panels",
        "Panel",
        "panels/config",
        PREFERENCE_PANELS_NS,
        PREFERENCE_PANELS_CH_FMT,
        PREFERENCE_PANELS_ACT_FMT,
        NUM_PANEL_SLOTS,
        0,
        0,
        0x40,
        defaultPanelCh,
        defaultPanelActive,
        kPanelSlotLabels,
        wiringPanelSlotLabel,
        wiringPanelSlotCommand,
    };
    static const WiringBoardSpec holos = {
        kWiringBoardHolos,
        "holos",
        "Holo",
        "holos/config",
        PREFERENCE_HOLOS_NS,
        PREFERENCE_HOLOS_CH_FMT,
        PREFERENCE_HOLOS_ACT_FMT,
        NUM_HOLO_SLOTS,
        HOLO_SLOT_OFFSET,
        1,
        0x41,
        defaultHoloCh,
        defaultHoloActive,
        kHoloSlotLabels,
        wiringHoloSlotLabel,
        nullptr,
    };
    return (id == kWiringBoardPanels) ? panels : holos;
}

static bool wiringBoardIdFromName(const String &name, WiringBoardId &outId)
{
    if (name == "panels")
    {
        outId = kWiringBoardPanels;
        return true;
    }
    if (name == "holos")
    {
        outId = kWiringBoardHolos;
        return true;
    }
    return false;
}

static uint32_t wiringDefaultGroupForServoSlot(int servoSlot)
{
    if (servoSlot < 0 || servoSlot >= (NUM_PANEL_SLOTS + NUM_HOLO_SLOTS)) return 0;
    return pgm_read_dword(&servoSettings[servoSlot].group);
}

static int wiringApplyBoardConfig(const WiringBoardSpec &spec,
                                  const uint8_t *channels,
                                  const bool *actives)
{
    int activeCount = 0;
    for (int i = 0; i < spec.slotCount; i++)
    {
        int servoSlot = spec.servoSlotOffset + i;
        bool active = actives[i];
        uint8_t physCh = channels[i];
        uint32_t group = active ? wiringDefaultGroupForServoSlot(servoSlot) : 0;
        uint8_t pin = active ? wiringConfigFirmwarePin(spec.firmwareBoard, physCh) : 0;
        uint16_t start = servoDispatch.getStart(servoSlot);
        uint16_t end = servoDispatch.getEnd(servoSlot);
        uint16_t neutral = servoDispatch.getNeutral(servoSlot);

        if (active && group == 0)
        {
            logCapture.printf(
                "[Wiring] Warning: %s %s is enabled in the wiring config "
                "but won't respond to commands. Either uncheck it in the "
                "wiring config (if no servo is wired) or restore its command "
                "group bits in firmware.\n",
                spec.displayName,
                spec.bootLabels[i]);
        }

        servoDispatch.setServo(servoSlot, pin, start, end, neutral, group);
        if (active) activeCount++;
    }
    return activeCount;
}

static void wiringLoadBoardConfig(const WiringBoardSpec &spec)
{
    Preferences prefs;
    prefs.begin(spec.nvsNamespace, false);

    uint8_t channels[NUM_PANEL_SLOTS];
    bool actives[NUM_PANEL_SLOTS];
    int chOverrides = 0;
    int actOverrides = 0;

    for (int i = 0; i < spec.slotCount; i++)
    {
        char keyC[12];
        char keyA[12];
        snprintf(keyC, sizeof(keyC), spec.channelKeyFormat, i);
        snprintf(keyA, sizeof(keyA), spec.activeKeyFormat, i);

        bool hasActOverride = prefs.isKey(keyA);
        bool hasChOverride = prefs.isKey(keyC);
        actives[i] = hasActOverride ? prefs.getBool(keyA, spec.defaultActives[i])
                                     : spec.defaultActives[i];
        channels[i] = hasChOverride ? prefs.getUChar(keyC, spec.defaultChannels[i])
                                     : spec.defaultChannels[i];

        if (hasChOverride && channels[i] != spec.defaultChannels[i])
        {
            logCapture.printf("[Wiring] %s %s channel override: CH%u (default CH%u)\n",
                              spec.displayName,
                              spec.bootLabels[i],
                              (unsigned)channels[i],
                              (unsigned)spec.defaultChannels[i]);
            chOverrides++;
        }
        if (hasActOverride && actives[i] != spec.defaultActives[i])
        {
            logCapture.printf("[Wiring] %s %s %s by saved config (default %s)\n",
                              spec.displayName,
                              spec.bootLabels[i],
                              actives[i] ? "enabled" : "disabled",
                              spec.defaultActives[i] ? "enabled" : "disabled");
            actOverrides++;
        }
    }
    prefs.end();

    int activeCount = wiringApplyBoardConfig(spec, channels, actives);
    logCapture.printf("[Wiring] %s config loaded: %d active, %d inactive "
                       "(NVS overrides: %d channel, %d active flag)\n",
                       spec.displayName,
                       activeCount,
                       spec.slotCount - activeCount,
                       chOverrides,
                       actOverrides);
}

static void wiringCommissioningLoadPanels()
{
    wiringLoadBoardConfig(wiringBoardSpec(kWiringBoardPanels));
}

static void wiringCommissioningLoadHolos()
{
    wiringLoadBoardConfig(wiringBoardSpec(kWiringBoardHolos));
}

static bool wiringCommissioningPanelSlotActive(int slot)
{
    if (slot < 0 || slot >= NUM_PANEL_SLOTS) return false;

    const WiringBoardSpec &spec = wiringBoardSpec(kWiringBoardPanels);
    uint8_t channels[NUM_PANEL_SLOTS];
    bool actives[NUM_PANEL_SLOTS];
    wiringConfigRead(spec.nvsNamespace,
                     spec.channelKeyFormat,
                     spec.activeKeyFormat,
                     spec.slotCount,
                     spec.defaultChannels,
                     spec.defaultActives,
                     channels,
                     actives);
    return actives[slot];
}

static String wiringCommissioningBuildConfigJson(WiringBoardId id)
{
    const WiringBoardSpec &spec = wiringBoardSpec(id);
    uint8_t channels[NUM_PANEL_SLOTS];
    bool actives[NUM_PANEL_SLOTS];
    wiringConfigRead(spec.nvsNamespace,
                     spec.channelKeyFormat,
                     spec.activeKeyFormat,
                     spec.slotCount,
                     spec.defaultChannels,
                     spec.defaultActives,
                     channels,
                     actives);

    String json = "{";
    json.reserve(64 + spec.slotCount * 80);
    json += "\"board\":\"";
    json += spec.board;
    json += "\",\"slot_count\":";
    json += spec.slotCount;
    json += ",\"slots\":[";
    for (int i = 0; i < spec.slotCount; i++)
    {
        if (i > 0) json += ',';
        json += "{\"index\":";
        json += i;
        if (spec.labelFn)
        {
            json += ",\"label\":\"";
            json += wiringCommissioningJsonEscape(spec.labelFn(i));
            json += '"';
        }
        json += ",\"channel\":";
        json += channels[i];
        json += ",\"active\":";
        json += actives[i] ? "true" : "false";
        if (spec.commandFn)
        {
            json += ",\"cmd\":\"";
            json += wiringCommissioningJsonEscape(spec.commandFn(i));
            json += '"';
        }
        json += '}';
    }
    json += "]}";
    return json;
}

// Raw servo test state and helpers. These intentionally bypass ServoDispatch
// slot routing so builders can identify physical PCA9685 channels.
static const uint16_t WIRING_PWM_PANEL_OPEN   = 369;  // 1800 us
static const uint16_t WIRING_PWM_PANEL_CLOSED = 246;  // 1200 us
static const uint16_t WIRING_PWM_HOLO_A       = 410;  // 2000 us
static const uint16_t WIRING_PWM_HOLO_B       = 205;  // 1000 us
static const uint16_t WIRING_PWM_NEUTRAL      = 307;  // 1500 us
static const uint32_t WIRING_HOLO_SWEEP_HOLD_MS = 1000;

static int8_t sWiringPanelTestChannel = -1;
static int8_t sWiringHoloTestChannel = -1;
static volatile bool sWiringHoloSweepActive = false;
static uint8_t sWiringHoloSweepChannel = 0;
static uint8_t sWiringHoloSweepPhase = 0;
static uint32_t sWiringHoloSweepDeadline = 0;

struct WiringPwmWrite
{
    uint8_t boardAddr;
    uint8_t channel;
    uint16_t count;
};

typedef bool (*WiringPwmWriter)(const WiringPwmWrite &write);

static bool wiringWritePwmToWire(const WiringPwmWrite &write)
{
    Wire.beginTransmission(write.boardAddr);
    Wire.write(6 + write.channel * 4);
    Wire.write(0);
    Wire.write(0);
    Wire.write(lowByte(write.count));
    Wire.write(highByte(write.count));
    return Wire.endTransmission() == 0;
}

static WiringPwmWriter sWiringPwmWriter = wiringWritePwmToWire;

#ifdef WIRING_COMMISSIONING_TEST_HOOKS
static void wiringCommissioningSetPwmWriterForTest(WiringPwmWriter writer)
{
    sWiringPwmWriter = writer ? writer : wiringWritePwmToWire;
}

static void wiringCommissioningResetPwmWriterForTest()
{
    sWiringPwmWriter = wiringWritePwmToWire;
}
#endif

static bool wiringWritePwm(uint8_t boardAddr, uint8_t channel, uint16_t count)
{
    if (channel > 15) return false;
    WiringPwmWrite write = { boardAddr, channel, count };
    return sWiringPwmWriter(write);
}

static int8_t wiringActiveTestChannel(const WiringBoardSpec &spec)
{
    return (spec.id == kWiringBoardPanels) ? sWiringPanelTestChannel : sWiringHoloTestChannel;
}

static void wiringStopRawServoTest(const WiringBoardSpec &spec)
{
    if (spec.id == kWiringBoardPanels)
    {
        if (sWiringPanelTestChannel >= 0)
        {
            wiringWritePwm(spec.pcaAddress, sWiringPanelTestChannel, WIRING_PWM_PANEL_CLOSED);
            sWiringPanelTestChannel = -1;
        }
        return;
    }

    sWiringHoloSweepActive = false;
    if (sWiringHoloTestChannel >= 0)
    {
        wiringWritePwm(spec.pcaAddress, sWiringHoloTestChannel, WIRING_PWM_NEUTRAL);
        sWiringHoloTestChannel = -1;
    }
}

static void wiringStopRawServoTestBeforeApply(const WiringBoardSpec &spec)
{
    int8_t prevChannel = wiringActiveTestChannel(spec);
    if (prevChannel < 0) return;
    logCapture.printf("[Wiring] Stopping active raw servo test on %s CH%d "
                       "before live config apply\n",
                       spec.board,
                       (int)prevChannel);
    wiringStopRawServoTest(spec);
}

static void wiringCommissioningHoloSweepPoll()
{
    if (!sWiringHoloSweepActive) return;
    if ((int32_t)(millis() - sWiringHoloSweepDeadline) < 0) return;

    sWiringHoloSweepPhase = (sWiringHoloSweepPhase == 0) ? 1 : 0;
    wiringWritePwm(wiringBoardSpec(kWiringBoardHolos).pcaAddress,
                   sWiringHoloSweepChannel,
                   (sWiringHoloSweepPhase == 0) ? WIRING_PWM_HOLO_A : WIRING_PWM_HOLO_B);
    sWiringHoloSweepDeadline = millis() + WIRING_HOLO_SWEEP_HOLD_MS;
}

static bool wiringCommissioningSaveConfigFromBody(WiringBoardId id,
                                                  const String &body,
                                                  int &httpStatus,
                                                  String &response)
{
    const WiringBoardSpec &spec = wiringBoardSpec(id);
    if (body.length() == 0)
    {
        logCapture.printf("[API] %s rejected: empty body\n", spec.configPathName);
        httpStatus = 400;
        response = "{\"error\":\"empty body\"}";
        return false;
    }

    uint8_t channels[NUM_PANEL_SLOTS];
    bool actives[NUM_PANEL_SLOTS];
    String errMsg;
    if (!wiringConfigParseBody(body, spec.slotCount, channels, actives, errMsg))
    {
        logCapture.printf("[API] %s rejected: %s\n", spec.configPathName, errMsg.c_str());
        httpStatus = 400;
        response = String("{\"error\":\"") + wiringCommissioningJsonEscape(errMsg) + "\"}";
        return false;
    }
    if (!wiringConfigCheckConflicts(spec.slotCount, channels, actives, errMsg))
    {
        logCapture.printf("[API] %s rejected: %s\n", spec.configPathName, errMsg.c_str());
        httpStatus = 400;
        response = String("{\"error\":\"") + wiringCommissioningJsonEscape(errMsg) + "\"}";
        return false;
    }
    if (!wiringConfigSave(spec.nvsNamespace,
                          spec.channelKeyFormat,
                          spec.activeKeyFormat,
                          spec.slotCount,
                          channels,
                          actives))
    {
        logCapture.printf("[API] Error: %s NVS save failed "
                          "(flash full or NVS corrupt - investigate)\n",
                          spec.configPathName);
        httpStatus = 500;
        response = "{\"error\":\"NVS save failed\"}";
        return false;
    }

    wiringStopRawServoTestBeforeApply(spec);
    int activeCount = wiringApplyBoardConfig(spec, channels, actives);
    logCapture.printf("[API] %s saved and applied: %d active, %d inactive\n",
                       spec.configPathName,
                       activeCount,
                       spec.slotCount - activeCount);

    httpStatus = 200;
    response = "{\"ok\":true,\"applied\":true,\"reboot_required\":false}";
    return true;
}

static bool wiringParseRawServoTestBody(const String &body,
                                        WiringBoardId &outId,
                                        int &outChannel,
                                        bool wantChannel,
                                        String &errMsg)
{
    int boardKey = body.indexOf("\"board\"");
    if (boardKey < 0) { errMsg = "missing board"; return false; }
    int boardColon = body.indexOf(':', boardKey);
    int quoteStart = body.indexOf('"', boardColon + 1);
    int quoteEnd = (quoteStart >= 0) ? body.indexOf('"', quoteStart + 1) : -1;
    if (quoteStart < 0 || quoteEnd < 0) { errMsg = "board must be a string"; return false; }

    String boardStr = body.substring(quoteStart + 1, quoteEnd);
    if (!wiringBoardIdFromName(boardStr, outId))
    {
        errMsg = "board must be panels or holos";
        return false;
    }

    if (wantChannel)
    {
        int chKey = body.indexOf("\"channel\"");
        if (chKey < 0) { errMsg = "missing channel"; return false; }
        int chColon = body.indexOf(':', chKey);
        const char *chStart = body.c_str() + chColon + 1;
        char *chEnd = nullptr;
        long chVal = strtol(chStart, &chEnd, 10);
        if (chEnd == chStart || !jsonValueEndsCleanly(chEnd))
        { errMsg = "channel must be a number"; return false; }
        if (chVal < 0 || chVal > 15)
        { errMsg = "channel out of range (0-15)"; return false; }
        outChannel = (int)chVal;
    }
    return true;
}

static bool wiringCommissioningStartRawServoTestFromBody(const String &body,
                                                         int &httpStatus,
                                                         String &response)
{
    if (body.length() == 0)
    {
        logCapture.println("[API] /servo/test rejected: empty body");
        httpStatus = 400;
        response = "{\"error\":\"empty body\"}";
        return false;
    }

    WiringBoardId id = kWiringBoardPanels;
    int channel = -1;
    String errMsg;
    if (!wiringParseRawServoTestBody(body, id, channel, true, errMsg))
    {
        logCapture.printf("[API] /servo/test rejected: %s\n", errMsg.c_str());
        httpStatus = 400;
        response = String("{\"error\":\"") + wiringCommissioningJsonEscape(errMsg) + "\"}";
        return false;
    }

    const WiringBoardSpec &spec = wiringBoardSpec(id);
    int8_t prevChannel = wiringActiveTestChannel(spec);
    if (prevChannel >= 0)
    {
        logCapture.printf("[Wiring] Auto-stopping previous test on %s CH%d "
                           "before starting new pulse\n",
                           spec.board,
                           (int)prevChannel);
    }
    wiringStopRawServoTest(spec);

    if (id == kWiringBoardPanels)
    {
        wiringWritePwm(spec.pcaAddress, (uint8_t)channel, WIRING_PWM_PANEL_OPEN);
        sWiringPanelTestChannel = (int8_t)channel;
        logCapture.printf("[Wiring] Test pulse: panels CH%d held open "
                           "(stays open until /servo/stop or another test "
                           "on this board)\n", channel);
    }
    else
    {
        sWiringHoloTestChannel = (int8_t)channel;
        sWiringHoloSweepChannel = (uint8_t)channel;
        sWiringHoloSweepPhase = 0;
        wiringWritePwm(spec.pcaAddress, (uint8_t)channel, WIRING_PWM_HOLO_A);
        sWiringHoloSweepDeadline = millis() + WIRING_HOLO_SWEEP_HOLD_MS;
        sWiringHoloSweepActive = true;
        logCapture.printf("[Wiring] Test sweep started: holos CH%d "
                           "(alternating extremes with 1 s holds - "
                           "stays running until /servo/stop)\n", channel);
    }

    httpStatus = 200;
    response = String("{\"ok\":true,\"board\":\"") + spec.board +
               "\",\"channel\":" + channel + "}";
    return true;
}

static bool wiringCommissioningStopRawServoTestFromBody(const String &body,
                                                        int &httpStatus,
                                                        String &response)
{
    if (body.length() == 0)
    {
        logCapture.println("[API] /servo/stop rejected: empty body");
        httpStatus = 400;
        response = "{\"error\":\"empty body\"}";
        return false;
    }

    WiringBoardId id = kWiringBoardPanels;
    int unusedChannel = -1;
    String errMsg;
    if (!wiringParseRawServoTestBody(body, id, unusedChannel, false, errMsg))
    {
        logCapture.printf("[API] /servo/stop rejected: %s\n", errMsg.c_str());
        httpStatus = 400;
        response = String("{\"error\":\"") + wiringCommissioningJsonEscape(errMsg) + "\"}";
        return false;
    }

    const WiringBoardSpec &spec = wiringBoardSpec(id);
    int8_t prevChannel = wiringActiveTestChannel(spec);
    wiringStopRawServoTest(spec);
    if (prevChannel >= 0)
    {
        logCapture.printf("[Wiring] Test stopped: %s CH%d (servo returned "
                           "to %s)\n",
                           spec.board,
                           (int)prevChannel,
                           id == kWiringBoardPanels ? "closed" : "neutral");
    }
    else
    {
        logCapture.printf("[Wiring] /servo/stop: no active test on %s "
                           "(no-op)\n",
                           spec.board);
    }

    httpStatus = 200;
    response = String("{\"ok\":true,\"board\":\"") + spec.board + "\"}";
    return true;
}

#endif // USE_I2C_ADDRESS
