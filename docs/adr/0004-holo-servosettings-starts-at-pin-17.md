# ADR 0004 — Holo servoSettings[] entries start at firmware pin 17, not 16

**Status:** Accepted  
**Date:** 2026-05-23

## Context

`ServoDispatchPCA9685` maps firmware pin numbers to physical PCA9685 boards using:

```
chip    = (pin - 1) / 16   → 0 = first board (0x40), 1 = second board (0x41)
channel = (pin - 1) % 16
```

The natural expectation when adding a second 16-channel board (0x41) is that its
channels begin at firmware pin 16. However:

```
pin 16 → chip = (16-1)/16 = 0, channel = 15  → 0x40 board, physical CH15
pin 17 → chip = (17-1)/16 = 1, channel =  0  → 0x41 board, physical CH0
```

Pin 16 still addresses the **panel board** (0x40) at its last channel. The holo
board (0x41) only begins at pin 17.

The original firmware had `servoSettings[]` holo entries with pins
`{16, 17, 18, 19, 20, 21}`. This placed the first holo slot (FHP horizontal)
on 0x40 CH15 (the panel board's last physical channel) rather than 0x41 CH0.
The remaining holo slots were also each one position off, routing to the wrong
physical channel on the wrong board.

This was discovered by tracing the chip-boundary formula during the 2026-05-23
planning session and confirmed against the actual `servoSettings[]` in code.

## Decision

Holo `servoSettings[]` entries use firmware pins `{17, 18, 19, 20, 21, 22}`,
mapping to 0x41 physical CH0–5. The corrected table:

| Slot | Holo axis  | Firmware pin | chip | board | Physical CH |
|------|-----------|-------------|------|-------|-------------|
| 13   | FHP — H   | 17          | 1    | 0x41  | 0           |
| 14   | FHP — V   | 18          | 1    | 0x41  | 1           |
| 15   | THP — H   | 19          | 1    | 0x41  | 2           |
| 16   | THP — V   | 20          | 1    | 0x41  | 3           |
| 17   | RHP — V   | 21          | 1    | 0x41  | 4           |
| 18   | RHP — H   | 22          | 1    | 0x41  | 5           |

`holoConfigLoad()` applies the same formula at runtime:
`pin = active ? (16 + physCh + 1) : 0`

which for physical CH0 gives `16 + 0 + 1 = 17` — correctly landing on 0x41 CH0.

## Consequences

- Any future addition of holo servo slots must remember that 0x41 begins at
  firmware pin 17, not 16.
- The formula `16 + physCh + 1` encodes this boundary explicitly and must be
  used for all 0x41 channel conversions.
- A general pattern for any nth board: `pin = (n * 16) + physCh + 1` where
  n=0 for 0x40 and n=1 for 0x41.
- The original `{16, ...}` entries silently drove 0x40 CH15 — a panel board
  channel with no holo servo wired, producing no visible failure but no holo
  servo movement either.
