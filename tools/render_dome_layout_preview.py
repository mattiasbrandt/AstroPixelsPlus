#!/usr/bin/env python3
"""Render a review SVG from a validated dome layout template."""

from __future__ import annotations

import argparse
import html
import json
import sys
from pathlib import Path
from typing import Any

from generate_dome_layout_header import (
    DEFAULT_TEMPLATE,
    ValidationError,
    load_and_validate,
)


SEMANTIC_CLASS_BY_TYPE = {
    "holo": "holo",
    "logic": "logic",
    "panel": "panel",
    "psi": "psi",
}


def parse_view_box(value: str) -> tuple[float, float, float, float]:
    parts = value.split()
    if len(parts) != 4:
        raise ValueError(f"unsupported viewBox {value!r}")
    return tuple(float(part) for part in parts)  # type: ignore[return-value]


def fmt_number(value: float | int) -> str:
    number = float(value)
    if number.is_integer():
        return str(int(number))
    return f"{number:.3f}".rstrip("0").rstrip(".")


def svg_attr(value: Any) -> str:
    return html.escape(str(value), quote=True)


def svg_text(value: Any) -> str:
    return html.escape(str(value), quote=False)


def class_for_element(element: dict[str, Any]) -> str:
    classes = ["element", SEMANTIC_CLASS_BY_TYPE[element["element_type"]]]
    panel_kind = element.get("panel_kind")
    if panel_kind:
        classes.append(panel_kind)
    if element["commandable"]:
        classes.append("commandable")
    if not element["in_layout"]:
        classes.append("excluded")
    return " ".join(classes)


def render_geometry(element: dict[str, Any]) -> str:
    geometry = element["geometry"]
    title = svg_attr(
        f"{element['id']} {element['element_type']}"
        + (f" {element['panel_kind']}" if element.get("panel_kind") else "")
    )
    attrs = (
        f'class="{svg_attr(class_for_element(element))}" '
        f'data-element-id="{svg_attr(element["id"])}"'
    )
    if geometry["type"] == "svg_path":
        return f'<path {attrs} d="{svg_attr(geometry["d"])}"><title>{title}</title></path>'
    if geometry["type"] == "circle":
        return (
            f'<circle {attrs} cx="{fmt_number(geometry["cx"])}" '
            f'cy="{fmt_number(geometry["cy"])}" r="{fmt_number(geometry["r"])}">'
            f"<title>{title}</title></circle>"
        )
    if geometry["type"] == "ellipse":
        transform = ""
        if geometry["rotation"]:
            transform = (
                f' transform="rotate({fmt_number(geometry["rotation"])} '
                f'{fmt_number(geometry["cx"])} {fmt_number(geometry["cy"])})"'
            )
        return (
            f'<ellipse {attrs} cx="{fmt_number(geometry["cx"])}" '
            f'cy="{fmt_number(geometry["cy"])}" rx="{fmt_number(geometry["rx"])}" '
            f'ry="{fmt_number(geometry["ry"])}"{transform}>'
            f"<title>{title}</title></ellipse>"
        )
    return (
        f'<circle {attrs} cx="{fmt_number(geometry["cx"])}" '
        f'cy="{fmt_number(geometry["cy"])}" r="{fmt_number(geometry["r"])}">'
        f"<title>{title}</title></circle>"
    )


def render_callout(element: dict[str, Any]) -> list[str]:
    callout = element.get("callout")
    if not callout:
        return []
    lines: list[str] = []
    connector = callout.get("connector_to")
    if connector:
        lines.append(
            '<line class="callout-line" '
            f'x1="{fmt_number(callout["x"])}" y1="{fmt_number(callout["y"])}" '
            f'x2="{fmt_number(connector["x"])}" y2="{fmt_number(connector["y"])}" />'
        )
    lines.append(
        '<circle class="callout-ring" '
        f'cx="{fmt_number(callout["x"])}" cy="{fmt_number(callout["y"])}" '
        f'r="{fmt_number(callout["r"])}" />'
    )
    return lines


def render_label(element: dict[str, Any]) -> str:
    anchor = element["label_anchor"]
    return (
        '<text class="label" '
        f'data-label-for="{svg_attr(element["id"])}" '
        f'x="{fmt_number(anchor["x"])}" y="{fmt_number(anchor["y"])}">'
        f"{svg_text(element['label'])}</text>"
    )


def render_legend(x: float, y: float) -> list[str]:
    items = [
        ("panel commandable", "legend-commandable"),
        ("fixed panel", "legend-fixed"),
        ("holo", "legend-holo"),
        ("logic", "legend-logic"),
        ("PSI", "legend-psi"),
    ]
    lines = [f'<g class="legend" transform="translate({fmt_number(x)} {fmt_number(y)})">']
    for index, (label, css_class) in enumerate(items):
        row_y = index * 16
        lines.append(
            f'<rect class="{css_class}" x="0" y="{row_y}" width="12" height="10" rx="1" />'
        )
        lines.append(f'<text x="18" y="{row_y + 9}">{svg_text(label)}</text>')
    lines.append("</g>")
    return lines


def render_preview(template: dict[str, Any]) -> str:
    min_x, min_y, width, height = parse_view_box(template["coordinate_space"]["viewBox"])
    elements = [element for element in template["elements"] if element["in_layout"]]
    excluded = [element["id"] for element in template["elements"] if not element["in_layout"]]

    rendered_width = width + 170
    rendered_view_box = f"{fmt_number(min_x)} {fmt_number(min_y)} {fmt_number(rendered_width)} {fmt_number(height)}"
    center_x = min_x + width / 2
    center_y = min_y + height / 2
    dome_radius = min(width, height) * 0.43

    lines: list[str] = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        (
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{rendered_view_box}" '
            'role="img" aria-labelledby="preview-title preview-desc">'
        ),
        f'<title id="preview-title">{svg_text(template["template_name"])} layout preview</title>',
        (
            f'<desc id="preview-desc">Template {svg_text(template["template_id"])} '
            f'revision {template["template_revision"]}; '
            f'{len(elements)} rendered elements, {len(excluded)} excluded.</desc>'
        ),
        "<style>",
        "svg { background: #f8fafc; color: #172033; font-family: Arial, sans-serif; }",
        ".dome-outline { fill: #eef2f5; stroke: #9aa6b2; stroke-width: 2; }",
        ".element { stroke: #263240; stroke-width: 1.5; }",
        ".panel.ring.commandable, .panel.pie.commandable { fill: #f2c879; }",
        ".panel.fixed { fill: #c7ced6; }",
        ".holo { fill: #78b7d8; }",
        ".logic { fill: #8ec58f; }",
        ".psi { fill: #d8878c; }",
        ".callout-line { stroke: #53606d; stroke-width: 1; stroke-dasharray: 3 3; }",
        ".callout-ring { fill: none; stroke: #53606d; stroke-width: 1.2; }",
        ".label { fill: #172033; font-size: 11px; font-weight: 700; text-anchor: middle; dominant-baseline: central; }",
        ".meta { fill: #263240; font-size: 11px; }",
        ".meta-title { font-size: 13px; font-weight: 700; }",
        ".legend text { fill: #263240; font-size: 10px; dominant-baseline: central; }",
        ".legend rect { stroke: #263240; stroke-width: 1; }",
        ".legend-commandable { fill: #f2c879; }",
        ".legend-fixed { fill: #c7ced6; }",
        ".legend-holo { fill: #78b7d8; }",
        ".legend-logic { fill: #8ec58f; }",
        ".legend-psi { fill: #d8878c; }",
        "</style>",
        f'<circle class="dome-outline" cx="{fmt_number(center_x)}" cy="{fmt_number(center_y)}" r="{fmt_number(dome_radius)}" />',
        '<g id="geometry">',
    ]
    lines.extend(render_geometry(element) for element in elements)
    lines.append("</g>")

    lines.append('<g id="callouts">')
    for element in elements:
        lines.extend(render_callout(element))
    lines.append("</g>")

    lines.append('<g id="labels">')
    lines.extend(render_label(element) for element in elements)
    lines.append("</g>")

    meta_x = min_x + width + 20
    lines.extend(
        [
            f'<g class="meta" transform="translate({fmt_number(meta_x)} {fmt_number(min_y + 24)})">',
            f'<text class="meta-title" x="0" y="0">{svg_text(template["template_name"])}</text>',
            f'<text x="0" y="18">id: {svg_text(template["template_id"])}</text>',
            f'<text x="0" y="34">template rev: {template["template_revision"]}</text>',
            f'<text x="0" y="50">schema rev: {template["schema_revision"]}</text>',
            f'<text x="0" y="66">rendered: {len(elements)}</text>',
            f'<text x="0" y="82">excluded: {len(excluded)}</text>',
            "</g>",
        ]
    )
    lines.extend(render_legend(meta_x, min_y + 136))
    if excluded:
        lines.extend(
            [
                f'<g class="meta" transform="translate({fmt_number(meta_x)} {fmt_number(min_y + 235)})">',
                '<text class="meta-title" x="0" y="0">Excluded identities</text>',
                f'<text x="0" y="18">{svg_text(", ".join(excluded))}</text>',
                "</g>",
            ]
        )
    lines.append("</svg>")
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render a visual SVG preview from a validated dome layout template."
    )
    parser.add_argument(
        "--template",
        type=Path,
        default=DEFAULT_TEMPLATE,
        help="Template JSON file to preview. Defaults to the bundled MK4 template.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="SVG output file. Omit or use '-' to write to stdout.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        template = load_and_validate(args.template)
        rendered = render_preview(template)
        if args.output and str(args.output) != "-":
            args.output.write_text(rendered, encoding="utf-8")
            print(f"rendered {args.output}")
        else:
            print(rendered, end="")
        return 0
    except FileNotFoundError as exc:
        print(f"missing file: {exc.filename}", file=sys.stderr)
        return 2
    except (json.JSONDecodeError, ValidationError, ValueError) as exc:
        print(f"layout preview failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
