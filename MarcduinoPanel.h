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
    if (cmd[0] == '\0' || cmd[1] == '\0' || cmd[2] == '\0' || cmd[3] == '\0' || cmd[4] != '\0') return false;
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
        case 1:  mask = PANEL_P1;  return true;
        case 2:  mask = PANEL_P2;  return true;
        case 3:  mask = PANEL_P3;  return true;
        case 4:  mask = PANEL_P4;  return true;
        case 5:  return false;               // P5 — fixed panel, no servo
        case 6:  return false;               // P6 — fixed panel, no servo
        case 7:  mask = PANEL_P7;  return true;
        case 8:  mask = PANEL_PP1; return true;
        case 9:  mask = PANEL_PP2; return true;
        case 10: mask = PANEL_PP4; return true;
        case 11: mask = PANEL_P11; return true;
        case 12: mask = PANEL_PP6; return true;
        case 13: mask = PANEL_P13; return true;
        case 14: mask = (PIE_PANEL | TOP_PIE_PANEL); return true;  // MarcDuino V3 top-panels shortcut
        case 15: mask = DOME_PANELS_MASK; return true;              // MarcDuino V3 bottom-panels shortcut
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

// NOTE: :MV, #SO, #SC, #SW use getCommand() suffix parsing here.
//
// This works for stable ingress buffers such as MarcduinoSerial's internal
// fBuffer. For async web ingress, command memory can be temporary; those paths
// are handled synchronously in processMarcduinoCommandWithSource() before
// reaching Marcduino::processCommand().

MARCDUINO_ACTION(MovePanelCalibration, :MV, ({
    const char *args = Marcduino::getCommand();
    uint8_t tgt = 0;
    uint16_t val = 0;
    uint32_t msk = 0;
    if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
    {
        movePanelMaskToValue(msk, val);
    }
}))

MARCDUINO_ACTION(SavePanelOpenCalibration, #SO, ({
    const char *args = Marcduino::getCommand();
    uint8_t tgt = 0;
    uint16_t val = 0;
    uint32_t msk = 0;
    if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
    {
        applyPanelCalibrationToMask(msk, true, false, val);
    }
}))

MARCDUINO_ACTION(SavePanelClosedCalibration, #SC, ({
    const char *args = Marcduino::getCommand();
    uint8_t tgt = 0;
    uint16_t val = 0;
    uint32_t msk = 0;
    if (parseTwoDigitTarget(args, tgt) && parseFourDigitValue(args + 2, val) && panelTargetToMask(tgt, msk))
    {
        applyPanelCalibrationToMask(msk, false, true, val);
    }
}))

MARCDUINO_ACTION(SwapPanelOpenClosedCalibration, #SW, ({
    const char *args = Marcduino::getCommand();
    uint8_t tgt = 0;
    uint32_t msk = 0;
    if (parseTwoDigitTarget(args, tgt) && panelTargetToMask(tgt, msk))
    {
        swapPanelCalibrationInMask(msk);
    }
}))

MARCDUINO_ACTION(CloseAllPanels, :CL00, ({
    Marcduino::processCommand(player, "@4S3");
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, ALL_DOME_PANELS_MASK);
    schedulePanelRelease(ALL_DOME_PANELS_MASK);
}))

////////////////

MARCDUINO_ACTION(OpenAllPanels, :OP00, ({
    cancelPanelRelease();
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, ALL_DOME_PANELS_MASK);
}))

////////////////

MARCDUINO_ACTION(FlutterAllPanels, :OF00, ({
    cancelPanelRelease();
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
        cancelPanelRelease(group);
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllFOpenCloseRepeat, group, args[0], args[1], onEasing, offEasing);
        schedulePanelRelease(group, min(max((uint32_t)args[1] * 30u, (uint32_t)5000u), (uint32_t)30000u));
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
        cancelPanelRelease(group);
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
        cancelPanelRelease(group);
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllOpenClose, group, args[0], args[1], onEasing, offEasing);
        schedulePanelRelease(group, min(max((uint32_t)(args[0]+args[1])*15u, (uint32_t)3000u), (uint32_t)30000u));
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
        cancelPanelRelease(group);
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelAllOpenCloseLong, group, args[0], args[1], onEasing, offEasing);
        schedulePanelRelease(group, min(max((uint32_t)(args[0]+args[1])*15u, (uint32_t)5000u), (uint32_t)30000u));
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
        cancelPanelRelease(group);
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelWave, group, args[0], args[1], onEasing, offEasing);
        schedulePanelRelease(group, min(max((uint32_t)(args[0]+args[1])*15u, (uint32_t)3000u), (uint32_t)30000u));
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
        cancelPanelRelease(group);
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelWaveFast, group, args[0], args[1], onEasing, offEasing);
        schedulePanelRelease(group, min(max((uint32_t)(args[0]+args[1])*15u, (uint32_t)3000u), (uint32_t)30000u));
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
        cancelPanelRelease(group);
        Easing::Method onEasing = Easing::getEasingMethod(args[2]);
        Easing::Method offEasing = Easing::getEasingMethod(args[3]);
        SEQUENCE_PLAY_ONCE_VARSPEED_EASING(servoSequencer, SeqPanelOpenCloseWave, group, args[0], args[1], onEasing, offEasing);
        schedulePanelRelease(group, min(max((uint32_t)(args[0]+args[1])*15u, (uint32_t)3000u), (uint32_t)30000u));
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
        cancelPanelRelease(group);
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
        cancelPanelRelease(group);
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
        cancelPanelRelease(group);
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
        cancelPanelRelease(group);
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
        cancelPanelRelease(group);
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
        // Scale release delay by the off-speed so a slow close doesn't get cut short.
        // Default args[1]=50 → 1500ms; args[1]=200 → 2000ms; capped floor at 1500ms.
        schedulePanelRelease(group, min(max((uint32_t)args[1] * 10u, (uint32_t)1500u), (uint32_t)30000u));
    }
}))

////////////////
// MarcDuino V3 group shortcuts — decimal target 14 and 15 route through
// panelTargetToMask(), but the $-wildcard handlers parse their suffix as hex
// (strtol base-16), so :OP14 → 0x14 = 20, not the intended mask. These six
// literal handlers intercept :OP14/:CL14/:OF14 (all pie, incl. PP3) and
// :OP15/:CL15/:OF15 (all ring panels) before the wildcard can misfire.

MARCDUINO_ACTION(OpenTopPanels, :OP14, ({
    // target 14 = PIE_PANEL | TOP_PIE_PANEL (all pie panels, including PP3)
    cancelPanelRelease(PIE_PANEL | TOP_PIE_PANEL);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PIE_PANEL | TOP_PIE_PANEL);
}))

////////////////

MARCDUINO_ACTION(CloseTopPanels, :CL14, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PIE_PANEL | TOP_PIE_PANEL);
    schedulePanelRelease(PIE_PANEL | TOP_PIE_PANEL);
}))

////////////////

MARCDUINO_ACTION(FlutterTopPanels, :OF14, ({
    cancelPanelRelease(PIE_PANEL | TOP_PIE_PANEL);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PIE_PANEL | TOP_PIE_PANEL, 10, 50);
}))

////////////////

MARCDUINO_ACTION(OpenBottomPanels, :OP15, ({
    // target 15 = DOME_PANELS_MASK (all ring panels: small + medium + big)
    cancelPanelRelease(DOME_PANELS_MASK);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, DOME_PANELS_MASK);
}))

////////////////

MARCDUINO_ACTION(CloseBottomPanels, :CL15, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, DOME_PANELS_MASK);
    schedulePanelRelease(DOME_PANELS_MASK);
}))

////////////////

MARCDUINO_ACTION(FlutterBottomPanels, :OF15, ({
    cancelPanelRelease(DOME_PANELS_MASK);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, DOME_PANELS_MASK, 10, 50);
}))

////////////////

MARCDUINO_ACTION(OpenPanel1, :OP01, ({
    cancelPanelRelease(PANEL_P1);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_P1);
}))

////////////////

MARCDUINO_ACTION(OpenPanel2, :OP02, ({
    cancelPanelRelease(PANEL_P2);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_P2);
}))

////////////////

MARCDUINO_ACTION(OpenPanel3, :OP03, ({
    cancelPanelRelease(PANEL_P3);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_P3);
}))

////////////////

MARCDUINO_ACTION(OpenPanel4, :OP04, ({
    cancelPanelRelease(PANEL_P4);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_P4);
}))

////////////////

MARCDUINO_ACTION(OpenPanel5, :OP05, ({
    /* P5 (Magic Panel/frame) — fixed panel, no servo. Recognized for Marcduino compatibility. */
}))

////////////////

MARCDUINO_ACTION(OpenPanel6, :OP06, ({
    /* P6 — fixed panel, no servo. Recognized for Marcduino compatibility. */
}))

////////////////

MARCDUINO_ACTION(OpenPanel7, :OP07, ({
    cancelPanelRelease(PANEL_P7);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_P7);
}))

////////////////

MARCDUINO_ACTION(OpenPanelPP1, :OP08, ({
    cancelPanelRelease(PANEL_PP1);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP1);
}))

////////////////

MARCDUINO_ACTION(OpenPanelPP2, :OP09, ({
    cancelPanelRelease(PANEL_PP2);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP2);
}))

////////////////

MARCDUINO_ACTION(OpenPanelPP4, :OP10, ({
    cancelPanelRelease(PANEL_PP4);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP4);
}))

////////////////

MARCDUINO_ACTION(OpenPanel11, :OP11, ({
    cancelPanelRelease(PANEL_P11);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_P11);
}))

////////////////

MARCDUINO_ACTION(OpenPanelPP6, :OP12, ({
    cancelPanelRelease(PANEL_PP6);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP6);
}))

////////////////

MARCDUINO_ACTION(OpenPanel13, :OP13, ({
    cancelPanelRelease(PANEL_P13);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_P13);
}))

////////////////

MARCDUINO_ACTION(ClosePanel1, :CL01, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_P1);
    schedulePanelRelease(PANEL_P1);
}))

////////////////

MARCDUINO_ACTION(ClosePanel2, :CL02, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_P2);
    schedulePanelRelease(PANEL_P2);
}))

////////////////

MARCDUINO_ACTION(ClosePanel3, :CL03, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_P3);
    schedulePanelRelease(PANEL_P3);
}))

////////////////

MARCDUINO_ACTION(ClosePanel4, :CL04, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_P4);
    schedulePanelRelease(PANEL_P4);
}))

////////////////

MARCDUINO_ACTION(ClosePanel5, :CL05, ({
    /* P5 (Magic Panel/frame) — fixed panel, no servo. Recognized for Marcduino compatibility. */
}))

////////////////

MARCDUINO_ACTION(ClosePanel6, :CL06, ({
    /* P6 — fixed panel, no servo. Recognized for Marcduino compatibility. */
}))

////////////////

MARCDUINO_ACTION(ClosePanel7, :CL07, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_P7);
    schedulePanelRelease(PANEL_P7);
}))

////////////////

MARCDUINO_ACTION(ClosePanelPP1, :CL08, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP1);
    schedulePanelRelease(PANEL_PP1);
}))

////////////////

MARCDUINO_ACTION(ClosePanelPP2, :CL09, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP2);
    schedulePanelRelease(PANEL_PP2);
}))

////////////////

MARCDUINO_ACTION(ClosePanelPP4, :CL10, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP4);
    schedulePanelRelease(PANEL_PP4);
}))

////////////////

MARCDUINO_ACTION(ClosePanel11, :CL11, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_P11);
    schedulePanelRelease(PANEL_P11);
}))

////////////////

MARCDUINO_ACTION(ClosePanelPP6, :CL12, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP6);
    schedulePanelRelease(PANEL_PP6);
}))

////////////////

MARCDUINO_ACTION(ClosePanel13, :CL13, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_P13);
    schedulePanelRelease(PANEL_P13);
}))

////////////////

MARCDUINO_ACTION(FlutterPanel1, :OF01, ({
    cancelPanelRelease(PANEL_P1);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_P1, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanel2, :OF02, ({
    cancelPanelRelease(PANEL_P2);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_P2, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanel3, :OF03, ({
    cancelPanelRelease(PANEL_P3);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_P3, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanel4, :OF04, ({
    cancelPanelRelease(PANEL_P4);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_P4, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanel5, :OF05, ({
    /* P5 (Magic Panel/frame) — fixed panel, no servo. Recognized for Marcduino compatibility. */
}))

////////////////

MARCDUINO_ACTION(FlutterPanel6, :OF06, ({
    /* P6 — fixed panel, no servo. Recognized for Marcduino compatibility. */
}))

////////////////

MARCDUINO_ACTION(FlutterPanel7, :OF07, ({
    cancelPanelRelease(PANEL_P7);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_P7, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelPP1, :OF08, ({
    cancelPanelRelease(PANEL_PP1);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP1, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelPP2, :OF09, ({
    cancelPanelRelease(PANEL_PP2);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP2, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelPP4, :OF10, ({
    cancelPanelRelease(PANEL_PP4);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP4, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanel11, :OF11, ({
    cancelPanelRelease(PANEL_P11);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_P11, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanelPP6, :OF12, ({
    cancelPanelRelease(PANEL_PP6);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP6, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPanel13, :OF13, ({
    cancelPanelRelease(PANEL_P13);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_P13, 10, 50);
}))

////////////////

MARCDUINO_ACTION(OpenPiePanel1, :OPP1, ({
    cancelPanelRelease(PANEL_PP1);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP1);
}))

////////////////

MARCDUINO_ACTION(OpenPiePanel2, :OPP2, ({
    cancelPanelRelease(PANEL_PP2);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP2);
}))

////////////////

MARCDUINO_ACTION(OpenPiePanel3, :OPP3, ({
    cancelPanelRelease(PANEL_PP3);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP3);
}))

////////////////

MARCDUINO_ACTION(OpenPiePanel4, :OPP4, ({
    cancelPanelRelease(PANEL_PP4);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP4);
}))

////////////////

MARCDUINO_ACTION(OpenPiePanel5, :OPP5, ({
    cancelPanelRelease(PANEL_PP5);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP5);
}))

////////////////

MARCDUINO_ACTION(OpenPiePanel6, :OPP6, ({
    cancelPanelRelease(PANEL_PP6);
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllOpen, PANEL_PP6);
}))

////////////////

MARCDUINO_ACTION(ClosePiePanel1, :CLP1, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP1);
    schedulePanelRelease(PANEL_PP1);
}))

////////////////

MARCDUINO_ACTION(ClosePiePanel2, :CLP2, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP2);
    schedulePanelRelease(PANEL_PP2);
}))

////////////////

MARCDUINO_ACTION(ClosePiePanel3, :CLP3, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP3);
    schedulePanelRelease(PANEL_PP3);
}))

////////////////

MARCDUINO_ACTION(ClosePiePanel4, :CLP4, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP4);
    schedulePanelRelease(PANEL_PP4);
}))

////////////////

MARCDUINO_ACTION(ClosePiePanel5, :CLP5, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP5);
    schedulePanelRelease(PANEL_PP5);
}))

////////////////

MARCDUINO_ACTION(ClosePiePanel6, :CLP6, ({
    SEQUENCE_PLAY_ONCE(servoSequencer, SeqPanelAllClose, PANEL_PP6);
    schedulePanelRelease(PANEL_PP6);
}))

////////////////

MARCDUINO_ACTION(FlutterPiePanel1, :OFP1, ({
    cancelPanelRelease(PANEL_PP1);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP1, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPiePanel2, :OFP2, ({
    cancelPanelRelease(PANEL_PP2);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP2, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPiePanel3, :OFP3, ({
    cancelPanelRelease(PANEL_PP3);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP3, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPiePanel4, :OFP4, ({
    cancelPanelRelease(PANEL_PP4);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP4, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPiePanel5, :OFP5, ({
    cancelPanelRelease(PANEL_PP5);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP5, 10, 50);
}))

////////////////

MARCDUINO_ACTION(FlutterPiePanel6, :OFP6, ({
    cancelPanelRelease(PANEL_PP6);
    SEQUENCE_PLAY_ONCE_VARSPEED(servoSequencer, SeqPanelAllFlutter, PANEL_PP6, 10, 50);
}))
