# Ethiopic Keyboard

A two-layer Ethiopic input method for Linux: a platform-independent C++ core library (`libethio`) and an IBus engine wrapper (`ibus-ethiopic`). Amharic first, then other Ethiopic-script languages. Windows TSF and mobile wrappers planned.

## Architecture

```
ibus-ethiopic (GObject C++ glue)  →  libethio (C++ trie engine + JSON mappings)
```

Modeled on ibus-libpinyin and ibus-chewing (separate core lib + thin IBus wrapper). The core input engine mirrors m17n-lib's `MIMMap` trie structure, but in clean C++ with JSON mapping files instead of S-expressions.

## Project Structure

```
ethiopic-keyboard/
├── CMakeLists.txt                     # Top-level build
├── libethio/                          # Platform-independent C++ core
│   ├── include/ethio/
│   │   ├── engine.h                   # Input engine (trie descent, preedit/commit)
│   │   ├── mapping.h                  # Trie, MappingFile, JSON loader
│   │   └── json.hpp                   # Bundled nlohmann/json single-header
│   └── src/
│       ├── engine.cpp                 # Core filter/descend/flush/reset logic
│       └── mapping.cpp                # Trie insertion + JSON parsing
├── ibus-ethiopic/                     # IBus engine wrapper
│   ├── src/
│   │   ├── main.cpp                   # D-Bus entry point, factory registration
│   │   ├── engine.h                   # GObject IBusEthiopicEngine class
│   │   └── engine.cpp                 # Key events, preedit/commit, focus/reset
│   └── data/
│       ├── org.freedesktop.IBus.Ethiopic.xml.in  # IBus component template
│       └── org.freedesktop.IBus.Ethiopic.xml     # (generated at build time)
├── data/amharic/                      # Amharic SERA mapping files + generation scripts
├── tests/                             # Catch2-style standalone tests
│   ├── test_mapping.cpp               # Trie construction + JSON loading
│   ├── test_engine.cpp                # Core engine: key→Ethiopic sequences
│   └── test_ibus_engine.cpp           # IBus integration (key events, preedit, commit)
└── build*/                            # Build directories (gitignored)
```

## Requirements

- **CMake** 3.16+
- **C++17** compiler (tested with GCC 13+ and Clang 18+)
- **IBus** 1.5+ (`ibus-1.0` pkg-config package)
- **GLib** 2.0 (`glib-2.0`, `gio-2.0` pkg-config packages)
- **nlohmann/json** (bundled as `libethio/include/ethio/json.hpp`)

## Build & Install

```bash
# Configure (debug build)
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr

# Build
cmake --build build

# Run core tests
cd build && ctest --output-on-failure && cd ..

# Install (requires root for /usr prefix)
sudo cmake --install build
```

The install step places:
- `/usr/libexec/ibus-engine-ethiopic` — the engine executable
- `/usr/share/ibus/component/org.freedesktop.IBus.Ethiopic.xml` — IBus component registration
- `/usr/share/ibus-ethiopic/amharic/*.json` — SERA mapping data

### Custom prefix

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

Component XML files must be in the IBus component directory (`/usr/share/ibus/component/` or the equivalent under your prefix). IBus scans this directory to discover engines.

## Running

After install, restart IBus or run:

```bash
ibus restart
```

The engine now appears in IBus preferences. To add it:

```bash
ibus-setup
```

Go to the **Input Method** tab, add **Ethiopic (Amharic/SERA)**.

### Quick test without IBus restart

First, kill any running engine instance:

```bash
pkill -f ibus-engine-ethiopic
```

Then run the engine directly (IBus will connect to it when you switch to the input method):

```bash
G_MESSAGES_DEBUG=all /usr/libexec/ibus-engine-ethiopic --ibus
```

Now open a GTK application with IBus enabled:

```bash
GTK_IM_MODULE=ibus gedit
```

Switch to the Ethiopic input method (Super+Space by default) and type.

### Debug with GDB

```bash
gdb --args /usr/libexec/ibus-engine-ethiopic --ibus
```

## Input Methods

### SERA Transliteration (Default)

Type Amharic using the SERA (System for Ethiopic Representation in ASCII) scheme:

| Input | Output | Input | Output |
|-------|--------|-------|--------|
| `he`  | ሀ     | `hu`  | ሁ     |
| `hi`  | ሂ     | `ha`  | ሃ     |
| `hE`  | ሄ     | `h`   | ህ     |
| `ho`  | ሆ     | `hWa` | ኋ     |
| `le`  | ለ     | `me`  | መ     |
| `se`  | ሰ     | `be`  | በ     |

Special keys:
- `'` commits the current composition (acts as a delimiter)
- `''` produces a literal apostrophe
- `:` produces ፡ (word separator), `::` produces ። (period)

### Planned

- Typewriter layout (vowel-number suffixes: `h1`=ሀ, `h2`=ሁ, ...)
- Tigrinya, Guragigna, and other Ethiopic-script languages
- Windows TSF wrapper
- Mobile (Android/iOS) wrappers

## License

TBD

## References

| Project | Purpose |
|---------|---------|
| [ibus-libpinyin](https://github.com/libpinyin/ibus-libpinyin) | C++ core lib + IBus wrapper (architecture model) |
| [ibus-chewing](https://github.com/chewing/ibus-chewing) | Production C engine with separate core lib |
| [ibus-tmpl](https://github.com/ibus/ibus-tmpl) | Canonical IBus engine skeleton |
| [m17n-db `am-sera.mim`](https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git) | Reference Amharic SERA mapping |
| [ibus-m17n](https://github.com/ibus/ibus-m17n) | IBus ↔ m17n bridge |
| [IBus DevGuide](https://github.com/ibus/ibus/wiki/DevGuide) | Official IBus developer guide |
