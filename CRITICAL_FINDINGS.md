# Critical Findings and Improvement Candidates

## Scope
- Web UI changes are out of scope for now (frozen).
- Focus on non-web stability and correctness issues.

## Fix Status (Phase 1 — branch: fix/stability-phase1)

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

## Remaining Phase 1 Work (C1, C2)

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
