#!/usr/bin/env python3
"""Regression checks for Marcduino ingress body-link echo policy."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def source_block(header: str, name: str) -> str:
    match = re.search(
        rf"static const MarcduinoIngressSource {name} = \{{(.*?)\}};",
        header,
        re.S,
    )
    if match is None:
        raise AssertionError(f"missing source metadata {name}")
    return match.group(1)


class MarcduinoIngressEchoPolicyTests(unittest.TestCase):
    def test_body_link_sources_suppress_egress_and_web_sources_do_not(self) -> None:
        header = read("MarcduinoIngress.h")

        self.assertIn("true", source_block(header, "kMarcduinoIngressBodyLinkUart"))
        self.assertIn("true", source_block(header, "kMarcduinoIngressBodyLinkWifi"))
        self.assertIn("false", source_block(header, "kMarcduinoIngressWebApi"))
        self.assertIn("false", source_block(header, "kMarcduinoIngressWebSocket"))

    def test_admission_computes_queue_suppression_from_source_metadata(self) -> None:
        header = read("MarcduinoIngress.h")
        admit = re.search(
            r"static void marcduinoIngressAdmit\(const MarcduinoIngressSource &source, const char \*cmd\)\s*\{"
            r"(.*?)\n\}\n\nstatic void drainMarcduinoCommandQueue",
            header,
            re.S,
        )
        self.assertIsNotNone(admit)
        assert admit is not None

        body = admit.group(0)
        self.assertIn(
            "enqueueMarcduinoCommand(label, cmd, marcduinoIngressSuppressesBodyLinkEgress(source));",
            body,
        )
        self.assertNotIn("body-link-", body)
        self.assertNotIn("strncmp", body)

    def test_drain_applies_suppression_only_around_command_execution(self) -> None:
        header = read("MarcduinoIngress.h")
        drain = re.search(
            r"static void drainMarcduinoCommandQueue\(\)\s*\{.*?\n\}\n",
            header,
            re.S,
        )
        self.assertIsNotNone(drain)
        assert drain is not None
        body = drain.group(0)

        self.assertIn("bool previousSuppressBodyLinkEgress = sSuppressBodyLinkEgress;", body)
        self.assertIn("if (suppressBodyLinkEgress)", body)
        self.assertIn("sSuppressBodyLinkEgress = true;", body)
        self.assertIn("Marcduino::processCommand(player, cmd);", body)
        self.assertIn("sSuppressBodyLinkEgress = previousSuppressBodyLinkEgress;", body)

    def test_body_link_wifi_transport_is_thin_adapter(self) -> None:
        body_link = read("BodyLinkWiFi.h")

        self.assertIn(
            "marcduinoIngressAdmit(kMarcduinoIngressBodyLinkWifi, lineBuf);",
            body_link,
        )
        self.assertNotIn('"body-link-wifi"', body_link)
        self.assertNotIn("suppressBodyLinkEgress", body_link)


if __name__ == "__main__":
    unittest.main()
