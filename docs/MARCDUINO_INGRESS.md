# Marcduino Ingress Behavior

This note characterizes the command admission behavior that must be preserved
while unifying Marcduino command ingress.

## Entry Points

| Source | Current caller | Current admission path | Notes |
| --- | --- | --- | --- |
| REST API | `POST /api/cmd` and feature routes in `AsyncWebInterface.h` | `marcduinoIngressAdmit(kMarcduinoIngressWebApi, cmd)` | `/api/cmd` validates command text and returns HTTP 423 while sleeping. Feature routes synthesize fixed commands and rely on shared ingress for policy. |
| WebSocket | `/ws` text command frames | `marcduinoIngressAdmit(kMarcduinoIngressWebSocket, cmd)` | Parsed into a local 64-byte buffer. No per-message response; state is broadcast after admission. |
| USB serial | `Serial` in `mainLoop()` | `marcduinoIngressAdmit(kMarcduinoIngressUsbSerial, cmd)` | Uses `sBuffer`, capped by `CONSOLE_BUFFER_SIZE`. Optional pass-through to `COMMAND_SERIAL` happens before local admission. |
| Body-link UART | `COMMAND_SERIAL` in `handleBodySerial()` | `marcduinoIngressAdmit(kMarcduinoIngressBodyLinkUart, cmd)` then `drainMarcduinoCommandQueue()` | Heartbeat `#PAHB` is consumed by the transport and not admitted as a Marcduino command. Other lines update body-link activity and pump the queue before the serial loop consumes more buffered frames. |
| Body-link WiFi | UDP in `BodyLinkWiFi.h` | `marcduinoIngressAdmit(kMarcduinoIngressBodyLinkWifi, cmd)` | Heartbeat `#PAHB` is consumed by the transport and not admitted. UDP payload lines are capped at 64 bytes. |
| WiFi Marcduino | `WifiMarcduinoReceiver` callback | `marcduinoIngressAdmit(kMarcduinoIngressWifiMarcduino, cmd)` | Optional serial pass-through is controlled by `MARC_WIFI_SERIAL_PASS`. |
| I2C slave | `I2CReceiverBase` callback when `USE_I2C_ADDRESS` is enabled | `marcduinoIngressAdmit(kMarcduinoIngressI2CSlave, cmd)` | Logs the received frame before admission. This build mode disables servo support. |
| Internal dome sequence queue | `DomeSequences.h` | `enqueueMarcduinoCommand` | Used to avoid re-entrant `Marcduino::processCommand()` from sequence callbacks. |
| Legacy Marcduino serial parser | `MarcduinoSerial` when body-link is disabled | ReelTwo stream handler | Bypasses the fork ingress functions and is left out of scope unless the stream callback is explicitly rewired. |

## Admission Matrix

| Behavior | REST API / WebSocket | USB serial / WiFi Marcduino / I2C | Body-link UART / WiFi | Notes |
| --- | --- | --- | --- | --- |
| Sleep gate | Yes | Yes | Yes | Wake profile commands (`:SE11`, `:SE13`, `:SE14`, `#PAWU`), emergency `:SE00`, and `:CL00` pass during soft sleep. `/api/cmd` additionally returns HTTP 423 before admission. |
| Source logging | Logs `[CMD][source] command` before immediate handling | Same | Same | Queue dispatch later logs `[CMD][source][dispatch] command`. |
| Immediate servo move (`:SM`) | Runs immediately, bypasses queue | Same | Same | Invalid args are consumed and logged as `SM-invalid` or `SM-bad-slot`. |
| Visual preset (`DV:*`) | Runs immediately, bypasses queue | Same | Same | Unknown presets are consumed, increment unknown telemetry, and do not reach Marcduino handlers. |
| Visual authoring (`DL:`, `DT:`, `DH:`) | Runs immediately, bypasses queue | Same | Same | Rejected authoring commands are still consumed and counted so they do not fall through to legacy handlers. |
| Panel calibration (`:MV`, `#SO`, `#SC`, `#SW`) | Runs synchronously before queueing | Same | Same | Shared ingress keeps all transports safe from deferred `getCommand()` suffix parsing. |
| Queue admission | Enqueues after immediate handlers | Same | Same | Queue depth is 8 entries. Commands are copied with `strlcpy` into `CONSOLE_BUFFER_SIZE`. Long commands truncate silently at the queue buffer boundary. |
| Queue-full behavior | Drops command, increments `sMarcduinoQueueFullCount`, logs `[queue-full]` | Same | Same | Caller does not get a failure response today. |
| Mood reset dedupe (`:SE10`, `:SE11`, `:SE13`, `:SE14`) | Applied before queueing | Same | Same | Duplicate is dropped when the same mood command repeats within 2500 ms. |
| Body-link echo suppression | Not applied | Not applied | Queue item sets `suppressBodyLinkEgress` | Suppression is computed from source metadata. While dispatching a suppressed queue item, `sendBodyCommand()` logs and does not forward back to the active body-link transport. |
| Body-link heartbeat handling | N/A | N/A | Transport-local | `#PAHB` is not a Marcduino command. Dome heartbeat `#APHB` is emitted from heartbeat handling, not command admission. |

## UART Burst Backpressure

The body-link UART reader drains the shared Marcduino queue after every complete
non-heartbeat frame. This keeps all body-origin commands on the same ingress
path as the rest of the firmware, including source logging and echo suppression,
but prevents dense choreographies from filling the eight-entry queue during one
`handleBodySerial()` pass. Heartbeat frames remain transport-local and do not
pump command dispatch.

## Intentional Differences

- `/api/cmd` returning HTTP 423 while sleeping is intentional. WebSocket and
  transport callers only get log output because they do not have the same
  request/response contract.
- Body-link-origin commands intentionally suppress body-link egress while their
  Marcduino handlers run. This prevents command echo loops between body and
  dome.
- Body-link heartbeat frames intentionally stay in the transport adapter and do
  not enter Marcduino command admission.
- WiFi Marcduino serial pass-through intentionally remains a transport option
  outside local command admission.
- The legacy `MarcduinoSerial` stream path is intentionally separate when
  body-link is disabled. It is ReelTwo's parser path, not the fork's async
  transport layer.

## Drift Removed By Shared Ingress

- Mood reset dedupe is common policy across all `marcduinoIngressAdmit()`
  callers.
- Panel calibration commands are synchronous before queueing across all ingress
  sources.
- Body-link echo suppression is computed from `MarcduinoIngressSource` metadata,
  not raw source strings.
- Sleep gate, logging, immediate command handling, visual command handling, and
  queue admission live in `MarcduinoIngress.h`.

## Behavior Lock For Refactor

- Source labels in logs should stay recognizable:
  `astropixel-web-api`, `astropixel-web-ws`, `usb-serial`, `body-link-uart`,
  `body-link-wifi`, `wifi-marcduino`, and `i2c-slave`.
- Marcduino domain handlers remain compatibility-owned. Refactoring ingress must
  not rewrite the `@`, `:`, `*`, or `#AP` handler catalog.
- Queue capacity, queue truncation, and queue-full logging stay unchanged unless
  a separate behavior issue changes them.
- Async-web calibration commands must stay lifetime-safe after the refactor.

## Slice 3 Refactor Outcome

- Mood reset dedupe is now shared across REST, WebSocket, USB serial, body-link
  UART/WiFi, WiFi Marcduino, and I2C ingress. Repeating the same mood reset
  command within 2500 ms is dropped before queueing for every source.
- Panel calibration commands (`:MV`, `#SO`, `#SC`, `#SW`) are handled
  synchronously before queueing for every ingress source. The legacy
  `MARCDUINO_ACTION` handlers remain in `MarcduinoPanel.h` as compatibility
  fallbacks, but unified ingress does not rely on deferred suffix parsing.
- Queue capacity remains eight entries, queued commands are still copied with
  `strlcpy` into `CONSOLE_BUFFER_SIZE`, and queue-full behavior still logs and
  drops without surfacing a caller error.
