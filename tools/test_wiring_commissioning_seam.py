#!/usr/bin/env python3
"""Regression checks for the wiring commissioning module seam."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class WiringCommissioningSeamTests(unittest.TestCase):
    def test_raw_pwm_writes_go_through_internal_writer(self) -> None:
        header = read("WiringCommissioning.h")

        self.assertIn("struct WiringPwmWrite", header)
        self.assertIn("typedef bool (*WiringPwmWriter)(const WiringPwmWrite &write);", header)
        self.assertIn("static WiringPwmWriter sWiringPwmWriter = wiringWritePwmToWire;", header)
        self.assertIn("sWiringPwmWriter(write)", header)

        direct_wire_writes = re.findall(r"Wire\.(?:beginTransmission|write|endTransmission)\(", header)
        wire_adapter = re.search(
            r"static bool wiringWritePwmToWire\(const WiringPwmWrite &write\)\s*\{.*?\n\}",
            header,
            re.S,
        )
        self.assertIsNotNone(wire_adapter)
        assert wire_adapter is not None
        self.assertEqual(
            len(direct_wire_writes),
            len(re.findall(r"Wire\.(?:beginTransmission|write|endTransmission)\(", wire_adapter.group(0))),
        )

    def test_pwm_writer_has_test_hook_without_changing_routes(self) -> None:
        header = read("WiringCommissioning.h")
        async_web = read("AsyncWebInterface.h")

        self.assertIn("#ifdef WIRING_COMMISSIONING_TEST_HOOKS", header)
        self.assertIn("wiringCommissioningSetPwmWriterForTest", header)
        self.assertIn("wiringCommissioningResetPwmWriterForTest", header)

        servo_test_route = re.search(
            r'asyncServer\.on\("/api/servo/test", HTTP_POST,.*?request->send',
            async_web,
            re.S,
        )
        self.assertIsNotNone(servo_test_route)
        assert servo_test_route is not None
        route_body = servo_test_route.group(0)
        self.assertIn("wiringCommissioningStartRawServoTestFromBody", route_body)
        self.assertNotIn("Wire.", route_body)
        self.assertNotIn("WIRING_PWM_", route_body)

        servo_stop_route = re.search(
            r'asyncServer\.on\("/api/servo/stop", HTTP_POST,.*?request->send',
            async_web,
            re.S,
        )
        self.assertIsNotNone(servo_stop_route)
        assert servo_stop_route is not None
        stop_body = servo_stop_route.group(0)
        self.assertIn("wiringCommissioningStopRawServoTestFromBody", stop_body)
        self.assertNotIn("Wire.", stop_body)
        self.assertNotIn("WIRING_PWM_", stop_body)


if __name__ == "__main__":
    unittest.main()
