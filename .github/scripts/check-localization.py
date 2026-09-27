#!/usr/bin/env python3
"""Checks the localization files against the code and against en.json.

SPF loads a single language file per plugin, with no per-key fallback to
English, so a key missing from a translation shows up raw in-game. Fails
when:
- en.json lacks a key the code looks up for a setting, a settings group or
  effect, an effect's hint or a keybind (read from the C++ sources);
- a file isn't valid JSON, lacks or adds keys compared to en.json, or uses
  different "{name}" placeholders than the English string.
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOCALIZATION_DIR = ROOT / "localization"
SETTINGS_SCHEMA = ROOT / "src/core/SettingsSchema.hpp"
KEYBINDS = ROOT / "src/core/Keybinds.hpp"
EFFECT_TABS = ROOT / "src/ui/EffectTabs.cpp"
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


def between(text, start, end):
    """The part of `text` from `start` to the next `end`."""
    begin = text.index(start)
    return text[begin : text.index(end, begin)]


def required_keys():
    required = {"settings.enabled_desc": SETTINGS_SCHEMA}

    schema = between(
        SETTINGS_SCHEMA.read_text(encoding="utf-8"),
        "inline constexpr Setting kAll[] = {",
        "\n};",
    )
    setting_keys = re.findall(r'(?:Bool|Float|Speed)\("(settings\.[\w.]+)"', schema)
    for key in setting_keys:
        _, group, effect, name = key.split(".")
        # Groups and effects have their own title/description (native UI
        # sections, Quick Settings tabs and headers), like each setting.
        for path in (f"settings.{group}", f"settings.{group}.{effect}", key):
            required[f"{path}.title"] = SETTINGS_SCHEMA
            if path != key or name != "enabled":
                required[f"{path}.desc"] = SETTINGS_SCHEMA

    keybinds = KEYBINDS.read_text(encoding="utf-8")
    actions = re.findall(r"inline constexpr Action k\w+\{(.*?)\};", keybinds, re.DOTALL)
    for init in actions:
        loc_key = re.findall(r'"([^"]*)"', init)[3]  # group, name, id, loc_key
        required[f"{loc_key}.title"] = KEYBINDS
        required[f"{loc_key}.desc"] = KEYBINDS

    effects = between(
        EFFECT_TABS.read_text(encoding="utf-8"),
        "constexpr EffectUi kEffects[] = {",
        "\n};",
    )
    for prefix in re.findall(r'"(settings\.[\w.]+)",\s*true', effects):
        required[f"{prefix}.hint"] = EFFECT_TABS

    return required


def main():
    reference_path = LOCALIZATION_DIR / REFERENCE
    reference = load(reference_path)
    if reference is None:
        return 1

    failed = False
    for key, source in sorted(required_keys().items()):
        if key not in reference:
            print(
                f"::error file={reference_path}::missing key '{key}', "
                f"used by {source.relative_to(ROOT)}"
            )
            failed = True

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
        print(
            "en.json has every key the code uses, and all translation files match it."
        )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
