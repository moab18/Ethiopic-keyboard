#!/usr/bin/env python3
"""
SERA trie engine simulation — given Amharic words, prints the ASCII
key sequences needed to produce them, and flags disambiguation points.

Usage:
  python3 simulate_sera.py                              # run built-in test cases
  python3 simulate_sera.py "ሰላም" "ጤና ይስጥልኝ"          # query specific words
  python3 simulate_sera.py --disambig                    # show disambiguation analysis
  python3 simulate_sera.py --wordlist words.txt          # read words from file

Key design decisions shown:
  - How bare-consonant (6th-form) entries commit
  - How double-consonant initials (ss, sh, ch, ts, etc.) are disambiguated
  - How the trie engine decides when to commit vs. wait for more input
"""

import json
import sys
from pathlib import Path
from collections import defaultdict, OrderedDict

SCRIPT_DIR = Path(__file__).resolve().parent
JSON_PATH = SCRIPT_DIR / "sera.json"


def load_mapping():
    with open(JSON_PATH) as f:
        data = json.load(f)
    return data["states"]["init"]["map"]


def build_reverse_map(mapping):
    """Build Ethiopic char -> list of SERA key sequences (shortest first)."""
    rev = defaultdict(list)
    for key, val in mapping.items():
        rev[val].append(key)
    for val in rev:
        rev[val].sort(key=len)
    return dict(rev)


def build_trie(mapping):
    """Build a trie from the flat key->value mapping.
    Returns dict of dicts: trie[char][next_char]... → terminal value or submap.
    """
    trie = {}
    for key, val in mapping.items():
        node = trie
        for ch in key:
            node = node.setdefault(ch, {})
        node["\0"] = val  # terminal marker
    return trie


def lookup_primary(reverse_map, eth_char):
    """Return the primary (shortest) key sequence for an Ethiopic character."""
    if eth_char in reverse_map:
        return reverse_map[eth_char][0]
    return eth_char  # passthrough if not mapped


def lookup_all(reverse_map, eth_char):
    """Return all key sequences for an Ethiopic character."""
    return reverse_map.get(eth_char, [eth_char])


def simulate_word(word, reverse_map, trie):
    """Simulate typing an Amharic word and return the key-by-key transcript.

    Returns list of (eth_char, primary_key, alternatives, disambig_note)
    """
    transcript = []
    for i, ch in enumerate(word):
        keys = lookup_all(reverse_map, ch)
        primary = keys[0] if keys else ch
        alternatives = keys[1:] if len(keys) > 1 else []

        note = ""
        # Check if this sequence's prefix matches another valid terminal
        for j in range(1, len(primary)):
            prefix = primary[:j]
            if prefix in trie and "\0" in trie[prefix]:
                # The prefix itself is a valid terminal (could commit earlier)
                # Show what the prefix would commit
                prefix_char = trie[prefix]["\0"]
                if prefix_char != ch:
                    note = f"  ⚠ prefix '{prefix}' alone → {prefix_char}; engine must wait for full '{primary}'"

        # Check if this is a bare consonant (6th form) that could conflict
        # with a double-consonant initial
        if len(primary) == 1 and primary.isalpha() and primary.islower():
            # Single letter key — check if it's a prefix of a multi-letter mapping
            doubled = primary + primary
            if doubled in trie:
                doubled_val = trie[doubled]["\0"]
                note = f"  ⚠ bare '{primary}' (6th) vs double '{doubled}' → {doubled_val}; commit timing critical"

        transcript.append((ch, primary, alternatives, note))

    return transcript


def analyze_disambiguation(mapping, trie):
    """Find all key sequences where disambiguation is needed."""
    issues = []

    # Case 1: single-letter keys that are prefixes of multi-letter keys
    # e.g., "s" (6th form) is a prefix of "ss", "sh", "se", etc.
    preambles = OrderedDict()
    for key in sorted(mapping, key=len):
        for j in range(1, len(key)):
            prefix = key[:j]
            if prefix in mapping:
                if prefix not in preambles:
                    preambles[prefix] = []
                preambles[prefix].append(key)

    for prefix, extensions in sorted(preambles.items()):
        prefix_val = mapping[prefix]
        ext_vals = sorted(set(mapping[e] for e in extensions))
        issues.append(
            f"  '{prefix}' → {prefix_val}  conflicts with: "
            + ", ".join(f"'{e}'→{mapping[e]}" for e in sorted(set(extensions), key=len)[:6])
            + (" …" if len(set(extensions)) > 6 else "")
        )

    # Case 2: bare-consonant (6th form) vs. consonant + consonant (e.g., "s" vs "ss")
    bare_prefix_conflicts = {}
    for key, val in mapping.items():
        if len(key) == 1 and key.islower():
            for k2, v2 in mapping.items():
                if len(k2) >= 2 and k2[0] == key and k2[1:].isalpha() and k2[1].islower():
                    if key not in bare_prefix_conflicts:
                        bare_prefix_conflicts[key] = []
                    bare_prefix_conflicts[key].append((k2, v2))

    # Filter to only show the most important ones
    hitlist = {}
    for prefix, exts in bare_prefix_conflicts.items():
        unique_exts = list(OrderedDict.fromkeys(exts))
        if len(unique_exts) >= 2:
            hitlist[prefix] = unique_exts

    return issues, hitlist


def run_simulation(words, reverse_map, trie, show_details=True):
    """Run the simulation and print results."""
    sep = "─" * 70

    for word in words:
        word = word.strip()
        if not word:
            continue

        print(f"\n{sep}")
        print(f"  Word: {word}")
        print(sep)

        transcript = simulate_word(word, reverse_map, trie)
        total_keys = 0
        primary_seq = ""
        alt_sequences = []

        for eth_char, primary, alternatives, note in transcript:
            ucp = f"U+{ord(eth_char):04X}"
            total_keys += len(primary)

            # Build alternative sequences
            for idx, alt in enumerate(alternatives):
                while len(alt_sequences) <= idx:
                    alt_sequences.append("")
                alt_sequences[idx] += alt

            primary_seq += primary

            alts_str = ""
            if alternatives:
                alts_str = "  |  alt: " + ", ".join(alternatives)

            if note:
                print(f"    {eth_char}  {ucp}  →  \"{primary}\"  {alts_str}")
                print(f"    {note}")
            elif show_details:
                print(f"    {eth_char}  {ucp}  →  \"{primary}\"  {alts_str}")

        print(f"    {'─' * 40}")
        print(f"    Primary sequence:  {primary_seq}  ({total_keys} keystrokes)")
        if alt_sequences:
            for idx, alt_seq in enumerate(alt_sequences):
                print(f"    Alt #{idx+1}:           {alt_seq}")

        # Reconstruct what the trie engine would see
        print(f"\n    Trie traversal (engine simulation):")
        actual = simulate_engine_traversal(primary_seq, trie, reverse_map)

        # Check if the naive traversal matches the expected word
        ok, corrected, notes = check_disambiguation(primary_seq, word, trie, reverse_map)
        if not ok:
            print(f"\n    ═══ DISAMBIGUATION NEEDED ═══")
            print(f"    The naive key sequence '{primary_seq}' produces '{actual}'")
            print(f"    But the expected word is '{word}'")
            print(notes)
            print(f"\n    Corrected sequence with explicit commit delimiters ('|'):")
            print(f"      {corrected}")
            print(f"    (In the real engine, pressing Space or any non-matching")
            print(f"     key forces the pending 6th-form consonant to commit.)")


def simulate_engine_traversal(key_seq, trie, reverse_map):
    """Show how the trie engine would process a key sequence step by step.

    The engine does longest-prefix matching: it walks the trie as far as
    possible, consuming characters.  When it reaches a terminal that has
    NO further submaps, it commits.  When it reaches a terminal that DOES
    have submaps, it waits for more input.  When input ends at a terminal
    (even if it has submaps), it commits.
    """
    output = []
    step = 0
    buf = []  # accumulated path for display

    def commit(val, path_consumed):
        nonlocal step
        step += 1
        output.append(val)
        # Also group any accumulated non-committed path
        buf.clear()

    node = trie
    path = ""
    i = 0
    while i < len(key_seq):
        ch = key_seq[i]

        if ch in node:
            node = node[ch]
            path += ch
            i += 1

            if "\0" in node:
                # Terminal reached.  Check if there are further submaps.
                has_submaps = any(k != "\0" for k in node)
                if not has_submaps:
                    # No extensions possible → commit now
                    step += 1
                    output.append(node["\0"])
                    print(f"      Step {step}: \"{path}\" → commit {node['\0']}")
                    # Reset
                    node = trie
                    path = ""
                elif i >= len(key_seq):
                    # End of input — commit even though extensions exist
                    step += 1
                    output.append(node["\0"])
                    print(f"      Step {step}: \"{path}\" → commit {node['\0']}  (end of input)")
                else:
                    # Has submaps, more input coming → wait
                    pass
            elif i >= len(key_seq):
                # At a non-terminal node with no more input
                # Walk back to find last terminal, or passthrough
                step += 1
                output.append(path)
                print(f"      Step {step}: \"{path}\" → passthrough  (dead end)")
        else:
            # Current char doesn't extend the path
            if "\0" in node:
                # Commit the terminal we're at, then retry this char
                step += 1
                output.append(node["\0"])
                print(f"      Step {step}: \"{path}\" → commit {node['\0']}  (next key '{ch}' unmatched)")
                node = trie
                path = ""
                # Retry ch — don't advance i
            else:
                # Not at a terminal and can't extend — backtrack
                # Find last committed terminal in path and retry
                step += 1
                output.append(path[-1] if path else ch)
                print(f"      Step {step}: \"{path}{ch}\" → passthrough  (no match)")
                node = trie
                path = ""
                i += 1

    result = "".join(output)
    print(f"      Result: {result}")
    return result


def simulate_engine_traversal_silent(key_seq, trie):
    """Same traversal logic but returns result without printing.
    '|' acts as an explicit commit delimiter."""
    output = []
    node = trie
    i = 0
    while i < len(key_seq):
        ch = key_seq[i]
        if ch == "|":
            if "\0" in node:
                output.append(node["\0"])
            node = trie
            i += 1
            continue
        if ch in node:
            node = node[ch]
            i += 1
            if "\0" in node:
                has_submaps = any(k != "\0" for k in node)
                if not has_submaps:
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


def check_disambiguation(primary_seq, expected_word, trie, reverse_map):
    """Verify the primary sequence produces the expected output.
    If not, compute corrected sequence with '|' commit delimiters.
    Returns (ok, corrected_seq, notes)."""
    actual = simulate_engine_traversal_silent(primary_seq, trie)
    if actual == expected_word:
        return True, primary_seq, ""

    # Mismatch — greedily find where to insert commit delimiters
    corrected_parts = []
    notes = []
    for i, ch in enumerate(expected_word):
        keys = lookup_all(reverse_map, ch)
        primary = keys[0]
        if i > 0:
            probe = "".join(corrected_parts) + primary
            probe_result = simulate_engine_traversal_silent(probe, trie)
            expected_prefix = expected_word[:i+1]
            if probe_result != expected_prefix:
                corrected_parts.append("|")
                notes.append(f"  ⚠ need commit delimiter '|' between chars at position {i}: "
                             f"'{probe}' → {probe_result} instead of '{expected_prefix}'")
        corrected_parts.append(primary)

    corrected_seq = "".join(corrected_parts)
    return False, corrected_seq, "\n".join(notes) if notes else ""


def main():
    mapping = load_mapping()
    reverse_map = build_reverse_map(mapping)
    trie = build_trie(mapping)

    if "--disambig" in sys.argv:
        print("Disambiguation analysis\n")
        print("Prefix conflicts (single-letter keys that start longer sequences):")
        print("─" * 60)
        issues, bare_conflicts = analyze_disambiguation(mapping, trie)

        # Group by conflict type
        print("\n  [Bare consonant vs. double-consonant initials]")
        for prefix in sorted(bare_conflicts):
            exts = bare_conflicts[prefix]
            prefix_val = mapping[prefix]
            print(f"  '{prefix}' → {prefix_val}")
            for k2, v2 in exts[:8]:
                print(f"    '{k2}' → {v2}")
            if len(exts) > 8:
                print(f"    … and {len(exts)-8} more")

        print(f"\n  [All prefix conflicts ({len(issues)} total)]")
        for issue in issues[:30]:
            print(issue)
        if len(issues) > 30:
            print(f"  … and {len(issues)-30} more")
        return

    # Collect words
    words = []
    for arg in sys.argv[1:]:
        if arg.startswith("--"):
            continue
        words.append(arg)

    if not words:
        # Built-in test cases
        words = [
            "ሰላም",
            "ስም",
            "አበበ",
            "ጤና ይስጥልኝ",
            "ስስት፣",
            "ሥሥት፤",
            "ሥስት፤",
            "ግንኑነት፤",
            "ኢትዮጵያ",
            "በቀለ",
            "ሸዋ",
            "አማርኛ",
            "ፍቅር",
            "ሀገር",
        ]

    print(f"Ethiopic SERA Simulation")
    print(f"Mapping: {len(mapping)} key sequences → {len(reverse_map)} Ethiopic chars\n")
    run_simulation(words, reverse_map, trie)


if __name__ == "__main__":
    main()
