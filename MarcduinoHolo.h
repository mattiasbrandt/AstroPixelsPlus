////////////////
// CREDENDA FORK IMPROVEMENT: Added All Holos On/Off commands
// These commands control all three holo projectors (Front, Rear, Top) simultaneously
// *ON00 = All holos dim cycle random color
// *OF00 = All holos off

static bool parseTwoDigitProjector(const char *cmd, uint8_t &projector)
{
    if (cmd == nullptr || cmd[0] == '\0' || cmd[1] == '\0') return false;
    if (cmd[0] < '0' || cmd[0] > '9' || cmd[1] < '0' || cmd[1] > '9') return false;
    projector = uint8_t((cmd[0] - '0') * 10 + (cmd[1] - '0'));
    if (projector > 3) return false;
    return true;
}

static bool parseThreeDigitValue(const char *ptr, uint16_t &value)
{
    if (ptr == nullptr) return false;
    if (ptr[0] == '\0' || ptr[1] == '\0' || ptr[2] == '\0') return false;
    if (ptr[0] < '0' || ptr[0] > '9' || ptr[1] < '0' || ptr[1] > '9' || ptr[2] < '0' || ptr[2] > '9') return false;
    value = uint16_t((ptr[0] - '0') * 100 + (ptr[1] - '0') * 10 + (ptr[2] - '0'));
    return true;
}

static bool hasMinCommandLength(const char *cmd, size_t minLen)
{
    if (cmd == nullptr) return false;
    return strnlen(cmd, 64) >= minLen;
}

static bool parseRgbPayload(const char *cmd, uint16_t &r, uint16_t &g, uint16_t &b, uint16_t &brightness)
{
    if (!hasMinCommandLength(cmd, 12)) return false;
    return parseThreeDigitValue(cmd, r) &&
           parseThreeDigitValue(cmd + 3, g) &&
           parseThreeDigitValue(cmd + 6, b) &&
           parseThreeDigitValue(cmd + 9, brightness);
}

static bool parseColorIndexPayload(const char *cmd, uint8_t &colorIndex)
{
    if (!hasMinCommandLength(cmd, 1)) return false;
    if (cmd[0] < '0' || cmd[0] > '9') return false;
    colorIndex = uint8_t(cmd[0] - '0');
    return true;
}

static uint8_t rgbToHoloColorIndex(uint16_t r, uint16_t g, uint16_t b)
{
    if (r < 16 && g < 16 && b < 16) return 0;
    if (r >= g && r >= b)
    {
        if (g > 180 && b < 100) return 7;
        if (b > 180 && g > 180) return 8;
        if (b > 140) return 6;
        return 1;
    }
    if (g >= r && g >= b)
    {
        if (r > 180 && b < 100) return 2;
        if (b > 160) return 4;
        return 3;
    }
    if (b >= r && b >= g)
    {
        if (r > 160 && g > 160) return 9;
        if (r > 140) return 6;
        return 5;
    }
    return 9;
}

static void sendHoloLedSetColor(uint8_t projector, uint8_t colorIndex)
{
    char cmd[16];
    char projectorCode = (projector == 0) ? 'A' : (projector == 1) ? 'F' : (projector == 2) ? 'R' : 'T';
    snprintf(cmd, sizeof(cmd), "HP%c096", projectorCode);
    CommandEvent::process(cmd);
    snprintf(cmd, sizeof(cmd), "HP%c005%u", projectorCode, colorIndex);
    CommandEvent::process(cmd);
}

static void sendHoloCenterCommand(uint8_t projector)
{
    char cmd[16];
    char projectorCode = (projector == 0) ? 'A' : (projector == 1) ? 'F' : (projector == 2) ? 'R' : 'T';
    snprintf(cmd, sizeof(cmd), "HP%c1011", projectorCode);
    CommandEvent::process(cmd);
}

static void runHoloMechanicalTest(uint8_t projector)
{
    if (projector == 0)
    {
        CommandEvent::process(F("HPA105|8"));
        return;
    }
    if (projector == 1) CommandEvent::process(F("HPF105|8"));
    if (projector == 2) CommandEvent::process(F("HPR105|8"));
    if (projector == 3) CommandEvent::process(F("HPT105|8"));
}

MARCDUINO_ACTION(AllHoloOn, *ON00, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t colorIndex = 0;
                     uint16_t r = 0, g = 0, b = 0, brightness = 0;
                     if (parseColorIndexPayload(cmd, colorIndex))
                     {
                         sendHoloLedSetColor(0, colorIndex);
                     }
                     else if (parseRgbPayload(cmd, r, g, b, brightness) && brightness > 0)
                     {
                         sendHoloLedSetColor(0, rgbToHoloColorIndex(r, g, b));
                     }
                     else
                     {
                         CommandEvent::process(F("HPA005"));
                     }
                 }))

////////////////

MARCDUINO_ACTION(AllHoloOFF, *OF00, ({
                     // All Holo off
                     CommandEvent::process(F("HPA096"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloOn, *ON01, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t colorIndex = 0;
                     uint16_t r = 0, g = 0, b = 0, brightness = 0;
                     if (parseColorIndexPayload(cmd, colorIndex))
                     {
                         sendHoloLedSetColor(1, colorIndex);
                     }
                     else if (parseRgbPayload(cmd, r, g, b, brightness) && brightness > 0)
                     {
                         sendHoloLedSetColor(1, rgbToHoloColorIndex(r, g, b));
                     }
                     else
                     {
                         CommandEvent::process(F("HPF005"));
                     }
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloOff, *OF01, ({
                     // Front Holo Off
                     CommandEvent::process(F("HPF096"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloOn, *ON02, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t colorIndex = 0;
                     uint16_t r = 0, g = 0, b = 0, brightness = 0;
                     if (parseColorIndexPayload(cmd, colorIndex))
                     {
                         sendHoloLedSetColor(2, colorIndex);
                     }
                     else if (parseRgbPayload(cmd, r, g, b, brightness) && brightness > 0)
                     {
                         sendHoloLedSetColor(2, rgbToHoloColorIndex(r, g, b));
                     }
                     else
                     {
                         CommandEvent::process(F("HPR005"));
                     }
                 }))

////////////////

MARCDUINO_ACTION(RearHoloOff, *OF02, ({
                     // Rear Holo Off
                     CommandEvent::process(F("HPR096"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloOn, *ON03, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t colorIndex = 0;
                     uint16_t r = 0, g = 0, b = 0, brightness = 0;
                     if (parseColorIndexPayload(cmd, colorIndex))
                     {
                         sendHoloLedSetColor(3, colorIndex);
                     }
                     else if (parseRgbPayload(cmd, r, g, b, brightness) && brightness > 0)
                     {
                         sendHoloLedSetColor(3, rgbToHoloColorIndex(r, g, b));
                     }
                     else
                     {
                         CommandEvent::process(F("HPT005"));
                     }
                 }))

////////////////

MARCDUINO_ACTION(TopHoloOff, *OF03, ({
                     // Top Holo Off
                     CommandEvent::process(F("HPT096"));
                 }))

////////////////

MARCDUINO_ACTION(ResetAllHolos, *ST00, ({
                     // Reset all holos
                     CommandEvent::process(F("HPA0000"));
                 }))

////////////////

MARCDUINO_ACTION(AllHolosDisableTwitch, *HD07, ({
                     // Disable holo twitch modes and force all holo channels to idle
                     CommandEvent::process(F("HPS7"));
                 }))

////////////////

MARCDUINO_ACTION(AllHolosEnableDefaultTwitch, *HD08, ({
                     // Enable default holo twitch choreography pattern set
                     CommandEvent::process(F("HPS8"));
                 }))

////////////////

MARCDUINO_ACTION(AllHolosEnableRandomTwitch, *HD09, ({
                     // Enable randomized holo twitch choreography pattern set
                     CommandEvent::process(F("HPS9"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloRandomMove, *RD01, ({
                     // Front Holo Move Random
                     CommandEvent::process(F("HPF104"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloRandomMove, *RD02, ({
                     // Rear Holo Move Random
                     CommandEvent::process(F("HPR104"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloRandomMove, *RD03, ({
                     // Top Holo Move Random
                     CommandEvent::process(F("HPT104"));
                 }))

////////////////

MARCDUINO_ACTION(RadarEyePulse, *HRS3, ({
                     // Radar Eye Dim pulse random color
                     CommandEvent::process(F("HPD0030"));
                 }))

////////////////

MARCDUINO_ACTION(RadarEyePulseRed, *HRSR, ({
                     // Radar Eye Dim pulse random color
                     CommandEvent::process(F("HPD00312"));
                 }))

////////////////

MARCDUINO_ACTION(RadarEyeRainbow, *HRS6, ({
                     // Radar Eye rainbow
                     CommandEvent::process(F("HPD006"));
                 }))

////////////////

MARCDUINO_ACTION(RadarEyeCycle, *HRS4, ({
                     // Radar Eye cycle random color
                     CommandEvent::process(F("HPD0040"));
                 }))

////////////////

MARCDUINO_ACTION(RadarEyeOff, *OF04, ({
                     // Radar Eye Off
                     CommandEvent::process(F("HPD096"));
                 }))

////////////////
// PULSE COMMANDS - LED Light Effects (NOT servo movements)
// These commands create a pulsing LED effect with random colors
// HPF/HPR/HPT 0030 = Dim pulse random color effect on LEDs
// Commands: *HPS301 (Front), *HPS302 (Rear), *HPS303 (Top)

MARCDUINO_ACTION(FrontHoloPulse, *HPS301, ({
                     // Front Holo Dim pulse random color
                     CommandEvent::process(F("HPF0030"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPulse, *HPS302, ({
                     // Rear Holo Dim pulse random color
                     CommandEvent::process(F("HPR0030"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPulse, *HPS303, ({
                     // Top Holo Dim pulse random color
                     CommandEvent::process(F("HPT0030"));
                 }))

////////////////
// RAINBOW COMMANDS - LED Light Effects (NOT servo movements)
// These commands cycle through rainbow colors on the LEDs
// HPF/HPR/HPT 006 = Rainbow color cycling effect on LEDs
// Commands: *HPS601 (Front), *HPS602 (Rear), *HPS603 (Top)

MARCDUINO_ACTION(FrontHoloRainbow, *HPS601, ({
                     // Front Holo rainbow
                     CommandEvent::process(F("HPF006"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloRainbow, *HPS602, ({
                     // Rear Holo rainbow
                     CommandEvent::process(F("HPR006"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloRainbow, *HPS603, ({
                     // Top Holo rainbow
                     CommandEvent::process(F("HPT006"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosDown, *HP001, ({
                     // Front Holo Position Down
                     CommandEvent::process(F("HPF1010"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosCenter, *HP101, ({
                     // Front Holo Position Center
                     CommandEvent::process(F("HPF1011"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosUp, *HP201, ({
                     // Front Holo Position Up
                     CommandEvent::process(F("HPF1012"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosLeft, *HP301, ({
                     // Front Holo Position Left
                     CommandEvent::process(F("HPF1013"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosUpperLeft, *HP401, ({
                     // Front Holo Position Upper Left
                     CommandEvent::process(F("HPF1014"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosLowerLeft, *HP501, ({
                     // Front Holo Position Lower Left
                     CommandEvent::process(F("HPF1015"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosRight, *HP601, ({
                     // Front Holo Position Right
                     CommandEvent::process(F("HPF1016"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosUpperRight, *HP701, ({
                     // Front Holo Position Upper Right
                     CommandEvent::process(F("HPF1017"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloPosLowerRight, *HP801, ({
                     // Front Holo Position Lower Right
                     CommandEvent::process(F("HPF1018"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosDown, *HP002, ({
                     // Rear Holo Position Down
                     CommandEvent::process(F("HPR1010"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosCenter, *HP102, ({
                     // Rear Holo Position Center
                     CommandEvent::process(F("HPR1011"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosUp, *HP202, ({
                     // Rear Holo Position Up
                     CommandEvent::process(F("HPR1012"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosLeft, *HP302, ({
                     // Rear Holo Position Left
                     CommandEvent::process(F("HPR1013"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosUpperLeft, *HP402, ({
                     // Rear Holo Position Upper Left
                     CommandEvent::process(F("HPR1014"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosLowerLeft, *HP502, ({
                     // Rear Holo Position Lower Left
                     CommandEvent::process(F("HPR1015"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosRight, *HP602, ({
                     // Rear Holo Position Right
                     CommandEvent::process(F("HPR1016"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosUpperRight, *HP702, ({
                     // Rear Holo Position Upper Right
                     CommandEvent::process(F("HPR1017"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloPosLowerRight, *HP802, ({
                     // Rear Holo Position Lower Right
                     CommandEvent::process(F("HPR1018"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosDown, *HP003, ({
                     // Top Holo Position Down
                     CommandEvent::process(F("HPT1010"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosCenter, *HP103, ({
                     // Top Holo Position Center
                     CommandEvent::process(F("HPT1011"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosUp, *HP203, ({
                     // Top Holo Position Up
                     CommandEvent::process(F("HPT1012"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosLeft, *HP303, ({
                     // Top Holo Position Left
                     CommandEvent::process(F("HPT1013"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosUpperLeft, *HP403, ({
                     // Top Holo Position Upper Left
                     CommandEvent::process(F("HPT1014"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosLowerLeft, *HP503, ({
                     // Top Holo Position Lower Left
                     CommandEvent::process(F("HPT1015"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosRight, *HP603, ({
                     // Top Holo Position Right
                     CommandEvent::process(F("HPT1016"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosUpperRight, *HP703, ({
                     // Top Holo Position Upper Right
                     CommandEvent::process(F("HPT1017"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloPosLowerRight, *HP803, ({
                     // Top Holo Position Lower Right
                     CommandEvent::process(F("HPT1018"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloWag, *HW01, ({
                     // Front Holo Wags Left/Right 5 times
                     CommandEvent::process(F("HPF105|5"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloWag, *HW02, ({
                     // Rear Holo Wags Left/Right 5 times
                     CommandEvent::process(F("HPR105|5"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloWag, *HW03, ({
                     // Top Holo Wags Left/Right 5 times
                     CommandEvent::process(F("HPT105|5"));
                 }))

////////////////

MARCDUINO_ACTION(AllHoloWag, *HW00, ({
                     // Move all three holos left/right wag pattern together
                     CommandEvent::process(F("HPA105|5"));
                 }))

////////////////

MARCDUINO_ACTION(FrontHoloNod, *HN01, ({
                     // Front Holo Nods Up/Down 5 times
                     CommandEvent::process(F("HPF106|5"));
                 }))

////////////////

MARCDUINO_ACTION(RearHoloNod, *HN02, ({
                     // Rear Holo Nods Up/Down 5 times
                     CommandEvent::process(F("HPR106|5"));
                 }))

////////////////

MARCDUINO_ACTION(TopHoloNod, *HN03, ({
                     // Top Holo Nods Up/Down 5 times
                     CommandEvent::process(F("HPT106|5"));
                 }))

////////////////

MARCDUINO_ACTION(AllHoloNod, *HN00, ({
                     // Move all three holos up/down nod pattern together
                     CommandEvent::process(F("HPA106|5"));
                 }))

////////////////

MARCDUINO_ACTION(HoloColorSetCompat, *CO, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t projector = 0;
                     uint16_t r = 0, g = 0, b = 0, brightness = 0;
                     if (!hasMinCommandLength(cmd, 14) ||
                         !parseTwoDigitProjector(cmd, projector) ||
                         !parseThreeDigitValue(cmd + 2, r) ||
                         !parseThreeDigitValue(cmd + 5, g) ||
                         !parseThreeDigitValue(cmd + 8, b) ||
                         !parseThreeDigitValue(cmd + 11, brightness))
                     {
                         DEBUG_PRINTLN("[HOLO] Invalid *CO command");
                     }
                     else if (brightness == 0)
                     {
                         if (projector == 0 || projector > 3) CommandEvent::process(F("HPA096"));
                         else if (projector == 1) CommandEvent::process(F("HPF096"));
                         else if (projector == 2) CommandEvent::process(F("HPR096"));
                         else if (projector == 3) CommandEvent::process(F("HPT096"));
                     }
                     else
                     {
                         uint8_t colorIndex = rgbToHoloColorIndex(r, g, b);
                         sendHoloLedSetColor(projector, colorIndex);
                     }
                 }))

////////////////

MARCDUINO_ACTION(HoloCenterCompat, *CH, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t projector = 0;
                     if (!hasMinCommandLength(cmd, 2) || !parseTwoDigitProjector(cmd, projector))
                     {
                         DEBUG_PRINTLN("[HOLO] Invalid *CH command");
                     }
                     else
                     {
                         sendHoloCenterCommand(projector);
                     }
                 }))

////////////////

MARCDUINO_ACTION(HoloRCCompat, *RC, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t projector = 0;
                     if (!hasMinCommandLength(cmd, 2) || !parseTwoDigitProjector(cmd, projector))
                     {
                         DEBUG_PRINTLN("[HOLO] Invalid *RC command");
                     }
                     else
                     {
                         sendHoloCenterCommand(projector);
                     }
                 }))

////////////////

MARCDUINO_ACTION(HoloTestCompat, *TE, ({
                     const char *cmd = Marcduino::getCommand();
                     uint8_t projector = 0;
                     if (!hasMinCommandLength(cmd, 2) || !parseTwoDigitProjector(cmd, projector))
                     {
                         DEBUG_PRINTLN("[HOLO] Invalid *TE command");
                     }
                     else
                     {
                         runHoloMechanicalTest(projector);
                     }
                 }))

////////////////

MARCDUINO_ACTION(MDFrontHoloOn, @6T1, ({
                     // Front Holo Dim cycle random color
                     CommandEvent::process(F("HPF0040"));
                 }))

////////////////

MARCDUINO_ACTION(MDTopHoloOn, @7T1, ({
                     // Top Holo Dim cycle random color
                     CommandEvent::process(F("HPT0040"));
                 }))

////////////////

MARCDUINO_ACTION(MDRearHoloOn, @8T1, ({
                     // Rear Holo Dim cycle random color
                     CommandEvent::process(F("HPR0040"));
                 }))

////////////////

MARCDUINO_ACTION(MDFrontHoloOff, @6D, ({
                     // Front Holo Off
                     CommandEvent::process(F("HPF0000"));
                 }))

////////////////

MARCDUINO_ACTION(MDTopHoloOff, @7D, ({
                     // Top Holo Off
                     CommandEvent::process(F("HPT0000"));
                 }))

////////////////

MARCDUINO_ACTION(MDRearHoloOff, @8D, ({
                     // Rear Holo Off
                     CommandEvent::process(F("HPR0000"));
                 }))

////////////////

MARCDUINO_ACTION(HoloCommand, @HP, ({
                     char sHoloCommand[64];
                     snprintf(sHoloCommand, sizeof(sHoloCommand), "HP%s", Marcduino::getCommand());
                     CommandEvent::process(sHoloCommand);
                 }))

////////////////
