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
JSON_PATH = SCRIPT_DIR.parent / "data" / "amharic" / "am-sera.json"


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
        # Double-consonant: "hh" should give ḫ-family, not h+h
        ("hh", "ኅ"),
        ("hhe", "ኀ"),
        ("hhWe", "ኈ"),
        ("hha", "ኃ"),

        # Plain h with next consonant: should commit h first, then next
        ("hl", "ህል"),  # h(ə) + l(ə)
        ("hb", "ህብ"),  # h(ə) + b(ə)

        # Double-consonant: "cc" should give č̣-family
        ("cce", "ጨ"),
        ("ccu", "ጩ"),

        # Plain c with next consonant
        ("cb", "ችብ"),  # c(ə) + b(ə)

        # Vowel disambiguation: "ea" → ኧ (not e+a) since "ea" is a defined mapping
        ("ea", "ኧ"),

        # "ae" → ኤ (standalone vowel)
        ("ae", "ኤ"),

        # Bare consonant vs vowel sequence: "he" → ሀ, "h" → ህ
        ("he", "ሀ"),
        ("hel", "ሀል"),  # he commits ሀ, then l → ል

        # Capital alternate families
        ("Se", "ሠ"),    # ś-family (capital S, not ss)
        ("Su", "ሡ"),
        ("De", "ጸ"),    # ṣ-family (capital D)
        ("Te", "ጠ"),    # ṭ-family (capital T)
        ("Ze", "ዠ"),    # ž-family (capital Z)
        ("Ce", "ጨ"),    # č̣-family (capital C, same as cce)
        ("Fe", "ፀ"),    # ś́-family (capital F)
        ("Pe", "ፐ"),    # p-family (capital P)

        # sh for š-family
        ("she", "ሸ"),
        ("shu", "ሹ"),
        ("sh", "ሽ"),

        # Slash-prefix for ḫ-family
        ("/he", "ኀ"),
        ("/ha", "ኃ"),
        ("/hWe", "ኈ"),

        # Standalone vowels
        ("a", "አ"),
        ("e", "እ"),
        ("x", "እ"),
        ("xe", "አ"),
        ("xa", "ኣ"),

        # Numerals
        ("`1", "፩"),
        ("`10", "፲"),

        # Punctuation passthrough and transforms
        ("::", "።"),
        (":::", ":"),
        ("/?", "፧"),

        # Labiovelars
        ("gWe", "ጐ"),
        ("kWe", "ኰ"),
        ("kWa", "ኳ"),
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
        ok = len(result) == 0
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {name}")
        if not ok:
            failed += 1
            for item in result:
                if len(item) == 3:
                    in_seq, exp, got = item
                    print(f"         seq='{in_seq}'  expected='{exp}'  got='{got}'")
                else:
                    print(f"         {item}")
        return ok

    print("=" * 66)
    print("  Amharic SERA Ambiguities Test Suite")
    print(f"  Mapping: {len(mapping)} entries")
    print("=" * 66)

    # Test 1: Basic mappings
    failures = test_basic_mappings(mapping, trie)
    run_test("1. Basic key→character mappings", lambda: failures)

    # Test 2: Prefix ambiguities
    failures = test_prefix_ambiguities(mapping, trie, reverse_map)
    run_test("2. Prefix disambiguation", lambda: failures)

    # Test 3: Specific known sequences
    failures = test_specific_ambiguities(mapping, trie, reverse_map)
    run_test("3. Known-critical sequences", lambda: failures)

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

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
