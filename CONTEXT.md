# AstroPixelsPlus — Domain Glossary

Terms used consistently across firmware, documentation, and UI in this fork.

---

## Servo addressing

**Physical channel**
The channel number printed on the PCA9685 board silkscreen (0–15). This is the
number a builder sees when looking at the board and the only number surfaced in
the web UI. Two boards are in use: panel board (I2C 0x40) and holo board (0x41).
Physical channels on each board are independently numbered 0–15.

**Firmware channel**
The 1-indexed number used internally by `ServoDispatchPCA9685`:
`firmware_channel = physical_channel + 1`. Never shown to operators or builders.
The conversion is done once in firmware and hidden from all UI and documentation.

**Servo slot**
A numbered position in `servoSettings[]` that binds a physical channel to a
panel address bit. Slot index and Marcduino command number are independent — a
slot's command is determined by which panel address bit it carries, not by its
array position (slot 4 holds P7 and responds to `:OP07`, not `:OP05`). Distinct
from both the physical channel and the panel identity.

**Panel address bit**
A unique bitmask constant carried in a servo slot's group field, named after the
builder-facing panel identity it controls: `PANEL_P1`, `PANEL_P7`, `PANEL_PP1`,
`PANEL_PP6`, etc. `panelTargetToMask()` maps each `:OPnn` / `:OPPn` command to
the corresponding bit, selecting only the slot(s) carrying that bit. Distinct
from panel group shortcuts (`PIE_PANEL`, `DOME_PANELS_MASK`) which combine
multiple bits to address a set of panels at once.

---

## Panel identity

**Panel identity** (P1, P2, P3, P4, P7, P11, P13, PP1, PP2, PP3, PP4, PP5, PP6)
The printed-droid community standard label for a specific physical panel on the
Mr. Baddeley MK4 complex dome. Panel identity is fixed to the dome — it does not
change regardless of how the servo is wired. Used as the display label in firmware
comments, UI, and documentation.

**Servoed panel**
A panel position with a servo wired on the standard MK4 build: P1, P2, P3, P4,
P7, P11, P13 (ring panels) and PP1, PP2, PP4, PP6 (pie panels). These are the
individually addressable positions in the wiring config UI.

**Unserviced panel**
A panel position that exists physically on the dome and has a firmware servo slot,
but has no servo wired on the standard MK4 build: PP3 (top pie, firmware type bit
`TOP_PIE_PANEL`) and PP5 (pie, firmware type bit `PIE_PANEL` — see ADR 0008).
Shown in the wiring config UI as inactive by default; a builder may activate them
if they wire a servo.

**Fixed panel**
A dome position permanently occupied by electronics or mechanics — no servo slot
exists and no servo is ever wired: P5 (Magic Panel/frame), P6, P8 (Rear PSI),
P9 (Rear Logic Display), P10, P12 (Front Logic Display), P14 (Front PSI).
Not shown in the wiring config UI.

---

## Panel command verbs

**`:OP`** — **Open.** Drives the targeted panel(s) to the calibrated open pulse.

**`:CL`** — **Close.** Drives the targeted panel(s) to the calibrated closed pulse,
then schedules a PWM release (servo grind protection).

**`:OF`** — **Flutter** (not "off"). Runs the `SeqPanelAllFlutter` animation —
rapid open/close cycles — on the targeted panel(s). The two-letter `OF` is a
Marcduino-community shorthand for the flutter animation; it does not mean
"power off." There is no per-panel power-off command in this firmware; PWM
release is handled implicitly by `schedulePanelRelease()` after `:CL*`.

**`:SE`** — **Sequence.** Triggers a named choreography (wave, marching ants,
short/medium/long, etc.). Targets the panel set baked into the sequence.

These verbs combine with a panel target number (`:OP01` ring panel P1,
`:OPP3` pie panel PP3, `:OF00` flutter all, ...) to form the full command string
processed by `Marcduino::processCommand()`.

**FHP** — Front Holo Projector. AstroPixels firmware name. Community equivalent: **HP1**.
Located at the front-bottom of the dome.

**RHP** — Rear Holo Projector. AstroPixels firmware name. Community equivalent: **HP2**.
Located at the rear-top of the dome.

**THP** — Top Holo Projector. AstroPixels firmware name. Community equivalent: **HP3**.
Located in the top center pie area of the dome.

Both naming conventions are shown in the UI: "FHP (HP1)", "RHP (HP2)", "THP (HP3)".

---

## Activities

**Wiring config**
The commissioning-time activity of assigning physical board channels to servo slots
and marking which slots have servos wired. Done once at build or rewire time. Distinct
from operational control (commanding panels to open/close during operation).

**Raw servo test**
A commissioning-time action that writes directly to a physical PCA9685 channel
to identify which wired servo is connected there. It bypasses servo slots and
Marcduino command routing. Panel raw servo tests hold the channel open until
stopped; holo raw servo tests sweep the selected channel until stopped.

**Operational control**
Day-to-day commands sent to panels and holos: open, close, sequences, holo position.
The primary purpose of the web UI panel and holo pages.
