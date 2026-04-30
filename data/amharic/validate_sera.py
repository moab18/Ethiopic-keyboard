#!/usr/bin/env python3
"""
Validate and report on the Amharic SERA JSON mapping file.

Usage:
  python3 validate_sera.py                # validate data/amharic/sera.json
  python3 validate_sera.py --coverage     # show coverage per consonant family
  python3 validate_sera.py --aliases      # show all alias groups
  python3 validate_sera.py --missing      # show Ethiopic chars with no mapping
"""

import json
import sys
from collections import defaultdict
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
MAPPING_PATH = SCRIPT_DIR / "sera.json"

# Vowel suffix naming
VOWEL_NAMES = {0: "ä", 1: "u", 2: "i", 3: "a", 4: "e", 5: "ə", 6: "o"}

# Amharic-relevant consonant base offsets from U+1200
CONSONANT_BASES = {
    0x000: ("h",  "h",     7, "U+1200", "h"),           # ሀ–ሆ
    0x008: ("l",  "l",     7, "U+1208", "l"),           # ለ–ሎ
    0x010: ("H",  "ḥ / ħ", 7, "U+1210", "ḥ"),           # ሐ–ሖ
    0x018: ("m",  "m",     7, "U+1218", "m"),           # መ–ሞ
    0x020: ("ss", "ś",     7, "U+1220", "ś (archaic)"), # ሠ–ሦ
    0x028: ("r",  "r",     7, "U+1228", "r"),           # ረ–ሮ
    0x030: ("s",  "s",     7, "U+1230", "s"),           # ሰ–ሶ
    0x038: ("S",  "š / sh",7, "U+1238", "š"),           # ሸ–ሾ
    0x040: ("Q",  "q / kʼ",7, "U+1240", "q"),           # ቀ–ቆ
    0x048: ("QW", "qʷ",    5, "U+1248", "qʷ (labiovelar)"),  # ቈ–ቍ
    0x060: ("b",  "b",     7, "U+1260", "b"),           # በ–ቦ
    0x068: ("v",  "v",     7, "U+1268", "v"),           # ቨ–ቮ
    0x070: ("t",  "t",     7, "U+1270", "t"),           # ተ–ቶ
    0x078: ("C",  "č / ch",7, "U+1278", "č"),           # ቸ–ቾ
    0x080: ("x",  "ḫ / kh",7, "U+1280", "ḫ"),           # ኀ–ኆ
    0x088: ("xW", "ḫʷ",    5, "U+1288", "ḫʷ (labiovelar)"), # ኈ–ኍ
    0x090: ("n",  "n",     7, "U+1290", "n"),           # ነ–ኖ
    0x098: ("N",  "ñ / ny",7, "U+1298", "ñ"),           # ኘ–ኞ
    0x0A0: ("a",  "ʾ / ʼ", 7, "U+12A0", "ʾ (glottal)"), # አ–ኦ
    0x0A8: ("k",  "k",     7, "U+12A8", "k"),           # ከ–ኮ
    0x0B0: ("kW", "kʷ",    5, "U+12B0", "kʷ (labiovelar)"), # ኰ–ኵ
    0x0B8: ("K",  "ḵ / kh",7, "U+12B8", "ḵ"),           # ኸ–ኾ
    0x0C8: ("w",  "w",     7, "U+12C8", "w"),           # ወ–ዎ
    0x0D0: ("A",  "ʿ / ʽ", 7, "U+12D0", "ʿ (pharyngeal)"), # ዐ–ዖ
    0x0D8: ("z",  "z",     7, "U+12D8", "z"),           # ዘ–ዞ
    0x0E0: ("Z",  "ž / zh",7, "U+12E0", "ž"),           # ዠ–ዦ
    0x0E8: ("y",  "y",     7, "U+12E8", "y"),           # የ–ዮ
    0x0F0: ("d",  "d",     7, "U+12F0", "d"),           # ደ–ዶ
    0x0F8: ("j",  "j / ǧ", 7, "U+12F8", "j"),           # ጀ–ጆ
    0x100: ("g",  "g",     7, "U+1300", "g"),           # ገ–ጎ
    0x108: ("gW", "gʷ",    5, "U+1308", "gʷ (labiovelar)"), # ጐ–ጕ
    0x120: ("T",  "ṭ / tʼ",7, "U+1320", "ṭ"),           # ጠ–ጦ
    0x128: ("CC", "č̣ / chʼ",7,"U+1328","č̣"),             # ጨ–ጮ
    0x130: ("PP", "p̣ / pʼ",7, "U+1330", "p̣"),           # ጰ–ጶ
    0x138: ("SS", "ṣ / tsʼ",7,"U+1338","ṣ"),             # ጸ–ጾ
    0x140: ("SSs","ṣ́",    7, "U+1340", "ṣ́ (archaic)"),  # ፀ–ፆ
    0x148: ("f",  "f",     7, "U+1348", "f"),           # ፈ–ፎ
    0x150: ("p",  "p",     7, "U+1350", "p"),           # ፐ–ፖ
}

# Labiovelar vowel offsets (5 forms, not 7)
LABIOVELAR_VOWEL_OFFSETS = [0, 2, 3, 4, 5]


def load_mapping(path=MAPPING_PATH):
    with open(path) as f:
        data = json.load(f)
    return data["states"]["init"]["map"]


def validate(mapping):
    errors = []
    # Group keys by target character
    target_to_keys = defaultdict(list)
    for key, val in mapping.items():
        target_to_keys[val].append(key)

    PASSTHROUGH_OK = set("'*,.?:;")
    for val, keys in target_to_keys.items():
        if len(val) != 1:
            continue
        cp = ord(val)
        if not (0x1200 <= cp <= 0x137F or cp in (0xAB, 0xBB, 0x2018, 0x201C)):
            if val in PASSTHROUGH_OK:
                continue
            errors.append(f"Non-Ethiopic target: {val!r} U+{cp:04X} via keys {keys}")

    # Check for key collisions (JSON silently dedupes, but we can't detect in loaded data)
    return errors


def coverage_report(mapping):
    """Show which consonant families are fully/partially covered."""
    lines = []
    lines.append(f"{'Family':<8} {'Name':<12} {'Ethiopic chars':>16}  {'Key sequences':>15}  Status")
    lines.append("-" * 80)

    target_to_keys = defaultdict(list)
    for key, val in mapping.items():
        target_to_keys[val].append(key)

    for offset, (sera, name, nforms, ubase, desc) in CONSONANT_BASES.items():
        base = 0x1200 + offset
        n_mapped = 0
        n_aliases = 0

        is_labiovelar = (nforms == 5)
        if is_labiovelar:
            vowel_offsets = LABIOVELAR_VOWEL_OFFSETS
        else:
            vowel_offsets = range(7)

        for vi in vowel_offsets:
            cp = base + vi
            ch = chr(cp)
            if ch in target_to_keys:
                n_mapped += 1
                n_aliases += len(target_to_keys[ch])

        status = "OK" if n_mapped == nforms else f"MISSING {nforms - n_mapped}"
        lines.append(
            f"{sera:<8} {name:<12} {n_mapped:>2}/{nforms:<3} mapped   "
            f"{n_aliases:>3} aliases     {status}"
        )

    return "\n".join(lines)


def alias_report(mapping):
    """Show all creative alias groups (characters with >1 key sequence)."""
    target_to_keys = defaultdict(list)
    for key, val in mapping.items():
        target_to_keys[val].append(key)

    lines = []
    multi = {ch: keys for ch, keys in target_to_keys.items() if len(keys) > 1}
    for ch, keys in sorted(multi.items(), key=lambda x: -len(x[1])):
        cp = ord(ch)
        lines.append(f"  {ch} U+{cp:04X}: {len(keys)} aliases → {', '.join(repr(k) for k in keys[:10])}{' ...' if len(keys) > 10 else ''}")
    return "\n".join(lines)


def missing_report(mapping):
    """Show Ethiopic codepoints in Amharic-relevant range that have no mapping."""
    targets = set(mapping.values())
    missing = []
    for offset, (sera, name, nforms, ubase, desc) in CONSONANT_BASES.items():
        base = 0x1200 + offset
        is_labiovelar = (nforms == 5)
        offsets = LABIOVELAR_VOWEL_OFFSETS if is_labiovelar else range(7)
        for vi in offsets:
            ch = chr(base + vi)
            if ch not in targets:
                missing.append(f"  {ch} U+{base+vi:04X} — {name} vowel-{vi} ({VOWEL_NAMES.get(vi, '?')})")

    # Also check punctuation range U+1361–U+1368
    for cp in range(0x1361, 0x1369):
        ch = chr(cp)
        if ch not in targets:
            missing.append(f"  {ch} U+{cp:04X} — Ethiopic punctuation")

    # Numerals U+1369–U+137C
    for cp in range(0x1369, 0x137D):
        ch = chr(cp)
        if ch not in targets:
            missing.append(f"  {ch} U+{cp:04X} — Ethiopic numeral")

    if missing:
        return "Unmapped Ethiopic characters:\n" + "\n".join(missing)
    return "All Amharic-relevant Ethiopic characters have at least one mapping."


def main():
    mapping = load_mapping()
    errors = validate(mapping)

    if errors:
        print("VALIDATION ERRORS:")
        for e in errors:
            print(f"  - {e}")
        print()

    if "--coverage" in sys.argv:
        print(coverage_report(mapping))
        print()
    elif "--aliases" in sys.argv:
        print("Characters with creative alias variants:")
        print(alias_report(mapping))
        print()
    elif "--missing" in sys.argv:
        print(missing_report(mapping))
        print()
    else:
        # Default: brief summary
        print(f"Mapping file: {MAPPING_PATH}")
        print(f"Total key sequences: {len(mapping)}")
        print(f"Unique Ethiopic characters: {len([v for v in set(mapping.values()) if len(v) == 1 and 0x1200 <= ord(v) <= 0x137F])}")
        print(f"Errors: {len(errors)}")
        if errors:
            for e in errors:
                print(f"  - {e}")


if __name__ == "__main__":
    main()
