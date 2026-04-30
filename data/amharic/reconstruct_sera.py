#!/usr/bin/env python3
"""
Reconstruct sera.json from the canonical m17n am-sera.mim mapping,
plus an additional '3'-suffix delimiter for disambiguation.

Design decisions:
  1. Extract exact SERA mapping from m17n am-sera.mim
  2. Remove non-ASCII keys and empty-string entries
  3. Preserve m17n '`' prefix and '2' suffix conventions
  4. Add '3' suffix as an additional delimiter (mirrors '2',
     easier to type — adjacent key on QWERTY)
  5. Sort keys by length then alphabetically for correct trie order
"""

import json
import re
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
MIM_PATH = SCRIPT_DIR / "m17n-am-sera.mim"
OUT_PATH = SCRIPT_DIR / "sera.json"


def parse_mim(filepath):
    """Parse m17n am-sera.mim and return {keyseq: value_str} dict."""
    mappings = {}
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    for m in re.finditer(r'\("([^"]*)"\s+(\?.)\s*\)', content):
        mappings[m.group(1)] = m.group(2)[1]

    for m in re.finditer(r'\("([^"]*)"\s+"([^"]*)"\s*\)', content):
        mappings[m.group(1)] = m.group(2)

    return mappings


def build_mapping():
    """Build the complete SERA mapping."""
    m17n = parse_mim(MIM_PATH)

    out = {}

    # ── Clean m17n entries ──
    Y_VARIANT_KEYS = {"MY", "KY", "kY", "CY", "cY", "TY", "tY", "PY", "pY",
                       "mY", "rY", "MYa", "RYa", "mYa", "rYa"}
    for key, val in m17n.items():
        if val == "":
            continue
        if not all(32 <= ord(c) <= 126 for c in key):
            continue
        if key in Y_VARIANT_KEYS:
            continue
        out[key] = val

    # ═══════════════════════════════════════════════════════════
    # '3'-SUFFIX DELIMITER — mirrors m17n's '2' suffix
    #
    # Families that need disambiguation (doubled-consonant ambiguity):
    #   s  vs ss  → plain vs ś  (ሠ)
    #   h  vs hh  → plain vs ḫ  (ኀ)
    #   S  vs SS  → ṣ vs ṣ́    (ፀ)
    #
    # Pattern: <consonant>3<vowel>  — same as m17n's <consonant>2<vowel>
    #   h3e → ኀ   (same as h2e, hhe)
    #   s3e → ሠ   (same as s2e, sse)
    #   S3e → ፀ   (same as S2e, SSe)
    # Also for aynu vowels: e3 → ዐ (same as e2, `e)
    # ═══════════════════════════════════════════════════════════

    def add_3suffix_family(plain_stem, alt_base_char, has_2suffix=True):
        """Add '3'-suffix entries mirroring the '2'-suffix convention.
        
        plain_stem: the plain consonant letter (e.g., 's', 'h', 'S')
        alt_base_char: 1st vowel form of the alternate family (e.g., 'ሠ', 'ኀ', 'ፀ')
        has_2suffix: if True, skip 2-suffix entries already in m17n
        """
        base = ord(alt_base_char)

        # 7 vowel forms: consonant + '3' + vowel suffix
        out[plain_stem + "3e"] = chr(base + 0)   # ä
        out[plain_stem + "3u"] = chr(base + 1)   # u
        out[plain_stem + "3i"] = chr(base + 2)   # i
        out[plain_stem + "3a"] = chr(base + 3)   # a
        out[plain_stem + "3E"] = chr(base + 4)   # e
        out[plain_stem + "3ee"] = chr(base + 4)  # e alias
        out[plain_stem + "3"] = chr(base + 5)    # ə (bare consonant + 3)
        out[plain_stem + "3o"] = chr(base + 6)   # o

    # ś family (ሠ, U+1220): s3 → ś
    add_3suffix_family("s", "ሠ")
    # sW labiovelar: s3W → ሧ (U+1227, single form at base+7)
    out["s3W"] = chr(ord("ሠ") + 7)
    out["s3Wa"] = chr(ord("ሠ") + 7)

    # ḫ family (ኀ, U+1280): h3 → ḫ
    add_3suffix_family("h", "ኀ")
    # hW labiovelar block (U+1288..128D): h3W → same as h2W/hhW
    lw_hh = ord("ኀ") + 8
    out["h3We"] = chr(lw_hh + 0)
    out["h3Wi"] = chr(lw_hh + 2)
    out["h3W"] = chr(lw_hh + 3)
    out["h3Wa"] = chr(lw_hh + 3)
    out["h3WE"] = chr(lw_hh + 4)
    out["h3Wee"] = chr(lw_hh + 4)
    out["h3Wu"] = chr(lw_hh + 5)
    out["h3W'"] = chr(lw_hh + 5)

    # ṣ́ family (ፀ, U+1340): S3 → ṣ́
    add_3suffix_family("S", "ፀ")
    # SW labiovelar: S3W → ጿ (same as S2W/SSW, per m17n convention)
    out["S3W"] = "ጿ"
    out["S3Wa"] = "ጿ"

    # ʿ/aynu vowel family (ዐ, U+12D0): vowel3 → aynu
    aynu_base = ord("ዐ")
    for vi, vch in enumerate(["e", "u", "i", "a", "E", "ee", "", "o"]):
        if vi == 5:  # ə form: bare digit
            out["e3"] = chr(aynu_base + 0)   # wait, e3 → ä form
            # Actually: aynu vowel mapping is different — vowels directly + 3
            # Let me redo this properly
            pass

    # Aynu 3-suffix entries:
    # In m17n: e2 → ዐ (ä), u2 → ዑ, i2 → ዒ, a2 → ዓ, E2 → ዔ, I2 → ዕ, o2 → ዖ
    # Mirror: e3 → ዐ, u3 → ዑ, i3 → ዒ, a3 → ዓ, E3 → ዔ, I3 → ዕ, o3 → ዖ
    out["e3"] = "ዐ"    # ä
    out["u3"] = "ዑ"    # u
    out["i3"] = "ዒ"    # i
    out["a3"] = "ዓ"    # a
    out["E3"] = "ዔ"    # e
    out["ee3"] = "ዔ"   # e alias
    out["I3"] = "ዕ"    # ə
    out["o3"] = "ዖ"    # o

    # Also uppercase variants for aynu 3-suffix
    out["U3"] = "ዑ"
    out["A3"] = "ዓ"
    out["O3"] = "ዖ"

    # ── Fix punctuation entries that the simple regex may have missed ──
    punct_fixes = {
        ";": "፤",
        ";;": ";",
        ":": "፡",
        "::": "።",
        ":::": ":",
        ".": "።",
        "...": ".",
        ",": "፣",
        ",,": ",",
        "-:": "፥",
        ":-": "፦",
        "*": "*",
        "**": "፨",
        ":|:": "፨",
        "??": "፧",
        "???": "?",
    }
    for k, v in punct_fixes.items():
        if k not in out:
            out[k] = v

    # ── Ensure every ASCII letter has a mapping ──
    import string
    for ch in string.ascii_lowercase + string.ascii_uppercase:
        if ch not in out:
            ch_lower = ch.lower()
            if ch_lower in out:
                out[ch] = out[ch_lower]

    # ── Final cleanup: no non-ASCII keys ──
    for key in list(out.keys()):
        if not all(32 <= ord(c) <= 126 for c in key):
            del out[key]

    return out


def verify_mapping(mapping):
    """Check for missing Ethiopic characters."""
    from validate_sera import CONSONANT_BASES, LABIOVELAR_VOWEL_OFFSETS
    targets = set(mapping.values())
    missing = []
    for offset, (sera, name, nforms, ubase, desc) in CONSONANT_BASES.items():
        base = 0x1200 + offset
        offsets = LABIOVELAR_VOWEL_OFFSETS if nforms == 5 else range(7)
        for vi in offsets:
            ch = chr(base + vi)
            if ch not in targets:
                missing.append(f"  {ch} U+{base+vi:04X} — {name}")
    if missing:
        print(f"WARNING: {len(missing)} missing Ethiopic chars:")
        for m in missing:
            print(m)
    else:
        print("All Ethiopic families fully covered.")


def build_json():
    mapping = build_mapping()
    verify_mapping(mapping)

    def sort_key(k):
        v = mapping[k]
        cp = ord(v[0]) if v else 0
        return (cp, len(k), k)
    sorted_keys = sorted(mapping.keys(), key=sort_key)
    sorted_map = {k: mapping[k] for k in sorted_keys}

    data = {
        "input_method": "am-sera",
        "title": "Amharic (SERA)",
        "description": (
            "Amharic input using SERA (System for Ethiopic Representation in ASCII) "
            "transliteration. Based on m17n am-sera.mim by Anthony et al. "
            "Delimiters for alternative consonant forms: "
            "backtick prefix ('`'), number '2' suffix (from m17n), "
            "and number '3' suffix (adjacent to 2 on keyboard). "
            "Examples: s2e / s3e → ሠ, h2e / h3e → ኀ, S2e / S3e → ፀ. "
            "Uppercase maps to emphatic/palatal forms where distinct; "
            "otherwise both cases produce the same Ethiopic character."
        ),
        "version": "2.1.0",
        "based_on": "m17n MIM/am-sera.mim + 3-suffix delimiter",
        "states": {
            "init": {
                "map": sorted_map
            }
        }
    }
    return data


def main():
    data = build_json()

    with open(OUT_PATH, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    m = data['states']['init']['map']
    eth = [v for v in set(m.values()) if len(v) == 1 and 0x1200 <= ord(v) <= 0x137F]
    print(f"Wrote {len(m)} entries to {OUT_PATH}")
    print(f"  Unique Ethiopic chars: {len(eth)}")


if __name__ == "__main__":
    main()
