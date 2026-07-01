#!/usr/bin/env python3
"""Generate the firmware dome layout table from the reviewed JSON template."""

from __future__ import annotations

import argparse
import difflib
import json
import math
import re
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEMPLATE = ROOT / "templates/dome-layouts/mr-baddeley-complex-dome-mk4.json"
DEFAULT_OUTPUT = ROOT / "GeneratedDomeLayout.h"
DEFAULT_TEMPLATE_ID = "mr-baddeley-complex-dome-mk4"
DEFAULT_TEMPLATE_NAME = "Mr Baddeley Complex Dome MK4"
DEFAULT_TEMPLATE_REVISION = 1

KNOWN_IDS = {
    "P1",
    "P2",
    "P3",
    "P4",
    "P5",
    "P6",
    "P7",
    "P8",
    "P9",
    "P10",
    "P11",
    "P12",
    "P13",
    "P14",
    "PP1",
    "PP2",
    "PP3",
    "PP4",
    "PP5",
    "PP6",
    "HP1",
    "HP2",
    "HP3",
    "RLD",
    "FLD",
    "RPSI",
    "FPSI",
    "MP",
}
PANEL_KINDS = {"ring", "pie", "fixed"}
ELEMENT_TYPES = {"panel", "holo", "logic", "psi"}
CAPABILITIES = {
    "open",
    "close",
    "flutter",
    "light",
    "aim",
    "center",
    "test",
    "display_text",
    "effect",
}
PANEL_ACTION_CAPABILITIES = ["open", "close", "flutter"]
GEOMETRY_TYPES = {"svg_path", "circle", "ellipse", "point"}
PATH_COMMAND_RE = re.compile(r"[AaCcHhLlMmQqSsTtVvZz]")


class ValidationError(Exception):
    """Raised when the layout template violates the v1 contract."""


def fail(message: str) -> None:
    raise ValidationError(message)


def require_dict(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        fail(f"{context} must be an object")
    return value


def require_list(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        fail(f"{context} must be an array")
    return value


def require_string(value: Any, context: str, *, allow_empty: bool = False) -> str:
    if not isinstance(value, str):
        fail(f"{context} must be a string")
    if not allow_empty and not value:
        fail(f"{context} must not be empty")
    return value


def require_bool(value: Any, context: str) -> bool:
    if not isinstance(value, bool):
        fail(f"{context} must be a boolean")
    return value


def require_int(value: Any, context: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        fail(f"{context} must be an integer")
    return value


def require_number(value: Any, context: str) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        fail(f"{context} must be a number")
    number = float(value)
    if not math.isfinite(number):
        fail(f"{context} must be finite")
    return number


def optional_string(value: Any, context: str) -> str | None:
    if value is None:
        return None
    return require_string(value, context)


def validate_number_in_space(value: Any, context: str) -> float:
    number = require_number(value, context)
    if number < -32.0 or number > 512.0:
        fail(f"{context}={number:g} is outside the 480-space guard band")
    return number


def validate_point(value: Any, context: str) -> dict[str, float]:
    point = require_dict(value, context)
    extra = sorted(set(point) - {"x", "y"})
    if extra:
        fail(f"{context} has unknown keys: {', '.join(extra)}")
    return {
        "x": validate_number_in_space(point.get("x"), f"{context}.x"),
        "y": validate_number_in_space(point.get("y"), f"{context}.y"),
    }


def validate_callout(value: Any, context: str) -> dict[str, Any]:
    callout = require_dict(value, context)
    extra = sorted(set(callout) - {"x", "y", "r", "connector_to"})
    if extra:
        fail(f"{context} has unknown keys: {', '.join(extra)}")
    result: dict[str, Any] = {
        "x": validate_number_in_space(callout.get("x"), f"{context}.x"),
        "y": validate_number_in_space(callout.get("y"), f"{context}.y"),
        "r": require_number(callout.get("r"), f"{context}.r"),
    }
    if result["r"] <= 0:
        fail(f"{context}.r must be positive")
    if "connector_to" in callout:
        result["connector_to"] = validate_point(
            callout["connector_to"], f"{context}.connector_to"
        )
    return result


def validate_geometry(value: Any, context: str) -> dict[str, Any]:
    geometry = require_dict(value, context)
    geometry_type = require_string(geometry.get("type"), f"{context}.type")
    if geometry_type not in GEOMETRY_TYPES:
        fail(f"{context}.type={geometry_type!r} is not supported")

    if geometry_type == "svg_path":
        expected = {"type", "d"}
        path = require_string(geometry.get("d"), f"{context}.d")
        if not PATH_COMMAND_RE.search(path):
            fail(f"{context}.d does not look like SVG path data")
        result: dict[str, Any] = {"type": geometry_type, "d": path}
    elif geometry_type == "circle":
        expected = {"type", "cx", "cy", "r"}
        radius = require_number(geometry.get("r"), f"{context}.r")
        if radius <= 0:
            fail(f"{context}.r must be positive")
        result = {
            "type": geometry_type,
            "cx": validate_number_in_space(geometry.get("cx"), f"{context}.cx"),
            "cy": validate_number_in_space(geometry.get("cy"), f"{context}.cy"),
            "r": radius,
        }
    elif geometry_type == "ellipse":
        expected = {"type", "cx", "cy", "rx", "ry", "rotation"}
        rx = require_number(geometry.get("rx"), f"{context}.rx")
        ry = require_number(geometry.get("ry"), f"{context}.ry")
        if rx <= 0 or ry <= 0:
            fail(f"{context}.rx and {context}.ry must be positive")
        result = {
            "type": geometry_type,
            "cx": validate_number_in_space(geometry.get("cx"), f"{context}.cx"),
            "cy": validate_number_in_space(geometry.get("cy"), f"{context}.cy"),
            "rx": rx,
            "ry": ry,
            "rotation": require_number(geometry.get("rotation", 0), f"{context}.rotation"),
        }
    else:
        expected = {"type", "cx", "cy", "r"}
        # A point uses the ADR default marker radius of 6 unless the template opts in.
        radius = require_number(geometry.get("r", 6), f"{context}.r")
        if radius <= 0:
            fail(f"{context}.r must be positive")
        result = {
            "type": geometry_type,
            "cx": validate_number_in_space(geometry.get("cx"), f"{context}.cx"),
            "cy": validate_number_in_space(geometry.get("cy"), f"{context}.cy"),
            "r": radius,
        }

    extra = sorted(set(geometry) - expected)
    if extra:
        fail(f"{context} has unknown keys: {', '.join(extra)}")
    return result


def validate_template(
    data: Any, *, enforce_default_identity: bool = False
) -> dict[str, Any]:
    template = require_dict(data, "template")
    expected_top = {
        "schema_revision",
        "template_id",
        "template_name",
        "template_revision",
        "model",
        "source",
        "coordinate_space",
        "elements",
    }
    extra_top = sorted(set(template) - expected_top)
    if extra_top:
        fail(f"template has unknown keys: {', '.join(extra_top)}")

    if require_int(template.get("schema_revision"), "schema_revision") != 1:
        fail("schema_revision must be 1")
    template_id = require_string(template.get("template_id"), "template_id")
    template_name = require_string(template.get("template_name"), "template_name")
    template_revision = require_int(template.get("template_revision"), "template_revision")
    if template_revision < 1:
        fail("template_revision must be positive")
    if enforce_default_identity:
        if template_id != DEFAULT_TEMPLATE_ID:
            fail(f"template_id must be {DEFAULT_TEMPLATE_ID}")
        if template_name != DEFAULT_TEMPLATE_NAME:
            fail(f"template_name must be {DEFAULT_TEMPLATE_NAME}")
        if template_revision != DEFAULT_TEMPLATE_REVISION:
            fail(f"template_revision must be {DEFAULT_TEMPLATE_REVISION}")

    coordinate_space = require_dict(template.get("coordinate_space"), "coordinate_space")
    if coordinate_space != {"viewBox": "0 0 480 480"}:
        fail('coordinate_space must be exactly {"viewBox": "0 0 480 480"}')

    elements = require_list(template.get("elements"), "elements")
    seen: set[str] = set()
    normalized_elements: list[dict[str, Any]] = []

    for index, raw_element in enumerate(elements):
        context = f"elements[{index}]"
        element = require_dict(raw_element, context)
        expected_element = {
            "id",
            "label",
            "aliases",
            "in_layout",
            "element_type",
            "panel_kind",
            "mounted_on",
            "commandable",
            "capabilities",
            "render_order",
            "geometry",
            "label_anchor",
            "callout",
        }
        extra_element = sorted(set(element) - expected_element)
        if extra_element:
            fail(f"{context} has unknown keys: {', '.join(extra_element)}")
        common_required = {
            "id",
            "label",
            "aliases",
            "in_layout",
            "element_type",
            "panel_kind",
            "mounted_on",
            "commandable",
            "capabilities",
            "render_order",
        }
        missing_element = sorted(common_required - set(element))
        if missing_element:
            fail(f"{context} is missing required keys: {', '.join(missing_element)}")

        element_id = require_string(element.get("id"), f"{context}.id")
        if element_id not in KNOWN_IDS:
            fail(f"{context}.id={element_id!r} is not in the v1 known identity set")
        if element_id in seen:
            fail(f"duplicate element id {element_id}")
        seen.add(element_id)

        label = require_string(element.get("label"), f"{context}.label")
        aliases = [
            require_string(alias, f"{context}.aliases[{alias_index}]")
            for alias_index, alias in enumerate(
                require_list(element.get("aliases"), f"{context}.aliases")
            )
        ]
        if len(aliases) != len(set(aliases)):
            fail(f"{context}.aliases contains duplicates")

        in_layout = require_bool(element.get("in_layout"), f"{context}.in_layout")
        element_type = require_string(element.get("element_type"), f"{context}.element_type")
        if element_type not in ELEMENT_TYPES:
            fail(f"{context}.element_type={element_type!r} is not supported")

        panel_kind = optional_string(element.get("panel_kind"), f"{context}.panel_kind")
        if element_type == "panel":
            if panel_kind not in PANEL_KINDS:
                fail(f"{context}.panel_kind is required for panel elements")
        elif panel_kind is not None:
            fail(f"{context}.panel_kind must be null for non-panel elements")

        mounted_on = optional_string(element.get("mounted_on"), f"{context}.mounted_on")
        commandable = require_bool(element.get("commandable"), f"{context}.commandable")
        capabilities = [
            require_string(capability, f"{context}.capabilities[{capability_index}]")
            for capability_index, capability in enumerate(
                require_list(element.get("capabilities"), f"{context}.capabilities")
            )
        ]
        unknown_caps = sorted(set(capabilities) - CAPABILITIES)
        if unknown_caps:
            fail(f"{context}.capabilities has unknown values: {', '.join(unknown_caps)}")
        if len(capabilities) != len(set(capabilities)):
            fail(f"{context}.capabilities contains duplicates")
        if commandable:
            if element_type != "panel" or panel_kind not in {"ring", "pie"}:
                fail(f"{context}.commandable is only valid for ring/pie panels in v1")
            if capabilities != PANEL_ACTION_CAPABILITIES:
                fail(
                    f"{context}.capabilities must be "
                    f"{PANEL_ACTION_CAPABILITIES!r} for commandable panels"
                )
        else:
            illegal = sorted(set(capabilities) & set(PANEL_ACTION_CAPABILITIES))
            if illegal:
                fail(
                    f"{context}.capabilities has movement capabilities on a "
                    f"non-commandable element: {', '.join(illegal)}"
                )

        render_order = require_int(element.get("render_order"), f"{context}.render_order")
        if render_order < 0:
            fail(f"{context}.render_order must be non-negative")

        normalized: dict[str, Any] = {
            "id": element_id,
            "label": label,
            "aliases": aliases,
            "in_layout": in_layout,
            "element_type": element_type,
            "panel_kind": panel_kind,
            "mounted_on": mounted_on,
            "commandable": commandable,
            "capabilities": capabilities,
            "render_order": render_order,
        }
        if in_layout:
            missing_render = sorted({"geometry", "label_anchor"} - set(element))
            if missing_render:
                fail(f"{context} is missing required render keys: {', '.join(missing_render)}")
            normalized["geometry"] = validate_geometry(
                element.get("geometry"), f"{context}.geometry"
            )
            normalized["label_anchor"] = validate_point(
                element.get("label_anchor"), f"{context}.label_anchor"
            )
            if "callout" in element:
                normalized["callout"] = validate_callout(
                    element["callout"], f"{context}.callout"
                )
        elif "geometry" in element or "label_anchor" in element or "callout" in element:
            fail(f"{context} is outside the layout and must not define render geometry")

        normalized_elements.append(normalized)

    missing_ids = sorted(KNOWN_IDS - seen)
    if missing_ids:
        fail(f"template is missing explicit known identities: {', '.join(missing_ids)}")

    known_in_layout = {element["id"] for element in normalized_elements if element["in_layout"]}
    for element in normalized_elements:
        mounted_on = element["mounted_on"]
        if mounted_on is not None:
            if mounted_on not in known_in_layout:
                fail(f"{element['id']}.mounted_on references non-rendered element {mounted_on}")
            if mounted_on == element["id"]:
                fail(f"{element['id']}.mounted_on cannot reference itself")

    normalized_template = dict(template)
    normalized_template["elements"] = sorted(
        normalized_elements, key=lambda item: (item["render_order"], item["id"])
    )
    return normalized_template


def cpp_string(value: str | None) -> str:
    if value is None:
        return "nullptr"
    return json.dumps(value)


def cpp_bool(value: bool) -> str:
    return "true" if value else "false"


def cpp_float(value: float | int | None) -> str:
    if value is None:
        return "0.0f"
    literal = f"{float(value):.6g}"
    if "." not in literal and "e" not in literal and "E" not in literal:
        literal += ".0"
    return f"{literal}f"


def symbol_suffix(element_id: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", element_id)


def enum_name(geometry_type: str) -> str:
    return {
        "svg_path": "DomeLayoutGeometryType::SvgPath",
        "circle": "DomeLayoutGeometryType::Circle",
        "ellipse": "DomeLayoutGeometryType::Ellipse",
        "point": "DomeLayoutGeometryType::Point",
    }[geometry_type]


def emit_string_array(lines: list[str], name: str, values: list[str]) -> str:
    if not values:
        return "nullptr"
    lines.append(f"static constexpr const char *const {name}[] = {{")
    for value in values:
        lines.append(f"    {cpp_string(value)},")
    lines.append("};")
    lines.append("")
    return name


def render_header(template: dict[str, Any]) -> str:
    lines: list[str] = [
        "#pragma once",
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "// Generated by tools/generate_dome_layout_header.py.",
        "// Edit templates/dome-layouts/mr-baddeley-complex-dome-mk4.json instead.",
        "",
        "namespace DomeLayout {",
        "",
        f"static constexpr int kSchemaRevision = {template['schema_revision']};",
        f"static constexpr int kTemplateRevision = {template['template_revision']};",
        f"static constexpr const char *kTemplateId = {cpp_string(template['template_id'])};",
        f"static constexpr const char *kTemplateName = {cpp_string(template['template_name'])};",
        f"static constexpr const char *kModel = {cpp_string(template['model'])};",
        f"static constexpr const char *kSource = {cpp_string(template['source'])};",
        'static constexpr const char *kCoordinateSpaceViewBox = "0 0 480 480";',
        "static constexpr float kDefaultPointMarkerRadius = 6.0f;",
        "",
        "enum class DomeLayoutGeometryType {",
        "    SvgPath,",
        "    Circle,",
        "    Ellipse,",
        "    Point,",
        "};",
        "",
        "struct DomeLayoutPoint {",
        "    bool present;",
        "    float x;",
        "    float y;",
        "};",
        "",
        "struct DomeLayoutCallout {",
        "    bool present;",
        "    float x;",
        "    float y;",
        "    float r;",
        "    bool connectorPresent;",
        "    float connectorX;",
        "    float connectorY;",
        "};",
        "",
        "struct DomeLayoutElement {",
        "    const char *id;",
        "    const char *label;",
        "    const char *elementType;",
        "    const char *panelKind;",
        "    const char *mountedOn;",
        "    bool inLayout;",
        "    bool commandable;",
        "    const char *const *aliases;",
        "    size_t aliasCount;",
        "    const char *const *capabilities;",
        "    size_t capabilityCount;",
        "    int renderOrder;",
        "    DomeLayoutGeometryType geometryType;",
        "    const char *svgPath;",
        "    float cx;",
        "    float cy;",
        "    float r;",
        "    float rx;",
        "    float ry;",
        "    float rotation;",
        "    DomeLayoutPoint labelAnchor;",
        "    DomeLayoutCallout callout;",
        "};",
        "",
    ]

    for element in template["elements"]:
        suffix = symbol_suffix(element["id"])
        emit_string_array(lines, f"kAliases_{suffix}", element["aliases"])
        emit_string_array(lines, f"kCapabilities_{suffix}", element["capabilities"])

    lines.extend(
        [
            "static constexpr DomeLayoutElement kElements[] = {",
        ]
    )

    for element in template["elements"]:
        suffix = symbol_suffix(element["id"])
        aliases_name = f"kAliases_{suffix}" if element["aliases"] else "nullptr"
        capabilities_name = (
            f"kCapabilities_{suffix}" if element["capabilities"] else "nullptr"
        )
        geometry = element.get("geometry", {"type": "point", "cx": 0, "cy": 0, "r": 0})
        label_anchor = element.get("label_anchor", {"x": 0, "y": 0})
        callout = element.get("callout")
        connector = callout.get("connector_to") if callout else None
        lines.extend(
            [
                "    {",
                f"        {cpp_string(element['id'])},",
                f"        {cpp_string(element['label'])},",
                f"        {cpp_string(element['element_type'])},",
                f"        {cpp_string(element['panel_kind'])},",
                f"        {cpp_string(element['mounted_on'])},",
                f"        {cpp_bool(element['in_layout'])},",
                f"        {cpp_bool(element['commandable'])},",
                f"        {aliases_name},",
                f"        {len(element['aliases'])},",
                f"        {capabilities_name},",
                f"        {len(element['capabilities'])},",
                f"        {element['render_order']},",
                f"        {enum_name(geometry['type'])},",
                f"        {cpp_string(geometry.get('d'))},",
                f"        {cpp_float(geometry.get('cx'))},",
                f"        {cpp_float(geometry.get('cy'))},",
                f"        {cpp_float(geometry.get('r'))},",
                f"        {cpp_float(geometry.get('rx'))},",
                f"        {cpp_float(geometry.get('ry'))},",
                f"        {cpp_float(geometry.get('rotation'))},",
                f"        {{ {cpp_bool(element['in_layout'])}, {cpp_float(label_anchor['x'])}, {cpp_float(label_anchor['y'])} }},",
            ]
        )
        if callout:
            lines.append(
                "        { "
                f"true, {cpp_float(callout['x'])}, {cpp_float(callout['y'])}, "
                f"{cpp_float(callout['r'])}, {cpp_bool(connector is not None)}, "
                f"{cpp_float(connector['x'] if connector else None)}, "
                f"{cpp_float(connector['y'] if connector else None)} "
                "},"
            )
        else:
            lines.append("        { false, 0.0f, 0.0f, 0.0f, false, 0.0f, 0.0f },")
        lines.extend(["    },", ""])

    lines.extend(
        [
            "};",
            "",
            "static constexpr size_t kElementCount = sizeof(kElements) / sizeof(kElements[0]);",
            "",
            "}  // namespace DomeLayout",
            "",
        ]
    )
    return "\n".join(lines)


def load_and_validate(
    template_path: Path, *, enforce_default_identity: bool = False
) -> dict[str, Any]:
    with template_path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    return validate_template(data, enforce_default_identity=enforce_default_identity)


def load_and_render(template_path: Path) -> str:
    template = load_and_validate(template_path, enforce_default_identity=True)
    return render_header(template)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate GeneratedDomeLayout.h from the MK4 dome layout template"
    )
    parser.add_argument("--template", type=Path, default=DEFAULT_TEMPLATE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit non-zero if the output file is not up to date",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        rendered = load_and_render(args.template)
        if args.check:
            existing = args.output.read_text(encoding="utf-8")
            if existing != rendered:
                diff = difflib.unified_diff(
                    existing.splitlines(),
                    rendered.splitlines(),
                    fromfile=str(args.output),
                    tofile="generated",
                    lineterm="",
                )
                print("\n".join(diff))
                print(
                    f"{args.output} is stale; run "
                    f"tools/generate_dome_layout_header.py",
                    file=sys.stderr,
                )
                return 1
            print(f"{args.output} is up to date")
            return 0
        args.output.write_text(rendered, encoding="utf-8")
        print(f"generated {args.output}")
        return 0
    except FileNotFoundError as exc:
        print(f"missing file: {exc.filename}", file=sys.stderr)
        return 2
    except (json.JSONDecodeError, ValidationError) as exc:
        print(f"layout generation failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
