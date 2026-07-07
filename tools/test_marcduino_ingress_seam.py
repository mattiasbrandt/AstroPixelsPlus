#!/usr/bin/env python3
"""Regression checks for the unified Marcduino ingress seam."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ADMIT_START = (
    "static void marcduinoIngressAdmit(const MarcduinoIngressSource &source, const char *cmd)\n"
    "{"
)


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def block_between(text: str, start: str, end: str) -> str:
    start_index = text.find(start)
    if start_index == -1:
        raise AssertionError(f"missing block start {start!r}")
    end_index = text.find(end, start_index)
    if end_index == -1:
        raise AssertionError(f"missing block end {end!r}")
    return text[start_index:end_index]


class MarcduinoIngressSeamTests(unittest.TestCase):
    def test_all_transport_callers_use_the_ingress_interface(self) -> None:
        ino = read("AstroPixelsPlus.ino")
        async_web = read("AsyncWebInterface.h")
        body_link = read("BodyLinkWiFi.h")

        combined = "\n".join([ino, async_web, body_link])
        self.assertNotIn("processMarcduinoCommandWithSource(", combined)
        self.assertNotIn("processMarcduinoCommandWithSourceMain", combined)

        for source in [
            "kMarcduinoIngressWebApi",
            "kMarcduinoIngressWebSocket",
            "kMarcduinoIngressUsbSerial",
            "kMarcduinoIngressBodyLinkUart",
            "kMarcduinoIngressBodyLinkWifi",
            "kMarcduinoIngressWifiMarcduino",
            "kMarcduinoIngressI2CSlave",
        ]:
            self.assertIn(f"marcduinoIngressAdmit({source}", combined)

    def test_sleep_gate_runs_before_any_command_effect(self) -> None:
        header = read("MarcduinoIngress.h")
        admit = block_between(
            header,
            ADMIT_START,
            "static void drainMarcduinoCommandQueue()",
        )

        self.assertLess(
            admit.index("shouldBlockCommandDuringSleep(cmd)"),
            admit.index('logCapture.printf("[CMD][%s] %s\\n", label, cmd);'),
        )
        self.assertLess(
            admit.index("shouldBlockCommandDuringSleep(cmd)"),
            admit.index("handleImmediateServoMoveCommand(label, cmd)"),
        )
        self.assertLess(
            admit.index("shouldBlockCommandDuringSleep(cmd)"),
            admit.index("applyDomeVisualPresetCommand(label, cmd)"),
        )

    def test_wake_and_emergency_commands_remain_sleep_exceptions(self) -> None:
        ino = read("AstroPixelsPlus.ino")
        wake_profile = block_between(
            ino,
            "static bool isWakeProfileCommand(const char *cmd)",
            "bool shouldBlockCommandDuringSleep",
        )
        sleep_gate = block_between(
            ino,
            "bool shouldBlockCommandDuringSleep(const char *cmd)",
            "static void applySoftSleepOutputs",
        )

        for command in [":SE11", ":SE13", ":SE14", "#PAWU"]:
            self.assertIn(command, wake_profile)
        self.assertIn("if (isWakeProfileCommand(cmd)) return false;", sleep_gate)
        self.assertIn('strcmp(cmd, ":SE00") == 0', sleep_gate)
        self.assertIn('strcmp(cmd, ":CL00") == 0', sleep_gate)
        self.assertIn("return true;", sleep_gate)

    def test_duplicate_mood_reset_is_dropped_before_queueing(self) -> None:
        header = read("MarcduinoIngress.h")
        admit = block_between(
            header,
            ADMIT_START,
            "static void drainMarcduinoCommandQueue()",
        )

        self.assertLess(
            admit.index("shouldDropDuplicateMoodReset(cmd)"),
            admit.index("handleImmediateServoMoveCommand(label, cmd)"),
        )
        self.assertLess(
            admit.index("shouldDropDuplicateMoodReset(cmd)"),
            admit.index("enqueueMarcduinoCommand(label, cmd"),
        )
        self.assertIn("[mood-duplicate-dropped]", admit)

    def test_immediate_visual_and_calibration_commands_bypass_queue(self) -> None:
        header = read("MarcduinoIngress.h")
        admit = block_between(
            header,
            ADMIT_START,
            "static void drainMarcduinoCommandQueue()",
        )

        bypasses = [
            "handleImmediateServoMoveCommand(label, cmd)",
            "applyDomeVisualPresetCommand(label, cmd)",
            "applyDomeVisualAuthoringCommand(label, cmd)",
            "marcduinoIngressHandlePanelCalibrationCommand(cmd)",
        ]
        enqueue_index = admit.index("enqueueMarcduinoCommand(label, cmd")
        for call in bypasses:
            self.assertLess(admit.index(call), enqueue_index)
            self.assertIn(f"if ({call})\n        return;", admit)

    def test_calibration_lifetime_safety_lives_in_ingress_not_async_routes(self) -> None:
        header = read("MarcduinoIngress.h")
        async_web = read("AsyncWebInterface.h")

        calibration = block_between(
            header,
            "static bool marcduinoIngressHandlePanelCalibrationCommand",
            "static void marcduinoIngressAdmit",
        )
        for command in [":MV", "#SO", "#SC", "#SW"]:
            self.assertIn(command, calibration)
        self.assertIn("before queueing keeps all transports safe", calibration)

        command_admission_section = block_between(
            async_web,
            "static bool guardSleep(AsyncWebServerRequest *request, const char *cmd)",
            "static bool isSensitivePrefKey",
        )
        self.assertNotIn("parseTwoDigitTarget", command_admission_section)
        self.assertNotIn("movePanelMaskToValue", command_admission_section)
        self.assertIn("marcduinoIngressAdmit(kMarcduinoIngressWebApi", async_web)

    def test_queue_capacity_truncation_and_full_logging_are_preserved(self) -> None:
        header = read("MarcduinoIngress.h")
        enqueue = block_between(
            header,
            "static bool enqueueMarcduinoCommand",
            "static bool dequeueMarcduinoCommand",
        )

        self.assertIn("static PendingMarcduinoCommand sMarcduinoQueue[8];", header)
        self.assertIn("char cmd[CONSOLE_BUFFER_SIZE];", header)
        self.assertIn("strlcpy(entry.cmd, cmd, sizeof(entry.cmd));", enqueue)
        self.assertIn("sMarcduinoQueueFullCount++;", enqueue)
        self.assertIn("[queue-full]", enqueue)
        self.assertIn("return queued;", enqueue)

    def test_body_link_origin_suppresses_egress_through_ingress_metadata(self) -> None:
        header = read("MarcduinoIngress.h")

        self.assertRegex(
            header,
            r"kMarcduinoIngressBodyLinkWifi = \{[\s\S]*?MARCDUINO_INGRESS_BODY_LINK_WIFI,\s*true,",
        )
        self.assertRegex(
            header,
            r"kMarcduinoIngressWebApi = \{[\s\S]*?MARCDUINO_INGRESS_WEB_API,\s*false,",
        )
        self.assertIn(
            "enqueueMarcduinoCommand(label, cmd, marcduinoIngressSuppressesBodyLinkEgress(source));",
            header,
        )


if __name__ == "__main__":
    unittest.main()
