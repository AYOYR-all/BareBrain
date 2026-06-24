from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def macro_name(key: str) -> str:
    return "BRN_PROFILE_" + re.sub(r"[^A-Za-z0-9]+", "_", key).strip("_").upper()


def c_literal(value: object) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return repr(value)
    return json.dumps(str(value))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate build-time configuration from firmware Profile")
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    profile = json.loads(args.profile.read_text(encoding="utf-8-sig"))
    config = profile.get("config", {})
    if not isinstance(config, dict):
        raise ValueError("profile.config must be an object")

    lines = [
        "#pragma once",
        "",
        "/* Generated from the firmware Profile. */",
    ]
    for key, value in sorted(config.items()):
        lines.append(f"#define {macro_name(key)} {c_literal(value)}")
    lines.append("")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
