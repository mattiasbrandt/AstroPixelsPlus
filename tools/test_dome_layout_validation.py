#!/usr/bin/env python3
"""Regression tests for the dome layout template validator."""

from __future__ import annotations

import copy
import json
import math
import unittest
from pathlib import Path

from generate_dome_layout_header import ValidationError, validate_template


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE_PATH = ROOT / "templates/dome-layouts/mr-baddeley-complex-dome-mk4.json"


def load_template() -> dict:
    with TEMPLATE_PATH.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def clone_template() -> dict:
    return copy.deepcopy(load_template())


def element_by_id(template: dict, element_id: str) -> dict:
    for element in template["elements"]:
        if element["id"] == element_id:
            return element
    raise AssertionError(f"missing test fixture element {element_id}")


class DomeLayoutValidationTests(unittest.TestCase):
    def assertValidationFails(self, template: dict, expected: str) -> None:
        with self.assertRaisesRegex(ValidationError, expected):
            validate_template(template)

    def test_bundled_template_validates_in_general_and_strict_modes(self) -> None:
        template = clone_template()

        general = validate_template(template)
        strict = validate_template(template, enforce_default_identity=True)

        self.assertEqual(general["template_id"], "mr-baddeley-complex-dome-mk4")
        self.assertEqual(strict["template_id"], "mr-baddeley-complex-dome-mk4")
        self.assertEqual(len(general["elements"]), 28)

    def test_non_mk4_identity_is_valid_for_review_but_not_for_firmware_generation(self) -> None:
        template = clone_template()
        template["template_id"] = "community-test-layout"
        template["template_name"] = "Community Test Layout"
        template["template_revision"] = 2

        general = validate_template(template)
        self.assertEqual(general["template_id"], "community-test-layout")
        with self.assertRaisesRegex(ValidationError, "template_id must be"):
            validate_template(template, enforce_default_identity=True)

    def test_missing_known_id_fails_but_explicit_in_layout_false_passes(self) -> None:
        template = clone_template()
        template["elements"] = [
            element for element in template["elements"] if element["id"] != "PP5"
        ]
        self.assertValidationFails(template, "missing explicit known identities: PP5")

        template = clone_template()
        pp5 = element_by_id(template, "PP5")
        pp5["in_layout"] = False
        pp5.pop("geometry", None)
        pp5.pop("label_anchor", None)
        pp5.pop("callout", None)

        normalized = validate_template(template)
        self.assertFalse(element_by_id(normalized, "PP5")["in_layout"])

    def test_duplicate_and_unknown_ids_fail(self) -> None:
        template = clone_template()
        element_by_id(template, "PP5")["id"] = "PP6"
        self.assertValidationFails(template, "duplicate element id PP6")

        template = clone_template()
        element_by_id(template, "PP5")["id"] = "PX"
        self.assertValidationFails(template, "is not in the v1 known identity set")

    def test_mounted_on_must_reference_a_rendered_non_self_element(self) -> None:
        template = clone_template()
        element_by_id(template, "HP3")["mounted_on"] = "HP3"
        self.assertValidationFails(template, "mounted_on cannot reference itself")

        template = clone_template()
        element_by_id(template, "HP3")["mounted_on"] = "PP5"
        pp5 = element_by_id(template, "PP5")
        pp5["in_layout"] = False
        pp5.pop("geometry", None)
        pp5.pop("label_anchor", None)
        pp5.pop("callout", None)
        self.assertValidationFails(template, "references non-rendered element PP5")

    def test_in_layout_render_keys_are_required_and_excluded_geometry_is_rejected(self) -> None:
        template = clone_template()
        element_by_id(template, "PP5").pop("geometry")
        self.assertValidationFails(template, "missing required render keys: geometry")

        template = clone_template()
        pp5 = element_by_id(template, "PP5")
        pp5["in_layout"] = False
        self.assertValidationFails(template, "outside the layout and must not define render geometry")

    def test_commandable_policy_is_v1_panel_movement_only(self) -> None:
        template = clone_template()
        p8 = element_by_id(template, "P8")
        p8["commandable"] = True
        p8["capabilities"] = ["open", "close", "flutter"]
        self.assertValidationFails(template, "commandable is only valid for ring/pie panels")

        template = clone_template()
        pp1 = element_by_id(template, "PP1")
        pp1["capabilities"] = ["close", "open", "flutter"]
        self.assertValidationFails(template, "capabilities must be")

        template = clone_template()
        hp1 = element_by_id(template, "HP1")
        hp1["capabilities"] = ["open"]
        self.assertValidationFails(template, "movement capabilities on a non-commandable element")

    def test_descriptive_non_panel_capabilities_are_allowed(self) -> None:
        template = clone_template()
        hp1 = element_by_id(template, "HP1")
        hp1["capabilities"] = ["light", "aim", "center", "test", "effect"]
        fld = element_by_id(template, "FLD")
        fld["capabilities"] = ["display_text", "effect"]

        normalized = validate_template(template)

        self.assertIn("center", element_by_id(normalized, "HP1")["capabilities"])
        self.assertIn("display_text", element_by_id(normalized, "FLD")["capabilities"])

    def test_point_radius_defaults_to_six(self) -> None:
        template = clone_template()
        rpsi = element_by_id(template, "RPSI")
        rpsi["geometry"].pop("r", None)

        normalized = validate_template(template)

        self.assertEqual(element_by_id(normalized, "RPSI")["geometry"]["r"], 6)

    def test_bad_numbers_and_geometry_fail(self) -> None:
        template = clone_template()
        element_by_id(template, "HP1")["geometry"]["cx"] = math.nan
        self.assertValidationFails(template, "must be finite")

        template = clone_template()
        element_by_id(template, "HP1")["geometry"]["rx"] = 0
        self.assertValidationFails(template, "rx and elements")

        template = clone_template()
        element_by_id(template, "HP1")["geometry"]["cx"] = 513
        self.assertValidationFails(template, "outside the 480-space guard band")

        template = clone_template()
        element_by_id(template, "PP1")["geometry"]["d"] = "123 456 789"
        self.assertValidationFails(template, "does not look like SVG path data")


if __name__ == "__main__":
    unittest.main()
