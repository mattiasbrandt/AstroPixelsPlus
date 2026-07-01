# Dome Layout Templates

This folder holds reviewable JSON source templates for the dome layout contract.
The firmware currently bundles `mr-baddeley-complex-dome-mk4.json` by generating
`GeneratedDomeLayout.h` from it, but additional templates can be reviewed here
before any runtime upload/selection feature exists.

## V1 Policy

V1 templates are display templates only:

- They may change geometry, labels, aliases, label anchors, callouts, render
  order, and `in_layout` membership.
- They must use the known v1 identity set from `schema-v1.json`.
- They must explicitly include or exclude every known identity.
- They must not define commands, command targets, slots, channels, buses, SPI
  chains, PCA9685 details, or firmware behavior.
- They must keep commandable movement limited to ring/pie panels with
  `open`, `close`, and `flutter`.
- They must keep non-panel capabilities descriptive/context-only.

The selected firmware template is still the bundled MK4 template. Runtime
template upload/selection is intentionally deferred until a later slice with
validation, preview, persistence, and rollback.

## Validate Templates

`schema-v1.json` is useful for editor hints and basic shape validation, but the
Python validator is the canonical review gate. It enforces cross-element rules
that JSON Schema does not express here, including duplicate IDs, full known-ID
inventory, mounted relationships, commandable capability policy, finite numeric
coordinates, and the 480-space guard band.

Validate every reviewable template in this folder:

```bash
python3 tools/validate_dome_layout_templates.py
```

Validate a specific candidate template:

```bash
python3 tools/validate_dome_layout_templates.py templates/dome-layouts/my-layout.json
```

Check that the generated firmware header still matches the bundled MK4 template:

```bash
python3 tools/check_dome_layout_generated.py
```

Use strict bundled validation only for the template compiled into firmware:

```bash
python3 tools/validate_dome_layout_templates.py --strict-bundled templates/dome-layouts/mr-baddeley-complex-dome-mk4.json
```

`tools/generate_dome_layout_header.py` is not the community-template validator.
It intentionally rejects non-MK4 template identity because firmware generation
is still pinned to the bundled MK4 template.

## Review Checklist

- `template_id` is stable, lowercase/kebab-case, and not reused for a different
  physical layout.
- `template_revision` increments when geometry, labels, anchors, callouts, or
  layout membership changes.
- `coordinate_space.viewBox` remains `0 0 480 480` for schema v1.
- Every known identity is present exactly once.
- Identities outside the selected physical layout use `in_layout:false` and omit
  render geometry.
- Shared regions such as `P6`/`MP`/`P5` remain discrete elements even if they
  share geometry or callouts.
- Mounted features use `mounted_on` instead of being hidden only in host labels.
- No template field leaks command strings, slots, channels, or backend hardware
  details.
