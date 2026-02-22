# Plan: Migrate AstroPixelsPlus Web UI to ESPAsyncWebServer

## Context

The ReelTwo WifiWebServer / WButton / WElement system allocates heap during global C++ static initialization (before FreeRTOS starts), which limits the project to ~50 WButton instances total. This has blocked adding new controls and features to the web UI. The goal is to replace only the web layer with ESPAsyncWebServer — keeping all ReelTwo core functionality (Marcduino, LogicEngineRenderer, ServoDispatch, ESPNOW, OTA, WifiAccess) completely unchanged.

## Goals

1. **Remove the WButton limit** — move the entire web UI to SPIFFS-served HTML/CSS/JS
2. **Health dashboard** — index page shows traffic-light indicators for subsystem health
3. **Live log console** — index page shows a scrolling log window with recent Serial debug output
4. **Sequence descriptions** — sequence trigger pages show human-readable descriptions of what each sequence does
5. **Extensible** — easy to add new pages/controls without hitting memory limits

## Decisions

- **UI**: Modern HTML/CSS/JS files stored in SPIFFS (`/data/` directory) — no compile-time button limits
- **Control→Robot**: REST API for commands (`POST /api/cmd`), WebSocket for real-time state back to browser
- **WiFi layer**: Keep `WifiAccess` from ReelTwo for preference storage and AP mode setup

## What Changes vs What Stays

### Replaced
- `wifi/WifiWebServer.h` include and `WifiWebServer<>` object
- `WebPages.h` (all WButton/WSelect/WElement declarations)

### Unchanged (zero modifications)
- ReelTwo core: Marcduino, CommandEvent, LogicEngineRenderer, ServoDispatch, DFPlayer
- `WifiAccess` — WiFi setup and credential preferences
- `ArduinoOTA` — OTA firmware updates
- ESPNOW/SMQ droid remote subsystem
- All `Marcduino*.h` command handlers
- `AstroPixelsPlus.ino` main loop, setup, hardware init

## New Dependencies (platformio.ini)

```
mathieucarbou/ESPAsyncWebServer @ ^3.3.0
mathieucarbou/AsyncTCP @ ^3.3.0
```

(Use mathieucarbou fork — actively maintained, ESP32 stable, works with ESP-IDF 5.x)

## New File Structure

```
/data/                    → SPIFFS partition (pio run -t uploadfs)
  index.html              → Dashboard: navigation + health traffic lights + log console
  panels.html             → Panel control (no button count limit)
  holos.html              → Holo projector control with descriptions
  logics.html             → Logic display control with descriptions
  sequences.html          → Choreographed sequence triggers with descriptions
  setup.html              → Setup navigation
  serial.html             → Serial/Marcduino config
  sound.html              → Audio player config
  wifi.html               → WiFi credentials
  remote.html             → Droid Remote config
  firmware.html           → OTA firmware upload
  style.css               → Shared CSS (mobile-friendly)
  app.js                  → Shared JS (WebSocket client, fetch helpers, health poller)
  sequences.json          → Sequence descriptions data (name, command, description)

AsyncWebInterface.h       → Replaces WebPages.h; registers all routes and API handlers
LogCapture.h              → Ring buffer log capture, tees Serial output for WS broadcast
```

## API Design

| Endpoint | Method | Body / Response | Action |
|----------|--------|-----------------|--------|
| `/api/cmd` | POST | `cmd=:OP01` | `Marcduino::processCommand(player, cmd)` |
| `/api/state` | GET | returns JSON | Current state snapshot (includes health) |
| `/api/health` | GET | returns JSON | Subsystem health only (lightweight) |
| `/api/pref` | POST | `key=K&val=V` | `preferences.put*()` + optional reboot |
| `/api/reboot` | POST | — | `reboot()` |
| `/upload/firmware` | POST | multipart | `Update.*` API (same as current) |
| `/ws` | WS | — | Push JSON state + health + log lines |

**State JSON example:**
```json
{
  "fldSeq": 0, "rldSeq": 0, "fldSpeed": 5, "rldSpeed": 5,
  "vol": 50, "wifiEnabled": true
}
```

**Health JSON example:**
```json
{
  "i2c_panels": true,
  "i2c_holos": true,
  "sound_module": true,
  "wifi": true,
  "remote": false,
  "spiffs": true,
  "i2c_devices": ["0x40 PCA9685 Panels", "0x41 PCA9685 Holos", "0x3C OLED"]
}
```

**WebSocket message types:**
```json
{"type": "state", "data": { ... }}
{"type": "health", "data": { ... }}
{"type": "log", "line": "14:32:05 Marcduino: :OP01"}
```

WebSocket broadcasts on state change, health change, and new log lines.

## Key Integration Points (Unchanged API)

```cpp
// Command injection — same as today, called from /api/cmd handler:
Marcduino::processCommand(player, cmdString);
CommandEvent::process(F("..."));

// WiFi setup — same as today, wifiAccess passed into initAsyncWeb():
wifiAccess.setNetworkCredentials(...);

// Preference reads — same Preferences.h keys, unchanged
preferences.getBool(PREFERENCE_WIFI_ENABLED, true);
```

---

## Implementation Phases (Thin Slices)

### Phase 0 — Create feature branch
- [x] `git checkout -b feature/async-webserver`
- [x] Write this plan to `tasks/todo.md`

### Phase 1 — Infrastructure swap (verify nothing breaks)
- [x] Add ESPAsyncWebServer + AsyncTCP to `platformio.ini` (also added `board_build.filesystem = spiffs`)
- [x] Create `AsyncWebInterface.h`:
  - `AsyncWebServer asyncServer(80)` and `AsyncWebSocket ws("/ws")`
  - SPIFFS static file serving with `serveStatic()`
  - `POST /api/cmd` → `Marcduino::processCommand(player, cmd)`
  - `GET /api/state` → JSON with wifiEnabled, remoteEnabled, remoteConnected, uptime, freeHeap
  - `POST /api/pref` → `preferences.putString()` with optional reboot
  - `POST /api/reboot` → `reboot()`
  - `POST /upload/firmware` → OTA with FLD/RLD progress display (ported from WebPages.h)
  - WebSocket handler for incoming commands + `broadcastState()` for push updates
  - `initAsyncWeb()` called from setup, `asyncWebLoop()` called from eventLoopTask
- [x] In `AstroPixelsPlus.ino`:
  - Line 164: `#include <ESPAsyncWebServer.h>` (replaced `wifi/WifiWebServer.h`)
  - Line 528: `#include "AsyncWebInterface.h"` (replaced `WebPages.h`)
  - Line 879: `initAsyncWeb()` (replaced `webServer.setConnect(...)`)
  - Line 1160: `asyncWebLoop()` (replaced `webServer.handle()`)
- [x] Create placeholder `/data/index.html` (navigation + basic state display)
- [x] Archive `WebPages.h` → `WebPages.h.bak` (`.gitignore` already covers `*.*.bak`)
- [x] **Verify**: `pio run -e astropixelsplus` compiles clean — SUCCESS (RAM 16.5%, Flash 79.9%, 5.7s)
  - Fix: removed `static` from `sRemoteConnected`/`sRemoteConnecting`/`sRemoteAddress` in `.ino` (conflicted with `extern` in `AsyncWebInterface.h`)
- [ ] **Verify**: WiFi AP comes up, browser can reach 192.168.4.1, OTA still works, Marcduino still works via Serial2 ← requires hardware test

### Phase 2 — Core control pages
- [ ] Build `panels.html`, `holos.html`, `logics.html` with full control sets
  - JS: `fetch('/api/cmd', {method:'POST', body: new URLSearchParams({cmd: ':OP01'})})` per button
  - No limit on number of buttons
- [ ] Add WebSocket client in `app.js` — reconnects on drop, updates UI elements with state JSON

### Phase 3 — Settings pages
- [ ] Build `serial.html`, `sound.html`, `wifi.html`, `remote.html`, `firmware.html`
- [ ] Settings pages use `POST /api/pref` for saves; reboot triggered server-side

### Phase 4 — Health dashboard (index page)
- [ ] Implement `GET /api/health` endpoint in `AsyncWebInterface.h`
  - Re-run I2C scan at request time (probe 0x40, 0x41, 0x3C, etc.)
  - Report: `i2c_panels` (0x40 responds), `i2c_holos` (0x41 responds), `sound_module` (sMarcSound module != kDisabled), `wifi` (wifiAccess.isConnected()), `remote` (sRemoteConnected), `spiffs` (SPIFFS mounted OK)
  - Return device list from I2C scan
- [ ] Build health traffic lights on `index.html`
  - Green circle = subsystem OK, red circle = subsystem down/missing
  - Indicators: Servo Panels, Servo Holos, Sound Module, WiFi, Droid Remote, SPIFFS
  - Poll `/api/health` on page load; optionally push via WebSocket on change
- [ ] Add I2C periodic health check (optional — lightweight probe every 10s in main loop, broadcast via WS if status changes)

### Phase 5 — Live log console (index page)
- [ ] Create a ring buffer log capture (replace/tee `Serial.print` output)
  - Capture last ~50 lines of debug output in a circular buffer
  - Forward to WebSocket clients as `{"type":"log","line":"..."}` messages
- [ ] Build log console widget on `index.html`
  - Scrolling `<pre>` or `<div>` with monospace text
  - Auto-scroll to bottom, with pause-on-scroll-up behavior
  - Show timestamp + message
- [ ] Ensure log capture does not block or slow the main loop (async send, drop if WS buffer full)

### Phase 6 — Sequence descriptions
- [ ] Create sequence description data (JSON or JS object served from SPIFFS)
  - Choreographed sequences (`:SE00`–`:SE09`, `:SE10`–`:SE15`, `:SE50`–`:SE57`)
  - Each entry: command, name, description of what happens (panels, holos, logics, sound, timing)
  - Source descriptions from code comments in `MarcduinoSequence.h` (see reference below)
- [ ] Show descriptions on sequence trigger pages
  - Tooltip or expandable detail below each button/dropdown entry
  - Include: what physical actions happen, approximate duration, whether it includes sound
- [ ] Logic sequence descriptions on `logics.html` (24 sequences from `logic-sequences.h`)
- [ ] Holo command descriptions on `holos.html`

### Phase 7 — State and polish
- [ ] Implement full state JSON (current sequences, speed, volume, WiFi mode, etc.)
- [ ] Broadcast state on changes so multiple browser tabs stay in sync
- [ ] Style with mobile-friendly CSS (usable from phone on the WiFi AP)

### Future ideas (not yet planned)
- _User can add more feature ideas here as they come up_

---

## Critical Files to Modify

| File | Change |
|------|--------|
| `platformio.ini` | Add ESPAsyncWebServer + AsyncTCP dependencies |
| `AstroPixelsPlus.ino` | Replace WifiWebServer include/object with AsyncWebInterface; integrate LogCapture |
| `WebPages.h` | Archive as `WebPages.h.bak` (keep for reference) |
| `AsyncWebInterface.h` | **New** — all route/API/WebSocket registration, health endpoint |
| `LogCapture.h` | **New** — ring buffer log capture for WS broadcast |
| `/data/*.html` | **New** — all web UI pages including health dashboard |
| `/data/style.css` | **New** — shared styles |
| `/data/app.js` | **New** — WebSocket + fetch helpers + health/log display |
| `/data/sequences.json` | **New** — sequence descriptions data |

---

## Verification Steps

1. `pio run -e astropixelsplus` — compiles clean
2. `pio run -e astropixelsplus -t uploadfs` — SPIFFS uploaded
3. `pio run -e astropixelsplus -t upload` — firmware uploaded
4. Connect to WiFi AP `AstroPixels`, open `http://192.168.4.1`
5. **Index page**: health traffic lights show green/red for each subsystem
6. **Index page**: log console shows live debug output scrolling
7. Click each panel button — verify physical servo response
8. **Sequence page**: each sequence shows description alongside trigger button
9. Change logic sequence dropdown — verify logic display changes
10. WebSocket: open two browser tabs, trigger action in one, verify both update
11. OTA: upload firmware via `/firmware` page
12. Reboot via web UI — verify it comes back on the AP
13. Disconnect a servo board — verify health indicator goes red

---

## Rollback

`WebPages.h.bak` is kept throughout. To revert:
- Restore the `WifiWebServer` include in `AstroPixelsPlus.ino`
- Delete `AsyncWebInterface.h`
- No hardware or SPIFFS changes are destructive

---

## Resume Instructions

1. Read this file for current state
2. Check git branch: `git branch` — should be on `feature/async-webserver`
3. Check `git log --oneline -5` for what was last committed
4. Find first unchecked `[ ]` item above and continue from there

### Session State (last updated: 2026-02-22)

**Phase 1 build PASSED.** `pio run -e astropixelsplus` compiles clean (RAM 16.5%, Flash 79.9%).

**Current step**: Ready for commit, then Phase 2 (core control pages).

**Nothing is committed yet.** All changes are unstaged.

**Files changed (not committed)**:
- `platformio.ini` — added ESPAsyncWebServer + AsyncTCP deps, `board_build.filesystem = spiffs`
- `AstroPixelsPlus.ino` — 4 edits swapping WifiWebServer → AsyncWebInterface + removed `static` from remote vars
- `AsyncWebInterface.h` — **new** (239 lines)
- `data/index.html` — **new** (placeholder)
- `WebPages.h.bak` — archived copy (gitignored)
- `tasks/todo.md` — this plan file

**NOTE**: `git diff --stat` shows ~27 files with changes — most are line-ending (CRLF/LF) conversions, not content changes. The actual content changes are only the files listed above. Consider normalizing line endings in a separate commit or using `.gitattributes`.

---

## Working Notes

_Capture discoveries, decisions, and constraints as work progresses._

- ESP32 heap is limited; ESPAsyncWebServer uses async callbacks so it doesn't block the main loop
- SPIFFS partition must fit all HTML/CSS/JS files; keep assets minimal (no large frameworks)
- The 64-byte Marcduino command buffer limit still applies to commands sent via `/api/cmd`
- `WifiAccess` handles AP/STA mode switching and credential persistence — do not duplicate this

### Health Signal Sources (from codebase analysis)

| Subsystem | Init Check | Runtime Check | How to Detect |
|-----------|:---:|:---:|---------------|
| PCA9685 Panels (0x40) | I2C scan at boot | **None** — must add | `Wire.beginTransmission(0x40); Wire.endTransmission()` == 0 |
| PCA9685 Holos (0x41) | I2C scan at boot | **None** — must add | `Wire.beginTransmission(0x41); Wire.endTransmission()` == 0 |
| Sound module | `sMarcSound.begin()` returns bool | **None** | Check `sMarcSound` module state != kDisabled |
| WiFi | `wifiAccess` init | `wifiAccess.isConnected()` | Already available |
| Droid Remote | SMQ init | `sRemoteConnected` flag | Already tracked in `AstroPixelsPlus.ino:532` |
| SPIFFS | `SPIFFS.begin()` at boot | N/A | Store init result in a global bool |
| OLED display | `sDisplay.begin()` return | N/A | `sDisplay.isEnabled()` |

Key gap: PCA9685 servo controllers discard all I2C error return codes. Runtime health check requires re-probing the I2C address — lightweight (one byte transaction).

### Log Capture Design Notes

- `Serial.print()` goes to UART0 (USB). Cannot hook it without replacing the Print class or using `esp_log_set_vprintf()`.
- Simplest approach: create a `LogCapture` class that wraps `Print`, tees output to both Serial and a ring buffer. Replace `DEBUG_PRINT`/`DEBUG_PRINTLN` macros to also write to the capture buffer.
- Alternative: use `esp_log_set_vprintf()` to intercept ESP-IDF log output (only captures ESP_LOGx calls, not Arduino Serial.print).
- Ring buffer size: ~4KB (50 lines x ~80 chars) — small enough for ESP32 heap.
- WebSocket send must be non-blocking: if WS buffer is full, drop the log line.

### Sequence Descriptions Reference (from MarcduinoSequence.h)

These descriptions will be served as a JSON file in SPIFFS for the sequence pages to display.

| Command | Name | Description |
|---------|------|-------------|
| `:SE00` | Stop | Stops all running sequences |
| `:SE01` | Scream | Scream sound + alarm logics (3s) + all panels open/close |
| `:SE02` | Wave | Sound + sequential panel wave animation |
| `:SE03` | Smirk/Wave | Sound + fast panel wave |
| `:SE04` | Open/Close Wave | Sound + panel open/close wave |
| `:SE05` | Beep Cantina | Beep cantina sound + fire logics + holo short circuit + marching ants panels (15s) |
| `:SE06` | Short Circuit | Alarm → scream → failure logics + holo short circuit + smoke + sparks + fakes being dead (8s) then resets |
| `:SE07` | Cantina Dance | Orchestral cantina + disco logics + holo short circuit + panel dance (46s) |
| `:SE08` | Leia Message | Leia sound + Leia logics + front holo Leia position (45s) |
| `:SE09` | Disco | Disco sound + panel dance + FLD rainbow + RLD scrolls "STAR WARS" text (45s) |
| `:SE10` | Faint/Recover | Standard Marcduino faint and recover sequence |
| `:SE13` | Quiet Mode | Suppresses random animations |
| `:SE14` | Mid-Awake Mode | Moderate random animation frequency |
| `:SE15` | Full-Awake Mode | Full random animation frequency |
| `:SE50` | Scream (no panels) | Scream logics + sound only, panels stay closed |
| `:SE51` | Scream (panels only) | All panels open/close, no sound or logics |
| `:SE52` | Wave (panels only) | Panel wave animation only |
| `:SE53` | Smirk/Wave (panels only) | Fast wave panels only |
| `:SE54` | Open/Wave (panels only) | Open/close wave panels only |
| `:SE55` | Marching Ants (panels only) | Marching ants panel pattern |
| `:SE56` | Faint (panels only) | All panels open/close long (700-900ms) |
| `:SE57` | Rhythmic (panels only) | All panels open/close at speed 900 |
| `$815` | Harlem Shake | Fire logics + holo short circuit + shake panels + random steps (26.5s) |
| `$821` | Girl on Fire | Panel dance + fire logics + holo + smoke + fire strip (53s) |
| `$720` | Yoda Clear Mind | Opens panel group 6 + Yoda LED on holo (15s) |

### Logic Sequences Reference (from logic-sequences.h)

24 named LED patterns: Normal, Alarm, Failure, Leia, March, Solid Color, Flash Color, Flip Flop Color, Flip Flop Alt Color, Color Swap, Rainbow, Red Alert, Mic Bright, Mic Rainbow, Lights Out, Text, Text Scroll Left/Right/Up, Roaming Pixel, Horizontal Scan, Vertical Scan, Fire, Random.
