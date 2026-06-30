#!/usr/bin/env python3
"""Fail if GeneratedDomeLayout.h does not match the reviewed JSON template."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    return subprocess.call(
        [
            sys.executable,
            str(REPO_ROOT / "tools/generate_dome_layout_header.py"),
            "--check",
        ],
        cwd=str(REPO_ROOT),
    )


if __name__ == "__main__":
    raise SystemExit(main())
