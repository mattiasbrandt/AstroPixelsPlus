# Critical Findings and Improvement Candidates

## Scope
- Web UI changes are out of scope for now (frozen).
- Focus on non-web stability and correctness issues.

## Fix Status (Phase 1 — branch: fix/stability-phase1, merged to main)

| ID | Finding | Status | Commit | Notes |
|----|---------|--------|--------|-------|
| C1 | Unbounded JSON construction causing heap fragmentation | ✅ Fixed | `3762d8e` | Added `String::reserve()` to `buildStateJson()`, `buildHealthJson()`, `buildI2CDiagnosticsJson()` in `AsyncWebInterface.h` |
| C2 | Race conditions on shared I2C cache state | ✅ N/A (non-issue) | — | Verified: `eventLoopTask` pinned to Core 0, same as ESPAsyncWebServer. All I2C cache access is same-core; no mutex needed. |
| C3 | `stopRandom()` sets `fRandomEnabled = true` (should be false) | ✅ Fixed | `b28a1a1` | `MarcduinoSound.h:327` — corrected logic |
| C4 | Command handlers cast away const and mutate shared Marcduino buffer | ✅ Fixed | `9b72140` | `MarcduinoPanel.h` — all 17 handlers now copy to local `cmdCopy[64]` before parsing |
| C5 | Dead `return false;` at top of `LogicEffectBitmap` | ✅ Fixed | `b28a1a1` | `effects/BitmapEffect.h:3` — removed dead code that prevented effect from running |
| C6 | MetaBalls stride bug (`i*mb_number+x` vs `i*w+x`) | ✅ Already correct | — | Verified in current code — CRITICAL_FINDINGS.md originally described older state |
| C7 | Debug `Serial.println` spam in MetaBalls effect | ✅ Already removed | — | Verified — no Serial.println in current `MeatBallsEffect.h` |
| C7b | Floating-point color halving `R*0.5` in MetaBalls | ✅ Fixed | `b28a1a1` | `effects/MeatBallsEffect.h:125` — replaced with `R>>1` bitshift |
| C8 | `FadeAndScrollEffect` palette random range off-by-one | ✅ Fixed | `b28a1a1` | `effects/FadeAndScrollEffect.h:61` — `random(kPaletteLast)` → `random(kPaletteLast + 1)` |
| C9 | `#APRNAME`/`#APRSECRET` preference key mismatch | ✅ Already correct | — | Verified: `#APRNAME` uses `PREFERENCE_REMOTE_HOSTNAME`, `#APRSECRET` uses `PREFERENCE_REMOTE_SECRET` — original finding was inaccurate |
| H1 | Null pointer dereference risk in effect constructors | ✅ Fixed | `d302208` | `effects/FadeAndScrollEffect.h`, `FractalEffect.h`, `MeatBallsEffect.h` — added null checks on heap allocations |
| L3 | Makefile lacks documentation on `../Arduino.mk` relative path | ✅ Fixed | `2145da1` | Added comments explaining the relative path dependency and that PlatformIO is canonical |
| L5 | String literals in DEBUG_PRINT/PRINTLN not wrapped in `F()` macro | ✅ Fixed | `054893e`, `cd48053` | All 16 bare string literals in `AstroPixelsPlus.ino` now wrapped with `F()` to move from RAM to flash |

| ID | Finding | Status | Notes |
|----|---------|--------|-------|
| C3 | `stopRandom()` sets `fRandomEnabled = true` (should be false) | ✅ Fixed | MarcduinoSound.h:327 |
| C4 | Command handlers cast away const and mutate shared Marcduino buffer | ✅ Fixed | MarcduinoPanel.h — all 17 handlers now copy to local `cmdCopy[64]` |
| C5 | Dead `return false;` at top of `LogicEffectBitmap` prevented effect from running | ✅ Fixed | effects/BitmapEffect.h:3 |
| C6 | MetaBalls stride bug (`i*mb_number+x` vs `i*w+x`) | ✅ Already correct | Verified in current code — CRITICAL_FINDINGS.md described older state |
| C7 | Debug `Serial.println` spam in MetaBalls effect | ✅ Already removed | Verified — no Serial.println in current MeatBallsEffect.h |
| C7b | Floating-point color halving `R*0.5` in MetaBalls | ✅ Fixed | effects/MeatBallsEffect.h:125 — replaced with `R>>1` bitshift |
| C8 | `FadeAndScrollEffect` palette random range off-by-one | ✅ Fixed | effects/FadeAndScrollEffect.h:61 — `random(kPaletteLast)` → `random(kPaletteLast + 1)` |
| C9 | `#APRNAME`/`#APRSECRET` preference key mismatch | ✅ Already correct | Verified: `#APRNAME` uses `PREFERENCE_REMOTE_HOSTNAME`, `#APRSECRET` uses `PREFERENCE_REMOTE_SECRET` — the finding in this file was inaccurate |

## Confirmed Safe Behavior
- Effect object ownership is safe: `LogicEngineRenderer::setEffectObject` deletes previous objects.
- `#APRNAME` and `#APRSECRET` handlers use correct preference keys and defaults.

## Historical Context

This file was created during a comprehensive code review ("code remediation") to track correctness, robustness, and cleanup issues. All Phase 1 items have been addressed. The firmware has been validated on hardware with all fixes applied.

---

## Additional Observations (Unchanged)

### C1: Unbounded JSON Construction (AsyncWebInterface.h)
- **Problem:** `buildStateJson()`, `buildHealthJson()`, `buildI2CDiagnosticsJson()` use String concatenation without size bounds, risking heap fragmentation on rapid API calls.
- **Status:** Pending — ArduinoJson not in platformio.ini deps; fix will use size-capped String approach.

### C2: Race Conditions on Shared I2C Cache State (AsyncWebInterface.h)
- **Problem:** Shared globals (`cachedPanelsOk`, `cachedHolosOk`, etc.) and `i2cProbeFailures` accessed from async web handlers (Core 0) and main loop (Core 1) without synchronization.
- **Status:** Pending — will extend existing `portMUX_TYPE sArtooTelemetryMux` pattern.

## Additional Observations (Unchanged)
- `AstroPixelsPlus.ino` uses blocking delays (`delay(1000)`, `delay(100)`, `delay(3000)`, `vTaskDelay(1)`). These are not confirmed critical, but can cause responsiveness hiccups depending on call paths.
- Logic display types are selected at compile time in `AstroPixelsPlus.ino` (AstroPixel vs RSeries variants), so effect code should not assume a specific width/height.
- Marcduino command buffer size defaults to 64 bytes in ReelTwo (`MarcduinoSerial<BUFFER_SIZE=64>`). `AstroPixelsPlus.ino` uses `MarcduinoSerial<>`, so commands longer than 63 chars are truncated before parsing. `Marcduino::getCommand()` points into this buffer and is only safe during the current handler.

## References
- `effects/MeatBallsEffect.h`
- `effects/BitmapEffect.h`
- `effects/FadeAndScrollEffect.h`
- `MarcduinoPanel.h`
- `MarcduinoSound.h`
- `AstroPixelsPlus.ino`
- ReelTwo `LogicEngine.h` (effect object ownership)
