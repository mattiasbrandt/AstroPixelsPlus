# Critical Findings and Improvement Candidates

## Scope
- Web UI changes are out of scope for now (frozen).
- Focus on non-web stability and correctness issues.

## Verified Findings

### 1) MetaBalls indexing bug (effects/MeatBallsEffect.h)
- Issue: `mb_vx` and `mb_vy` are allocated as `mb_number * w` and `mb_number * h` but indexed as `i * mb_number + x/y`.
- Impact: Overwrites data and produces incorrect metaballs rendering; latent risk if dimensions or mb_number change.
- Fix: Use `i * w + x` for `mb_vx` and `i * h + y` for `mb_vy`.

### 2) Debug spam in MetaBalls effect (effects/MeatBallsEffect.h)
- Issue: 5x `Serial.println(randomDouble())` on effect init.
- Impact: Serial noise and timing jitter during effect changes.
- Fix: Remove or guard behind a debug macro.

### 3) Web UI fragility (research note only)
- Observation: ReelTwo WElement/WButton/WStyle constructors allocate heap at static init; this is a known crash source on ESP32.
- Action: No changes planned; keep Web UI frozen.

## Confirmed Safe Behavior
- Effect object ownership is safe: `LogicEngineRenderer::setEffectObject` deletes previous objects.

## Suggested Next Steps (non-web)
1. Apply metaballs stride fix.
2. Remove or gate Serial debug spam.
3. Optional: quick scan for similar stride patterns in other effects.

## Planned (No Code Changes Yet)
- Prepare minimal patch for metaballs stride fix (non-behavioral).
- Propose gating or removal of MetaBalls Serial debug prints.
- Review blocking delays in `AstroPixelsPlus.ino` for potential non-blocking alternatives if they are on hot paths.

## Additional Observations
- `AstroPixelsPlus.ino` uses blocking delays (`delay(1000)`, `delay(100)`, `delay(3000)`, `vTaskDelay(1)`). These are not confirmed critical, but can cause responsiveness hiccups depending on call paths.
- Logic display types are selected at compile time in `AstroPixelsPlus.ino` (AstroPixel vs RSeries variants), so effect code should not assume a specific width/height.
- Marcduino command buffer size defaults to 64 bytes in ReelTwo (`MarcduinoSerial<BUFFER_SIZE=64>`). `AstroPixelsPlus.ino` uses `MarcduinoSerial<>`, so commands longer than 63 chars are truncated before parsing. `Marcduino::getCommand()` points into this buffer and is only safe during the current handler.
- `RemoteName`/`RemoteSecret` handlers use inconsistent preference keys/defaults: `#APRNAME` compares against `PREFERENCE_REMOTE_SECRET` (should be `PREFERENCE_REMOTE_HOSTNAME`) and `#APRSECRET` compares against `SMQ_HOSTNAME` (should be `SMQ_SECRET`).
- `effects/FadeAndScrollEffect.h` uses `random(1)` which always returns 0, so RGB/Half palettes are never selected.

## Proposed Change Set (Non-Web)
- Fix metaballs buffer indexing to use `i * width + x` and `i * height + y` stride.
- Remove or guard Serial debug spam in MetaBalls effect constructor.
- (Optional) Audit blocking delays for safe non-blocking replacements if they appear in hot paths.
- Update `#APRNAME` and `#APRSECRET` handlers to use correct preference keys/defaults.
- Fix palette selection randomness in `FadeAndScrollEffect`.
- If longer Marcduino commands are needed, increase `MarcduinoSerial<BUFFER_SIZE>` explicitly and document the new limit.

## References
- `effects/MeatBallsEffect.h`
- `AstroPixelsPlus.ino`
- ReelTwo `LogicEngine.h` (effect object ownership)
