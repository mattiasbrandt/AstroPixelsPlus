#!/usr/bin/env python3
"""Validate reviewable dome layout template JSON files."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from generate_dome_layout_header import ValidationError, load_and_validate


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEMPLATE_DIR = REPO_ROOT / "templates/dome-layouts"
SCHEMA_FILE = DEFAULT_TEMPLATE_DIR / "schema-v1.json"


def discover_templates(template_dir: Path) -> list[Path]:
    return sorted(
        path
        for path in template_dir.glob("*.json")
        if path.resolve() != SCHEMA_FILE.resolve()
    )


def validate_file(path: Path, *, enforce_default_identity: bool) -> tuple[bool, str]:
    try:
        template = load_and_validate(
            path, enforce_default_identity=enforce_default_identity
        )
    except FileNotFoundError:
        return False, f"{path}: missing file"
    except json.JSONDecodeError as exc:
        return False, f"{path}: invalid JSON: {exc}"
    except ValidationError as exc:
        return False, f"{path}: {exc}"

    return (
        True,
        (
            f"{path}: ok "
            f"({template['template_id']} rev {template['template_revision']}, "
            f"{len(template['elements'])} elements)"
        ),
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate dome layout templates against the v1 AstroPixelsPlus "
            "identity/geometry contract."
        )
    )
    parser.add_argument(
        "templates",
        nargs="*",
        type=Path,
        help="Template JSON files to validate. Defaults to every template in templates/dome-layouts/.",
    )
    parser.add_argument(
        "--strict-bundled",
        action="store_true",
        help=(
            "Also require the bundled MK4 template identity/revision. Use this "
            "for firmware generation checks, not community template review."
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    templates = args.templates or discover_templates(DEFAULT_TEMPLATE_DIR)
    if not templates:
        print("no template JSON files found", file=sys.stderr)
        return 2

    ok = True
    for path in templates:
        valid, message = validate_file(
            path, enforce_default_identity=args.strict_bundled
        )
        print(message, file=sys.stdout if valid else sys.stderr)
        ok = ok and valid
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
