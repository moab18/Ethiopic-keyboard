# Ethiopic Keyboard — Plan

**Created:** 4/27/2026
**Updated:** 5/24/2026

---

## Architecture

```
ibus-ethiopic (GObject C++ glue, ~600 lines) ─┐
                                                ├── libethio (platform-independent C++ core: trie engine + JSON mappings)
ethiopic-tsf   (COM DLL, ~2,200 lines)        ─┘
```

Two-layer design: a platform-independent C++17 core library (`libethio`) and a thin IBus engine wrapper (`ibus-ethiopic`). The same `libethio` will drive Windows TSF, macOS IMKit, Android, and iOS wrappers — only the OS glue changes.

Modeled on ibus-libpinyin / ibus-chewing (separate core lib + IBus wrapper). Core engine mirrors m17n-lib's `MIMMap` trie structure but uses clean C++ with JSON mapping files.

### Key technical facts

- **Syllable formula:** `codepoint = BASE_CONSONANT + vowel_offset` where offset ∈ {0..6} for 7 vowels (ä,u,i,a,e,ə,o), with labiovelar offsets {0,1,2,5,8} for kw/gw/qw/hw families.
- **Input mode:** SERA transliteration.
- **Mapping files:** JSON under `data/<language>/<mode>.json` — loaded at runtime.
- **IBus key event contract:** `process_key_event(keyval, keycode, state)`. Return `TRUE` if consumed, `FALSE` to pass through.
- **Always skip:** release events (`IBUS_RELEASE_MASK`), Super-key combos (`IBUS_MOD4_MASK`), password fields.

### Reference implementations

| Project | URL | Role |
|---------|-----|------|
| ibus-tmpl | `https://github.com/ibus/ibus-tmpl` | IBus engine skeleton |
| ibus-chewing | `https://github.com/chewing/ibus-chewing` | Production C engine + separate core lib |
| ibus-libpinyin | `https://github.com/libpinyin/ibus-libpinyin` | C++ core lib + IBus wrapper |
| m17n `am-sera.mim` | `https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git` | Canonical Amharic SERA mapping |

---

## Project Structure

```
ethiopic-keyboard/
├── CMakeLists.txt
├── libethio/                          # Platform-independent C++ core
│   ├── include/ethio/
│   │   ├── engine.h                   # Input engine (trie descent, preedit/commit)
│   │   ├── mapping.h                  # Trie, MappingFile, JSON loader
│   │   ├── wordlist.h                 # Word list with prefix/bigram suggestions
│   │   ├── logger.h                   # Debug logging
│   │   └── json.hpp                   # Streaming JSON parser
│   └── src/
│       ├── engine.cpp                 # Core filter/descend/flush/reset logic
│       ├── mapping.cpp                # Trie insertion + JSON parsing
│       ├── wordlist.cpp               # Word list loading + binary-search suggest
│       └── logger.cpp                 # Debug logging
├── ibus-ethiopic/                     # IBus engine wrapper
│   ├── src/
│   │   ├── main.cpp                   # D-Bus entry point, factory registration
│   │   ├── engine.h                   # GObject IBusEthiopicEngine class
│   │   └── engine.cpp                 # Key events, preedit/commit, focus/reset
│   └── data/
│       ├── default.xml
│       └── ethiopic.xml
├── data/amharic/                      # Amharic SERA mapping + wordlist
│   ├── am-sera.json                   # SERA transliteration mapping
│   ├── wordlist.json                  # Static word pool
│   ├── bigrams.json                   # Bigram transition data
│   └── names.json                     # Proper name entries
├── tests/                             # Unit tests
│   ├── test_mapping.cpp               # Trie construction + JSON loading
│   ├── test_engine.cpp                # Core engine: key→Ethiopic sequences
│   ├── test_features.cpp              # Feature-level behavior + edge cases
│   ├── test_wordlist.cpp              # Word list loading + suggestions
│   └── test_ibus_engine.cpp           # IBus integration (key events, preedit, commit)
└── python-helper/                     # Mapping generation/validation scripts
```

---

## Development Roadmap

| Phase | Status | Deliverables |
|-------|--------|--------------|
| **Core library** | Done | `libethio` with trie engine, SERA mapping, JSON loader, wordlist/bigram support, unit tests |
| **IBus engine** | Done | `ibus-ethiopic` wrapper, key event processing, preedit/commit, lookup table, word suggestions |
| **Language Support** | Any ethiopic language such as Amharic, Tigrinya, Guragigna  etc which uses Ethiopic Alphabets . |
| **Linux packaging** | Next | RPM (Fedora), DEB (Debian/Ubuntu), PKGBUILD (Arch) |
| **Windows TSF** | Done | TSF TextService COM DLL, candidate popup, Ctrl+Shift passthrough, word suggestions |
| **macOS** | Planned | Input Method Kit app (Objective-C++) wrapping `libethio` |
| **Android** | Planned | InputMethodService + JNI wrapping `libethio` |
| **iOS** | Planned | UIInputViewController + Swift/C++ bridging |
| **Advanced features** | Ongoing | Word prediction, user dictionary, fuzzy matching |

---

## Packaging & Cross-Platform Plan

`libethio` is C++17 with zero OS dependencies — it compiles identically on all targets. Each platform only needs a thin wrapper.

### Linux

| Distro | Format | Key files |
|--------|--------|-----------|
| **Fedora** | RPM | `.spec` file: `libethio-devel` + `ibus-ethiopic`. Target COPR first. |
| **Debian/Ubuntu** | DEB | `debian/control`, `debian/rules`. Split: `libethio-dev`, `ibus-ethiopic`. |
| **Arch** | PKGBUILD | Single `PKGBUILD` running `cmake --build && cmake --install`. Submit to AUR. |
| **openSUSE** | RPM | Reuse Fedora `.spec`. |

Install layout:
```
/usr/libexec/ibus-engine-ethiopic
/usr/share/ibus/component/ethiopic.xml
/usr/share/ibus-ethiopic/amharic/    (am-sera.json, wordlist.json, etc.)
/usr/include/ethio/                  (engine.h, mapping.h, etc.)
/usr/lib64/libethio.a                (static lib)
```

### Windows (TSF)

- COM-based DLL: `ITfTextInputProcessor`, `ITfKeyEventSink`
- Key events → `ethio::Engine::filter()` → commit via `ITfComposition`
- Package as MSI installer

### macOS

- `IMKInputController` subclass (Objective-C++)
- `handle(_:client:)` → `libethio` → `insertText()`
- Package as `.app` in `/Library/Input Methods/`

### Android

- Cross-compile `libethio` via NDK
- `InputMethodService` + JNI → `Engine::filter()` → `commitText()` / `setComposingText()`

### iOS

- `UIInputViewController` + Swift/C++ bridging
- `textDocumentProxy.insertText()`

---

## Current Status

- `libethio` core: trie engine, JSON mapping loader, wordlist/bigram/names support
- `ibus-ethiopic` wrapper: key event processing, preedit/commit, lookup table, word suggestions
- `ethiopic-tsf` Windows wrapper: COM DLL with TSF integration, candidate popup window, Ctrl+Shift passthrough toggle, word suggestions
- Unit tests: 6 suites passing (mapping, engine, features, wordlist, IBus integration, TSF integration)
- SERA mapping: complete for all Ethiopic characters and engine supports any Ethiopic language

### Next actions

1. Write RPM `.spec` — package for Fedora
2. Write DEB packaging (`debian/` directory) for Ubuntu/Debian
3. Write PKGBUILD for Arch AUR
4. WiX MSI installer for Windows
5. Android InputMethodService + JNI wrapper
6. macOS IMKit wrapper
