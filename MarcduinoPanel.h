////////////////

static bool parseTwoDigitTarget(const char *cmd, uint8_t &target)
{
    if (cmd == nullptr || cmd[0] == '\0' || cmd[1] == '\0') return false;
    if (cmd[0] < '0' || cmd[0] > '9' || cmd[1] < '0' || cmd[1] > '9') return false;
    target = uint8_t((cmd[0] - '0') * 10 + (cmd[1] - '0'));
    return true;
}

static bool parseFourDigitValue(const char *cmd, uint16_t &value)
{
    if (cmd == nullptr) return false;
    for (uint8_t i = 0; i < 4; i++)
    {
        if (cmd[i] < '0' || cmd[i] > '9') return false;
    }
    value = uint16_t((cmd[0] - '0') * 1000 + (cmd[1] - '0') * 100 + (cmd[2] - '0') * 10 + (cmd[3] - '0'));
    return true;
}

static bool panelTargetToMask(uint8_t target, uint32_t &mask)
{
    switch (target)
    {
        case 0:  mask = ALL_DOME_PANELS_MASK; return true;
        case 1:  mask = PANEL_GROUP_1; return true;
        case 2:  mask = PANEL_GROUP_2; return true;
        case 3:  mask = PANEL_GROUP_3; return true;
        case 4:  mask = PANEL_GROUP_4; return true;
        case 5:  mask = PANEL_GROUP_5; return true;
        case 6:  mask = PANEL_GROUP_6; return true;
        case 7:  mask = PANEL_GROUP_7; return true;
        case 8:  mask = PANEL_GROUP_8; return true;
        case 9:  mask = PANEL_GROUP_9; return true;
        case 10: mask = PANEL_GROUP_10; return true;
        case 11: mask = PIE_PANEL; return true;
        case 12: mask = DOME_PANELS_MASK; return true;
        case 13: mask = TOP_PIE_PANEL; return true;
        case 14: mask = (PIE_PANEL | TOP_PIE_PANEL); return true;
        case 15: mask = DOME_PANELS_MASK; return true;
        default: return false;
    }
}

static bool convertPanelValueToPulse(uint16_t servoIndex, uint16_t rawValue, uint16_t &pulse)
{
    if (rawValue <= 180)
    {
        pulse = servoDispatch.scaleToPos(servoIndex, float(rawValue) / 180.0f);
        return true;
    }
    if (rawValue >= 544 && rawValue <= 2500)
    {
        pulse = rawValue;
        return true;
    }
    return false;
}

static bool isPanelServoByGroup(uint32_t group)
{
    return (group & (SMALL_PANEL | MEDIUM_PANEL | BIG_PANEL | PIE_PANEL | TOP_PIE_PANEL | MINI_PANEL)) != 0;
}

static void persistPanelCalibrationValue(uint16_t servoIndex, bool openValue, uint16_t pulse)
{
    char key[8];
    snprintf(key, sizeof(key), openValue ? "so%02u" : "sc%02u", servoIndex);
    preferences.putUShort(key, pulse);
}

static bool applyPanelCalibrationToMask(uint32_t mask, bool setOpen, bool setClosed, uint16_t rawValue)
{
    bool changed = false;
    for (uint16_t i = 0; i < servoDispatch.getNumServos(); i++)
    {
        uint32_t group = servoDispatch.getGroup(i);
        if (!isPanelServoByGroup(group) || (group & mask) == 0) continue;

        uint16_t pulse = 0;
        if (!convertPanelValueToPulse(i, rawValue, pulse)) return false;

        if (setOpen)
        {
            servoDispatch.setStart(i, pulse);
            persistPanelCalibrationValue(i, true, pulse);
            changed = true;
        }
        if (setClosed)
        {
            servoDispatch.setEnd(i, pulse);
            persistPanelCalibrationValue(i, false, pulse);
            changed = true;
        }
    }
    return changed;
}

static bool movePanelMaskToValue(uint32_t mask, uint16_t rawValue)
{
    bool moved = false;
    for (uint16_t i = 0; i < servoDispatch.getNumServos(); i++)
    {
        uint32_t group = servoDispatch.getGroup(i);
        if (!isPanelServoByGroup(group) || (group & mask) == 0) continue;

        uint16_t pulse = 0;
        if (!convertPanelValueToPulse(i, rawValue, pulse)) return false;
        servoDispatch.moveToPulse(i, pulse);
        moved = true;
    }
    return moved;
}

static bool swapPanelCalibrationInMask(uint32_t mask)
{
    bool swapped = false;
    for (uint16_t i = 0; i < servoDispatch.getNumServos(); i++)
    {
        uint32_t group = servoDispatch.getGroup(i);
        if (!isPanelServoByGroup(group) || (group & mask) == 0) continue;

        uint16_t openPulse = servoDispatch.getStart(i);
        uint16_t closePulse = servoDispatch.getEnd(i);
        servoDispatch.setStart(i, closePulse);
        servoDispatch.setEnd(i, openPulse);
        persistPanelCalibrationValue(i, true, closePulse);
        persistPanelCalibrationValue(i, false, openPulse);
        swapped = true;
    }
    return swapped;
}

MARCDUINO_ACTION(MovePanelCalibration, :MV, ({
    const char *cmd = Marcduino::getCommand();
    uint8_t target = 0;
    uint16_t value = 0;
    uint32_t mask = 0;
    if (cmd == nullptr || !parseTwoDigitTarget(cmd, target) || !parseFourDigitValue(cmd + 2, value) || !panelTargetToMask(target, mask))
    {
        DEBUG_PRINTLN("[PANEL CAL] Invalid :MV command");
    }
    else if (!movePanelMaskToValue(mask, value))
    {
        DEBUG_PRINTLN("[PANEL CAL] :MV unsupported target/value for this build");
    }
}))

MARCDUINO_ACTION(SavePanelOpenCalibration, #SO, ({
    const char *cmd = Marcduino::getCommand();
    uint8_t target = 0;
    uint16_t value = 0;
    uint32_t mask = 0;
    if (cmd == nullptr || !parseTwoDigitTarget(cmd, target) || !parseFourDigitValue(cmd + 2, value) || !panelTargetToMask(target, mask))
    {
        DEBUG_PRINTLN("[PANEL CAL] Invalid #SO command");
    }
    else if (!applyPanelCalibrationToMask(mask, true, false, value))
    {
        DEBUG_PRINTLN("[PANEL CAL] #SO unsupported target/value for this build");
    }
}))

MARCDUINO_ACTION(SavePanelClosedCalibration, #SC, ({
    const char *cmd = Marcduino::getCommand();
    uint8_t target = 0;
    uint16_t value = 0;
    uint32_t mask = 0;
    if (cmd == nullptr || !parseTwoDigitTarget(cmd, target) || !parseFourDigitValue(cmd + 2, value) || !panelTargetToMask(target, mask))
    {
        DEBUG_PRINTLN("[PANEL CAL] Invalid #SC command");
    }
    else if (!applyPanelCalibrationToMask(mask, false, true, value))
    {
        DEBUG_PRINTLN("[PANEL CAL] #SC unsupported target/value for this build");
    }
}))

MARCDUINO_ACTION(SwapPanelOpenClosedCalibration, #SW, ({
    const char *cmd = Marcduino::getCommand();
    uint8_t target = 0;
    uint32_t mask = 0;
    if (cmd == nullptr || !parseTwoDigitTarget(cmd, target) || !panelTargetToMask(target, mask))
    {
        DEBUG_PRINTLN("[PANEL CAL] Invalid #SW command");
    }
    else if (!swapPanelCalibrationInMask(mask))
    {
        DEBUG_PRINTLN("[PANEL CAL] #SW unsupported target for this build");
    }
}))

MARCDUINO_ACTION(CloseAllPanels, :CL00, ({
    Marcduino::processCommand(player, "@4S3");
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, ALL_DOME_PANELS_MASK);
}))

////////////////

MARCDUINO_ACTION(OpenAllPanels, :OP00, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, ALL_DOME_PANELS_MASK);
}))

////////////////

MARCDUINO_ACTION(FlutterAllPanels, :OF00, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, ALL_DOME_PANELS_MASK, 10, 50);
}))

////////////////

MARCDUINO_ACTION(SetServoEasing, :SF, ({
    uint32_t group = 0;
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* gstr = strchr(cmd, '$');
    if (gstr != nullptr)
    {
        *gstr++ = '\0';
        group = strtol(gstr, 0, 16);
    }
    Easing::Method method = Easing::getEasingMethod(strtol(cmd, 0, 10));
    if (method == nullptr)
        method = Easing::LinearInterpolation;
    if (group != 0)
        servoDispatch.setServosEasingMethod(group, method);
}))

MARCDUINO_ACTION(SetServoPosition, :SQ, ({
    int32_t args[4] = { 0, 0, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    uint8_t argcount = 0;
    numberparams(cmd, argcount, args, SizeOfArray(args));
    if (argcount >= 2)
    {
        servoDispatch.moveToPulse(args[0], args[1]);
    }
}))

MARCDUINO_ACTION(SetServoLimits, :SL, ({
    int32_t args[5] = { 0, 0, 0, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    uint8_t argcount = 0;
    numberparams(cmd, argcount, args, SizeOfArray(args));
    if (argcount >= 3)
    {
        servoDispatch.setServo(args[0],
            servoDispatch.getPin(args[0]),
            args[1], /* start pulse */
            args[2], /* end pulse */
            (argcount >= 4) ? args[3] : args[1], /* neutral pulse */
            (argcount >= 5) ? args[4] : servoDispatch.getGroup(args[0])); /* group */
    }
}))

MARCDUINO_ACTION(MoveServos, :SM, ({
    int32_t args[5] = { 0, 0, 0, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    uint8_t argcount = 0;
    numberparams(cmd, argcount, args, SizeOfArray(args));
    if (argcount == 2)
    {
        servoDispatch.moveToPulse(args[0], args[1]);
    }
    else if (argcount == 3)
    {
        servoDispatch.moveToPulse(args[0], args[1], args[2]);
    }
    else if (argcount == 4)
    {
        servoDispatch.moveToPulse(args[0], args[1], args[2], args[3]);
    }
    else if (argcount >= 5)
    {
        servoDispatch.moveToPulse(args[0], args[1], args[2], args[3], args[4]);
    }
}))

MARCDUINO_ACTION(OpenCloseRepeatPanelGroupDynamic, :OCR$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllFOpenCloseRepeat, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(FlutterPanelGroupDynamic, :OF$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllFlutter, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(OpenClosePanelGroupDynamic, :OC$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllOpenClose, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(OpenClosePanelLongGroupDynamic, :OCL$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllOpenCloseLong, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(WavePanelGroupDynamic, :OW$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelWave, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(FastWavePanelGroupDynamic, :OWF$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelWaveFast, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(OpenCloseWavePanelGroupDynamic, :OWC$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelOpenCloseWave, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(MarchingAntPanelGroupDynamic, :OMA$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelMarchingAnts, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(AlternatePanelGroupDynamic, :OAP$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAlternate, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(DancePanelGroupDynamic, :OD$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelDance, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(ShakePanelGroupDynamic, :OS$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelLongHarlemShake, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(OpenPanelGroupDynamic, :OP$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllOpen, group, args[0], args[1], onEasing, offEasing);
    }
}))

MARCDUINO_ACTION(ClosePanelGroupDynamic, :CL$, ({
    int32_t args[4] = { 10, 50, 0, 0 };
    char cmdCopy[64];
    strncpy(cmdCopy, Marcduino::getCommand(), sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';
    char* cmd = cmdCopy;
    char* pstr = strchr(cmd, ',');
    if (pstr != nullptr)
    {
        *pstr++ = '\0';
        uint8_t argcount = 0;
        numberparams(pstr, argcount, args, SizeOfArray(args));
    }
    uint32_t group = strtol(cmd, 0, 16);
    if (group != 0)
    {
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        // servoDispatch.setServosEasingMethod(TOP_PIE_PANEL, Easing::BounceEaseOut);
        // servoDispatch.moveServosToPulse(TOP_PIE_PANEL, 0, 1000, 1850);
        // servoDispatch.moveServosToPulse(group, args[0], args[1], args[2], args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllClose, group, args[0], args[1], onEasing, offEasing);
    }
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup1, :OP01, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_1);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup2, :OP02, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_2);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup3, :OP03, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_3);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup4, :OP04, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_4);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup5, :OP05, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_5);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup6, :OP06, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_6);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup7, :OP07, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_7);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup8, :OP08, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_8);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup9, :OP09, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_9);
}))

////////////////

MARCDUINO_ACTION(OpenPanelGroup10, :OP10, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_GROUP_10);
}))

////////////////

MARCDUINO_ACTION(OpenTopPanels, :OP11, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PIE_PANEL);
}))

////////////////

MARCDUINO_ACTION(OpenBottomPanels, :OP12, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, DOME_PANELS_MASK);
}))


MARCDUINO_ACTION(ClosePanelGroup1, :CL01, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_1);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup2, :CL02, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_2);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup3, :CL03, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_3);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup4, :CL04, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_4);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup5, :CL05, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_5);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup6, :CL06, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_6);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup7, :CL07, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_7);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup8, :CL08, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_8);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup9, :CL09, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_9);
}))

////////////////

MARCDUINO_ACTION(ClosePanelGroup10, :CL10, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_GROUP_10);
}))

////////////////

MARCDUINO_ACTION(CloseTopPanels, :CL11, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PIE_PANEL);
}))

////////////////

MARCDUINO_ACTION(CloseBottomPanels, :CL12, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, DOME_PANELS_MASK);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup1, :OF01, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_1, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup2, :OF02, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_2, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup3, :OF03, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_3, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup4, :OF04, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_4, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup5, :OF05, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_5, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup6, :OF06, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_6, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup7, :OF07, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_7, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup8, :OF08, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_8, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup9, :OF09, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_9, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelGroup10, :OF10, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_GROUP_10, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterTopPanels, :OF11, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PIE_PANEL, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterBottomPanels, :OF12, ({
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, DOME_PANELS_MASK, 10, 50);
}))
