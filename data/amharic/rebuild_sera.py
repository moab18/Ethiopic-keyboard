#!/usr/bin/env python3
"""
Rebuild sera.json: remove Y-variant mappings and reorder by Unicode codepoint.

Removes these m17n-derived mappings considered bugs or later alternations:
  MY, mY, rY, MYa, RYa, mYa, rYa

Reorders all entries by the Unicode codepoint of the target character.
Multi-character targets are ordered by the first character's codepoint.
For the same target, shorter keys come first.
"""

import json
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
JSON_IN = SCRIPT_DIR / "sera.json"
JSON_OUT = SCRIPT_DIR / "sera.json"

Y_VARIANT_KEYS = {"MY", "mY", "rY", "MYa", "RYa", "mYa", "rYa"}


def sort_key(item):
    key, val = item
    cp = ord(val[0]) if val else 0
    return (cp, len(key), key)


def main():
    with open(JSON_IN, "r", encoding="utf-8") as f:
        data = json.load(f)

    mapping = data["states"]["init"]["map"]

    removed = 0
    for k in list(mapping):
        if k in Y_VARIANT_KEYS:
            print(f"  REMOVED: '{k}' -> {mapping[k]}")
            del mapping[k]
            removed += 1

    sorted_items = sorted(mapping.items(), key=sort_key)
    sorted_map = {k: v for k, v in sorted_items}

    data["states"]["init"]["map"] = sorted_map
    data["version"] = "2.1.0"

    with open(JSON_OUT, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    eth = [v for v in set(sorted_map.values()) if len(v) == 1 and 0x1200 <= ord(v) <= 0x137F]
    print(f"\nRemoved {removed} Y-variant mappings")
    print(f"Wrote {len(sorted_map)} entries to {JSON_OUT}")
    print(f"Unique Ethiopic chars: {len(eth)}")


if __name__ == "__main__":
    main()
