#!/usr/bin/env python3
"""Convert m17n MIM file to JSON mapping format like sera.json."""

import re
import json
import sys


def parse_value(val_text):
    if val_text.startswith('"'):
        i = 1
        while i < len(val_text):
            if val_text[i] == '\\':
                i += 2
            elif val_text[i] == '"':
                return val_text[1:i]
            else:
                i += 1
        return val_text[1:]
    elif val_text.startswith('?'):
        if val_text.startswith('??'):
            return '?'
        elif len(val_text) >= 3 and val_text[1] == '\\':
            return val_text[2]
        else:
            return val_text[1:]
    return val_text


def parse_mim(filepath):
    entries = []
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(';'):
                continue
            m = re.match(r'\("((?:[^"\\]|\\.)*?)"\s+([^)]+)\)', line)
            if not m:
                continue
            key = m.group(1)
            val_expr = m.group(2)
            value = parse_value(val_expr)
            entries.append((key, value))
    return entries


def main():
    if len(sys.argv) < 2:
        print("Usage: convert_mim_to_json.py <input.mim> [output.json]")
        sys.exit(1)

    infile = sys.argv[1]
    outfile = sys.argv[2] if len(sys.argv) > 2 else None

    entries = parse_mim(infile)

    # Build the map dict preserving order
    mapping = {}
    for key, value in entries:
        mapping[key] = value

    output = {
        "input_method": "am-sera",
        "title": "Amharic (SERA)",
        "description": "Amharic input using SERA (System for Ethiopic Representation in ASCII) transliteration. Based on modified m17n am-sera.mim with restored labiovelar forms.",
        "version": "1.0.0",
        "based_on": "m17n MIM/am-sera.mim (modified)",
        "states": {
            "init": {
                "map": mapping
            }
        }
    }

    json_str = json.dumps(output, ensure_ascii=False, indent=2) + "\n"

    if outfile:
        with open(outfile, 'w', encoding='utf-8') as f:
            f.write(json_str)
        print(f"Wrote {len(mapping)} entries to {outfile}")
    else:
        print(json_str)


if __name__ == '__main__':
    main()
