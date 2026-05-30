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
// D_ prefix = slot index (0..12); distinct from PANEL_P* address-bit bitmasks.
// See ADR 0009 for the rename rationale (names that cannot lie).
// =============================================================================
#define D_P1    0   /* ring panel P1   — PANEL_P1  :OP01 */
#define D_P2    1   /* ring panel P2   — PANEL_P2  :OP02 */
#define D_P3    2   /* ring panel P3   — PANEL_P3  :OP03 */
#define D_P4    3   /* ring panel P4   — PANEL_P4  :OP04 */
#define D_P7    4   /* ring panel P7   — PANEL_P7  :OP07 */
#define D_P11   5   /* ring panel P11  — PANEL_P11 :OP11 */
#define D_P13   6   /* ring panel P13  — PANEL_P13 :OP13 */
#define D_PP5   7   /* pie panel  PP5  — PANEL_PP5 :OPP5 (unserviced on MK4 — ADR 0008) */
#define D_PP1   8   /* pie panel  PP1  — PANEL_PP1 :OP08 / :OPP1 */
#define D_PP2   9   /* pie panel  PP2  — PANEL_PP2 :OP09 / :OPP2 */
#define D_PP4  10   /* pie panel  PP4  — PANEL_PP4 :OP10 / :OPP4 */
#define D_PP6  11   /* pie panel  PP6  — PANEL_PP6 :OP12 / :OPP6 */
#define D_PP3  12   /* pie panel  PP3  — PANEL_PP3 :OPP3 (unserviced on MK4) */

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

// Non-blocking dispatch target. Set by a DM:* Marcduino handler (which runs as a
// one-shot animation on `player`), drained once in mainLoop() via
// player.animateOnce(). Lets the toggle handler pick an open/close animation
// WITHOUT calling animateOnce() re-entrantly from inside player.animate().
extern AnimationStep dome_pendingAnim;

// Ring-only group mask — the 7 servoed ring panels (slots 0-6). Used by
// schedulePanelRelease()/cancelPanelRelease() so DM:LOW only releases ring servos.
#define RING_PANELS_MASK (PANEL_P1 | PANEL_P2 | PANEL_P3 | PANEL_P4 | PANEL_P7 | PANEL_P11 | PANEL_P13)

// =============================================================================
// Core helpers
// =============================================================================

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

// Request a timed dome rotation from the body controller.
// speed_pct: -100 (full reverse) .. +100 (full forward)
// duration_ms: body auto-stops the motor after this many milliseconds.
// A caller should keep the sequence running (e.g. DO_WAIT_MILLIS(duration_ms))
// before domeEndSequence() so dome=seqoff does not arrive mid-rotation.
static void domeRotate(int speed_pct, uint32_t duration_ms)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "dome=rot,%d,%lu", speed_pct, duration_ms);
    sendBodyCommand(buf);
}

// Non-blocking staggered wave: issues moveToPulse for an ordered index list,
// each panel offset by stepMs (reproduces the old serialized wait=true timing).
// Returns the wave envelope in ms so the caller's DO_WAIT can cover it.
static uint32_t domeStaggerMove(const uint8_t* order, uint8_t count,
                                uint16_t pos, uint32_t moveMs, uint32_t stepMs)
{
    for (uint8_t k = 0; k < count; k++)
        servoDispatch.moveToPulse(order[k], (uint32_t)k * stepMs, moveMs, pos);
    return (count > 0) ? ((uint32_t)(count - 1) * stepMs + moveMs) : 0;
}

// Easing for the Bloom sequence is now applied non-blockingly (set method + fire
// moves in a DO_ONCE, reset method in a later step) — see domePieSetEasing /
// domePieMoveAll defined after the pie index arrays below.

// =============================================================================
// File-scope panel index arrays — used by domeRandomPanels() and sequence bodies.
// Ring panels: all 7 servoed ring positions on standard MK4.
// Pie panels: all 6 pie slots (PP3/PP5 are no-ops on MK4, live when wired).
// All panels: all 13 slots in identity order.
// =============================================================================
static const uint8_t ringPanels[] = { D_P1, D_P2, D_P3, D_P4, D_P7, D_P11, D_P13 };
static const uint8_t piePanels[]  = { D_PP1, D_PP2, D_PP3, D_PP4, D_PP5, D_PP6 };
static const uint8_t allPanels[]  = { D_P1, D_P2, D_P3, D_P4, D_P7, D_P11, D_P13,
                                      D_PP1, D_PP2, D_PP3, D_PP4, D_PP5, D_PP6 };

// Non-blocking pie easing helpers (used by Bloom): set the easing method / fire
// moves in a DO_ONCE; the caller's DO_WAIT covers the move, a later step resets.
static void domePieSetEasing(float (*method)(float))
{
    for (uint8_t i = 0; i < 6; i++)
        servoDispatch.setServoEasingMethod(piePanels[i], method);
}
static void domePieMoveAll(uint16_t pos, uint32_t moveMs)
{
    for (uint8_t i = 0; i < 6; i++)
        servoDispatch.moveToPulse(piePanels[i], moveMs, pos);
}

// =============================================================================
// Random panel selection (Fisher-Yates shuffle; indices = servoDispatch indices)
// =============================================================================
static uint8_t domeRandomPanels(uint8_t numRing, uint8_t numPie, uint8_t* out)
{
    uint8_t ring[7], pie[6];
    memcpy(ring, ringPanels, sizeof(ring));
    memcpy(pie,  piePanels,  sizeof(pie));

    for (uint8_t i = 6; i > 0; i--) { uint8_t j = random(i + 1); uint8_t t = ring[i]; ring[i] = ring[j]; ring[j] = t; }
    for (uint8_t i = 5; i > 0; i--) { uint8_t j = random(i + 1); uint8_t t = pie[i];  pie[i]  = pie[j];  pie[j]  = t; }

    if (numRing > 7) numRing = 7;
    if (numPie  > 6) numPie  = 6;

    uint8_t total = 0;
    for (uint8_t i = 0; i < numRing; i++) out[total++] = ring[i];
    for (uint8_t i = 0; i < numPie;  i++) out[total++] = pie[i];
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
// Pie release mask + wave envelopes + reverse-order pie list (for close waves).
#define DOME_PIE_RELEASE_MASK  (PANEL_PP1 | PANEL_PP2 | PANEL_PP3 | PANEL_PP4 | PANEL_PP5 | PANEL_PP6)
#define DOME_PIE_OPEN_WAVE_MS  (6 * DOME_MOVE_FASTSPEED)  /* 6 pies @100 = 600 */
#define DOME_PIE_CLOSE_WAVE_MS (6 * DOME_MOVE_SPEED)      /* 6 pies @150 = 900 */
static const uint8_t pieRevPanels[6] = { D_PP6, D_PP5, D_PP4, D_PP3, D_PP2, D_PP1 };

// Pies open: wave open PP1→PP6, then 2x (close PP6→PP1, reopen PP1→PP6), all @FASTSPEED.
ANIMATION(domePiesOpen)
{
    DO_START()
    DO_ONCE({ cancelPanelRelease(DOME_PIE_RELEASE_MASK); domeBeginSequence(12); dome_PiesOpen = true; })
    DO_WAIT_MILLIS(100)
    DO_ONCE({ if (preferences.getBool("dm_happy_sound", true)) domeSendToBody("HAPPY"); })
    // iteration 1
    DO_ONCE({ domeStaggerMove(piePanels,    6, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, DOME_MOVE_FASTSPEED); })
    DO_WAIT_MILLIS(DOME_PIE_OPEN_WAVE_MS)
    DO_ONCE({ domeStaggerMove(pieRevPanels, 6, DOME_PANEL_CLOSE,    DOME_MOVE_FASTSPEED, DOME_MOVE_FASTSPEED); })
    DO_WAIT_MILLIS(DOME_PIE_OPEN_WAVE_MS)
    DO_ONCE({ domeStaggerMove(piePanels,    6, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, DOME_MOVE_FASTSPEED); })
    DO_WAIT_MILLIS(DOME_PIE_OPEN_WAVE_MS)
    // iteration 2
    DO_ONCE({ domeStaggerMove(piePanels,    6, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, DOME_MOVE_FASTSPEED); })
    DO_WAIT_MILLIS(DOME_PIE_OPEN_WAVE_MS)
    DO_ONCE({ domeStaggerMove(pieRevPanels, 6, DOME_PANEL_CLOSE,    DOME_MOVE_FASTSPEED, DOME_MOVE_FASTSPEED); })
    DO_WAIT_MILLIS(DOME_PIE_OPEN_WAVE_MS)
    DO_ONCE({ domeStaggerMove(piePanels,    6, DOME_PIE_PANEL_OPEN, DOME_MOVE_FASTSPEED, DOME_MOVE_FASTSPEED); })
    DO_WAIT_MILLIS(DOME_PIE_OPEN_WAVE_MS + 1000)
    DO_ONCE({ schedulePanelRelease(DOME_PIE_RELEASE_MASK, 1); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// Pies close: single close wave PP1→PP6 @SPEED, settle, release.
ANIMATION(domePiesClose)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(DOME_PIE_RELEASE_MASK);
        domeBeginSequence(12);
        dome_PiesOpen = false;
        domeResetHolos();
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");
        domeStaggerMove(piePanels, 6, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, DOME_MOVE_SPEED);
    })
    DO_WAIT_MILLIS(DOME_PIE_CLOSE_WAVE_MS + 800)
    DO_ONCE({ schedulePanelRelease(DOME_PIE_RELEASE_MASK, 1); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Open / Close Low Panels  (non-blocking — runs on `player` as DO_* animations)
//
// Faithful port of the original serialized waves. Each panel previously moved
// with wait=true (it fully completed before the next started), so panel k began
// at k*moveMs. We reproduce that exact timing non-blockingly with moveToPulse
// start-delays (servoDispatch animates them from mainLoop), then a single
// DO_WAIT_MILLIS covers the wave envelope. No busy-loop, no internal
// AnimatedEvent::process() pumping — mainLoop stays live for the whole sequence.
// =============================================================================

// Wave envelope helpers (durations chosen to match the original serialized timing).
#define DOME_LOW_WAVE_MS   (6 * DOME_MOVE_SPEED + DOME_MOVE_SPEED)   /* 7 panels @150 = 1050 */
#define DOME_LOW_CLOSE_MS  (1000 + DOME_MOVE_SPEED)                  /* last start 1000 + 150 = 1150 */
#define DOME_LOW_FINAL_MS  (400 + DOME_MOVE_FASTSPEED)              /* last start 400 + 100 = 500  */

// open wave order: P1, P13, P11, P2, P3, P4, P7 (each 150ms, staggered 150ms)
static void domeLowWaveOpen()
{
    static const uint8_t order[7] = { D_P1, D_P13, D_P11, D_P2, D_P3, D_P4, D_P7 };
    for (uint8_t k = 0; k < 7; k++)
        servoDispatch.moveToPulse(order[k], k * DOME_MOVE_SPEED, DOME_MOVE_SPEED, DOME_PANEL_OPEN);
}

// close wave: P7,P4,P3,P2,P1 at 150ms steps, then +50ms gaps before P13 and P11
// (original domeWaitTime(50) between them).
static void domeLowWaveClose()
{
    servoDispatch.moveToPulse(D_P7,  0,    DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P4,  150,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P3,  300,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P2,  450,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P1,  600,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P13, 800,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P11, 1000, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
}

// final open: P11/P13/P1 together, then P2..P7 stepped, all at FASTSPEED.
static void domeLowFinalOpen()
{
    servoDispatch.moveToPulse(D_P11, 0,   DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P13, 0,   DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P1,  0,   DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P2,  100, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P3,  200, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P4,  300, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P7,  400, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
}

// close branch order (reverse-of-final-open arc): P4,P2,P1,P3,P13,P7,P11 @150ms steps.
static void domeLowCloseAll()
{
    static const uint8_t order[7] = { D_P4, D_P2, D_P1, D_P3, D_P13, D_P7, D_P11 };
    for (uint8_t k = 0; k < 7; k++)
        servoDispatch.moveToPulse(order[k], k * DOME_MOVE_SPEED, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
}

// Open animation: two open/close waves, then a final open; panels left open,
// PWM released after a 1s settle. domeEndSequence() runs from DO_RESET on
// completion (or if interrupted by a new animation).
ANIMATION(domeLowOpen)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(RING_PANELS_MASK);
        domeBeginSequence(15);
        dome_LowOpen = true;
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");
    })
    // iteration 1
    DO_ONCE({ domeLowWaveOpen(); })
    DO_WAIT_MILLIS(DOME_LOW_WAVE_MS)
    DO_ONCE({ domeLowWaveClose(); })
    DO_WAIT_MILLIS(DOME_LOW_CLOSE_MS)
    // iteration 2
    DO_ONCE({ domeLowWaveOpen(); })
    DO_WAIT_MILLIS(DOME_LOW_WAVE_MS)
    DO_ONCE({ domeLowWaveClose(); })
    DO_WAIT_MILLIS(DOME_LOW_CLOSE_MS)
    // final open + settle, then cut PWM (panels hold open by friction)
    DO_ONCE({ domeLowFinalOpen(); })
    DO_WAIT_MILLIS(DOME_LOW_FINAL_MS + 1000)
    DO_ONCE({ schedulePanelRelease(RING_PANELS_MASK, 1); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// Close animation: single close wave, settle, release.
ANIMATION(domeLowClose)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(RING_PANELS_MASK);
        domeBeginSequence(15);
        dome_LowOpen = false;
        domeResetHolos();
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");
    })
    DO_ONCE({ domeLowCloseAll(); })
    DO_WAIT_MILLIS(DOME_LOW_WAVE_MS + 1000)
    DO_ONCE({ schedulePanelRelease(RING_PANELS_MASK, 1); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Open / Close All Panels
// =============================================================================
// Close all: single staggered close wave over all 13 slots @SPEED, settle, release.
ANIMATION(domeAllClose)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(ALL_DOME_PANELS_MASK);
        domeBeginSequence(10);
        dome_AllOpen = false;
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");
        domeStaggerMove(allPanels, 13, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, DOME_MOVE_SPEED);
    })
    DO_WAIT_MILLIS(13 * DOME_MOVE_SPEED + 500)
    DO_ONCE({ schedulePanelRelease(ALL_DOME_PANELS_MASK, 1); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// Open all: pies wave open, ring snap open together, then a 2x P1/P2/PP2/PP4
// twinkle. The twinkle is sequential same-servo moves, so it loops via a
// backward DO_WHILE on sDomeAllTwinkle (label declared before the jump).
// Ring snaps open together (kept in a helper so the array literal's commas
// are not parsed as DO_ONCE macro-argument separators).
static void domeAllRingOpen()
{
    static const uint8_t ringOrder[7] = { D_P11, D_P13, D_P1, D_P2, D_P3, D_P4, D_P7 };
    for (uint8_t k = 0; k < 7; k++)
        servoDispatch.moveToPulse(ringOrder[k], 0, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN);
}

static uint8_t sDomeAllTwinkle = 0;
ANIMATION(domeAllOpen)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(ALL_DOME_PANELS_MASK);
        domeBeginSequence(10);
        dome_AllOpen = true;
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");
        domeStaggerMove(piePanels, 6, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED, DOME_MOVE_SPEED);
    })
    DO_WAIT_MILLIS(6 * DOME_MOVE_SPEED)
    DO_ONCE({ domeAllRingOpen(); sDomeAllTwinkle = 0; })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    // ---- twinkle pass (repeats 2x via DO_WHILE below) ----
    DO_ONCE_LABEL(kAllTwinkle, { servoDispatch.moveToPulse(D_P1, DOME_MOVE_FASTSPEED, DOME_PANEL_75_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    DO_ONCE({ servoDispatch.moveToPulse(D_P1, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN); })
    DO_WAIT_MILLIS(80)
    DO_ONCE({ servoDispatch.moveToPulse(D_P2, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    DO_ONCE({ servoDispatch.moveToPulse(D_P2, DOME_MOVE_FASTSPEED, DOME_PANEL_75_OPEN); })
    DO_WAIT_MILLIS(80)
    DO_ONCE({ servoDispatch.moveToPulse(D_P2, DOME_MOVE_FASTSPEED, DOME_PANEL_OPEN); })
    DO_WAIT_MILLIS(100)
    DO_ONCE({ servoDispatch.moveToPulse(D_PP2, DOME_MOVE_FASTSPEED, DOME_PANEL_75_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    DO_ONCE({ servoDispatch.moveToPulse(D_PP2, DOME_MOVE_FASTSPEED, DOME_PIE_PANEL_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED + 80)
    DO_ONCE({ servoDispatch.moveToPulse(D_PP4, DOME_MOVE_FASTSPEED, DOME_PANEL_75_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    DO_ONCE({ servoDispatch.moveToPulse(D_PP4, DOME_MOVE_FASTSPEED, DOME_PIE_PANEL_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    DO_WHILE(++sDomeAllTwinkle < 2, kAllTwinkle)
    // ---- end twinkle ----
    DO_WAIT_MILLIS(800)
    DO_ONCE({ schedulePanelRelease(ALL_DOME_PANELS_MASK, 1); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Flutter — quick open-close flap, all panels end closed
// =============================================================================
ANIMATION(domeFlutter)
{
    DO_START()
    // phase 1 — ring sweep P1→P2→P3→P4→P7→P11→P13 (75% open)
    DO_ONCE({ domeBeginSequence(10);
              domeStaggerMove(ringPanels, 7, DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, DOME_MOVE_SPEED); })
    DO_WAIT_MILLIS(7 * DOME_MOVE_SPEED)
    // phase 2 — pie sweep PP1→PP6 (75% open)
    DO_ONCE({ domeStaggerMove(piePanels, 6, DOME_PANEL_75_OPEN, DOME_MOVE_SPEED, DOME_MOVE_SPEED); })
    DO_WAIT_MILLIS(6 * DOME_MOVE_SPEED)
    // close ring
    DO_ONCE({ domeStaggerMove(ringPanels, 7, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, DOME_MOVE_SPEED); })
    DO_WAIT_MILLIS(7 * DOME_MOVE_SPEED)
    // close pies
    DO_ONCE({ domeStaggerMove(piePanels, 6, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, DOME_MOVE_SPEED); })
    DO_WAIT_MILLIS(6 * DOME_MOVE_SPEED + 500)
    DO_ONCE({
        schedulePanelRelease(ALL_DOME_PANELS_MASK, 1);
        dome_PiesOpen = false;
        dome_AllOpen  = false;
        dome_LowOpen  = false;
    })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Bloom — pie panels ease open, wiggle, close
// =============================================================================
static uint8_t sDomeBloomWiggle = 0;
ANIMATION(domeBloom)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(DOME_PIE_RELEASE_MASK);
        domeBeginSequence(8);
        // all 6 pies bloom open together with a sine ease-out
        domePieSetEasing(Easing::SineEaseOut);
        domePieMoveAll(DOME_PANEL_OPEN, 1200);
    })
    DO_WAIT_MILLIS(1250)
    DO_ONCE({ domePieSetEasing(Easing::LinearInterpolation); sDomeBloomWiggle = 0; })
    DO_WAIT_MILLIS(2000)
    // wiggle 3x between 1900 and full open, sine ease-in-out, 130ms each leg
    DO_ONCE_LABEL(kBloomWiggle, { domePieSetEasing(Easing::SineEaseInOut); domePieMoveAll(1900, 130); })
    DO_WAIT_MILLIS(180)
    DO_ONCE({ domePieMoveAll(DOME_PIE_PANEL_OPEN, 130); })
    DO_WAIT_MILLIS(180)
    DO_WHILE(++sDomeBloomWiggle < 3, kBloomWiggle)
    DO_ONCE({ domePieSetEasing(Easing::LinearInterpolation); })
    DO_WAIT_MILLIS(1000)
    DO_ONCE({ domePieMoveAll(DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED); })
    DO_WAIT_MILLIS(500)
    DO_ONCE({ schedulePanelRelease(DOME_PIE_RELEASE_MASK, 1); dome_PiesOpen = false; })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Scream — all panels burst open with random fluttering, then close
// =============================================================================
// Scream: burst all panels open, then 10 random single-panel flutters, then
// close all. The per-flutter panel is random, so it is captured in a static and
// the 10-iteration loop runs via a backward DO_WHILE.
static uint8_t sScreamIter  = 0;
static uint8_t sScreamPanel = 0;
ANIMATION(domeScream)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(ALL_DOME_PANELS_MASK);
        domeBeginSequence(15);
        CommandEvent::process(F("HPA0070")); // all holos short circuit random color
        CommandEvent::process(F("HPA105|5")); // all holos wag 5 times
        FLD.selectSequence(LogicEngineRenderer::REDALERT, FLD.kDefault, 0, 15);
        RLD.selectSequence(LogicEngineRenderer::REDALERT, RLD.kDefault, 0, 15);
        frontPSI.selectSequence(LogicEngineRenderer::REDALERT, frontPSI.kDefault, 0, 15);
        rearPSI.selectSequence(LogicEngineRenderer::REDALERT, rearPSI.kDefault, 0, 15);
        domeSendToBody("SCREAM");
        dome_AllOpen = true;
        // burst open — pies together @SPEED, ring together @FASTSPEED
        domeStaggerMove(piePanels, 6, DOME_PIE_PANEL_OPEN, DOME_MOVE_SPEED, 0);
        domeStaggerMove(ringPanels, 7, DOME_PANEL_OPEN, DOME_MOVE_FASTSPEED, 0);
        randomSeed(analogRead(0));
        sScreamIter = 0;
    })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED + 100)
    // ---- random flutter pass (repeats 10x) ----
    DO_ONCE_LABEL(kScream, {
        sScreamPanel = allPanels[random(13)];
        servoDispatch.moveToPulse(sScreamPanel, DOME_MOVE_FASTSPEED, DOME_PANEL_50_OPEN);
    })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    DO_ONCE({ servoDispatch.moveToPulse(sScreamPanel, DOME_MOVE_FASTSPEED, DOME_PIE_PANEL_OPEN); })
    DO_WAIT_MILLIS(80)
    DO_ONCE({ servoDispatch.moveToPulse(sScreamPanel, DOME_MOVE_FASTSPEED, DOME_PANEL_50_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_FASTSPEED)
    DO_ONCE({ servoDispatch.moveToPulse(sScreamPanel, DOME_MOVE_FASTSPEED, DOME_PIE_PANEL_OPEN); })
    DO_WAIT_MILLIS(100)
    DO_WHILE(++sScreamIter < 10, kScream)
    // ---- end flutter ----
    DO_WAIT_MILLIS(2800)
    DO_ONCE({
        dome_AllOpen = false;
        if (preferences.getBool("dm_happy_sound", true))
            domeSendToBody("HAPPY");
        domeStaggerMove(allPanels, 13, DOME_PANEL_CLOSE, DOME_MOVE_SPEED, 0);
    })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED + 500)
    DO_ONCE({ schedulePanelRelease(ALL_DOME_PANELS_MASK, 1); domeResetHolos(); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Overload — random panels sluggishly drift open, then snap closed
// =============================================================================
// Overload: random panels drift open one at a time with random gaps, hold, snap
// closed. Random selection/gap persist in statics across the DO_WHILE loop.
static uint8_t  sOverloadPanels[6];
static uint8_t  sOverloadCount = 0;
static uint8_t  sOverloadIdx   = 0;
static uint32_t sOverloadGapMs = 0;
ANIMATION(domeOverload)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(ALL_DOME_PANELS_MASK);
        domeBeginSequence(12);
        FLD.selectSequence(LogicEngineRenderer::FAILURE);
        RLD.selectSequence(LogicEngineRenderer::FAILURE);
        CommandEvent::process(F("HPA0070")); // all holos short circuit random color
        frontPSI.selectSequence(LogicEngineRenderer::FAILURE, frontPSI.kDefault, 0, 12);
        rearPSI.selectSequence(LogicEngineRenderer::FAILURE, rearPSI.kDefault, 0, 12);
        domeSendToBody("OVERLOAD");
        sOverloadCount = domeRandomPanels(4, 2, sOverloadPanels);
        sOverloadIdx = 0;
    })
    // ---- drift one panel open with a random gap, repeat for each chosen panel ----
    DO_ONCE_LABEL(kOverload, {
        int pos = random(DOME_PANEL_25_OPEN, DOME_PANEL_50_OPEN + 1);
        servoDispatch.moveToPulse(sOverloadPanels[sOverloadIdx], DOME_MOVE_OVERLOAD, (uint16_t)pos);
        sOverloadGapMs = random(400, 900);
    })
    DO_WAIT_MILLIS(sOverloadGapMs)
    DO_ONCE({ sOverloadIdx++; })
    DO_WHILE(sOverloadIdx < sOverloadCount, kOverload)
    // ---- hold, then snap all chosen panels closed ----
    DO_WAIT_MILLIS(2500)
    DO_ONCE({
        for (uint8_t i = 0; i < sOverloadCount; i++)
            servoDispatch.moveToPulse(sOverloadPanels[i], DOME_MOVE_FASTSPEED, DOME_PANEL_CLOSE);
    })
    DO_WAIT_MILLIS(800)
    DO_ONCE({
        schedulePanelRelease(ALL_DOME_PANELS_MASK, 1);
        domeResetHolos();
        domeResetLogics();
        domeResetPSIs();
    })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Heart — rainbow holos, sweet message on logics
// =============================================================================
ANIMATION(domeHeart)
{
    DO_START()
    DO_ONCE({
        domeBeginSequence(10);
        CommandEvent::process(F("HPF006|10"));
        CommandEvent::process(F("HPR006|10"));
        CommandEvent::process(F("HPT006|10"));
        FLD.selectScrollTextLeft("You're\nWonderful", FLD.kDefault, 0, 10);
        frontPSI.selectSequence(LogicEngineRenderer::FLASHCOLOR, frontPSI.kDefault, 0, 10);
        domeSendToBody("HEART");
    })
    DO_WAIT_SEC(10)
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Alarm — pulsing red holos and logics
// =============================================================================
ANIMATION(domeAlarm)
{
    DO_START()
    DO_ONCE({
        domeBeginSequence(10);
        CommandEvent::process(F("HPA0021|10")); // all holos red flashes
        FLD.selectSequence(LogicEngineRenderer::ALARM, FLD.kDefault, 0, 10);
        RLD.selectSequence(LogicEngineRenderer::ALARM, RLD.kDefault, 0, 10);
        frontPSI.selectSequence(LogicEngineRenderer::ALARM, frontPSI.kDefault, 0, 10);
        rearPSI.selectSequence(LogicEngineRenderer::ALARM, rearPSI.kDefault, 0, 10);
        domeSendToBody("ALARM");
    })
    DO_WAIT_SEC(10)
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Disco — delegates to :SE09 (DiscoSequence), which owns the body sound cue
// =============================================================================
// Disco: hand :SE09 to the command queue (drained on the main loop, not
// re-entrantly here), bracketed by body coordination for the run.
ANIMATION(domeDisco)
{
    DO_START()
    DO_ONCE({ domeBeginSequence(46); enqueueMarcduinoCommand("dome-disco", ":SE09"); })
    DO_WAIT_SEC(46)
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Vader — imperial march
// =============================================================================
ANIMATION(domeVader)
{
    DO_START()
    DO_ONCE({
        domeBeginSequence(47);
        CommandEvent::process(F("HPA0021|47")); // all holos red flashes
        FLD.selectSequence(LogicEngineRenderer::MARCH, FLD.kRed, 0, 47);
        RLD.selectSequence(LogicEngineRenderer::MARCH, RLD.kRed, 0, 47);
        frontPSI.selectSequence(LogicEngineRenderer::MARCH, frontPSI.kDefault, 0, 47);
        rearPSI.selectSequence(LogicEngineRenderer::MARCH, rearPSI.kDefault, 0, 47);
        domeSendToBody("VADER");
    })
    DO_WAIT_SEC(47)
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Rock March — alt imperial march
// =============================================================================
// Rock March: step ring panels open/closed on the beat for 45s (130 BPM).
// Ring-only, so hardware-testable. Loops via DO_WHILE on a millis deadline.
static uint8_t  sRockIdx   = 0;
static uint32_t sRockStart = 0;
ANIMATION(domeRockMarch)
{
    DO_START()
    DO_ONCE({
        domeBeginSequence(47);
        CommandEvent::process(F("HPA0021|47")); // all holos red flashes
        FLD.selectSequence(LogicEngineRenderer::MARCH, FLD.kRed, 0, 47);
        RLD.selectSequence(LogicEngineRenderer::MARCH, RLD.kRed, 0, 47);
        frontPSI.selectSequence(LogicEngineRenderer::MARCH, frontPSI.kDefault, 0, 47);
        rearPSI.selectSequence(LogicEngineRenderer::MARCH, rearPSI.kDefault, 0, 47);
        domeSendToBody("ROCKMARCH");
        sRockIdx = 0;
        sRockStart = millis();
    })
    // ---- one beat: open ring[idx], hold to the beat, close ----
    DO_ONCE_LABEL(kRock, { servoDispatch.moveToPulse(ringPanels[sRockIdx % 7], DOME_MOVE_SPEED, DOME_PANEL_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED)
    DO_WAIT_MILLIS(923 - DOME_MOVE_SPEED * 2)
    DO_ONCE({ servoDispatch.moveToPulse(ringPanels[sRockIdx % 7], DOME_MOVE_SPEED, DOME_PANEL_CLOSE); sRockIdx++; })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED)
    DO_WHILE(millis() - sRockStart < 45000UL, kRock)
    // ---- end ----
    DO_ONCE({ schedulePanelRelease(RING_PANELS_MASK, 1); })
    DO_WAIT_MILLIS(2000)
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Hello There — P1 waves a greeting
// =============================================================================
// Wave P1 only — sequential moves on ONE servo, so each is its own step (a
// staggered batch can't queue multiple future moves for the same servo).
ANIMATION(domeHelloThere)
{
    DO_START()
    DO_ONCE({
        domeBeginSequence(4);
        FLD.selectScrollTextLeft("Hello\nThere", FLD.kDefault, 0, 10);
        RLD.selectScrollTextLeft("General Kenobi", RLD.randomColor());
        domeSendToBody("HELLO");
    })
    DO_ONCE({ servoDispatch.moveToPulse(D_P1, DOME_MOVE_SPEED, DOME_PANEL_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED + 10)
    DO_ONCE({ servoDispatch.moveToPulse(D_P1, DOME_MOVE_SPEED, DOME_PANEL_50_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED + 10)
    DO_ONCE({ servoDispatch.moveToPulse(D_P1, DOME_MOVE_SPEED, DOME_PANEL_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED + 10)
    DO_ONCE({ servoDispatch.moveToPulse(D_P1, DOME_MOVE_SPEED, DOME_PANEL_50_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED + 10)
    DO_ONCE({ servoDispatch.moveToPulse(D_P1, DOME_MOVE_SPEED, DOME_PANEL_OPEN); })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED + 10)
    DO_ONCE({ servoDispatch.moveToPulse(D_P1, DOME_MOVE_SPEED, DOME_PANEL_CLOSE); })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED)
    DO_ONCE({ schedulePanelRelease(PANEL_P1, 1); })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Leia — front HP runs Leia LED sequence, all other HPs off, logics Leia mode
// =============================================================================
ANIMATION(domeLeiaMode)
{
    DO_START()
    DO_ONCE({
        domeBeginSequence(36);
        CommandEvent::process(F("HPS101|36")); // front holo leia sequence
        CommandEvent::process(F("HPR02|36"));  // rear holo off
        CommandEvent::process(F("HPT02|36"));  // top holo off
        FLD.selectSequence(LogicEngineRenderer::LEIA, FLD.kDefault, 0, 36);
        RLD.selectSequence(LogicEngineRenderer::LEIA, RLD.kDefault, 0, 36);
        frontPSI.selectSequence(LogicEngineRenderer::LEIA, frontPSI.kDefault, 0, 36);
        rearPSI.selectSequence(LogicEngineRenderer::LEIA, rearPSI.kDefault, 0, 36);
        domeSendToBody("LEIA");
    })
    DO_WAIT_SEC(36)
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Reset All — close every panel, reset holos / logics / PSIs / body
// =============================================================================
ANIMATION(domeResetAll)
{
    DO_START()
    DO_ONCE({
        domeBeginSequence(4);
        // close all 13 panels — PP3 included so :OPP3 doesn't leave it stranded.
        // Original fired them simultaneously (no wait), so no stagger here.
        for (uint8_t i = 0; i < 13; i++)
            servoDispatch.moveToPulse(allPanels[i], DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    })
    DO_WAIT_MILLIS(DOME_MOVE_SPEED + 1000)
    DO_ONCE({
        schedulePanelRelease(ALL_DOME_PANELS_MASK, 1);
        dome_PiesOpen = false;
        dome_AllOpen  = false;
        dome_LowOpen  = false;
        domeResetHolos();
        domeResetLogics();
        domeResetPSIs();
        domeResetBody();
    })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Cantina — 15-second alternating panel dance at 130 BPM
// =============================================================================
// Cantina beat halves (all moves fire together, no per-move wait).
static void domeCantinaBeatA()
{
    // pie group A opens (PP1, PP3, PP4); group B closes (PP2, PP5, PP6)
    servoDispatch.moveToPulse(D_PP1, DOME_MOVE_SPEED, DOME_PIE_PANEL_OPEN);
    servoDispatch.moveToPulse(D_PP4, DOME_MOVE_SPEED, DOME_PIE_PANEL_OPEN);
    servoDispatch.moveToPulse(D_PP3, DOME_MOVE_SPEED, DOME_PIE_PANEL_OPEN); // top wedge pairs with PP4
    servoDispatch.moveToPulse(D_PP2, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_PP5, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_PP6, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P1,  DOME_MOVE_SPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P3,  DOME_MOVE_SPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P7,  DOME_MOVE_SPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P13, DOME_MOVE_SPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P2,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P4,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P11, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
}
static void domeCantinaBeatB()
{
    // pie group A closes (PP1, PP3, PP4); group B opens (PP2, PP5, PP6)
    servoDispatch.moveToPulse(D_PP1, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_PP4, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_PP3, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);    // top wedge pairs with PP4
    servoDispatch.moveToPulse(D_PP2, DOME_MOVE_SPEED, DOME_PIE_PANEL_OPEN);
    servoDispatch.moveToPulse(D_PP5, DOME_MOVE_SPEED, DOME_PIE_PANEL_OPEN);
    servoDispatch.moveToPulse(D_PP6, DOME_MOVE_SPEED, DOME_PIE_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P1,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P3,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P7,  DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P13, DOME_MOVE_SPEED, DOME_PANEL_CLOSE);
    servoDispatch.moveToPulse(D_P2,  DOME_MOVE_SPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P4,  DOME_MOVE_SPEED, DOME_PANEL_OPEN);
    servoDispatch.moveToPulse(D_P11, DOME_MOVE_SPEED, DOME_PANEL_OPEN);
}

static bool     sCantinaEven  = true;
static uint32_t sCantinaStart = 0;
ANIMATION(domeCantina)
{
    DO_START()
    DO_ONCE({
        cancelPanelRelease(ALL_DOME_PANELS_MASK);
        domeBeginSequence(17);
        domeSendToBody("CANTINA");
        CommandEvent::process(F("HPA0029|15")); // all holos white flashes
        FLD.selectSequence(LogicEngineRenderer::FLASHCOLOR, FLD.kBlue, 0, 15);
        RLD.selectSequence(LogicEngineRenderer::FLASHCOLOR, RLD.kBlue, 0, 15);
        frontPSI.selectSequence(LogicEngineRenderer::FLASHCOLOR, frontPSI.kDefault, 0, 15);
        rearPSI.selectSequence(LogicEngineRenderer::FLASHCOLOR, rearPSI.kDefault, 0, 15);
        sCantinaEven = true;
    })
    DO_WAIT_MILLIS(100)
    DO_ONCE({ sCantinaStart = millis(); })
    // ---- one beat (130 BPM ≈ 923ms), alternating halves, for 15s ----
    DO_ONCE_LABEL(kCantina, {
        if (sCantinaEven) domeCantinaBeatA(); else domeCantinaBeatB();
        sCantinaEven = !sCantinaEven;
    })
    DO_WAIT_MILLIS(923)
    DO_WHILE(millis() - sCantinaStart < 15000UL, kCantina)
    // ---- close everything, reset ----
    DO_ONCE({ domeStaggerMove(allPanels, 13, DOME_PANEL_CLOSE, DOME_MOVE_FASTSPEED, 0); })
    DO_WAIT_MILLIS(500)
    DO_ONCE({
        schedulePanelRelease(ALL_DOME_PANELS_MASK, 1);
        dome_PiesOpen = false;
        dome_AllOpen  = false;
        dome_LowOpen  = false;
        domeResetPSIs();
        domeResetLogics();
        domeResetHolos();
    })
    DO_RESET({ domeEndSequence(); })
    DO_END()
}

// =============================================================================
// Random — picks one sequence from the standard pool.
// Each underlying :SE sequence routes its own body sound via sendBodyCommand().
// Do NOT wrap this in domeBeginSequence() — that would double-coordinate the body.
// =============================================================================
// Random — picks one sequence from the standard pool and QUEUES it (drained on
// the main loop) rather than calling processCommand() re-entrantly. Each
// underlying :SE routes its own body sound, so this is NOT wrapped in
// domeBeginSequence(). Called directly from the DM:RANDOM handler.
static void domeRandomDispatch()
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
    enqueueMarcduinoCommand("dome-random", sequences[random(count)]);
}

// =============================================================================
// Marcduino serial command handlers — prefix "DM:" on COMMAND_SERIAL
//
// Send from any Marcduino-compatible device as:   DM:PIES\r
// =============================================================================

MARCDUINO_ACTION(DomeReset,     DM:RESET,       ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeResetAll; }))
MARCDUINO_ACTION(DomePies,      DM:PIES,        ({ if (!dome_seqRunning) dome_pendingAnim = dome_PiesOpen ? Animation_domePiesClose : Animation_domePiesOpen; }))
// DM:LOW stays MARCDUINO_ANIMATION (not MARCDUINO_ACTION) so the bare token LOW is
// not macro-expanded to 0x0 in the command string. The one-shot body picks the
// open or close animation by toggle and hands it to mainLoop via dome_pendingAnim
// (dispatched outside player.animate() to avoid re-entrant animateOnce()).
MARCDUINO_ANIMATION(DomeLow, DM:LOW)
{
    DO_START()
    DO_ONCE({
        if (!dome_seqRunning)
            dome_pendingAnim = dome_LowOpen ? Animation_domeLowClose : Animation_domeLowOpen;
    })
    DO_END()
}
MARCDUINO_ACTION(DomeOpenAll,   DM:OPENALL,     ({ if (!dome_seqRunning) dome_pendingAnim = dome_AllOpen ? Animation_domeAllClose : Animation_domeAllOpen; }))
MARCDUINO_ACTION(DomeLeia,      DM:LEIA,        ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeLeiaMode;   }))
MARCDUINO_ACTION(DomeHeart,     DM:HEART,       ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeHeart;      }))
MARCDUINO_ACTION(DomeHello,     DM:HELLO,       ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeHelloThere; }))
MARCDUINO_ACTION(DomeScream,    DM:SCREAM,      ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeScream;     }))
MARCDUINO_ACTION(DomeFlutter,   DM:FLUTTER,     ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeFlutter;    }))
MARCDUINO_ACTION(DomeOverload,  DM:OVERLOAD,    ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeOverload;   }))
MARCDUINO_ACTION(DomeBloom,     DM:BLOOM,       ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeBloom;      }))
MARCDUINO_ACTION(DomeCantina,   DM:CANTINA,     ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeCantina;    }))
MARCDUINO_ACTION(DomeAlarm,     DM:ALARM,       ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeAlarm;      }))
MARCDUINO_ACTION(DomeSeqDisco,  DM:DISCO,       ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeDisco;      }))
MARCDUINO_ACTION(DomeRockMarch, DM:ROCKMARCH,   ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeRockMarch;  }))
MARCDUINO_ACTION(DomeRandom,    DM:RANDOM,      ({ domeRandomDispatch(); }))
MARCDUINO_ACTION(DomeVader,     DM:VADER,       ({ if (!dome_seqRunning) dome_pendingAnim = Animation_domeVader;      }))

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
