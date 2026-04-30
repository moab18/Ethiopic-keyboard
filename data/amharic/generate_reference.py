#!/usr/bin/env python3
"""
Regenerate sera-reference.txt from sera.json.
"""
import json
from pathlib import Path
from collections import defaultdict

SCRIPT_DIR = Path(__file__).resolve().parent
JSON_PATH = SCRIPT_DIR / "sera.json"
REF_PATH = SCRIPT_DIR / "sera-reference.txt"

FAMILY_ORDER = [
    ("h",   "ሀ", "h / h",       "plain h"),
    ("l",   "ለ", "l / l",       "plain l"),
    ("H",   "ሐ", "H / ḥ / ħ",   "emphatic h"),
    ("m",   "መ", "m / m",       "plain m"),
    ("ss",  "ሠ", "ss / ś",      "archaic s (ś)"),
    ("r",   "ረ", "r / r",       "plain r"),
    ("s",   "ሰ", "s / s",       "plain s"),
    ("S",   "ሸ", "S / sh / š",   "sh sound"),
    ("Q",   "ቀ", "Q / kʼ / q",   "ejective k"),
    ("QW",  "ቈ", "QW / kʼW / qʷ","labiovelar q (5 vowels)"),
    ("b",   "በ", "b / b",       "plain b"),
    ("v",   "ቨ", "v / V",       "v (loanwords)"),
    ("t",   "ተ", "t / t",       "plain t"),
    ("C",   "ቸ", "C / ch / č",   "ch sound"),
    ("x",   "ኀ", "x / h2 / ḫ / kh","velar fricative"),
    ("xW",  "ኈ", "xW / hW / ḫʷ", "labiovelar ḫ (5 vowels)"),
    ("n",   "ነ", "n / n",       "plain n"),
    ("N",   "ኘ", "N / ny / gn / ñ","palatal n"),
    ("a",   "አ", "a / ʼa / ʾ",  "glottal stop (standalone vowels)"),
    ("k",   "ከ", "k / k",       "plain k"),
    ("kW",  "ኰ", "kW / kʷ",     "labiovelar k (5 vowels)"),
    ("K",   "ኸ", "K / kh / ḵ",   "velar fricative"),
    ("w",   "ወ", "w / w",       "plain w"),
    ("A",   "ዐ", "A / ʿ / ʼA",  "pharyngeal (standalone vowels)"),
    ("z",   "ዘ", "z / z",       "plain z"),
    ("Z",   "ዠ", "Z / zh / ž",   "zh sound"),
    ("y",   "የ", "y / y",       "plain y"),
    ("d",   "ደ", "d / d",       "plain d"),
    ("j",   "ጀ", "j / J",       "j sound"),
    ("g",   "ገ", "g / g",       "plain g"),
    ("gW",  "ጐ", "gW / gʷ",     "labiovelar g (5 vowels)"),
    ("T",   "ጠ", "T / tʼ / ṭ",  "ejective t"),
    ("CC",  "ጨ", "CC / Ch / chʼ / č̣","ejective ch"),
    ("PP",  "ጰ", "PP / Ph / pʼ / p̣","ejective p"),
    ("SS",  "ጸ", "SS / Ts / tsʼ / ṣ","ejective ts"),
    ("SSs", "ፀ", "SSs / ṣ́",     "emphatic s variant (archaic)"),
    ("f",   "ፈ", "f / f",       "plain f"),
    ("p",   "ፐ", "p / p",       "plain p"),
]

with open(JSON_PATH) as f:
    data = json.load(f)

mapping = data["states"]["init"]["map"]
target_to_keys = defaultdict(list)
for k, v in mapping.items():
    target_to_keys[v].append(k)

lines = []
W = 72

def hdr(text):
    lines.append("")
    lines.append("═" * W)
    lines.append(f"  {text}")
    lines.append("═" * W)

def vow(c):
    """Get the vowel suffix from a key for a given consonant prefix length."""
    return c

def pick_primary(prefix, cp):
    """Pick the shortest key from the aliases list."""
    candidates = target_to_keys.get(cp, [prefix])
    # Shortest is usually the primary
    primary = min(candidates, key=len)
    return primary, [c for c in candidates if c != primary]

# ============================================================
lines.append("# Amharic SERA — ASCII to Ethiopic Transliteration Reference")
lines.append("#")
lines.append("# Vowel key: e=ä  u=u  i=i  a=a  E/ee=e  (bare consonant)=ə  o=o")
lines.append("# Labiovelar: W suffix (We=ä Wi=i W/Wa=a WE/ee=e Wu=ə)")
lines.append("# Each family lists the primary form; creative aliases follow.")
lines.append("# =============================================================================")

for sera, eth_first, key_desc, desc in FAMILY_ORDER:
    # Find the base character (1st order)
    base_cp = ord(eth_first)
    # Determine if labiovelar
    is_lab = sera.endswith("W") or "W" in sera

    hdr(f"{sera} — {eth_first} family  ({key_desc} — {desc})")

    if is_lab:
        vowel_variants = [
            (0, "We", "ä 1st (labiovelar base)"),
            (2, "Wi", "i 3rd"),
            (3, "W",  "a 4th (bare W)"),
            (3, "Wa", "a 4th"),
            (4, "WE/Wee", "e 5th"),
            (5, "Wu", "ə 6th"),
        ]
    else:
        vowel_variants = [
            (0, "e",     "ä 1st"),
            (1, "u",     "u 2nd"),
            (2, "i",     "i 3rd"),
            (3, "a",     "a 4th"),
            (4, "E/ee",  "e 5th"),
            (5, "",      "ə 6th (bare consonant)"),
            (6, "o",     "o 7th"),
        ]

    primary_line = ""
    primary_bare = {}
    primary_second = {}

    for vo, vsuffix, vlabel in vowel_variants:
        if vsuffix == "":
            # 6th form — just the bare consonant
            cp = base_cp + vo
            ch = chr(cp)
            primary = sera
            if ch in target_to_keys:
                primary_bare[vo] = sera
            primary_line += f"  {sera:<8}= {ch}    "
        elif vsuffix == "WE/Wee":
            cp = base_cp + vo
            ch = chr(cp)
            primary_line += f"  {sera}E = {ch}    {sera}ee= {ch}    "
        elif vsuffix == "E/ee":
            cp = base_cp + vo
            ch = chr(cp)
            primary_line += f"  {sera}E = {ch}    {sera}ee= {ch}    "
        elif vsuffix == "Wa":
            cp = base_cp + vo
            ch = chr(cp)
            # Already shown as bare W above, just note
            primary_bare[vo] = sera + "a"
        elif vsuffix == "W":
            cp = base_cp + vo
            ch = chr(cp)
            primary_line += f"  {sera}W = {ch}    "
        else:
            cp = base_cp + vo
            ch = chr(cp)
            key = sera + vsuffix
            primary_line += f"  {key:<8}= {ch}    "

    lines.append("  " + primary_line.strip())

    # Gather creative aliases for this family
    family_cps = set()
    if is_lab:
        for vo in [0, 2, 3, 4, 5]:
            family_cps.add(base_cp + vo)
    else:
        for vo in range(7):
            family_cps.add(base_cp + vo)

    family_aliases = defaultdict(list)
    for cp_val in family_cps:
        ch = chr(cp_val)
        if ch in target_to_keys:
            for k in target_to_keys[ch]:
                # Skip the primary form(s)
                primary_form = sera
                if is_lab:
                    # various vowel patterns
                    vmap = {0: sera + "We", 2: sera + "Wi", 3: sera + "W",
                            3: sera + "Wa", 4: sera + "WE", 5: sera + "Wu"}
                    primaries = [sera + "We", sera + "Wi", sera + "W",
                                 sera + "Wa", sera + "WE", sera + "Wee", sera + "Wu"]
                else:
                    primaries = [sera + s for s in ["e", "u", "i", "a", "E", "ee", "o"]] + [sera]
                if k not in primaries:
                    family_aliases[cp_val].append(k)

    if any(family_aliases.values()):
        # Group aliases by their consonant prefix pattern
        alias_groups = defaultdict(lambda: defaultdict(list))
        for cp_val, keys in family_aliases.items():
            ch = chr(cp_val)
            for k in keys:
                # Extract the "family" of the alias — e.g., "hh" → H-family aliases
                # Group by the alias root (drop vowel suffix)
                if k.endswith("ee"):
                    root = k[:-2]
                elif len(k) > 0 and k[-1] in "euioaEI":
                    root = k[:-1]
                else:
                    root = k
                alias_groups[cp_val][root].append(k)

        lines.append("")
        lines.append("  Aliases:")

        # Find distinct alias families (e.g., "hh", "H2", "ḥ")
        alias_root_sets = defaultdict(list)
        for cp_val, roots in family_aliases.items():
            ch = chr(cp_val)
            for k in keys:
                # Determine the alias root (consonant part)
                if k.endswith("ee"):
                    root = k[:-2]
                elif k[-1:].isalpha() and k[-1:] in "aeiouAEIO":
                    root = k[:-1]
                else:
                    root = k
                if root not in alias_root_sets:
                    alias_root_sets[root] = []
                if k not in alias_root_sets[root]:
                    alias_root_sets[root].append(k)

        seen_roots = set()
        for cp_val in sorted(family_cps):
            ch = chr(cp_val)
            if ch not in family_aliases:
                continue
            keys = sorted(family_aliases[ch], key=len)
            # Group by root
            by_root = defaultdict(list)
            for k in keys:
                if k.endswith("ee"):
                    root = k[:-2]
                elif k[-1:].isalpha() and k[-1:] in "aeiouAEIO":
                    root = k[:-1]
                else:
                    root = k
                by_root[root].append(k)

        # Deduplicate the display — show one line per alias root
        alias_displayed = set()
        for cp_val in sorted(family_cps):
            ch = chr(cp_val)
            if ch not in family_aliases:
                continue
            keys_for_char = sorted(family_aliases[ch], key=len)
            if not keys_for_char:
                continue
            # Build display: show root then list all its vowel forms
            roots_seen = set()
            display_parts = []
            for k in keys_for_char:
                if k.endswith("ee"):
                    root = k[:-2]
                elif k[-1:].isalpha() and k[-1:] in "aeiouAEIO":
                    root = k[:-1]
                else:
                    root = k
                if root not in roots_seen:
                    roots_seen.add(root)
                    # Find all keys with this root
                    root_keys = [rk for rk in keys_for_char
                                 if rk.startswith(root) and (rk == root or rk[-1:] in "aeiouAEIO" or rk.endswith("ee"))]
                    if root not in alias_displayed:
                        alias_displayed.add(root)
                        display_parts.append(f"{root}e, {root}u …")

            if display_parts:
                # Actually let me simplify — just list keys compactly
                pass

        # Simplified approach: for each cp, show its alias keys
        shown = set()
        for cp_val in sorted(family_cps):
            ch = chr(cp_val)
            if ch not in family_aliases:
                continue
            alias_str = "  ".join(f"{k} = {ch}" for k in sorted(family_aliases[ch], key=len))
            if alias_str not in shown:
                shown.add(alias_str)
                lines.append(f"    {alias_str}")

lines.append("")
lines.append("")
lines.append("╔" + "═" * (W-2) + "╗")
lines.append("║  PUNCTUATION" + " " * (W-20) + "║")
lines.append("╚" + "═" * (W-2) + "╝")
lines.append("")

punct_map = {
    "፡": ("word space", [":"]),
    "።": ("full stop", ["::", ":-"]),
    "፣": ("comma", [","]),
    "፤": ("semicolon", [";", ":-"]),
    "፥": ("colon", ["-:"]),
    "፦": ("preface colon", ["-::"]),
    "፧": ("question mark", ["-:::", "?"]),
    "፨": ("paragraph separator", ["-::::"]),
}
for ch, (desc, keys) in punct_map.items():
    if ch in mapping:
        lines.append(f"  {'  |  '.join(keys):<24} = {ch}    {desc}")

lines.append("")
lines.append("")
lines.append("╔" + "═" * (W-2) + "╗")
lines.append("║  QUOTATION MARKS" + " " * (W-22) + "║")
lines.append("╚" + "═" * (W-2) + "╝")
lines.append("")

quote_map = {
    "\u00ab": ("left guillemet", ['""']),
    "\u00bb": ("right guillemet", ['"']),
    "\u2018": ("right curly single", ["'", "`'"]),
    "\u201c": ("left curly double", ["''", "``"]),
}
for ch, (desc, keys) in quote_map.items():
    if ch in mapping:
        lines.append(f"  {'  |  '.join(keys):<24} = {ch}    {desc}")

lines.append("")
lines.append("")
lines.append("╔" + "═" * (W-2) + "╗")
lines.append("║  ETHIOPIC NUMERALS  (prefix with backtick)" + " " * (W-44) + "║")
lines.append("╚" + "═" * (W-2) + "╝")
lines.append("")

num_keys = sorted([k for k in mapping if k.startswith("`") and len(k) > 1 and k[1:].isdigit()],
                  key=lambda x: int(x[1:]) if x[1:].isdigit() else 0)
if num_keys:
    for k in num_keys:
        lines.append(f"  {k:<12} = {mapping[k]}")

lines.append("")
lines.append("")
lines.append("╔" + "═" * (W-2) + "╗")
lines.append("║  VOWEL SUFFIX REFERENCE" + " " * (W-27) + "║")
lines.append("╚" + "═" * (W-2) + "╝")
lines.append("")
lines.append("  Order   Vowel        Suffix")
lines.append("  ─────   ─────        ──────")
lines.append("  1st     ä  [ə]       e")
lines.append("  2nd     u  [u]       u")
lines.append("  3rd     i  [i]       i")
lines.append("  4th     a  [aː]      a")
lines.append("  5th     e  [e]       E  (aliases: ee)")
lines.append("  6th     ə  [ɨ]       bare consonant")
lines.append("  7th     o  [o]       o")
lines.append("")
lines.append("  Labiovelar W suffix (vowel shifts after W):")
lines.append("    We     = ä (1st)      Wi     = i (3rd)")
lines.append("    W/Wa   = a (4th)      WE/ee  = e (5th)")
lines.append("    Wu     = ə (6th)")

lines.append("")
lines.append("")
lines.append("╔" + "═" * (W-2) + "╗")
lines.append("║  CONSONANT CHEAT SHEET" + " " * (W-27) + "║")
lines.append("╚" + "═" * (W-2) + "╝")
lines.append("")
lines.append(f"  {'SERA':<10} {'Sound':<18} {'Character':<10}  Aliases")
lines.append(f"  {'────':<10} {'─────':<18} {'─────────':<10}  ───────")
for sera, eth_first, key_desc, desc in FAMILY_ORDER:
    char = chr(ord(eth_first))
    primary = sera
    # Gather all alias roots for this family's 1st form
    cp = ord(eth_first)
    ch = chr(cp)
    aliases = sorted([k for k in target_to_keys.get(ch, []) if k != sera], key=len)
    alias_str = ", ".join(aliases[:8])
    if len(aliases) > 8:
        alias_str += " …"
    lines.append(f"  {sera:<10} {desc:<18} {char:<10}  {alias_str}")

lines.append("")
lines.append("")
lines.append("═" * W)
lines.append(f"  Source: m17n MIM/am-sera.mim  ·  Unicode Ethiopic U+1200–U+137F")
lines.append(f"  Project: ethiopic-keyboard  ·  data/amharic/sera.json  ({len(mapping)} mappings)")
lines.append("═" * W)

with open(REF_PATH, "w") as f:
    f.write("\n".join(lines) + "\n")

print(f"Wrote {REF_PATH} ({len(lines)} lines)")
