# AstroPixelsPlus — Copilot Instructions

The maintained agent and contributor guide for this repository is
[`CLAUDE.md`](../CLAUDE.md). Read it first; it covers the build, the HTTP OTA
workflow, the bench-only USB rule, the planning gate and the architecture.

Reference material:

- [`docs/SETUP.md`](../docs/SETUP.md) — build and first-time setup
- [`docs/HARDWARE_WIRING.md`](../docs/HARDWARE_WIRING.md) — pins, servos, LEDs
- [`docs/COMMANDS.md`](../docs/COMMANDS.md) — Marcduino command reference
- [`docs/REST_API.md`](../docs/REST_API.md) — REST and WebSocket API
- [`docs/adr/`](../docs/adr/) — architecture decisions
- [`FORK_IMPROVEMENTS.md`](../FORK_IMPROVEMENTS.md) — what this fork changes

Hard rule repeated here: when the full droid is connected, never upload over
USB (the cable usually goes to another controller, not the dome ESP32). USB is
for explicit bench mode only. Use
`make ota OTA_IP=astropixelsplus.local` and `make uploadfs OTA_IP=...`.
