// FlthyHoloExtras.h
// Ported from thePunderWoman/AstroPixelsPlus fork by Jessica Janiuk
// https://github.com/thePunderWoman/AstroPixelsPlus
// Inspired by FlthyHPs sketch by Ryan Sondgeroth
// No functional changes from source — command prefixes confirmed conflict-free.

#pragma once

////////////////
// SHORT CIRCUIT
// Flashes the HP LEDs on and off with intervals that slow over time (20 cycles).
// Orange (color 7) matches the original FlthyHPs default short circuit color.
////////////////

MARCDUINO_ACTION(FrontHoloShortCircuit, *SC01, ({
    CommandEvent::process(F("HPF0077"));
}))

////////////////

MARCDUINO_ACTION(RearHoloShortCircuit, *SC02, ({
    CommandEvent::process(F("HPR0077"));
}))

////////////////

MARCDUINO_ACTION(TopHoloShortCircuit, *SC03, ({
    CommandEvent::process(F("HPT0077"));
}))

////////////////

MARCDUINO_ACTION(AllHoloShortCircuit, *SC00, ({
    CommandEvent::process(F("HPA0077"));
}))

////////////////
// ALL-HP CYCLE
// Spinning single-LED cycle on all three holos simultaneously.
// Individual-HP cycle commands (*ON01/02/03) already exist in MarcduinoHolo.h.
////////////////

MARCDUINO_ACTION(AllHoloCycle, *CY00, ({
    CommandEvent::process(F("HPA0040"));
}))

////////////////
// ALL-HP DIM PULSE
// Slow color breathe on all three holos simultaneously.
// Individual-HP dim pulse commands (*HPS301/302/303) already exist in MarcduinoHolo.h.
////////////////

MARCDUINO_ACTION(AllHoloDimPulse, *PL00, ({
    CommandEvent::process(F("HPA0030"));
}))

////////////////
// SOLID COLOR - BLUE (color 5)
////////////////

MARCDUINO_ACTION(FrontHoloSolidBlue, *SB01, ({
    CommandEvent::process(F("HPF0055"));
}))

////////////////

MARCDUINO_ACTION(RearHoloSolidBlue, *SB02, ({
    CommandEvent::process(F("HPR0055"));
}))

////////////////

MARCDUINO_ACTION(TopHoloSolidBlue, *SB03, ({
    CommandEvent::process(F("HPT0055"));
}))

////////////////

MARCDUINO_ACTION(AllHoloSolidBlue, *SB00, ({
    CommandEvent::process(F("HPA0055"));
}))

////////////////
// SOLID COLOR - WHITE (color 9)
////////////////

MARCDUINO_ACTION(FrontHoloSolidWhite, *SW01, ({
    CommandEvent::process(F("HPF0059"));
}))

////////////////

MARCDUINO_ACTION(RearHoloSolidWhite, *SW02, ({
    CommandEvent::process(F("HPR0059"));
}))

////////////////

MARCDUINO_ACTION(TopHoloSolidWhite, *SW03, ({
    CommandEvent::process(F("HPT0059"));
}))

////////////////

MARCDUINO_ACTION(AllHoloSolidWhite, *SW00, ({
    CommandEvent::process(F("HPA0059"));
}))

////////////////
// HOLO MODE LOOP
// Enables the autonomous random LED mode loop on all holos simultaneously.
// Uses the S9 major sequence: random HP servo twitching + random LED sequence
// cycling through ColorProjector (2), DimPulse (3), Cycle (4), SolidColor (5),
// Rainbow (6). ShortCircuit (7) is intentionally excluded from the random loop.
////////////////

MARCDUINO_ACTION(AllHoloModeLoop, *ML00, ({
    CommandEvent::process(F("HPS9"));
}))

////////////////
// RESET TO MODE LOOP
// Resets all holos and returns to the random mode loop instead of the normal
// AstroPixels off state. Use *ST00 (ResetAllHolos) to return to normal off state.
////////////////

MARCDUINO_ACTION(AllHoloResetToLoop, *RL00, ({
    CommandEvent::process(F("HPS9"));
}))

////////////////
