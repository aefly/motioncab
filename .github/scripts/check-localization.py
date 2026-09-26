#!/usr/bin/env python3
"""Checks every localization/<lang>.json against en.json.

SPF loads a single language file per plugin, with no per-key fallback to
English, so a key missing from a translation shows up raw in-game. Fails
when a file isn't valid JSON, lacks or adds keys compared to en.json, or
uses different "{name}" placeholders than the English string.
"""

import json
import re
import sys
from pathlib import Path

LOCALIZATION_DIR = Path(__file__).resolve().parents[2] / "localization"
REFERENCE = "en.json"
PLACEHOLDER = re.compile(r"\{\w+\}")


def flatten(node, prefix=""):
    out = {}
    for key, value in node.items():
        path = f"{prefix}{key}"
        if isinstance(value, dict):
            out.update(flatten(value, path + "."))
        else:
            out[path] = value
    return out


def load(path):
    try:
        return flatten(json.loads(path.read_text(encoding="utf-8")))
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        print(f"::error file={path}::invalid JSON: {e}")
        return None


def main():
    reference_path = LOCALIZATION_DIR / REFERENCE
    reference = load(reference_path)
    if reference is None:
        return 1

    failed = False
    for path in sorted(LOCALIZATION_DIR.glob("*.json")):
        if path.name == REFERENCE:
            continue
        strings = load(path)
        if strings is None:
            failed = True
            continue
        for key in sorted(reference.keys() - strings.keys()):
            print(f"::error file={path}::missing key '{key}'")
            failed = True
        for key in sorted(strings.keys() - reference.keys()):
            print(f"::error file={path}::unknown key '{key}' (not in en.json)")
            failed = True
        for key in sorted(reference.keys() & strings.keys()):
            expected = set(PLACEHOLDER.findall(str(reference[key])))
            actual = set(PLACEHOLDER.findall(str(strings[key])))
            if expected != actual:
                print(
                    f"::error file={path}::key '{key}' has placeholders "
                    f"{sorted(actual)}, expected {sorted(expected)}"
                )
                failed = True

    if not failed:
        print("All translation files match en.json.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
