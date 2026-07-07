#!/usr/bin/env python3
"""Regression checks for operator-disabled panel runtime interlock."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class OperatorDisabledInterlockTests(unittest.TestCase):
    def test_element_status_is_not_documented_as_advisory_only(self) -> None:
        status_header = read("DomeElementStatus.h")

        self.assertNotIn("must not gate", status_header)
        self.assertNotIn("never blocks commands", read("AsyncWebInterface.h"))

    def test_runtime_overlay_exists_and_is_boot_applied_before_setup_event(self) -> None:
        ino = read("AstroPixelsPlus.ino")

        self.assertIn("domeReloadPanelRoutingWithDisabledOverlay", ino)
        self.assertIn("domeApplyDisabledPanelOverlay", ino)

        setup = re.search(r"void setup\(\).*?SetupEvent::ready\(\);", ino, re.S)
        self.assertIsNotNone(setup)
        assert setup is not None
        setup_block = setup.group(0)
        self.assertIn("panelConfigLoad();", setup_block)
        self.assertIn("domeApplyDisabledPanelOverlay();", setup_block)
        self.assertLess(
            setup_block.index("domeApplyDisabledPanelOverlay();"),
            setup_block.index("SetupEvent::ready();"),
        )

    def test_status_and_panel_config_posts_reapply_runtime_overlay(self) -> None:
        async_web = read("AsyncWebInterface.h")

        panels_post = re.search(
            r'asyncServer\.on\("/api/panels/config", HTTP_POST,.*?request->send',
            async_web,
            re.S,
        )
        self.assertIsNotNone(panels_post)
        assert panels_post is not None
        self.assertIn("domeApplyDisabledPanelOverlay();", panels_post.group(0))

        status_post = re.search(
            r'asyncServer\.on\("/api/dome/element-status", HTTP_POST,.*?request->send\(200',
            async_web,
            re.S,
        )
        self.assertIsNotNone(status_post)
        assert status_post is not None
        self.assertIn("domeReloadPanelRoutingWithDisabledOverlay();", status_post.group(0))

    def test_disabled_overlay_cuts_pwm_and_zeroes_panel_slot_routing(self) -> None:
        ino = read("AstroPixelsPlus.ino")

        overlay = re.search(
            r"static void domeApplyDisabledPanelOverlay\(\)\s*\{.*?\n\}\n",
            ino,
            re.S,
        )
        self.assertIsNotNone(overlay)
        assert overlay is not None
        body = overlay.group(0)
        self.assertIn("domeCutPanelServoPwm(slot)", body)
        self.assertIn("servoDispatch.setServo(slot, 0", body)
        self.assertIn("servoDispatch.setOutput(pin, false)", ino)
        self.assertIn("servoDispatch.disable(slot)", ino)

    def test_status_metadata_mismatch_fails_closed(self) -> None:
        status = read("DomeElementStatus.h")

        read_all = re.search(
            r"static bool domeElementStatusReadAll\(.*?\n\}\n\nstatic String",
            status,
            re.S,
        )
        self.assertIsNotNone(read_all)
        assert read_all is not None
        body = read_all.group(0)
        self.assertIn("bool metadataOk = domeElementStatusMetadataMatches(prefs);", body)
        self.assertIn("if (!metadataOk)", body)
        self.assertIn("return false;", body)

        build_json = re.search(
            r"static String domeElementStatusBuildJson\(.*?\n\}\n",
            status,
            re.S,
        )
        self.assertIsNotNone(build_json)
        assert build_json is not None
        json_body = build_json.group(0)
        self.assertIn("bool disabled = statusOk ? statuses[i].disabled : true;", json_body)
        self.assertIn('String reason = statusOk ? statuses[i].reason : "status unavailable";', json_body)


if __name__ == "__main__":
    unittest.main()
