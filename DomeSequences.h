// DomeSequences.h
// Ported from thePunderWoman/AstroPixelsPlus fork by Jessica Janiuk
// https://github.com/thePunderWoman/AstroPixelsPlus
// Original: DomeSequences.cpp (VarSpeedServo / Arduino Pro Mini, 2023), ported 2026
//
// Commands use the "DM:" Marcduino prefix — e.g. "DM:SCREAM\r"
// Adapted for this fork: panel index remapping, servo range, bodyLinkSend() transport.

#pragma once

// =============================================================================
// Panel servo indices — servoSettings[] array positions in AstroPixelsPlus.ino
// =============================================================================
#define DP1    0   /* Small Panel 1  — PANEL_GROUP_1 */
#define DP2    1   /* Small Panel 2  — PANEL_GROUP_2 */
#define DP3    2   /* Small Panel 3  — PANEL_GROUP_3 */
#define DP4    3   /* Small Panel 4  — PANEL_GROUP_4 */
#define DP5    4   /* Medium Panel 5 — PANEL_GROUP_5 */
#define DP6    5   /* Large Panel 6  — PANEL_GROUP_6 */
#define DPA    6   /* Mini Panel A   — MINI_PANEL     */
#define DPB    7   /* Mini Panel B   — MINI_PANEL     */
#define DPP1   8   /* Pie Panel 7    — PANEL_GROUP_7  */
#define DPP2   9   /* Pie Panel 8    — PANEL_GROUP_8  */
#define DPP3  10   /* Pie Panel 9    — PANEL_GROUP_9  */
#define DPP4  11   /* Pie Panel 10   — PANEL_GROUP_10 */

// =============================================================================
// Panel position values (pulse width in microseconds).
// Matches AstroPixelsPlus servoSettings[] default range (800–2200 µs).
// Per-panel calibration lives in NVS via the web UI — do NOT add offsets here.
// =============================================================================
#define DOME_PANEL_OPEN      2200   // matches servoSettings[] default max
#define DOME_PIE_PANEL_OPEN  2200   // pie panels same
#define DOME_PANEL_CLOSE      800   // matches servoSettings[] default min
#define DOME_PANEL_RANGE     (DOME_PANEL_OPEN - DOME_PANEL_CLOSE)
#define DOME_PANEL_75_OPEN   (DOME_PANEL_CLOSE + (DOME_PANEL_RANGE * 3 / 4))
#define DOME_PANEL_50_OPEN   (DOME_PANEL_CLOSE + (DOME_PANEL_RANGE / 2))
#define DOME_PANEL_25_OPEN   (DOME_PANEL_CLOSE + (DOME_PANEL_RANGE / 4))

// =============================================================================
// Move-time constants (milliseconds).
// Smaller = faster. Tune to taste for your servos.
// =============================================================================
#define DOME_MOVE_SLOWSPEED    500   // deliberate/dramatic
#define DOME_MOVE_VLOWSPEED    350
#define DOME_MOVE_LOWSPEED     200
#define DOME_MOVE_SPEED        150   // normal open/close
#define DOME_MOVE_FASTSPEED    100   // burst/snap open
#define DOME_MOVE_OPENSPEED    100
#define DOME_MOVE_CLOSESPEED   100
#define DOME_MOVE_OVERLOAD     300   // intentionally very slow drift

// =============================================================================
// Panel open/close state (toggle tracking) and re-entrancy guard.
// Defined in AstroPixelsPlus.ino; declared extern here so the header can be
// included only once without duplicating storage across translation units.
// =============================================================================
extern bool dome_PiesOpen;
extern bool dome_AllOpen;
extern bool dome_LowOpen;
extern bool dome_seqRunning;

// =============================================================================
// Core helpers
// =============================================================================

// Pump the full ReelTwo event loop while waiting.
// This lets PCA9685 servo tweens animate smoothly during blocking sequences.
// handleBodyLinkHeartbeat() is called each iteration to keep protoR2link alive
// during long blocking sequences; the function is internally rate-limited to 1 Hz.
static void domeWaitTime(unsigned long ms)
{
    unsigned long end = millis() + ms;
    while (millis() < end)
    {
        AnimatedEvent::process();
        handleBodyLinkHeartbeat();
    }
}

// Send a named body cue to protoArtoo via protoR2link.
// Routes over UART or WiFi depending on which transport is active.
static void domeSendToBody(const char* cmd)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "BD:%s", cmd);
    sendBodyCommand(buf);
}

// Notify the body controller that a dome sequence is starting (seconds = safety timeout).
// Body will pause random sounds/motion until dome=seqoff or timeout expires.
static void domeBeginSequence(unsigned int seconds)
{
    dome_seqRunning = true;
    char buf[32];
    snprintf(buf, sizeof(buf), "dome=seqon,%u", seconds);
    sendBodyCommand(buf);
}

static void domeEndSequence()
{
    sendBodyCommand("dome=seqoff");
    dome_seqRunning = false;
}

static inline void domeMove(uint8_t idx, uint16_t pos, uint32_t moveMs, bool wait = false)
{
    servoDispatch.moveToPulse(idx, moveMs, pos);
    if (wait) {
        while (servoDispatch.isActive(idx))
            AnimatedEvent::process();
    }
}

// =============================================================================
// Easing helpers — drive multiple servos with ServoDispatch easing methods.
// =============================================================================
static void domeEaseSineInOut(const uint8_t* idx, uint8_t count,
                              int /*from*/, int to, unsigned int durationMs)
{
    for (uint8_t i = 0; i < count; i++)
        servoDispatch.setServoEasingMethod(idx[i], Easing::SineEaseInOut);
    for (uint8_t i = 0; i < count; i++)
        servoDispatch.moveToPulse(idx[i], durationMs, (uint16_t)to);
    domeWaitTime(durationMs + 50);
    for (uint8_t i = 0; i < count; i++)
        servoDispatch.setServoEasingMethod(idx[i], Easing::LinearInterpolation);
}

static void domeEaseOut(const uint8_t* idx, uint8_t count,
                        int /*from*/, int to, unsigned int durationMs)
{
    for (uint8_t i = 0; i < count; i++)
        servoDispatch.setServoEasingMethod(idx[i], Easing::SineEaseOut);
    for (uint8_t i = 0; i < count; i++)
        servoDispatch.moveToPulse(idx[i], durationMs, (uint16_t)to);
    domeWaitTime(durationMs + 50);
    for (uint8_t i = 0; i < count; i++)
        servoDispatch.setServoEasingMethod(idx[i], Easing::LinearInterpolation);
}

// =============================================================================
// Random panel selection (Fisher-Yates shuffle; indices = servoDispatch indices)
// =============================================================================
static uint8_t domeRandomPanels(uint8_t numLower, uint8_t numPie, uint8_t* out)
{
    static const uint8_t lowerPanels[] = { DP1, DP2, DP3, DP4, DP5, DP6, DPA, DPB };
    static const uint8_t piePanels[]   = { DPP1, DPP2, DPP3, DPP4 };

    uint8_t lower[8], pie[4];
    memcpy(lower, lowerPanels, sizeof(lower));
    memcpy(pie,   piePanels,   sizeof(pie));

    for (uint8_t i = 7; i > 0; i--) { uint8_t j = random(i + 1); uint8_t t = lower[i]; lower[i] = lower[j]; lower[j] = t; }
    for (uint8_t i = 3; i > 0; i--) { uint8_t j = random(i + 1); uint8_t t = pie[i];   pie[i]   = pie[j];   pie[j]   = t; }

    if (numLower > 8) numLower = 8;
    if (numPie   > 4) numPie   = 4;

    uint8_t total = 0;
    for (uint8_t i = 0; i < numLower; i++) out[total++] = lower[i];
    for (uint8_t i = 0; i < numPie;   i++) out[total++] = pie[i];
    return total;
}

// =============================================================================
// Reset helpers
// =============================================================================
static void domeResetHolos()
{
    CommandEvent::process(F("HPS9"));
}

static void domeResetLogics()
{
    RLD.selectSequence(LogicEngineRenderer::NORMAL);
    FLD.selectSequence(LogicEngineRenderer::NORMAL);
}

static void domeResetPSIs()
{
    frontPSI.selectSequence(LogicEngineRenderer::NORMAL);
    rearPSI.selectSequence(LogicEngineRenderer::NORMAL);
}

static void domeResetBody()
{
    domeSendToBody("RESET");
}

// =============================================================================
// Open / Close Pie Panels
// =============================================================================
static void domeOpenClosePies()
{
    domeBeginSequence(12);

    if (dome_PiesOpen)
    {
        dome_PiesOpen = false;
        domeResetHolos();
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");

        domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);

        domeWaitTime(800);
        servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
        servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
    }
    else
    {
        dome_PiesOpen = true;
        domeWaitTime(100);
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");

        for (int i = 0; i < 2; i++)
        {
            // wave open
            domeMove(DPP2, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP1, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP4, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP3, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            // wave close
            domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED, true);
            // reopen
            domeMove(DPP2, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP1, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP4, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP3, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
        }

        domeWaitTime(1000);
        servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
        servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
    }

    domeEndSequence();
}

// =============================================================================
// Open / Close Low Panels
// =============================================================================
static void domeOpenCloseLow()
{
    domeBeginSequence(15);

    if (dome_LowOpen)
    {
        dome_LowOpen = false;
        domeResetHolos();
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");

        domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);

        domeWaitTime(1000);
        servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
        servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
        servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
        servoDispatch.disable(DPA);  servoDispatch.disable(DPB);
    }
    else
    {
        dome_LowOpen = true;
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");

        for (int i = 0; i < 2; i++)
        {
            // wave open — DPA/DPB/DP6 non-blocking so they move with DP1
            domeMove(DP1,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);
            domeMove(DPA,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);
            domeMove(DPB,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);
            domeMove(DP6,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);
            domeMove(DP2,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);
            domeMove(DP3,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);
            domeMove(DP4,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);
            domeMove(DP5,  DOME_PANEL_OPEN, DOME_MOVE_SPEED, true);

            // wave close
            domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
            domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
            domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
            domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
            domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
            domeWaitTime(50);
            domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
            domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
            domeWaitTime(50);
            domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        }

        // final open
        domeMove(DP6,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DPA,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DPB,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DP1,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
        domeMove(DP2,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
        domeMove(DP3,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
        domeMove(DP4,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
        domeMove(DP5,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);

        domeWaitTime(1000);
        servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
        servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
        servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
        servoDispatch.disable(DPA);  servoDispatch.disable(DPB);
    }

    domeEndSequence();
}

// =============================================================================
// Open / Close All Panels
// =============================================================================
static void domeOpenCloseAll()
{
    domeBeginSequence(10);

    if (dome_AllOpen)
    {
        dome_AllOpen = false;
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");

        domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
        domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);

        domeWaitTime(500);
        servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
        servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
        servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
        servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
        servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
        servoDispatch.disable(DPA);  servoDispatch.disable(DPB);
    }
    else
    {
        dome_AllOpen = true;
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");

        // open pies
        domeMove(DPP2, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED, true);
        domeMove(DPP3, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED, true);
        domeMove(DPP1, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED, true);
        domeMove(DPP4, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED, true);

        // open lows (non-blocking except last)
        domeMove(DP6,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DPA,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DPB,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DP1,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DP2,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DP3,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DP4,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeMove(DP5,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);

        // twinkle DP1/DP2 and DPP2/DPP3
        for (int i = 0; i < 2; i++)
        {
            domeMove(DP1,  DOME_PANEL_75_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DP1,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
            domeWaitTime(80);
            domeMove(DP2,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DP2,  DOME_PANEL_75_OPEN, DOME_MOVE_FASTSPEED);
            domeWaitTime(80);
            domeMove(DP2,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
            domeWaitTime(100);

            domeMove(DPP2, DOME_PANEL_75_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP2, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
            domeWaitTime(80);
            domeMove(DPP3, DOME_PANEL_75_OPEN, DOME_MOVE_FASTSPEED, true);
            domeMove(DPP3, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, true);
        }

        domeWaitTime(800);
        servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
        servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
        servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
        servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
        servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
        servoDispatch.disable(DPA);  servoDispatch.disable(DPB);
    }

    domeEndSequence();
}

// =============================================================================
// Flutter — quick open-close flap, all panels end closed
// =============================================================================
static void domeFlutter()
{
    domeBeginSequence(10);

    domeMove(DP1,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeMove(DP2,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeMove(DP3,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeWaitTime(20);
    domeMove(DP4,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeWaitTime(20);
    domeMove(DP5,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeMove(DP6,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeMove(DPA,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeMove(DPB,  DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);

    domeMove(DPP2, DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeMove(DPP1, DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeMove(DPP4, DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);
    domeWaitTime(20);
    domeMove(DPP3, DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, true);

    domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);

    domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);
    domeWaitTime(20);
    domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, true);

    domeWaitTime(500);

    servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
    servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
    servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
    servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
    servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
    servoDispatch.disable(DPA);  servoDispatch.disable(DPB);

    dome_PiesOpen = false;
    dome_AllOpen  = false;
    dome_LowOpen  = false;

    domeEndSequence();
}

// =============================================================================
// Bloom — pie panels ease open, wiggle, close
// =============================================================================
static void domeBloom()
{
    domeBeginSequence(8);

    static const uint8_t pies[] = { DPP1, DPP2, DPP3, DPP4 };

    domeEaseOut(pies, 4, DOME_PANEL_CLOSE, DOME_PANEL_OPEN, 1200);
    domeWaitTime(2000);

    for (uint8_t i = 0; i < 3; i++)
    {
        domeEaseSineInOut(pies, 4, DOME_PIE_PANEL_OPEN, 1900, 130);
        domeEaseSineInOut(pies, 4, 1900, DOME_PIE_PANEL_OPEN, 130);
    }

    domeWaitTime(1000);

    domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);

    domeWaitTime(500);
    servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
    servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);

    dome_PiesOpen = false;
    domeEndSequence();
}

// =============================================================================
// Scream — all panels burst open with random fluttering, then close
// =============================================================================
static void domeScream()
{
    domeBeginSequence(15);

    CommandEvent::process(F("HPA0070")); // all holos short circuit random color
    CommandEvent::process(F("HPA105|5")); // all holos wag 5 times

    FLD.selectSequence(LogicEngineRenderer::REDALERT, FLD.kDefault, 0, 15);
    RLD.selectSequence(LogicEngineRenderer::REDALERT, RLD.kDefault, 0, 15);
    frontPSI.selectSequence(LogicEngineRenderer::REDALERT, frontPSI.kDefault, 0, 15);
    rearPSI.selectSequence(LogicEngineRenderer::REDALERT, rearPSI.kDefault, 0, 15);

    domeSendToBody("SCREAM");

    dome_AllOpen = true;

    // burst open
    domeMove(DPP2, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
    domeMove(DPP3, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
    domeMove(DPP1, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
    domeMove(DPP4, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
    domeMove(DP6,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeMove(DPA,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeMove(DPB,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeMove(DP1,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeMove(DP2,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeMove(DP3,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeMove(DP4,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeMove(DP5,  DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED);
    domeWaitTime(DOME_MOVE_FASTSPEED + 100);

    // random flutter
    static const uint8_t allPanels[] = { DPP1, DPP2, DPP3, DPP4, DP1, DP2, DP3, DP4, DP5, DP6, DPA, DPB };
    randomSeed(analogRead(0));
    for (int i = 0; i < 10; i++)
    {
        uint8_t idx = allPanels[random(12)];
        domeMove(idx, DOME_PANEL_50_OPEN, DOME_MOVE_FASTSPEED, true);
        domeMove(idx, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeWaitTime(80);
        domeMove(idx, DOME_PANEL_50_OPEN, DOME_MOVE_FASTSPEED, true);
        domeMove(idx, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED);
        domeWaitTime(100);
    }

    domeWaitTime(800);
    domeWaitTime(2000);

    // close
    dome_AllOpen = false;
    if (preferences.getBool("dm_happy_sound", true))
        domeSendToBody("HAPPY");

    domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeWaitTime(DOME_MOVE_SPEED + 500);

    servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
    servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
    servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
    servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
    servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
    servoDispatch.disable(DPA);  servoDispatch.disable(DPB);

    domeResetHolos();
    domeEndSequence();
}

// =============================================================================
// Overload — random panels sluggishly drift open, then snap closed
// =============================================================================
static void domeOverload()
{
    domeBeginSequence(12);

    FLD.selectSequence(LogicEngineRenderer::FAILURE);
    RLD.selectSequence(LogicEngineRenderer::FAILURE);

    CommandEvent::process(F("HPA0070")); // all holos short circuit random color
    frontPSI.selectSequence(LogicEngineRenderer::FAILURE, frontPSI.kDefault, 0, 12);
    rearPSI.selectSequence(LogicEngineRenderer::FAILURE, rearPSI.kDefault, 0, 12);

    domeSendToBody("OVERLOAD");

    uint8_t panels[6];
    uint8_t count = domeRandomPanels(4, 2, panels);

    for (uint8_t i = 0; i < count; i++)
    {
        int pos = random(DOME_PANEL_25_OPEN, DOME_PANEL_50_OPEN + 1);
        domeMove(panels[i], (uint16_t)pos, DOME_MOVE_OVERLOAD);
        domeWaitTime(random(400, 900));
    }

    domeWaitTime(2500);

    for (uint8_t i = 0; i < count; i++)
        domeMove(panels[i], DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeWaitTime(800);
    for (uint8_t i = 0; i < count; i++)
        servoDispatch.disable(panels[i]);

    domeResetHolos();
    domeResetLogics();
    domeResetPSIs();

    domeEndSequence();
}

// =============================================================================
// Heart — rainbow holos, sweet message on logics
// =============================================================================
static void domeHeart()
{
    domeBeginSequence(10);

    CommandEvent::process(F("HPF006|10"));
    CommandEvent::process(F("HPR006|10"));
    CommandEvent::process(F("HPT006|10"));
    FLD.selectScrollTextLeft("You're\nWonderful", FLD.kDefault, 0, 10);
    frontPSI.selectSequence(LogicEngineRenderer::FLASHCOLOR, frontPSI.kDefault, 0, 10);
    domeSendToBody("HEART");
    domeWaitTime(10000);
    domeEndSequence();
}

// =============================================================================
// Alarm — pulsing red holos and logics
// =============================================================================
static void domeAlarm()
{
    domeBeginSequence(10);

    CommandEvent::process(F("HPA0021|10")); // all holos red flashes

    FLD.selectSequence(LogicEngineRenderer::ALARM, FLD.kDefault, 0, 10);
    RLD.selectSequence(LogicEngineRenderer::ALARM, RLD.kDefault, 0, 10);
    frontPSI.selectSequence(LogicEngineRenderer::ALARM, frontPSI.kDefault, 0, 10);
    rearPSI.selectSequence(LogicEngineRenderer::ALARM, rearPSI.kDefault, 0, 10);

    domeSendToBody("ALARM");
    domeWaitTime(10000);
    domeEndSequence();
}

// =============================================================================
// Disco — delegates to :SE09 (DiscoSequence), which owns the body sound cue
// =============================================================================
static void domeDisco()
{
    domeBeginSequence(46);
    Marcduino::processCommand(player, ":SE09");
    domeEndSequence();
}

// =============================================================================
// Vader — imperial march
// =============================================================================
static void domeVader()
{
    domeBeginSequence(47);

    CommandEvent::process(F("HPA0021|47")); // all holos red flashes

    FLD.selectSequence(LogicEngineRenderer::MARCH, FLD.kRed, 0, 47);
    RLD.selectSequence(LogicEngineRenderer::MARCH, RLD.kRed, 0, 47);
    frontPSI.selectSequence(LogicEngineRenderer::MARCH, frontPSI.kDefault, 0, 47);
    rearPSI.selectSequence(LogicEngineRenderer::MARCH, rearPSI.kDefault, 0, 47);

    domeSendToBody("VADER");
    domeWaitTime(47000);
    domeEndSequence();
}

// =============================================================================
// Rock March — alt imperial march
// =============================================================================
static void domeRockMarch()
{
    domeBeginSequence(47);

    CommandEvent::process(F("HPA0021|47")); // all holos red flashes

    FLD.selectSequence(LogicEngineRenderer::MARCH, FLD.kRed, 0, 47);
    RLD.selectSequence(LogicEngineRenderer::MARCH, RLD.kRed, 0, 47);
    frontPSI.selectSequence(LogicEngineRenderer::MARCH, frontPSI.kDefault, 0, 47);
    rearPSI.selectSequence(LogicEngineRenderer::MARCH, rearPSI.kDefault, 0, 47);

    domeSendToBody("ROCKMARCH");
    domeWaitTime(47000);
    domeEndSequence();
}

// =============================================================================
// Hello There — DP1 waves a greeting
// =============================================================================
static void domeHelloThere()
{
    domeBeginSequence(4);

    FLD.selectScrollTextLeft("Hello\nThere", FLD.kDefault, 0, 10);
    RLD.selectScrollTextLeft("General Kenobi", RLD.randomColor());

    domeSendToBody("HELLO");

    domeMove(DP1, DOME_PANEL_OPEN,    DOME_MOVE_SPEED, true);
    domeWaitTime(10);
    domeMove(DP1, DOME_PANEL_50_OPEN, DOME_MOVE_SPEED, true);
    domeWaitTime(10);
    domeMove(DP1, DOME_PANEL_OPEN,    DOME_MOVE_SPEED, true);
    domeWaitTime(10);
    domeMove(DP1, DOME_PANEL_50_OPEN, DOME_MOVE_SPEED, true);
    domeWaitTime(10);
    domeMove(DP1, DOME_PANEL_OPEN,    DOME_MOVE_SPEED, true);
    domeWaitTime(10);
    domeMove(DP1, DOME_PANEL_CLOSE,   DOME_MOVE_SPEED, true);
    servoDispatch.disable(DP1);

    domeEndSequence();
}

// =============================================================================
// Leia — front HP runs Leia LED sequence, all other HPs off, logics Leia mode
// =============================================================================
static void domeLeiaMode()
{
    domeBeginSequence(36);

    CommandEvent::process(F("HPS101|36")); // front holo leia sequence
    CommandEvent::process(F("HPR02|36"));  // rear holo off
    CommandEvent::process(F("HPT02|36"));  // top holo off

    FLD.selectSequence(LogicEngineRenderer::LEIA, FLD.kDefault, 0, 36);
    RLD.selectSequence(LogicEngineRenderer::LEIA, RLD.kDefault, 0, 36);
    frontPSI.selectSequence(LogicEngineRenderer::LEIA, frontPSI.kDefault, 0, 36);
    rearPSI.selectSequence(LogicEngineRenderer::LEIA, rearPSI.kDefault, 0, 36);

    domeSendToBody("LEIA");
    domeWaitTime(36000);
    domeEndSequence();
}

// =============================================================================
// Reset All — close every panel, reset holos / logics / PSIs / body
// =============================================================================
static void domeResetAll()
{
    domeBeginSequence(4);

    domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
    domeWaitTime(DOME_MOVE_SPEED + 1000);

    servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
    servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
    servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
    servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
    servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
    servoDispatch.disable(DPA);  servoDispatch.disable(DPB);

    dome_PiesOpen = false;
    dome_AllOpen  = false;
    dome_LowOpen  = false;

    domeResetHolos();
    domeResetLogics();
    domeResetPSIs();
    domeResetBody();

    domeEndSequence();
}

// =============================================================================
// Cantina — 15-second alternating panel dance at 130 BPM
// =============================================================================
static void domeCantina()
{
    domeBeginSequence(17);
    domeSendToBody("CANTINA");

    CommandEvent::process(F("HPA0029|15")); // all holos white flashes

    FLD.selectSequence(LogicEngineRenderer::FLASHCOLOR, FLD.kBlue, 0, 15);
    RLD.selectSequence(LogicEngineRenderer::FLASHCOLOR, RLD.kBlue, 0, 15);
    frontPSI.selectSequence(LogicEngineRenderer::FLASHCOLOR, frontPSI.kDefault, 0, 15);
    rearPSI.selectSequence(LogicEngineRenderer::FLASHCOLOR, rearPSI.kDefault, 0, 15);

    // 130 BPM ≈ 923 ms per beat
    const unsigned long BEAT_MS  = 923;
    const unsigned long DURATION = 15000;
    domeWaitTime(100);

    bool evenOpen = true;
    unsigned long endTime = millis() + DURATION;

    while (millis() < endTime)
    {
        if (evenOpen)
        {
            domeMove(DPP1, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DPP3, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DP1,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DP3,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DP5,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DPA,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
        }
        else
        {
            domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DPP2, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DPP4, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_SPEED);
            domeMove(DP2,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DP4,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DP6,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
            domeMove(DPB,  DOME_PANEL_OPEN, DOME_MOVE_SPEED);
        }
        evenOpen = !evenOpen;
        domeWaitTime(BEAT_MS);
    }

    domeMove(DPP1, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPP2, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPP3, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPP4, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DP1,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DP2,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DP3,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DP4,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DP5,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DP6,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPA,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED);
    domeMove(DPB,  DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED, true);

    domeWaitTime(500);

    servoDispatch.disable(DPP1); servoDispatch.disable(DPP2);
    servoDispatch.disable(DPP3); servoDispatch.disable(DPP4);
    servoDispatch.disable(DP1);  servoDispatch.disable(DP2);
    servoDispatch.disable(DP3);  servoDispatch.disable(DP4);
    servoDispatch.disable(DP5);  servoDispatch.disable(DP6);
    servoDispatch.disable(DPA);  servoDispatch.disable(DPB);

    dome_PiesOpen = false;
    dome_AllOpen  = false;
    dome_LowOpen  = false;

    domeResetPSIs();
    domeResetLogics();
    domeResetHolos();

    domeEndSequence();
}

// =============================================================================
// Random — picks one sequence from the standard pool.
// Each underlying :SE sequence routes its own body sound via sendBodyCommand().
// Do NOT wrap this in domeBeginSequence() — that would double-coordinate the body.
// =============================================================================
static void domeRandom()
{
    static const char* const sequences[] = {
        ":SE02",  // Wave
        ":SE03",  // SmirkWave
        ":SE04",  // OpenCloseWave
        ":SE05",  // BeepCantina
        ":SE06",  // Short
        ":SE52",  // WavePanel
        ":SE53",  // SmirkWavePanel
        ":SE54",  // OpenWave
        ":SE55",  // MarchingAnts
        ":SE56",  // Faint
        ":SE57",  // Rythmic
        ":SE58",  // PanelWaveByeBye (fork-specific)
        "$815",   // HarlemShake
        "$821",   // GirlOnFire
        "$720",   // YodaClearMind
    };
    static const uint8_t count = sizeof(sequences) / sizeof(sequences[0]);
    Marcduino::processCommand(player, sequences[random(count)]);
}

// =============================================================================
// Marcduino serial command handlers — prefix "DM:" on COMMAND_SERIAL
//
// Send from any Marcduino-compatible device as:   DM:PIES\r
// =============================================================================

MARCDUINO_ACTION(DomeReset,     DM:RESET,       ({ if (!dome_seqRunning) domeResetAll();      }))
MARCDUINO_ACTION(DomePies,      DM:PIES,        ({ if (!dome_seqRunning) domeOpenClosePies(); }))
MARCDUINO_ACTION(DomeLow,       DM:LOW,         ({ if (!dome_seqRunning) domeOpenCloseLow();  }))
MARCDUINO_ACTION(DomeOpenAll,   DM:OPENALL,     ({ if (!dome_seqRunning) domeOpenCloseAll();  }))
MARCDUINO_ACTION(DomeLeia,      DM:LEIA,        ({ if (!dome_seqRunning) domeLeiaMode();      }))
MARCDUINO_ACTION(DomeHeart,     DM:HEART,       ({ if (!dome_seqRunning) domeHeart();         }))
MARCDUINO_ACTION(DomeHello,     DM:HELLO,       ({ if (!dome_seqRunning) domeHelloThere();    }))
MARCDUINO_ACTION(DomeScream,    DM:SCREAM,      ({ if (!dome_seqRunning) domeScream();        }))
MARCDUINO_ACTION(DomeFlutter,   DM:FLUTTER,     ({ if (!dome_seqRunning) domeFlutter();       }))
MARCDUINO_ACTION(DomeOverload,  DM:OVERLOAD,    ({ if (!dome_seqRunning) domeOverload();      }))
MARCDUINO_ACTION(DomeBloom,     DM:BLOOM,       ({ if (!dome_seqRunning) domeBloom();         }))
MARCDUINO_ACTION(DomeCantina,   DM:CANTINA,     ({ if (!dome_seqRunning) domeCantina();       }))
MARCDUINO_ACTION(DomeAlarm,     DM:ALARM,       ({ if (!dome_seqRunning) domeAlarm();         }))
MARCDUINO_ACTION(DomeSeqDisco,  DM:DISCO,       ({ if (!dome_seqRunning) domeDisco();         }))
MARCDUINO_ACTION(DomeRockMarch, DM:ROCKMARCH,   ({ if (!dome_seqRunning) domeRockMarch();     }))
MARCDUINO_ACTION(DomeRandom,    DM:RANDOM,      ({ if (!dome_seqRunning) domeRandom();        }))
MARCDUINO_ACTION(DomeVader,     DM:VADER,       ({ if (!dome_seqRunning) domeVader();         }))

// =============================================================================
// MarcduinoSequence.h aliases — expose predefined SE sequences via DM: names
//
// Conflicts with dome-local sequences (SCREAM, CANTINA, LEIA) get an "SE"
// prefix to make clear they are the standard Marcduino versions.
// =============================================================================
MARCDUINO_ACTION(DomeSeqStop,           DM:STOP,           ({ Marcduino::processCommand(player, ":SE00"); }))
MARCDUINO_ACTION(DomeSeqScream,         DM:SESCREAM,       ({ Marcduino::processCommand(player, ":SE01"); }))
MARCDUINO_ACTION(DomeSeqWave,           DM:WAVE,           ({ Marcduino::processCommand(player, ":SE02"); }))
MARCDUINO_ACTION(DomeSeqSmirkWave,      DM:SMIRKWAVE,      ({ Marcduino::processCommand(player, ":SE03"); }))
MARCDUINO_ACTION(DomeSeqOCWave,         DM:OCWAVE,         ({ Marcduino::processCommand(player, ":SE04"); }))
MARCDUINO_ACTION(DomeSeqBeepCantina,    DM:BEEPCANTINA,    ({ Marcduino::processCommand(player, ":SE05"); }))
MARCDUINO_ACTION(DomeSeqShort,          DM:SHORT,          ({ Marcduino::processCommand(player, ":SE06"); }))
MARCDUINO_ACTION(DomeSeqSecantina,      DM:SECANTINA,      ({ Marcduino::processCommand(player, ":SE07"); }))
MARCDUINO_ACTION(DomeSeqSeleia,         DM:SELEIA,         ({ Marcduino::processCommand(player, ":SE08"); }))
MARCDUINO_ACTION(DomeSeqScreamNoPanel,  DM:SCREAMNOPANEL,  ({ Marcduino::processCommand(player, ":SE50"); }))
MARCDUINO_ACTION(DomeSeqScreamPanel,    DM:SCREAMPANEL,    ({ Marcduino::processCommand(player, ":SE51"); }))
MARCDUINO_ACTION(DomeSeqWavePanel,      DM:WAVEPANEL,      ({ Marcduino::processCommand(player, ":SE52"); }))
MARCDUINO_ACTION(DomeSeqSmirkWavePanel, DM:SMIRKWAVEPANEL, ({ Marcduino::processCommand(player, ":SE53"); }))
MARCDUINO_ACTION(DomeSeqOpenWave,       DM:OPENWAVE,       ({ Marcduino::processCommand(player, ":SE54"); }))
MARCDUINO_ACTION(DomeSeqMarchingAnts,   DM:MARCHINGANTS,   ({ Marcduino::processCommand(player, ":SE55"); }))
MARCDUINO_ACTION(DomeSeqFaint,          DM:FAINT,          ({ Marcduino::processCommand(player, ":SE56"); }))
MARCDUINO_ACTION(DomeSeqRythmic,        DM:RYTHMIC,        ({ Marcduino::processCommand(player, ":SE57"); }))
MARCDUINO_ACTION(DomeSeqHarlemShake,    DM:HARLEMSHAKE,    ({ Marcduino::processCommand(player, "$815"); }))
MARCDUINO_ACTION(DomeSeqGirlOnFire,     DM:GIRLONFIRE,     ({ Marcduino::processCommand(player, "$821"); }))
MARCDUINO_ACTION(DomeSeqYoda,           DM:YODA,           ({ Marcduino::processCommand(player, "$720"); }))

// Fork-specific sequences not in Jessica's original alias list
MARCDUINO_ACTION(DomeSeqTopPanels,      DM:TOPPANELS,      ({ Marcduino::processCommand(player, ":SE12"); }))
MARCDUINO_ACTION(DomeSeqWiggle,         DM:WIGGLE,         ({ Marcduino::processCommand(player, ":SE16"); }))
MARCDUINO_ACTION(DomeSeqByeBye,         DM:BYEBYE,         ({ Marcduino::processCommand(player, ":SE58"); }))
