# REST API Integration Guide

This document provides practical examples for integrating AstroPixelsPlus with home automation systems via REST API.

## Overview

AstroPixelsPlus exposes a REST API for controlling dome functions programmatically.

**Base URL:** `http://<astropixels-ip>/api/`

```bash
curl http://192.168.4.1/api/state
```

---

## Core Control Endpoints

### System State

#### GET /api/state

Returns current system state including panel positions, active effects, and system status.

**Use Case:** Monitor current dome state before sending commands

```bash
curl http://192.168.1.100/api/state
```

**Response:**
```json
{
  "panels": {...},
  "holos": {...},
  "logics": {...},
  "system": {...}
}
```

---

### Command Execution

#### POST /api/cmd

Execute any Marcduino command via HTTP POST.

**Parameters:**
- `cmd` - The Marcduino command string

**Use Cases:**

```bash
# Open all panels
curl -X POST http://192.168.1.100/api/cmd \
  -d "cmd=:OP00"

# Close all panels
curl -X POST http://192.168.1.100/api/cmd \
  -d "cmd=:CL00"

# Trigger scream sequence
curl -X POST http://192.168.1.100/api/cmd \
  -d "cmd=:SE01"

# Set logics to red alert
curl -X POST http://192.168.1.100/api/cmd \
  -d "cmd=@0T5"

# All logics normal
curl -X POST http://192.168.1.100/api/cmd \
  -d "cmd=@0T1"
```

---

### Sleep Mode Control

#### POST /api/sleep

Enter sleep mode (quiet state with minimal activity).

**Use Case:** Put R2 in quiet mode when not on display

```bash
# Enter sleep mode
curl -X POST http://192.168.1.100/api/sleep
```

#### POST /api/wake

Wake from sleep mode and restore active state.

**Use Case:** Activate R2 for display or interaction

```bash
# Wake up
curl -X POST http://192.168.1.100/api/wake
```

---

### Health & Diagnostics

#### GET /api/health

Quick health check with system status.

**Use Case:** Monitoring system health in automation dashboards

```bash
curl http://192.168.1.100/api/health
```

**Response:**
```json
{
  "status": "ok",
  "free_heap": 45000,
  "min_free_heap": 42000,
  "wifi_rssi": -45,
  "i2c_errors": 0
}
```

#### GET /api/diag/i2c

I2C bus diagnostics and device scan.

```bash
# Quick scan
curl http://192.168.1.100/api/diag/i2c

# Force full scan
curl http://192.168.1.100/api/diag/i2c?force=1
```

---

## Dome Layout & Element Status

### GET /api/dome/layout

Returns the editor-facing dome layout model composed from the selected dome
layout template, live runtime state, and persisted operator element status. The
selected template is either the immutable bundled MK4 fallback or an installed
custom display template.

```bash
curl http://192.168.1.100/api/dome/layout
```

The response includes stable element IDs, labels, aliases, geometry, render
order, commandability, panel kind, and advisory availability fields. It does not
include Marcduino command strings, servo slots, PCA9685 channels, or hardware bus
details.

```json
{
  "schema_revision": 1,
  "template_id": "mr-baddeley-complex-dome-mk4",
  "template_revision": 1,
  "layout_source": "bundled",
  "coordinate_space": { "viewBox": "0 0 480 480" },
  "runtime_state_ts": 123456,
  "elements": [
    {
      "id": "PP3",
      "element_type": "panel",
      "panel_kind": "pie",
      "commandable": true,
      "active": false,
      "disabled": true,
      "disabled_reason": "Upper pie linkage binding"
    }
  ]
}
```

### GET /api/dome/layout-template

Returns the current template selection and installed custom-template status.

```bash
curl http://192.168.1.100/api/dome/layout-template
```

### POST /api/dome/layout-template

Installs a validated custom display template JSON into SPIFFS and selects it.
The firmware rejects templates that omit known identities, use an unsupported
schema, define runtime fields, or leak backend details such as command strings,
slots, channels, buses, or targets. The bundled MK4 template remains available
for rollback.

```bash
curl -X POST http://192.168.1.100/api/dome/layout-template \
  -H "Content-Type: application/json" \
  --data-binary @templates/dome-layouts/my-layout.json
```

### POST /api/dome/layout-template/select

Switches between the installed custom template and bundled MK4.

```bash
curl -X POST http://192.168.1.100/api/dome/layout-template/select \
  -H "Content-Type: application/json" \
  -d '{"source":"bundled"}'
```

### DELETE /api/dome/layout-template

Deletes the installed custom template and rolls `/api/dome/layout` back to the
bundled MK4 template.

```bash
curl -X DELETE http://192.168.1.100/api/dome/layout-template
```

### GET /api/dome/element-status

Returns persisted operator status for every known layout element.

```bash
curl http://192.168.1.100/api/dome/element-status
```

### POST /api/dome/element-status

Persists advisory disabled flags and optional short reasons. Status changes take
effect immediately and survive reboot, but they do not block raw Marcduino
commands.

```bash
curl -X POST http://192.168.1.100/api/dome/element-status \
  -H "Content-Type: application/json" \
  -d '{"elements":[{"id":"PP3","disabled":true,"disabled_reason":"Upper pie linkage binding"}]}'
```

To clear a disabled flag:

```bash
curl -X POST http://192.168.1.100/api/dome/element-status \
  -H "Content-Type: application/json" \
  -d '{"elements":[{"id":"PP3","disabled":false}]}'
```

---

## Sequence & Effect Control

### Triggering Sequences

Use `/api/cmd` endpoint with sequence commands:

```bash
# Scream sequence
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE01"

# Wave sequence
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE02"

# Quiet mode (sleep)
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE10"

# Awake mode
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE11"

# March sequence
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE51"
```

### Common Sequences

| Command | Effect |
|---------|--------|
| `:SE00` | Stop all sequences |
| `:SE01` | Scream |
| `:SE02` | Wave |
| `:SE10` | Quiet mode (sound stopped, holos idle, no panel motion) |
| `:SE11` | Full awake |
| `:SE51` | Panel march |

---

## Light & Display Control

### Logic Displays

```bash
# All logics normal
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T1"

# All logics red alert
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T5"

# All logics flash
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T2"

# Lights out
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T15"
```

### Holo Projectors

```bash
# All holos on
curl -X POST http://192.168.1.100/api/cmd -d "cmd=*ON00"

# All holos off
curl -X POST http://192.168.1.100/api/cmd -d "cmd=*OF00"

# Reset holos
curl -X POST http://192.168.1.100/api/cmd -d "cmd=*ST00"
```

---

## Panel Control

### Basic Panel Commands

```bash
# Open all panels
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:OP00"

# Close all panels
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:CL00"

# Flutter all panels
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:OF00"

# Open specific panel group
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:OP01"
```

### Panel Sequences

```bash
# Marching ants effect
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE55"

# Wave goodbye
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE58"
```

---

## Automation Scenarios

### Scene: R2 Active Display

```bash
# Wake up
curl -X POST http://192.168.1.100/api/wake

# Open panels with march sequence
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:SE51"

# Logics normal
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T1"
```

### Scene: R2 Quiet/Standby

```bash
# Enter sleep mode
curl -X POST http://192.168.1.100/api/sleep

# Or manually:
# Close panels
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:CL00"
# Lights out
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T15"
```

### Scene: Photo Opportunity

```bash
# Open panels
curl -X POST http://192.168.1.100/api/cmd -d "cmd=:OP00"

# Logics red alert for drama
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T5"

# Or flash mode for attention
curl -X POST http://192.168.1.100/api/cmd -d "cmd=@0T2"
```

---

## Integration Examples

### Home Assistant RESTful Command

```yaml
# configuration.yaml
rest_command:
  r2_wake:
    url: "http://192.168.1.100/api/wake"
    method: POST
    
  r2_sleep:
    url: "http://192.168.1.100/api/sleep"
    method: POST
    
  r2_cmd:
    url: "http://192.168.1.100/api/cmd"
    method: POST
    payload: 'cmd={{ command }}'
    content_type: "application/x-www-form-urlencoded"
```

### Node-RED Flow

```javascript
// HTTP Request node
// Method: POST
// URL: http://192.168.1.100/api/cmd
// Payload: cmd=:OP00
```

### Generic HTTP Client

All examples use standard HTTP POST with form-encoded data:

```
Content-Type: application/x-www-form-urlencoded

Body: cmd=:OP00
```

---

## Error Handling

**Common Responses:**

```json
// Success
{"ok":true}

// Sleep mode blocked command
{"error":"sleeping","hint":"POST /api/wake"}

// Invalid command (no effect, no error for silent failures)
```

---

## Notes

- Commands are processed asynchronously
- Panel movements take time (100-500ms depending on easing)
- Sleep mode blocks most commands (except wake)
- All commands support the standard Marcduino syntax
- For complete command reference, see [COMMANDS.md](./COMMANDS.md)

---

## Rate Limiting

No explicit rate limiting, but avoid:
- Sending commands more than every 100ms
- Rapidly toggling sleep/wake
- Multiple simultaneous panel sequences

---

## Security

- HTTPS not supported on ESP32 AsyncWebServer; this API is intended for home LAN use only
- Default WiFi password should be changed

---

*For hardware wiring and setup, see [SETUP.md](./SETUP.md) and [HARDWARE_WIRING.md](./HARDWARE_WIRING.md)*
