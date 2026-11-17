# Coding Standards (review rules)

Rules a reviewer applies to a diff. Mechanical checks live in `make test`
(command routing, dispatch, layout drift); these are the judgement calls those
tests cannot make.

## Command dispatch

A change that touches how commands reach handlers (`MarcduinoIngress.h`,
`Marcduino::processCommand()` callers, `player`, `dome_pendingAnim`, the
legacy `MarcduinoSerial` setup) states its effect on each of these, in the PR
or commit message, and adds a scenario to
`tools/test_marcduino_dispatch_host.py` for any ordering it changes:

- **Pre-emption:** the shared `player` holds one animation; a newly dispatched
  command ends a running one (its `DO_RESET` runs).
- **Dome sequences:** `DM:*` handlers only set `dome_pendingAnim`;
  `dome_seqRunning` is set by the sequence's first step and cleared by its
  `DO_RESET`.
- **Legacy serial:** `MarcduinoSerial` bypasses ingress and dispatches one line
  per tick from its own buffer.
- **Echo suppression:** `sSuppressBodyLinkEgress` covers scheduling only, not
  handler bodies (see `docs/MARCDUINO_INGRESS.md`, *Open Question*).

## Honest responses

A route or UI control reports success only for an action that can happen in
this build. A route for a compiled-out feature is not registered (wrap it in
the same `#if AP_ENABLE_*` as the feature) rather than answering `{"ok":true}`.

## Library behavior

A claim about ReelTwo behavior in code comments, docs or a PR cites the file in
`.pio/libdeps/astropixelsplus/Reeltwo/src` it was read from. ReelTwo gadget
and holo classes take `CommandEvent` tokens, not Marcduino commands: they are
reached through the `@AP` / `@HP` forwarding handlers or `DO_COMMAND`, and
their numeric arguments use the class's own encoding (e.g. `selectEffect`).
