# Wiring Commissioning Behavior

This note characterizes the wiring config and raw servo test behavior that the
firmware preserves while deepening the implementation.

## Wiring Config

Wiring config is a commissioning-time activity. It assigns physical PCA9685
silkscreen channels to servo slots and marks slots active or inactive.

Panel config:

- Route: `GET /api/panels/config`, `POST /api/panels/config`
- Board: PCA9685 `0x40`
- Slot count: 13
- GET response includes `cmd` per slot.
- POST body shape: `{"slots":[{"index":0,"channel":0,"active":true}, ...]}`

Holo config:

- Route: `GET /api/holos/config`, `POST /api/holos/config`
- Board: PCA9685 `0x41`
- Slot count: 6
- GET response omits `cmd`.
- POST body shape matches panel config.

Validation:

- `slots` must exist and be an array.
- Slot count must match the board.
- Each slot object must include `index`, `channel`, and `active`.
- `index` must match the array position.
- `channel` must be a number from 0 to 15.
- `active` must be `true` or `false`.
- Two active slots on the same board must not share a channel.
- Inactive slots may share channels because they are excluded from routing.

Successful save:

- Persists all slot `channel` and `active` values to NVS.
- Stops any active raw servo test on the same board.
- Live-applies routing via `ServoDispatch::setServo()`.
- Returns `{"ok":true,"applied":true,"reboot_required":false}`.

Runtime apply:

- Active slot: firmware pin is derived from physical channel and board index.
- Inactive slot: `pin=0` and `group=0`.
- Pulse/calibration values are preserved from current `servoDispatch` state.
- Command group bits are restored from immutable `servoSettings[]` defaults.

Boot apply:

- `panelConfigLoad()` and `holoConfigLoad()` still run after `Wire.begin()` and
  before `SetupEvent::ready()`.
- That ordering remains load-bearing because `SetupEvent::ready()` can trigger
  the first PCA9685 write.

## Raw Servo Test

Raw servo test writes directly to a physical PCA9685 channel. It bypasses servo
slot routing and Marcduino commands so a builder can identify wiring.

Routes:

- `POST /api/servo/test`
- `POST /api/servo/stop`

Request shape:

- Test: `{"board":"panels","channel":0}` or `{"board":"holos","channel":0}`
- Stop: `{"board":"panels"}` or `{"board":"holos"}`

Behavior:

- Only one raw servo test runs per board.
- Starting a second test on the same board stops the previous channel first.
- Panel raw servo test opens the channel and holds it until stopped.
- Holo raw servo test sweeps the channel between two extremes until stopped.
- Stop is idempotent; stopping a board with no active test still returns
  success.
- Config save stops any active raw servo test on that board before live apply.

## Verification

Use these checks after wiring commissioning changes:

```bash
pio run -e astropixelsplus
pio run -e astropixelsplus -t buildfs
python3 tools/command_compat_matrix.py --dry-run
make gate
```

Hardware checks when a dome is attached:

- Start panel raw servo test, save panel config, verify the held channel closes.
- Start holo raw servo test, save holo config, verify sweep stops.
- Change a mapped channel, save, then send the matching Marcduino command and
  verify the newly mapped channel moves.
