#!/usr/bin/env python3
"""
Generate sera-reference.txt from the am-sera.json mapping.
Groups entries by target character, sorted by Unicode codepoint.
"""
import json
from pathlib import Path
from collections import defaultdict

SCRIPT_DIR = Path(__file__).resolve().parent
JSON_PATH = SCRIPT_DIR.parent / "data" / "amharic" / "am-sera.json"
REF_PATH = SCRIPT_DIR.parent / "docs" / "ethio-am-sera-reference.txt"


def main():
    with open(JSON_PATH, encoding="utf-8") as f:
        data = json.load(f)

    mapping = data["states"]["init"]["map"]

    target_to_keys = defaultdict(list)
    for k, v in mapping.items():
        target_to_keys[v].append(k)

    lines = []
    lines.append("Amharic SERA — ASCII to Ethiopic Reference")
    lines.append(f"Source: data/amharic/am-sera.json  ({len(mapping)} entries)")
    lines.append("=" * 70)
    lines.append("")

    def sort_target(val):
        if val and len(val) >= 1:
            return ord(val[0])
        return 0

    sorted_targets = sorted(target_to_keys.keys(), key=sort_target)

    for val in sorted_targets:
        keys = sorted(target_to_keys[val], key=len)
        key_list = ", ".join(repr(k) for k in keys[:6])
        if len(keys) > 6:
            key_list += f" … ({len(keys)} total)"

        if not val:
            lines.append(f"  <commit>  ←  {key_list}")
        elif len(val) == 1:
            cp = ord(val)
            lines.append(f"  {val}  U+{cp:04X}  ←  {key_list}")
        else:
            lines.append(f"  {val!r}  ←  {key_list}")

    lines.append("")
    lines.append("=" * 70)

    with open(REF_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print(f"Wrote {REF_PATH} ({len(lines)} lines)")


if __name__ == "__main__":
    main()
