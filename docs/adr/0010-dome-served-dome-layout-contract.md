# ADR 0010 — Dome-served dome layout model contract

**Status:** Draft  
**Date:** 2026-06-29

## Context

The protoArtoo body editor currently uses a vendored MK4 dome panel picker. That
copy can drift from AstroPixelsPlus and cannot know which panels are actually
wired on the connected dome. The long-term contract is that the dome reports the
layout model at runtime and the body editor renders from that model when
connected.

AstroPixelsPlus already exposes `/api/panels/config`, which reports panel slot
wiring state. That endpoint is a wiring-configuration API, not the editor-facing
dome layout model: it does not define geometry, model revision, panel kind,
fixed dome features, or the stable contract for rendering a picker.

The current firmware and UI are prepared around the Mr Baddeley Complex Dome
MK4 layout. That layout should be treated as the bundled reference template and
the most common community standard, not as the only possible dome shape. Builders
may customize MK4 domes or build other astromech dome types. The model contract
should leave room for user-defined layouts that can be exported, shared, and
eventually contributed back as reusable templates.

The template versioning model must also support future layout/design iterations
from Mr Baddeley or other recognized astromech designers without conflating
those visual/design revisions with API schema compatibility.

The backend command surface and canonical panel identities intentionally follow
broader astromech builder terminology, including the Printed Droid R2-D2
terminology references for dome panel and projector names. AstroPixelsPlus uses
those names as the supported backend identity set for the first template system.
Some projects, including AstroPixels/AstroPixelsPlus, use local names for the
same physical features. The layout contract should expose canonical community
identities and project-local aliases where they differ, rather than forcing one
vocabulary to erase the other.

## Responsibility split

Most implementation work for this contract lives in the AstroPixelsPlus dome
fork, but the contract only succeeds if protoArtoo aligns its coordinator and
sequence editor around the same boundary.

### AstroPixelsPlus / dome owns

- The `/api/dome/layout` composed read model.
- The bundled MK4 dome layout template and future exported/contributed template
  assets under `templates/dome-layouts/`.
- Template schema and validation for known identities, duplicate IDs,
  `in_layout`, geometry types, mounted relationships, labels, aliases, callouts,
  and render order.
- Generated firmware layout data (`GeneratedDomeLayout.h`) and the generator /
  drift-check tooling that keeps it in sync with the JSON template.
- Runtime `active` state as seen by the booted dome firmware.
- Operator-maintained element suppression via `/api/dome/element-status`.
- Keeping raw Marcduino compatibility intact: suppression/status does not block
  manual Marcduino command handlers in v1.
- Documenting the shipped behavior in `FORK_IMPROVEMENTS.md`.

### protoArtoo / body owns

- Fetching `/api/dome/layout` when connected and rendering the sequence/editor
  picker from `elements[]`.
- Keeping the vendored MK4 model as an offline/fallback visual model only.
- Caching geometry/layout by `template_id`, `template_revision`, and
  `schema_revision`; runtime availability from cache is stale/unverified unless
  refreshed by a live layout fetch.
- Treating unsupported schemas, unreachable domes, and unknown runtime state
  conservatively.
- The coordinator command map: translating canonical element IDs plus generic
  capabilities into actual body/dome command behavior.
- Sequence editor behavior for inactive, disabled, or `in_layout:false` targets:
  visible diagnostics, non-actionable controls, load/edit/save resilience, and
  run-time skip/block policy.
- Preserving the accepted protoArtoo learned-sequence storage posture for v1:
  saved sequence steps remain command-string based unless a separate body ADR
  deliberately supersedes that decision.
- Proxying the dome layout as a thin byte relay for browser/editor consumers;
  body firmware does not parse or recompose the full layout JSON.

### Shared contract boundary

`/api/dome/layout` describes layout identity and high-level usability. It does
not expose low-level backend details such as Marcduino command strings, servo
slots, PCA9685 channels, SPI chains, or hardware bus addresses. Those stay in
AstroPixelsPlus subsystem-specific config/diagnostics or in protoArtoo's
coordinator mapping as appropriate.

## Decision

Add a dome-owned editor layout endpoint:

```text
GET /api/dome/layout
```

This supersedes the earlier draft endpoint name `/api/panels/model`. The
contract is a full dome layout, not a panel-only model, so the primary endpoint
should not be panel-scoped. A future `/api/panels/model` alias or filtered view
can be added only if there is a concrete compatibility need.

The endpoint returns structured dome element metadata and structured geometry.
The body editor should render the dome picker/layout from this data rather than
scraping or embedding the dome UI's `panels.html` SVG. Commandable panels are
one element type within the layout, not the entire layout contract.

Custom templates are initially display templates only. They may provide alternate
geometry and labels for known AstroPixelsPlus/community layout identities, but
they do not define new command mappings, new firmware slots, or new backend
behavior. Those remain owned by the firmware command surface until community
demand justifies a broader extension.

V1 templates must only reference known astromech/MK4 backend layout identities
from the supported identity set:

```text
P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14
PP1, PP2, PP3, PP4, PP5, PP6
HP1, HP2, HP3
RLD, FLD, RPSI, FPSI
MP
```

Unknown backend/layout IDs are rejected by template validation. Duplicate IDs are
rejected. A known ID with alternate geometry or label text is allowed. Missing
known IDs are allowed only if the template explicitly marks the identity outside
the selected layout.

For backend-known identities that firmware or protoArtoo may reason about, the
template must explicitly include or exclude the identity. Purely decorative or
layout-only elements that are not tied to backend command/status semantics can
be more flexible.

Templates may explicitly mark a known identity as outside the selected layout:

```json
{ "id": "PP5", "in_layout": false, "label": "PP5" }
```

Silent omission of a known identity is a validation failure. This keeps accidental
template drift visible while still supporting custom domes, repair variants, and
maker builds where a dome section was assembled or glued in an order that does
not fully match the community MK4 complex dome layout.

Reusable templates should live as versioned JSON assets in the repository,
separate from C++ firmware constants, under:

```text
templates/dome-layouts/
```

Firmware bundles one selected/default template, initially
`mr-baddeley-complex-dome-mk4`, and `/api/dome/layout` serves that template
composed with local runtime state.

The firmware endpoint must not depend on parsing a SPIFFS template JSON file at
request time. `/api/dome/layout` is firmware behavior and should keep working
even when web assets are stale or have not been uploaded. The practical v1 path
is to compose the response in C++ from firmware-owned runtime usability state
plus a compact bundled geometry table. The repository JSON template can later be
used to generate that table or checked against it.

The bundled firmware geometry table should be generated from the repository JSON
template from day one. The reviewable JSON template is the source of truth; a
small generator emits a firmware header/table suitable for serving
`/api/dome/layout` without runtime JSON parsing.

Template validation has two modes: general v1 validation for reviewable display
templates, and strict bundled-template validation for the firmware-selected MK4
template. Future community templates should be able to pass the general v1
contract without pretending to be `mr-baddeley-complex-dome-mk4`; firmware
generation remains pinned to the selected bundled template until runtime
template selection is deliberately implemented.

`schema-v1.json` is an authoring aid for the single-template JSON shape. The
Python validator/check tooling is the canonical review gate for v1 because it
also enforces cross-element invariants such as duplicate IDs, full known-ID
inventory, mounted relationships, commandable capability rules, finite numbers,
and coordinate guard bands.

The generated firmware header/table should be committed. PlatformIO builds stay
simple and deterministic, reviewers can inspect the exact compiled representation,
and a local/CI check verifies that generated output matches the JSON source.

Initial file layout:

```text
templates/dome-layouts/mr-baddeley-complex-dome-mk4.json
templates/dome-layouts/schema-v1.json
templates/dome-layouts/README.md
tools/generate_dome_layout_header.py
tools/check_dome_layout_generated.py
tools/validate_dome_layout_templates.py
tools/test_dome_layout_validation.py
tools/render_dome_layout_preview.py
tools/test_dome_layout_preview.py
GeneratedDomeLayout.h
```

`GeneratedDomeLayout.h` lives at the repository root beside the existing
firmware headers so `AsyncWebInterface.h` or a small route helper can include it
without changing the project's include layout.

Operator-maintained suppression state lives in a separate runtime-editable
status endpoint:

```text
GET  /api/dome/element-status
POST /api/dome/element-status
```

Example:

```json
{
  "elements": [
    {
      "id": "PP3",
      "disabled": true,
      "disabled_reason": "Upper pie linkage binding"
    }
  ]
}
```

`/api/dome/layout` composes this status into each matching element. The endpoint
is dome-scoped rather than panel-scoped so the same mechanism can later suppress
or flag holos, logic displays, PSI elements, or other dome layout elements.
Status changes persist across reboot but take effect immediately; they do not
require the wiring-config reboot cycle.

`/api/dome/layout` is the composed read model for external consumers such as
protoArtoo. Consumers should not need to fetch the template, wiring config, and
element status separately to determine what to render and what is usable. The
dome firmware composes template data, live wiring/runtime state, and persisted
element status into the layout response.

protoArtoo consumes element status through the composed `/api/dome/layout`
response in v1. It should not make a separate `/api/dome/element-status` fetch
for editor rendering; that endpoint is dome-owned for local operator status
editing and for the dome's own composition path.

Template/layout editing is distinct from operator status editing. V1 includes
runtime editing for element status (`disabled` / `disabled_reason`) because that
is local operational state. Editing geometry, labels, label anchors, callouts, or
selected layout templates is a template-authoring feature. It may be added if a
small and safe editing workflow is available, but it should not be conflated with
the core read-model endpoint.

Implementation slices:

- **V1:** bundled MK4 template, generated firmware table, `/api/dome/layout`,
  `/api/dome/element-status`, validation/check tooling, protoArtoo consumption.
- **V1.5:** visual/template authoring outside firmware, likely in protoArtoo or
  tooling, with JSON export for review and GitHub contribution.
- **V2:** dome-side custom template upload/selection, with validation, preview,
  persistence, and guaranteed rollback to bundled MK4.

For v1, element suppression is an editor/automation safety signal, not a raw
Marcduino command interlock. A disabled panel is excluded from body-editor
automation, but the firmware continues to accept compatible manual Marcduino
commands such as `:OPP3`.

Community template sharing should begin with exportable JSON files that can be
submitted by GitHub PR, reviewed, and added to a template folder with a stable
`template_id`, revision, and preview/documentation. The repository preview tool
renders review SVGs from the same canonical Python validation path used by the
firmware generator, but it validates in general template-review mode so valid
non-MK4 layouts are not rejected for lacking the bundled firmware identity. The
firmware command surface continues to define which panel identities and commands
are actually supported.

The first contract uses SVG path geometry per panel, in a declared coordinate
space:

The following JSON is explanatory. The current literal v1 field vocabulary is
defined by `templates/dome-layouts/schema-v1.json` and enforced by
`tools/validate_dome_layout_templates.py`.

```json
{
  "model": "MK4",
  "template_id": "mr-baddeley-complex-dome-mk4",
  "template_name": "Mr Baddeley Complex Dome MK4",
  "template_revision": 1,
  "schema_revision": 1,
  "source": "AstroPixelsPlus",
  "coordinate_space": { "viewBox": "0 0 480 480" },
  "runtime_state_ts": "2026-06-30T12:00:00Z",
  "elements": [
    {
      "id": "PP3",
      "label": "PP3",
      "element_type": "panel",
      "panel_kind": "pie",
      "render_order": 120,
      "label_anchor": { "x": 296.0, "y": 216.0 },
      "commandable": true,
      "capabilities": ["open", "close", "flutter"],
      "active": false,
      "disabled": false,
      "disabled_reason": null,
      "geometry": {
        "type": "svg_path",
        "d": "..."
      }
    },
    {
      "id": "HP1",
      "label": "Front Holo Projector",
      "element_type": "holo_projector",
      "aliases": ["FHP"],
      "render_order": 220,
      "commandable": true,
      "capabilities": ["light", "aim", "center", "test"],
      "geometry": {
        "type": "svg_path",
        "d": "..."
      }
    },
    {
      "id": "RPSI",
      "label": "Rear PSI",
      "element_type": "psi_indicator",
      "mounted_on": "P8",
      "render_order": 230,
      "label_anchor": { "x": 309.3, "y": 38.6 },
      "callout": {
        "x": 309.3,
        "y": 38.6,
        "connector_to": { "x": 301.2, "y": 62.2 }
      },
      "commandable": true,
      "capabilities": ["effect"],
      "geometry": {
        "type": "point",
        "cx": 309.3,
        "cy": 38.6,
        "r": 6
      }
    }
  ]
}
```

Geometry is interaction-grade, not CAD-grade: it must be accurate enough for
visual picking, highlighting, and layout recognition, but it does not claim
manufacturing tolerances. It also must not be overly crude. The existing
hand-tuned `data/panels.html` SVG is the rough v1 quality baseline for MK4 and
for reviewing future generated or contributed template geometry.

The v1 MK4 template inventory should mirror the current `data/panels.html`
layout baseline:

```text
P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14
PP1, PP2, PP3, PP4, PP5, PP6
HP1/FHP, HP2/RHP, HP3/THP
RLD, FLD, RPSI, FPSI
MP / Magic Panel where represented by the merged P6/MP/P5 structure
```

Mounted features such as RLD, FLD, RPSI, and FPSI may be modeled as
features associated with their host fixed panels (`P9`, `P12`, `P8`, `P14`)
rather than as independent commandable panels. The current MK4 SVG's combined
`P6 / MP / P5` region is acceptable for v1 as long as the individual identities
remain represented in labels/metadata.

Merged fixed regions should not collapse identities out of the model. For v1,
`P6`, `MP`, and `P5` are discrete elements with `commandable:false`; they may
share the same underlying geometry region or use separate point/label anchors
when the source SVG represents them as a combined physical structure.

Mounted features should be first-class elements with a relationship to their
host element via `mounted_on`, not hidden only inside the host panel label. This
lets UI/search/diagnostics highlight features such as RPSI, FPSI, RLD, FLD,
and HP3 while still preserving the physical host relationship.

`FLD` is the canonical front-logic identity. Labels may use "Front Logic
Displays" and aliases may include `FLDs`, `top_fld`, and `bottom_fld` when a UI
or implementation needs to acknowledge the split top/bottom physical displays.
The v1 layout treats `FLD` as the aggregate mounted feature on `P12`; optional
child elements for top/bottom FLD can be added later if editor workflows require
per-display geometry.

V1 geometry types are intentionally limited:

```json
{ "type": "svg_path", "d": "..." }
{ "type": "circle", "cx": 320.4, "cy": 172.5, "r": 6 }
{ "type": "ellipse", "cx": 240, "cy": 81, "rx": 14.1, "ry": 10.9, "rotation": 0 }
{ "type": "point", "cx": 309.3, "cy": 38.6, "r": 6 }
```

Arbitrary SVG fragments are out of scope for v1. Templates should remain
structured, validated, and renderable by the body editor without DOM scraping or
opaque SVG interpretation.

For `point` geometry, renderers use a default marker and hit radius of `6` in
template coordinate-space units when `r` is omitted. Templates may override the
default with an explicit `r`.

`render_order` controls drawing/z-order. It is meaningful for rendering only and
does not define domain semantics. Semantics come from fields such as `id`,
`element_type`, `mounted_on`, `commandable`, `active`, and
`disabled`.

Templates may include optional `label_anchor` and `callout` metadata. These are
presentation hints, not domain semantics. They preserve hand-tuned label and
callout placement from the current MK4 SVG while still allowing the body editor
to render in its own visual style.

Templates should not carry CSS classes, fill colors, stroke colors, or
UI-specific styling names in v1. Styling is derived by consumers from semantic
fields such as `element_type`, `panel_kind`, `commandable`, `active`,
`disabled`, and `in_layout`. Only layout-quality presentation hints such as
`render_order`, `label_anchor`, and `callout` belong in the template.

For commandable panel elements, `active` means physically commandable in the
current booted runtime, not merely configured in NVS. If `/api/panels/config` is
changed by POST and the firmware requires a reboot before that wiring is applied,
`/api/dome/layout` continues to report the pre-reboot runtime state until the
reboot occurs. For commandable non-panel elements, `active` means currently
usable by that subsystem. It may be omitted when the dome cannot provide a useful
runtime fact; consumers must treat missing runtime availability conservatively.

The layout model does not expose low-level backend details such as Marcduino
command strings, servo slots, PCA9685 channels, SPI chains, or hardware bus
addresses. Those belong in subsystem-specific configuration, diagnostics, and
command documentation. `/api/dome/layout` exposes the visible layout plus
high-level usability fields such as `commandable`, `capabilities`, `active`,
`disabled`, and `in_layout`.

protoArtoo's coordinator owns command mapping responsibility. It should consume
layout identities and high-level usability/capability signals from
`/api/dome/layout`, then resolve those identities to body-side commands through
its own coordinator layer. AstroPixelsPlus should not duplicate that command map
inside the layout contract.

`template_revision` tracks changes to a specific layout template's geometry,
labels, or presentation metadata. `schema_revision` tracks changes to the JSON
contract. A template can evolve without changing the API schema, and the schema
can evolve without implying that the MK4 geometry changed.

`id` is the stable canonical community/backend identity. `aliases` lists
project-local or alternate community names that refer to the same physical
element. For example, the layout may expose HP1/HP2/HP3 as canonical holo
projector identities while also listing AstroPixels names such as FHP/RHP/THP.
`label` is template-owned presentation text and may vary by template or future
localization layer. Runtime layout joins and editor view-model references should
use `id`, not `label`.

Holo projector canonical IDs follow the clearer Printed Droid/current UI pattern:
`HP1`, `HP2`, and `HP3`. AstroPixels names are aliases: `FHP`, `RHP`, and `THP`.

For commandable non-panel elements, the layout exposes high-level capabilities
as descriptive/context metadata. It does not enumerate the full Marcduino command
catalog for those subsystems. Detailed command behavior remains in command
documentation, existing protoArtoo controls, or future subsystem-specific
metadata endpoints.

Capabilities are generic coordinator-facing action tags, not AstroPixels command
names. For v1, only panel capabilities `open`, `close`, and `flutter` are
layout-picker actionable. Non-panel capabilities such as `light`, `aim`,
`center`, `test`, `display_text`, and `effect` are descriptive/context-only in
the layout picker; holo/logic/PSI authoring remains in existing protoArtoo
`DH:`/`DL:`/`DT:` controls unless a future body ADR expands picker authoring.

For v1, protoArtoo should treat `schema_revision` strictly. It supports
`schema_revision:1`; any other value is unsupported until explicitly handled.
Unknown optional fields within a supported schema may be ignored. A newer
`template_revision` is acceptable as long as `schema_revision` is supported.
Body-side caches should key durable geometry/template data by `template_id`,
`template_revision`, and `schema_revision`. Runtime fields such as `active` and
`disabled` are point-in-time facts and are trusted only from a fresh live fetch;
when served from cache they must be marked stale/unverified. The composed
response includes `runtime_state_ts` so consumers can reason about when runtime
state was produced.

For v1, `/api/dome/layout` returns the full layout JSON inline. Splitting
geometry into secondary assets, ETags, conditional requests, or runtime revision
tokens can wait until response size or fetch frequency proves it is needed.

Body-side fallback order:

1. If the connected dome serves `/api/dome/layout` with a supported
   `schema_revision`, use live geometry and live runtime availability.
2. If the live fetch fails but browser/local cache has a previously fetched
   layout with supported `schema_revision`, reuse cached geometry and mark all
   runtime availability stale/unverified.
3. If there is no usable cached live layout, use the vendored MK4 fallback layout
   and mark runtime availability unverified.
4. If the connected dome serves an unsupported `schema_revision`, do not
   partially trust it; use fallback and show an unsupported-schema warning.

Fallback visuals are acceptable. Fallback automation must be conservative:
unknown `active` state is not safe, and unknown pie/upper-panel state should not
be assumed safe for body-authored sequences.

Older endpoints such as `/api/panels/config` are not part of the v1 canonical
fallback hierarchy. Hydrating layout availability from old endpoints would be a
separate compatibility extension, not the default contract.

If an element is `in_layout:true` but inactive or disabled, the body editor keeps
it visible in the layout and styles it as unavailable. Action controls are hidden
or disabled. Unavailable elements must not break sequence verification, sequence
editing, sequence saving, or sequence execution; they are treated as unavailable
targets rather than schema errors.

Saved protoArtoo sequences remain loadable and editable when a referenced
element later becomes inactive, disabled, or excluded from the selected layout.
Verification should surface the affected steps as warnings or errors according
to coordinator policy, but saving should preserve authored intent unless the
operator explicitly removes or remaps the step. Running a sequence must skip or
block unsafe unavailable actions according to coordinator policy, never crash the
sequence runner.

For v1, saved protoArtoo sequence actions continue to persist the accepted
command-string shape, for example `{ "type": "dome", "cmd": ":OP01" }`. The
layout picker may operate in canonical element IDs and generic capabilities while
the user is editing, but the protoArtoo coordinator resolves that transient
`element_id + capability` selection to the existing command-string storage shape
before save/run. A future structured saved-step format requires a separate
protoArtoo ADR that explicitly supersedes its current learned-sequence storage
decision.

Because saved steps remain command-string based, protoArtoo also needs a reverse
map from command strings back to canonical layout IDs for editor highlighting and
diagnostics. The reverse map must key on command form, not aliases: numeric
targets such as `:OP01` resolve to ring panels such as `P1`, while pie-family
targets such as `:OPP1` resolve to `PP1`. The `P1` token in `:OPP1` is a command
target token, not the canonical ring-panel ID `P1`.

`disabled` is an operator-maintained suppression flag sourced from
`/api/dome/element-status`. It means the element may be wired and physically
commandable, but should be highlighted as unavailable and excluded from
body-side editing/automation because the builder knows it has a mechanical or
commissioning problem. `disabled_reason` is optional operator text for the UI.

Inactive commandable panels still appear in the model. For example, PP3 and PP5
appear on a standard MK4 with `active:false` and become `active:true` only when
the builder has wired and activated those identities and the firmware runtime has
loaded that configuration.

Sequence usability is derived by protoArtoo's coordinator from layout/runtime
state, not stored as a separate dome-provided status. For v1, a sequence target
is usable only when the element is in the selected layout, commandable, active,
not disabled, and supports the requested capability. If any of those facts are
unknown or false, the coordinator treats the action conservatively.

The coordinator rule is intentionally simple:

```text
usable_for_sequence =
  in_layout == true &&
  commandable == true &&
  active == true &&
  disabled != true &&
  requested_capability in capabilities
```

`in_layout:false` panel elements are known backend identities that the selected
template intentionally does not expose in the main visual picker. They remain in
the model data for diagnostics and configuration warnings. They are not usable
sequence targets. If wiring config reports an `in_layout:false` identity as
active, the body editor should warn that the firmware has an active commandable
identity for a panel the selected layout excludes.

`in_layout:false` elements remain in `/api/dome/layout` but do not require
geometry. The main picker hides them; diagnostics/settings may show them as known
backend identities excluded by the selected layout.

## Consequences

- ProtoArtoo can render, select, disable, annotate, and reason about dome elements from
  stable domain objects instead of DOM conventions in an opaque SVG.
- AstroPixelsPlus owns the geometry and identity contract, so the body editor no
  longer needs a primary vendored copy of the dome SVG.
- The body editor still needs an offline fallback model for cases where the dome
  is unavailable.
- The model endpoint must stay synchronized with the supported identity mapping
  from `servoSettings[]`, `DomeSequences.h`, and the accepted panel ADRs, without
  leaking low-level slots/channels/commands into the layout contract.
- The model needs a way to represent operator-known mechanical faults. This is
  distinct from wiring config: a disabled panel can remain wired and active at
  the firmware level while the editor treats it as unavailable.
- Runtime suppression/status lives behind `/api/dome/element-status`, avoiding
  wiring-config reboot semantics and keeping the mechanism extensible beyond
  panels.
- Raw Marcduino compatibility remains intact in v1; suppression does not block
  low-level command handlers.
- This is a major fork improvement and must be documented in
  `FORK_IMPROVEMENTS.md` when implementation lands.
- MK4 becomes a first bundled template, not a hardcoded ceiling. Future custom
  dome layouts should be represented as saved/exportable dome layout templates
  using the same schema where practical.
- The first custom-template slice is intentionally conservative: geometry and
  labels are portable, command semantics are not.
- Cache invalidation and compatibility checks can distinguish layout changes
  from API contract changes.
- Template assets can be reviewed and shared without editing firmware behavior.
  The selected template can later become configurable without changing the body
  editor's consumption path.
- Template validation prevents the body editor from drawing panels that the dome
  firmware cannot command.
- Explicit `in_layout:false` lets custom or imperfect physical builds describe
  intentional absence/mismatch without hiding accidental template mistakes.
- The main picker can hide absent identities while diagnostics still make backend
  mismatches visible.
- Alias metadata lets the UI show or search by both broader astromech terminology
  and project-local AstroPixels names.
- Labels travel with templates as default human-facing presentation text, while
  IDs remain stable logic keys.
- Non-panel layout elements can advertise broad capabilities without turning the
  layout endpoint into a full command reference.
- Body fallback behavior is deterministic when a connected dome serves an
  unsupported layout schema.
- The offline MK4 model remains useful as a visual fallback, but it does not
  grant safety to unknown runtime state.
- The GitHub issue and implementation plan should use `/api/dome/layout` as the
  primary contract name rather than the earlier `/api/panels/model` draft.
- The first implementation may duplicate geometry between a reviewable template
  JSON asset and a firmware table/header, but the firmware table/header should
  be generated from the JSON source of truth.

## References

- Printed Droid, "R2-D2 Terminology": https://www.printed-droid.com/kb/r2-d2-terminology/
- Printed Droid, "R2-D2 Terminology v1.2 2020-01" PDF: https://www.printed-droid.com/wp-content/uploads/2020/01/R2-D2-Terminology-v1.2-2020-01.pdf
