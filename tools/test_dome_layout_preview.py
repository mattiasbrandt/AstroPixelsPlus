#!/usr/bin/env python3
"""Regression tests for dome layout template SVG preview rendering."""

from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path

from generate_dome_layout_header import validate_template
from render_dome_layout_preview import render_preview


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE_PATH = ROOT / "templates/dome-layouts/mr-baddeley-complex-dome-mk4.json"


def clone_template() -> dict:
    with TEMPLATE_PATH.open("r", encoding="utf-8") as handle:
        return copy.deepcopy(json.load(handle))


def element_by_id(template: dict, element_id: str) -> dict:
    for element in template["elements"]:
        if element["id"] == element_id:
            return element
    raise AssertionError(f"missing test fixture element {element_id}")


class DomeLayoutPreviewTests(unittest.TestCase):
    def test_preview_renders_bundled_template_metadata_and_elements(self) -> None:
        template = validate_template(clone_template())

        svg = render_preview(template)

        self.assertIn('<svg xmlns="http://www.w3.org/2000/svg"', svg)
        self.assertIn("Mr Baddeley Complex Dome MK4 layout preview", svg)
        self.assertIn('data-element-id="PP3"', svg)
        self.assertIn('data-label-for="RPSI"', svg)
        self.assertIn("rendered: 28", svg)
        self.assertNotIn("Marcduino", svg)

    def test_preview_supports_general_non_mk4_templates(self) -> None:
        template = clone_template()
        template["template_id"] = "community-preview-layout"
        template["template_name"] = "Community Preview Layout"
        template["template_revision"] = 3
        normalized = validate_template(template)

        svg = render_preview(normalized)

        self.assertIn("Community Preview Layout", svg)
        self.assertIn("id: community-preview-layout", svg)
        self.assertIn("template rev: 3", svg)

    def test_preview_lists_excluded_identities_without_rendering_geometry(self) -> None:
        template = clone_template()
        pp5 = element_by_id(template, "PP5")
        pp5["in_layout"] = False
        pp5.pop("geometry", None)
        pp5.pop("label_anchor", None)
        pp5.pop("callout", None)
        normalized = validate_template(template)

        svg = render_preview(normalized)

        self.assertIn("excluded: 1", svg)
        self.assertIn("Excluded identities", svg)
        self.assertIn("PP5", svg)
        self.assertNotIn('data-element-id="PP5"', svg)

    def test_cli_writes_preview_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            output = Path(tmp_dir) / "preview.svg"
            from render_dome_layout_preview import main
            import sys

            original_argv = sys.argv
            try:
                sys.argv = [
                    "render_dome_layout_preview.py",
                    "--template",
                    str(TEMPLATE_PATH),
                    "--output",
                    str(output),
                ]
                self.assertEqual(main(), 0)
            finally:
                sys.argv = original_argv

            self.assertTrue(output.exists())
            self.assertIn('data-element-id="PP1"', output.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
