#!/usr/bin/env python3
"""Checks that commands the web UI and REST routes send can reach a handler.

Marcduino::processCommand() silently drops a command that matches no
MARCDUINO_ACTION / MARCDUINO_ANIMATION prefix, so a mistyped command in
data/*.html or a REST route answers "ok" and does nothing. This test collects
every command literal the UI and AsyncWebInterface.h send and checks it against
the handler prefixes registered in the repo plus the prefixes the ingress layer
handles itself. It also checks that every window.X global the pages use is
assigned somewhere in data/.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"

# Prefixes consumed by MarcduinoIngress.h before the Marcduino dispatcher.
# Each entry names the file that must still contain its string literal, so this
# list fails loudly if an intercept is removed.
INGRESS_PREFIXES = {
    ":SM": "AstroPixelsPlus.ino",
    "DV:": "DomeSequences.h",
    "DL:": "DomeSequences.h",
    "DT:": "DomeSequences.h",
    "DH:": "DomeSequences.h",
    ":MV": "MarcduinoIngress.h",
    "#SO": "MarcduinoIngress.h",
    "#SC": "MarcduinoIngress.h",
    "#SW": "MarcduinoIngress.h",
}

# Browser-provided window properties the pages may use.
BROWSER_GLOBALS = {"CSS", "location", "fetch", "WebSocket", "localStorage", "setTimeout", "document"}

HANDLER_RE = re.compile(r"MARCDUINO_(?:ACTION|ANIMATION)\(\s*\w+\s*,\s*([^,\s)]+)")
# Quoted literals shaped like Marcduino commands: :XX *XX @XX $NN #XX and XX: (DM:, DV:, ...).
COMMAND_LITERAL_RE = re.compile(
    r"""(['"])((?:[:*@$][A-Z0-9]|#[A-Z]{2}|[A-Z]{2}:)[^'"\s<>\\]*)\1(\s*\+)?"""
)
HEX_COLOR_RE = re.compile(r"^#[0-9A-Fa-f]{3,8}$")
ADMIT_LITERAL_RE = re.compile(r'marcduinoIngressAdmit\([^,]+,\s*"([^"]+)"\)')
ROUTE_STRING_CMD_RE = re.compile(r'String cmd = "([^"]+)"\s*\+')


def firmware_sources() -> list[Path]:
    return sorted(
        [p for p in ROOT.glob("*.h")] + [p for p in ROOT.glob("*.ino")] + list(ROOT.glob("effects/*.h"))
    )


def handler_prefixes() -> set[str]:
    prefixes: set[str] = set()
    for path in firmware_sources():
        prefixes.update(HANDLER_RE.findall(path.read_text(encoding="utf-8")))
    return prefixes | set(INGRESS_PREFIXES)


def reaches_handler(command: str, prefixes: set[str], partial: bool) -> bool:
    """Mirror Marcduino::processCommand() prefix matching.

    A handler prefix P matches when the command starts with P; a prefix "@X..."
    also matches a command that starts with a digit and with "X..." (ReelTwo's
    '@' shorthand). A partial literal (concatenated with more text at run time)
    also passes when it is itself the start of some prefix.
    """
    for prefix in prefixes:
        if command.startswith(prefix):
            return True
        if prefix.startswith("@") and command[:1].isdigit() and command.startswith(prefix[1:]):
            return True
        if partial and prefix.startswith(command):
            return True
    return False


def ui_command_literals() -> list[tuple[str, str, bool]]:
    found = []
    for path in sorted(list(DATA.glob("*.html")) + list(DATA.glob("*.js"))):
        text = path.read_text(encoding="utf-8")
        for match in COMMAND_LITERAL_RE.finditer(text):
            literal = match.group(2)
            if HEX_COLOR_RE.match(literal):
                continue
            line = text.count("\n", 0, match.start()) + 1
            # A literal concatenated with more text, or used as a prefix test
            # (cmd.startsWith('DM:'), cmd.indexOf(':SE')), is a command prefix.
            is_prefix_test = text[: match.start()].rstrip().endswith(("startsWith(", "indexOf("))
            partial = match.group(3) is not None or is_prefix_test
            found.append((f"data/{path.name}:{line}", literal, partial))
    return found


def route_command_literals() -> list[tuple[str, str, bool]]:
    text = (ROOT / "AsyncWebInterface.h").read_text(encoding="utf-8")
    found = []
    for regex, partial in ((ADMIT_LITERAL_RE, False), (ROUTE_STRING_CMD_RE, True)):
        for match in regex.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            found.append((f"AsyncWebInterface.h:{line}", match.group(1), partial))
    return found


class UiCommandRoutingTest(unittest.TestCase):
    def test_ingress_prefix_list_matches_source(self) -> None:
        for prefix, filename in INGRESS_PREFIXES.items():
            self.assertIn(f'"{prefix}"', (ROOT / filename).read_text(encoding="utf-8"),
                          f"ingress prefix {prefix} no longer handled in {filename}; update INGRESS_PREFIXES")

    def test_every_sent_command_reaches_a_handler(self) -> None:
        prefixes = handler_prefixes()
        literals = ui_command_literals() + route_command_literals()
        self.assertGreater(len(literals), 50, "command extraction found suspiciously few literals")
        unrouted = [
            f"{where}: {literal!r}{' (partial)' if partial else ''}"
            for where, literal, partial in literals
            if not reaches_handler(literal, prefixes, partial)
        ]
        self.assertEqual(unrouted, [], "commands that match no handler (silently dropped):\n" + "\n".join(unrouted))

    def test_window_globals_used_by_pages_are_assigned(self) -> None:
        texts = {p.name: p.read_text(encoding="utf-8")
                 for p in list(DATA.glob("*.html")) + list(DATA.glob("*.js"))}
        all_text = "\n".join(texts.values())
        used = set(re.findall(r"window\.([A-Za-z_]\w*)", all_text))
        used |= set(re.findall(r"typeof ([A-Za-z_]\w*) *[!=]==? *'function'", all_text))
        assigned = set(re.findall(r"window\.([A-Za-z_]\w*)\s*=(?!=)", all_text))
        missing = sorted(used - assigned - BROWSER_GLOBALS)
        self.assertEqual(missing, [], "window globals used by pages but never assigned in data/")


if __name__ == "__main__":
    unittest.main()
