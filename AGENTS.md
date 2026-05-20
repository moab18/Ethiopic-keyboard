# AGENTS.md — Ethiopic Keyboard IME

## Project summary

Two-layer Ethiopic input method: a platform-independent C++ core library (`libethio`) and an IBus engine wrapper (`ibus-ethiopic`). Amharic first, then other Ethiopic-script languages. Windows TSF and mobile wrappers follow later.

Full design rationale, directory layout, and roadmap are in `plan_ethiopic-keyboard.md`.

## Architecture

```
ibus-ethiopic (GObject C++ glue, ~500 lines)
  → libethio   (platform-independent C++ core: trie engine, syllabary tables)
```

Modeled on ibus-libpinyin / ibus-chewing (separate core lib + thin IBus wrapper).
Core input engine mirrors m17n-lib's `MIMMap` trie structure, but in clean C++ with JSON mapping files instead of S-expressions.

## Reference implementations (study before contributing)

| Project | Clone | Notes |
|---------|-------|-------|
| ibus-tmpl | `https://github.com/ibus/ibus-tmpl` | Canonical IBus engine skeleton |
| ibus-chewing | `https://github.com/chewing/ibus-chewing` | Production C engine with separate core lib |
| ibus-libpinyin | `https://github.com/libpinyin/ibus-libpinyin` | C++ core lib + IBus wrapper |
| m17n (am-sera.mim) | `https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git` — `MIM/am-sera.mim` | Canonical Amharic SERA mapping (757 lines) |
| ibus-m17n bridge | `https://github.com/ibus/ibus-m17n` | IBus ↔ m17n glue |

## Key technical facts

- **Ethiopic syllable formula:** `codepoint = BASE_CONSONANT + vowel_offset` where offset ∈ {0..6} for 7 vowels (ä,u,i,a,e,ə,o), with labiovelar offsets {0,1,2,5,8} for kw/gw/qw/hw families.
- **Input modes supported:** SERA transliteration (default).
- **Mapping files:** JSON under `data/<language>/<mode>.json` — loaded at runtime by `libethio`. Scriptable auto-generation from Unicode tables.
- **IBus key event contract:** `process_key_event` receives GDK keysym (e.g., `IBUS_KEY_a`), hardware keycode, modifier mask. Return `TRUE` if consumed, `FALSE` to pass through.
- **Always skip:** release events (`IBUS_RELEASE_MASK`), Super-key combos (`IBUS_MOD4_MASK`), password fields (`IBUS_INPUT_PURPOSE_PASSWORD`).
- **IBus engine debug:** `G_MESSAGES_DEBUG=all /usr/libexec/ibus-engine-ethiopic --ibus`

## Implementation order (phases)

1. Generate `data/amharic/sera.json` from Unicode syllable math + m17n `am-sera.mim` reference
2. Build `libethio` (trie engine + JSON loader + unit tests)
3. Wire `ibus-ethiopic` wrapper (GObject subclass, key event processing, preedit/commit)
4. Package as an RPM and test on Fedora
5. Add Tigrinya/Oromo mapping files, then Windows/mobile wrappers

## Style / conventions

- **C++ standard:** C++17
- **Build system:** CMake (3.16+)
- **JSON library:** bundled recursive-descent parser (`ethio/json.hpp`, ~255 lines)
- **IBus version:** ibus-1.0 >= 1.5
- **GObject macro:** `G_DECLARE_FINAL_TYPE` (modern GLib style)
- **Testing:** Catch2 or Google Test — core lib tests are platform-independent, IBus integration tests need a D-Bus session
- **No comments in code** unless the logic is genuinely non-obvious from the names
