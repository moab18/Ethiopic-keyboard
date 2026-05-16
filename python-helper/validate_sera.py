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
MAPPING_PATH = SCRIPT_DIR.parent / "data" / "amharic" / "am-sera.json"

# Vowel suffix naming
VOWEL_NAMES = {0: "ä", 1: "u", 2: "i", 3: "a", 4: "e", 5: "ə", 6: "o"}

# Amharic consonant base offsets from U+1200
# Labels are descriptive family names, not SERA key conventions
CONSONANT_BASES = {
    0x000: ("hə",     "h-family (ሀ–ሆ)",      7, "U+1200"),
    0x008: ("lə",     "l-family (ለ–ሎ)",      7, "U+1208"),
    0x010: ("ḥə",     "ḥ-family (ሐ–ሖ)",      7, "U+1210"),
    0x018: ("mə",     "m-family (መ–ሞ)",      7, "U+1218"),
    0x020: ("śə",     "ś-family (ሠ–ሦ)",      7, "U+1220"),
    0x028: ("rə",     "r-family (ረ–ሮ)",      7, "U+1228"),
    0x030: ("sə",     "s-family (ሰ–ሶ)",      7, "U+1230"),
    0x038: ("šə",     "š-family (ሸ–ሾ)",      7, "U+1238"),
    0x040: ("qə",     "q-family (ቀ–ቆ)",      7, "U+1240"),
    0x048: ("qʷə",    "qʷ labiovelar (ቈ–ቍ)", 5, "U+1248"),
    0x060: ("bə",     "b-family (በ–ቦ)",      7, "U+1260"),
    0x068: ("və",     "v-family (ቨ–ቮ)",      7, "U+1268"),
    0x070: ("tə",     "t-family (ተ–ቶ)",      7, "U+1270"),
    0x078: ("čə",     "č-family (ቸ–ቾ)",      7, "U+1278"),
    0x080: ("ḫə",     "ḫ-family (ኀ–ኆ)",      7, "U+1280"),
    0x088: ("ḫʷə",    "ḫʷ labiovelar (ኈ–ኍ)", 5, "U+1288"),
    0x090: ("nə",     "n-family (ነ–ኖ)",      7, "U+1290"),
    0x098: ("ñə",     "ñ-family (ኘ–ኞ)",      7, "U+1298"),
    0x0A0: ("ʾə",     "ʾ glottal (አ–ኦ)",     7, "U+12A0"),
    0x0A8: ("kə",     "k-family (ከ–ኮ)",      7, "U+12A8"),
    0x0B0: ("kʷə",    "kʷ labiovelar (ኰ–ኵ)", 5, "U+12B0"),
    0x0B8: ("ḵə",     "ḵ-family (ኸ–ኾ)",      7, "U+12B8"),
    0x0C8: ("wə",     "w-family (ወ–ዎ)",      7, "U+12C8"),
    0x0D0: ("ʿə",     "ʿ pharyngeal (ዐ–ዖ)",  7, "U+12D0"),
    0x0D8: ("zə",     "z-family (ዘ–ዞ)",      7, "U+12D8"),
    0x0E0: ("žə",     "ž-family (ዠ–ዦ)",      7, "U+12E0"),
    0x0E8: ("yə",     "y-family (የ–ዮ)",      7, "U+12E8"),
    0x0F0: ("də",     "d-family (ደ–ዶ)",      7, "U+12F0"),
    0x0F8: ("jə",     "j-family (ጀ–ጆ)",      7, "U+12F8"),
    0x100: ("gə",     "g-family (ገ–ጎ)",      7, "U+1300"),
    0x108: ("gʷə",    "gʷ labiovelar (ጐ–ጕ)", 5, "U+1308"),
    0x120: ("ṭə",     "ṭ-family (ጠ–ጦ)",      7, "U+1320"),
    0x128: ("č̣ə",     "č̣-family (ጨ–ጮ)",      7, "U+1328"),
    0x130: ("p̣ə",     "p̣-family (ጰ–ጶ)",      7, "U+1330"),
    0x138: ("ṣə",     "ṣ-family (ጸ–ጾ)",      7, "U+1338"),
    0x140: ("ś́ə",     "ś́-family (ፀ–ፆ)",      7, "U+1340"),
    0x148: ("fə",     "f-family (ፈ–ፎ)",      7, "U+1348"),
    0x150: ("pə",     "p-family (ፐ–ፖ)",      7, "U+1350"),
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

    PASSTHROUGH_OK = ["'", "*", ",", ".", "?", ":", ";", "/", " ", "\t"]
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

    for offset, (sera, name, nforms, ubase) in CONSONANT_BASES.items():
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
    for offset, (sera, name, nforms, ubase) in CONSONANT_BASES.items():
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
