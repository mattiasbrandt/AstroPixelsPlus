# ADR 0005 — No write-endpoint authentication

**Status:** Accepted  
**Date:** 2026-05-25

## Context

The firmware previously included a `checkWriteAuth()` guard on all POST/upload
endpoints. It read an `apitoken` string from NVS; if the NVS key was absent or
empty (the default, and in practice always the case), it returned `true`
unconditionally and logged a one-time warning. In effect, the guard was never
active — and the WebSocket command path bypassed it entirely regardless.

AstroPixelsPlus is a home-LAN device. It runs on a private WiFi network inside
a builder's workshop or event venue. The ESP32 AsyncWebServer does not support
TLS, so any token sent over HTTP would travel in cleartext on the same LAN
anyway. Operators and builders using this firmware do not configure API tokens
and have no expectation of write-protection on local network endpoints.

## Decision

The `checkWriteAuth()` function, `authWarningLogged` flag, and `apitoken` NVS
key are **deleted** from the firmware and all UI surfaces. All write endpoints
are open to any client on the same local network. No authentication layer is
added as a replacement.

## Consequences

- POST and upload endpoints accept requests from any host on the LAN without
  a token or header. This is the intended operational model for a home robot.
- The `apitoken` NVS key no longer exists; any value previously stored under
  that key in an existing install is ignored (Preferences silently returns the
  default for unknown keys).
- Future contributors must not reintroduce per-endpoint token checks unless the
  device gains TLS support — a plaintext token provides no meaningful security
  and adds user-visible configuration complexity with no benefit.
- If network-level access control is wanted, it belongs at the WiFi/router layer
  (e.g. isolate the AP SSID or use a VLAN), not in the firmware.
