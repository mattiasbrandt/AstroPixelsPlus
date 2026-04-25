# protoR2link Revisit Spec (Body + Dome Contract)

## Scope and intent

This document is the cross-project contract for `protoR2link` behavior between:
- Body firmware: `protoArtoo`
- Dome firmware: AstroPixelsPlus fork (`mattiasbrandt/AstroPixelsPlus`)

Naming convention used in this spec:
- `protoR2link` = neutral cross-project transport name
- `Dome Link` = body-side naming in protoArtoo (link from body to dome)
- `Body Link` = dome-side naming in AstroPixelsPlus (link from dome to body)

It records the current architecture, Tier 1 body-only cleanup scope, and the WiFi fallback transport design for immediate execution.

Related tracking task in this repo:
- `tasks/phase5-tasks.md` -> `T18 - protoR2link revisit: cleanup + WiFi fallback transport`
---

## 1. Current architecture (both sides)

### Body side (protoArtoo)

- Task: `DomeLinkTask`
  - Core: 1
  - Priority: 3
  - Poll interval: 10 ms
  - Stack: 3072 bytes
- Transport: UART2, 9600 baud, 8N1, over slip ring
  - TX pin: GPIO 33
  - RX pin: GPIO 34
- TX path:
  - Queue: `domeTxQueue` (FreeRTOS queue, 16 slots)
  - Sender writes line to UART2 and appends `\r`
- TX heartbeat:
  - Sends `#PAHB\r` at 1 Hz
  - Increments `robotState.bodyHbTx`
- RX path:
  - Static line buffer: 64 bytes
  - Delimiter: CR/LF terminated line
  - Overflow handling: line discarded
- RX heartbeat handling:
  - `#APHB` is intercepted
  - Updates `domeLastSeenMs` and increments `robotState.domeHbRx`
- RX command routing:
  - `parseDomeRxLine()` -> `parseMarcduinoCommand()`
  - Prefix behavior:
    - `$` -> AudioTask queue (sound playback)
    - `:SE3x` -> ServoTask queue (arm sequences)
    - `:OP`, `:CL`, `:MV` -> ServoTask queue (panel commands)
    - `:SE1x` -> `applyMood(..., fromDome=true)` for echo suppression
    - `#APSL`, `#APWU` -> sleep/wake sync
    - `*`, `@`, `%`, `&`, `!` -> silently discarded (dome-only prefixes)
- Connection state helper:
  - `domeConnected()` evaluates true when:
    - `domeLastSeenMs > 0`
    - `(millis() - domeLastSeenMs) < 5000`
- Status surface:
  - `/api/status` includes `dome_link` block (`state`, `hb_tx`, `hb_rx`, `last_rx_ms`)
- Feature toggle:
  - Config field: `cfg_enable_s3_dome_ctrl`
  - NVS key: `en_s3_dome_ctrl`
- Key files:
  - `include/dome_link.h`
  - `src/tasks/dome_link.cpp`
  - `src/drivers/dome_rx_parser.cpp`
  - `include/marcduino.h`

### Dome side (AstroPixelsPlus fork)

- `handleBodySerial()`:
  - Manual `COMMAND_SERIAL` read loop
  - Intercepts `#PAHB`
- `handleBodyLinkHeartbeat()`:
  - Sends `#APHB\r` at 1 Hz
- `sendBodyCommand()`:
  - Sends Marcduino commands to body during sequence execution
- `bodyLinkConnected()`:
  - 5 s timeout helper for connection state
- Sequence usage:
  - 13 sequences (`:SE01`-`:SE15`) call `sendBodyCommand()` for body-side actions
- Feature toggle:
  - NVS key: `mbodylink` (bool, default true)
- Observability:
  - Body-link status in `/api/health`
  - Body-link status in WebSocket state
  - Body-link badge in web UI
- Serial stream ownership detail:
  - When body link is enabled, `marcduinoSerial.setStream()` is disabled
- Source-tagged dispatch examples:
  - `astropixel-web-api`
  - `wifi-marcduino`
  - `usb-serial`

---

## 2. UART2 sharing model

ESP32 has three hardware UARTs. UART2 is shared by:
- Body-side Dome Link serial (`protoR2link` transport over GPIO 33 TX / GPIO 34 RX), and
- Audio RX status queries (GPIO 35 RX-only)

Selection is by boot-time config flag:
- Dome control active:
  - Body-side Dome Link owns UART2
  - Audio RX queries return cached state
- Dome control disabled:
  - Audio path owns UART2 RX
  - Dome task idles

Clarifications:
- Sound playback TX is not affected (software UART bit-bang on GPIO 26).
- Audio RX query path is setup/debug/troubleshooting only, not operationally critical.

---

## 3. What changes in Tier 1 (body side only)

Tier 1 is cleanup only. No dome-side protocol or behavior changes are required.

1. Rename `MD_DOME_HB_ACK` -> `MD_DOME_HB` (name cleanup only).
2. Add RX diagnostics counters:
   - `domeRxOverflowCount`
   - `domeRxUnknownCount`
3. Move `domeConnected()` from `main.cpp` to `dome_link.cpp`.
4. Document UART2 ownership model in `dome_link.h`.

---

## 4. WiFi fallback transport design (active phase, both sides)

### Requirement

No dependency on T10 hardware validation. Start body-side WiFi fallback now; dome-side AstroPixelsPlus changes may run in parallel and are not a blocker for body-side implementation.
Keep UART as primary transport when slip ring UART is healthy. Use WiFi fallback when UART heartbeat times out.
Both endpoints remain STA clients on the same LAN.

### Transport split

- Commands over HTTP REST:
  - Body -> dome: `POST /api/cmd`
  - Dome -> body: `POST /api/manual-command`
  - Rationale: both endpoints already exist and are validated.
- Heartbeat over UDP:
  - 1 Hz heartbeat using same payloads: `#PAHB` and `#APHB`
  - Port: 4901 (both sides)
  - Approx packet size: ~20 bytes

### Dome IP discovery

- Preferred: mDNS (resilient to DHCP changes)
- Fallback: manual NVS-configured peer IP

### Body transport selection logic (DomeLinkTask)

- Primary mode:
  - If `cfg_enable_s3_dome_ctrl` is enabled and UART heartbeat age is < 5 s, TX uses UART.
- Fallback mode:
  - If UART heartbeat age is > 5 s, and STA is connected, and dome peer is known, TX uses WiFi.
- RX behavior:
  - Listen on both available sources:
    - UART line parser (when UART path active)
    - UDP `parsePacket()` parser
- Recovery:
  - If UART heartbeat resumes, switch back to UART (preferred transport).

### Dome-side changes needed for WiFi transport

1. Add UDP listener on port 4901 in main loop via non-blocking `parsePacket()`.
2. Feed received command lines into existing `Marcduino::processCommand()` path.
3. Extend `sendBodyCommand()` with WiFi path (UDP/HTTP) alongside Serial2 TX.
4. Extend `handleBodyLinkHeartbeat()` to send `#APHB` via UDP when UART path is down.
5. Advertise `_marcduino._udp` via mDNS.
6. Add NVS config for WiFi transport enable/disable and body peer IP.
7. Add web UI indicator for active transport (`UART`, `WiFi`, `disconnected`).

---

## 5. Alternatives evaluated and rejected

- I2C over slip ring:
  - Rejected due to hardware and signal-integrity concerns.
  - GPIO 34 is input-only.
  - I2C is fragile under slip ring noise and can fail as stuck SDA bus.
- ESP-IDF native UART driver migration:
  - Rejected as unnecessary complexity at 9600 baud.
  - Current implementation already meets functional needs.
- CRC/checksum layer for protoR2link lines:
  - Rejected for current scope.
  - Harmful-valid-corruption risk is low; heartbeat timeout already handles link loss/failure.
- Event-driven UART handling:
  - Rejected as unnecessary complexity.
  - UART2 FIFO is 128 bytes; 10 ms polling at 9600 baud does not overflow in current design envelope.

---

## 6. Protocol reference

| Command | Direction | Rate | Purpose |
|---|---|---|---|
| `#PAHB\r` | Body -> Dome | 1 Hz | Body heartbeat |
| `#APHB\r` | Dome -> Body | 1 Hz | Dome heartbeat |
| `#PASL\r` | Body -> Dome | on change | Sleep sync |
| `#PAWU\r` | Body -> Dome | on change | Wake sync |
| `#APSL\r` | Dome -> Body | on change | Sleep sync from dome |
| `#APWU\r` | Dome -> Body | on change | Wake sync from dome |
| `:SExx\r` | Both directions | on trigger | Sequence commands |
| `$x\r` | Dome -> Body | on trigger | Sound playback |
| `:OPxx\r` / `:CLxx\r` | Dome -> Body | on trigger | Panel open/close |
| `:MVxxdddd\r` | Dome -> Body | on trigger | Panel position |

Connection states:
- `not_seen`: no heartbeat has ever been received.
- `connected`: last heartbeat age < 5 s.
- `lost`: heartbeat was seen previously, but age is now > 5 s.
