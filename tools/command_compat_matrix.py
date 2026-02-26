#!/usr/bin/env python3
"""AstroPixels command compatibility smoke runner.

Sends a curated command matrix to the firmware HTTP API and verifies
the controller remains healthy and responsive.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from typing import List, Tuple


@dataclass(frozen=True)
class MatrixEntry:
    group: str
    label: str
    command: str


DEFAULT_MATRIX: List[MatrixEntry] = [
    MatrixEntry("panels", "Open all panels", ":OP00"),
    MatrixEntry("panels", "Close all panels", ":CL00"),
    MatrixEntry("holos", "All holo lights on", "*ON00"),
    MatrixEntry("holos", "All holo lights off", "*OF00"),
    MatrixEntry("holos", "Reset holos", "*ST00"),
    MatrixEntry("logics", "All logics normal", "@0T1"),
    MatrixEntry("logics", "All logics red alert", "@0T5"),
    MatrixEntry("sequences", "Quiet reset", ":SE10"),
    MatrixEntry("sequences", "Full-awake", ":SE11"),
    MatrixEntry("sequences", "Awake+", ":SE14"),
    MatrixEntry("sound", "Random sound", "$R"),
    MatrixEntry("sound", "Stop sound", "$s"),
]


def http_get_json(base_url: str, path: str, timeout: float) -> dict:
    req = urllib.request.Request(base_url + path, method="GET")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        payload = resp.read().decode("utf-8", errors="replace")
    return json.loads(payload)


def http_post_cmd(
    base_url: str, token: str, cmd: str, timeout: float
) -> Tuple[int, str]:
    body = urllib.parse.urlencode({"cmd": cmd}).encode("utf-8")
    headers = {"Content-Type": "application/x-www-form-urlencoded"}
    if token:
        headers["X-AP-Token"] = token
    req = urllib.request.Request(
        base_url + "/api/cmd", data=body, headers=headers, method="POST"
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        text = resp.read().decode("utf-8", errors="replace")
        return resp.status, text


def pick_matrix(groups: List[str]) -> List[MatrixEntry]:
    if not groups:
        return DEFAULT_MATRIX
    allowed = set(groups)
    return [entry for entry in DEFAULT_MATRIX if entry.group in allowed]


def validate_state_shape(state: dict) -> Tuple[bool, str]:
    required = ["uptime", "wifiEnabled", "freeHeap"]
    missing = [key for key in required if key not in state]
    if missing:
        return False, "missing state keys: " + ", ".join(missing)
    return True, "ok"


def run_matrix(
    base_url: str, token: str, timeout: float, settle_ms: int, matrix: List[MatrixEntry]
) -> int:
    print(f"Target: {base_url}")
    print(f"Checks: {len(matrix)} command(s)")

    try:
        baseline_state = http_get_json(base_url, "/api/state", timeout)
        _ = http_get_json(base_url, "/api/health", timeout)
    except Exception as exc:  # noqa: BLE001
        print(f"FAIL baseline connectivity: {exc}")
        return 2

    ok, reason = validate_state_shape(baseline_state)
    if not ok:
        print(f"FAIL baseline state shape: {reason}")
        return 2

    previous_uptime = int(baseline_state.get("uptime", 0))
    failures = 0

    for index, entry in enumerate(matrix, start=1):
        prefix = f"[{index:02d}/{len(matrix):02d}] {entry.group:<9} {entry.command:<8}"
        try:
            status, _ = http_post_cmd(base_url, token, entry.command, timeout)
            if status != 200:
                failures += 1
                print(f"{prefix} FAIL http {status}")
                continue

            time.sleep(settle_ms / 1000.0)
            state = http_get_json(base_url, "/api/state", timeout)
            _ = http_get_json(base_url, "/api/health", timeout)
            ok, reason = validate_state_shape(state)
            if not ok:
                failures += 1
                print(f"{prefix} FAIL {reason}")
                continue

            current_uptime = int(state.get("uptime", 0))
            if current_uptime + 2 < previous_uptime:
                failures += 1
                print(
                    f"{prefix} FAIL possible reboot (uptime {previous_uptime}s -> {current_uptime}s)"
                )
                previous_uptime = current_uptime
                continue

            previous_uptime = current_uptime
            print(f"{prefix} PASS {entry.label}")
        except urllib.error.HTTPError as exc:
            failures += 1
            print(f"{prefix} FAIL http error {exc.code}")
        except Exception as exc:  # noqa: BLE001
            failures += 1
            print(f"{prefix} FAIL {exc}")

    print("-")
    if failures:
        print(f"Matrix result: FAIL ({failures}/{len(matrix)} failed)")
        return 1
    print("Matrix result: PASS")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run AstroPixels command compatibility matrix"
    )
    parser.add_argument(
        "--host", default="192.168.4.1", help="Target host/IP (default: 192.168.4.1)"
    )
    parser.add_argument(
        "--scheme", default="http", choices=["http", "https"], help="URL scheme"
    )
    parser.add_argument("--token", default="", help="Optional API write token")
    parser.add_argument(
        "--timeout", type=float, default=3.0, help="HTTP timeout seconds"
    )
    parser.add_argument(
        "--settle-ms",
        type=int,
        default=300,
        help="Delay after each command before state/health checks",
    )
    parser.add_argument(
        "--group",
        action="append",
        choices=["panels", "holos", "logics", "sequences", "sound"],
        help="Restrict to one or more groups (repeatable)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print matrix and exit without sending commands",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    base_url = f"{args.scheme}://{args.host}"
    matrix = pick_matrix(args.group or [])

    if not matrix:
        print("No matrix entries selected")
        return 2

    if args.dry_run:
        print(f"Target: {base_url}")
        print("Selected matrix entries:")
        for entry in matrix:
            print(f"- [{entry.group}] {entry.command:>8}  {entry.label}")
        return 0

    return run_matrix(base_url, args.token, args.timeout, args.settle_ms, matrix)


if __name__ == "__main__":
    sys.exit(main())
