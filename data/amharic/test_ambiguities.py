#!/usr/bin/env python3
"""
Ambiguities test suite for the Amharic SERA mapping.

Validates that the trie engine produces correct output for:
  1. All individual key→character mappings
  2. Concatenated sequences with prefix ambiguities
  3. Explicit commit-delimiter resolution ('|')
  4. Double-consonant disambiguation (ss vs s, hh vs h, etc.)

Usage:
  python3 test_ambiguities.py          # run all tests
  python3 test_ambiguities.py --list   # list ambiguous prefixes only
"""

import json
import sys
from pathlib import Path
from collections import defaultdict

SCRIPT_DIR = Path(__file__).resolve().parent
JSON_PATH = SCRIPT_DIR / "sera.json"


def load_mapping():
    with open(JSON_PATH, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data["states"]["init"]["map"]


def build_trie(mapping):
    trie = {}
    for key, val in mapping.items():
        node = trie
        for ch in key:
            node = node.setdefault(ch, {})
        node["\0"] = val
    return trie


def build_reverse_map(mapping):
    rev = defaultdict(list)
    for key, val in mapping.items():
        rev[val].append(key)
    for val in rev:
        rev[val].sort(key=len)
    return dict(rev)


def find_ambiguous_prefixes(mapping):
    """
    Find all single-character mappings that are prefixes of longer mappings with
    DIFFERENT target characters. These require commit-timing care.
    """
    ambiguous = defaultdict(list)
    for key, val in mapping.items():
        if len(key) == 1:
            for k2, v2 in mapping.items():
                if len(k2) > 1 and k2.startswith(key) and v2 != val:
                    ambiguous[key].append((k2, v2))
    # Sort extensions by length
    for k in ambiguous:
        ambiguous[k].sort(key=lambda x: len(x[0]))
    return dict(ambiguous)


def simulate_longest_match(key_seq, trie):
    """
    Longest-prefix-match engine:
    Walks trie as far as possible per input.  Commits when a terminal
    has no further submaps.  Waits when a terminal has submaps.
    On input exhaustion at a terminal, commits.
    """
    output = []
    node = trie
    i = 0
    while i < len(key_seq):
        ch = key_seq[i]
        if ch in node:
            node = node[ch]
            i += 1
            if "\0" in node:
                has_kids = any(k != "\0" for k in node)
                if not has_kids:
                    output.append(node["\0"])
                    node = trie
                elif i >= len(key_seq):
                    output.append(node["\0"])
            elif i >= len(key_seq):
                output.append(ch)
        else:
            if "\0" in node:
                output.append(node["\0"])
                node = trie
            else:
                output.append(ch)
                node = trie
                i += 1
    return "".join(output)


def test_basic_mappings(mapping, trie):
    """Test 1: Every key sequence in the mapping produces its target."""
    failures = []
    for key, expected in mapping.items():
        result = simulate_longest_match(key, trie)
        if result != expected:
            failures.append((key, expected, result))
    return failures


def test_prefix_ambiguities(mapping, trie, reverse_map):
    """
    Test 2: Concatenated sequences with potential prefix conflicts.
    For each ambiguous single-char prefix, test sequences like:
      prefix + next_char's_primary
    and verify the engine resolves boundaries correctly.
    """
    ambiguous = find_ambiguous_prefixes(mapping)
    failures = []

    for prefix, extensions in ambiguous.items():
        prefix_val = mapping[prefix]

        # Case A: prefix followed by a non-matching char commits prefix,
        # then the next char starts a new lookup.
        # Example: "sx" → engine sees "s" commits ስ, then "x" starts new lookup

        # Test with a consonant that has its own family
        next_cons = "l"  # arbitrary consonant not overlap-extending 's'
        seq = prefix + next_cons
        result = simulate_longest_match(seq, trie)
        expected = prefix_val + simulate_longest_match(next_cons, trie)
        if result != expected:
            failures.append((f"prefix+next({seq})", expected, result))

    return failures


def test_specific_ambiguities(mapping, trie, reverse_map):
    """
    Test 3: Specific known-ambiguous sequences.
    These test the critical double-consonant disambiguation cases.
    """
    failures = []
    reverse = reverse_map

    tests = [
        # Double-consonant: "ss" should give ś-family, not s+s
        ("ss", "ሥ"),
        ("sse", "ሠ"),
        ("ssu", "ሡ"),
        ("ssi", "ሢ"),
        ("ssa", "ሣ"),
        ("ssE", "ሤ"),
        ("ss", "ሥ"),
        ("sso", "ሦ"),

        # Plain s with next consonant: should commit s first, then next
        ("sl", "ስል"),  # s(ə) + l(ə)

        # hh should give ḫ-family
        ("hh", "ኅ"),
        ("hhe", "ኀ"),

        # SS should give ṣ́-family
        ("SS", "ፅ"),
        ("SSe", "ፀ"),

        # S alone → ጽ (ejective ts 6th form), s → ስ (plain s 6th form)
        ("Ss", "ጽስ"),

        # Vowel disambiguation: "ea" → ኧ (not e+a) since "ea" is a defined mapping
        ("ea", "ኧ"),

        # "eee" — "ee" maps to ዕ (aynu-ə), "e" maps to እ (alef-ə)
        # Actually "ee" → ዕ, so "eee" → ዕ + እ
        ("eee", "ዕእ"),

        # Bare consonant vs vowel sequence: "he" → ሀ, "h" → ህ
        # "he" should give ሀ not ህ+እ
        ("he", "ሀ"),
        ("hel", "ሀል"),  # he commits ሀ, then l → ል

        # fY / FY → ፚ (archaic fYa)
        ("fY", "ፚ"),
        ("fYa", "ፚ"),
        ("FY", "ፚ"),
        ("FYa", "ፚ"),

        # Backtick-prefixed: `he → ኀ family
        ("`he", "ኀ"),
        ("`ha", "ኃ"),

        # 2-suffix: h2e → ኀ
        ("h2e", "ኀ"),
        ("h2a", "ኃ"),

        # 3-suffix: h3e → ኀ
        ("h3e", "ኀ"),
        ("h3a", "ኃ"),

        # s2 vs s: s2e → ś-family ሠ, se → plain s ሰ
        ("s2e", "ሠ"),
        ("s2a", "ሣ"),
        ("se", "ሰ"),
        ("sa", "ሳ"),

        # S2 vs S: S2e → ṣ́-family ፀ, Se → ṣ-family ጸ
        ("S2e", "ፀ"),
        ("S2a", "ፃ"),
        ("Se", "ጸ"),
        ("Sa", "ጻ"),

        # s3 and S3 variants
        ("s3e", "ሠ"),
        ("S3e", "ፀ"),
    ]

    for seq, expected in tests:
        result = simulate_longest_match(seq, trie)
        if result != expected:
            failures.append((seq, expected, result))

    return failures


def test_commit_delimiter(mapping, trie, reverse_map):
    """
    Test 4: Explicit commit delimiter '|' forces commit of pending buffer.
    """
    failures = []

    tests = [
        # Two bare consonants: "s|b" → ስ + ብ
        ("s|b", "ስብ"),

        # Double consonant that's NOT a defined mapping: "b|b" → ብ + ብ
        ("b|b", "ብብ"),

        # "s|s" → ስ + ስ (plain, not ś-family)
        ("s|s", "ስስ"),

        # Force commit mid-vowel: "he|" → ሀ
        ("he|", "ሀ"),
    ]

    for seq, expected in tests:
        result = ""
        node = trie
        i = 0
        while i < len(seq):
            ch = seq[i]
            i += 1
            if ch == "|":
                if "\0" in node:
                    result += node["\0"]
                node = trie
                continue
            if ch in node:
                node = node[ch]
                if "\0" in node:
                    has_kids = any(k != "\0" for k in node)
                    if not has_kids:
                        result += node["\0"]
                        node = trie
            else:
                if "\0" in node:
                    result += node["\0"]
                    node = trie
                    if ch in node:
                        node = node[ch]
                    else:
                        result += ch
                        node = trie
                else:
                    result += ch
                    node = trie
        if "\0" in node:
            result += node["\0"]

        if result != expected:
            failures.append((seq, expected, result))

    return failures


def test_no_y_variants(mapping):
    """
    Test 5: Verify Y-variant mappings have been removed.
    """
    Y_KEYS = {"MY", "mY", "rY", "MYa", "RYa", "mYa", "rYa"}
    found = [k for k in Y_KEYS if k in mapping]
    return found


def test_value_uniqueness(mapping):
    """
    Test 6: Note characters that are now untypable after Y-variant removal.
    Expected: ፘ (U+1358) and ፙ (U+1359) were only accessible via Y-variants.
    This is informational — the Y-variants were intentionally removed.
    """
    target_to_keys = defaultdict(list)
    for k, v in mapping.items():
        target_to_keys[v].append(k)

    info = []
    for ch in ["ፘ", "ፙ"]:
        if ch not in target_to_keys:
            info.append(f"{ch} U+{ord(ch):04X} now untypable (Y-variants removed)")
    return info  # informational, not a failure


def main():
    mapping = load_mapping()
    trie = build_trie(mapping)
    reverse_map = build_reverse_map(mapping)

    total = 0
    failed = 0

    def run_test(name, test_fn, *args):
        nonlocal total, failed
        total += 1
        result = test_fn(*args)
        ok = len(result) == 0 or (isinstance(result, list) and len(result) == 0)
        if isinstance(result, bool):
            ok = result
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}")
        if not ok:
            failed += 1
            for item in result if isinstance(result, list) else []:
                if len(item) == 3:
                    in_seq, exp, got = item
                    print(f"         seq='{in_seq}'  expected='{exp}'  got='{got}'")
                else:
                    print(f"         {item}")
        return ok

    print("=" * 66)
    print("  Amharic SERA Ambiguities Test Suite")
    print(f"  Mapping: {len(mapping)} entries, sorted by Unicode codepoint")
    print("=" * 66)

    # Test 1: Basic mappings
    failures = test_basic_mappings(mapping, trie)
    run_test("1. Basic key→character mappings", lambda: failures)

    # Test 5: No Y-variants
    y_found = test_no_y_variants(mapping)
    if not run_test("5. No Y-variant remappings", lambda: y_found):
        pass

    # Test 6: Note untypable characters (informational)
    untypable = test_value_uniqueness(mapping)
    if untypable:
        for item in untypable:
            print(f"  [INFO] {item}")
    else:
        print("  [INFO] All characters have at least one mapping")

    # Test 2: Prefix ambiguities
    failures = test_prefix_ambiguities(mapping, trie, reverse_map)
    run_test("2. Prefix disambiguation", lambda: failures)

    # Test 3: Specific known ambiguities
    failures = test_specific_ambiguities(mapping, trie, reverse_map)
    run_test("3. Specific known-critical sequences", lambda: failures)

    # Test 4: Commit delimiter
    failures = test_commit_delimiter(mapping, trie, reverse_map)
    run_test("4. Explicit commit delimiter '|'", lambda: failures)

    # Summary
    print()
    print("=" * 66)
    print(f"  Results: {total - failed}/{total} passed")
    if failed:
        print(f"  {failed} test(s) FAILED")
    else:
        print("  All tests passed.")
    print("=" * 66)

    # List ambiguous prefixes if requested
    if "--list" in sys.argv and not failed:
        print()
        print("Ambiguous prefix analysis:")
        print("-" * 50)
        amb = find_ambiguous_prefixes(mapping)
        for prefix in sorted(amb):
            ext = amb[prefix]
            print(f"  '{prefix}' → {mapping[prefix]}  conflicts with:")
            for k2, v2 in ext[:10]:
                print(f"    '{k2}' → {v2}")
            if len(ext) > 10:
                print(f"    ... and {len(ext)-10} more")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
