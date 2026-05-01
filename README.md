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
│       └── ethiopic.xml.in             # IBus component template
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
- `/usr/share/ibus/component/ethiopic.xml` — IBus component registration
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

Type Amharic using the SERA (System for Ethiopic Representation in ASCII) scheme. The trie engine handles complex prefix disambiguation automatically — longer key sequences take priority over shorter ones, and leaf nodes auto-commit without requiring a delimiter.

#### Consonant Families

Each consonant has 7 vowel forms (1st–7th order: ä, u, i, a, e, ə, o). The 6th order (ə) uses the bare consonant key (no vowel suffix). The 5th order (e) uses uppercase `E`:

| Family   | 1st (ä) | 2nd (u) | 3rd (i) | 4th (a) | 5th (e) | 6th (ə) | 7th (o) |
|----------|---------|---------|---------|---------|---------|---------|---------|
| h (ሀ)   | `he` ሀ  | `hu` ሁ  | `hi` ሂ  | `ha` ሃ  | `hE` ሄ  | `h` ህ   | `ho` ሆ  |
| l (ለ)   | `le` ለ  | `lu` ሉ  | `li` ሊ  | `la` ላ  | `lE` ሌ  | `l` ል   | `lo` ሎ  |
| m (መ)   | `me` መ  | `mu` ሙ  | `mi` ሚ  | `ma` ማ  | `mE` ሜ  | `m` ም   | `mo` ሞ  |
| r (ረ)   | `re` ረ  | `ru` ሩ  | `ri` ሪ  | `ra` ራ  | `rE` ሬ  | `r` ር   | `ro` ሮ  |
| s (ሰ)   | `se` ሰ  | `su` ሱ  | `si` ሲ  | `sa` ሳ  | `sE` ሴ  | `s` ስ   | `so` ሶ  |
| š (ሸ)   | `xe` ሸ  | `xu` ሹ  | `xi` ሺ  | `xa` ሻ  | `xE` ሼ  | `x` ሽ   | `xo` ሾ  |
| q (ቀ)   | `qe` ቀ  | `qu` ቁ  | `qi` ቂ  | `qa` ቃ  | `qE` ቄ  | `q` ቅ   | `qo` ቆ  |
| b (በ)   | `be` በ  | `bu` ቡ  | `bi` ቢ  | `ba` ባ  | `bE` ቤ  | `b` ብ   | `bo` ቦ  |
| t (ተ)   | `te` ተ  | `tu` ቱ  | `ti` ቲ  | `ta` ታ  | `tE` ቴ  | `t` ት   | `to` ቶ  |
| c (ቸ)   | `ce` ቸ  | `cu` ቹ  | `ci` ቺ  | `ca` ቻ  | `cE` ቼ  | `c` ች   | `co` ቾ  |
| n (ነ)   | `ne` ነ  | `nu` ኑ  | `ni` ኒ  | `na` ና  | `nE` ኔ  | `n` ን   | `no` ኖ  |
| k (ከ)   | `ke` ከ  | `ku` ኩ  | `ki` ኪ  | `ka` ካ  | `kE` ኬ  | `k` ክ   | `ko` ኮ  |
| w (ወ)   | `we` ወ  | `wu` ዉ  | `wi` ዊ  | `wa` ዋ  | `wE` ዌ  | `w` ው   | `wo` ዎ  |
| z (ዘ)   | `ze` ዘ  | `zu` ዙ  | `zi` ዚ  | `za` ዛ  | `zE` ዜ  | `z` ዝ   | `zo` ዞ  |
| y (የ)   | `ye` የ  | `yu` ዩ  | `yi` ዪ  | `ya` ያ  | `yE` ዬ  | `y` ይ   | `yo` ዮ  |
| d (ደ)   | `de` ደ  | `du` ዱ  | `di` ዲ  | `da` ዳ  | `dE` ዴ  | `d` ድ   | `do` ዶ  |
| j (ጀ)   | `je` ጀ  | `ju` ጁ  | `ji` ጂ  | `ja` ጃ  | `jE` ጄ  | `j` ጅ   | `jo` ጆ  |
| g (ገ)   | `ge` ገ  | `gu` ጉ  | `gi` ጊ  | `ga` ጋ  | `gE` ጌ  | `g` ግ   | `go` ጎ  |
| f (ፈ)   | `fe` ፈ  | `fu` ፉ  | `fi` ፊ  | `fa` ፋ  | `fE` ፌ  | `f` ፍ   | `fo` ፎ  |
| p (ፐ)   | `pe` ፐ  | `pu` ፑ  | `pi` ፒ  | `pa` ፓ  | `pE` ፔ  | `p` ፕ   | `po` ፖ  |

#### Labiovelar Forms (CV + W)

Four consonant families support labiovelar (qu-, ku-, gu-, hu-) syllables using the `W` infix:

|       | h-family | q-family | k-family | g-family |
|-------|----------|----------|----------|----------|
| 1st   | `hWe` ኈ  | `qWe` ቈ  | `kWe` ኰ  | `gWe` ጐ  |
| 2nd   | `hWu` ኍ  | `qWu` ቝ  | `kWu` ኵ  | `gWu` ጕ  |
| 3rd   | `hWi` ኊ  | `qWi` ቊ  | `kWi` ኲ  | `gWi` ጒ  |
| 4th   | `hWa` ኋ  | `qWa` ቋ  | `kWa` ኳ  | `gWa` ጓ  |
| 5th   | `hWE` ኌ  | `qWE` ቌ  | `kWE` ኴ  | `gWE` ጔ  |

#### Slash-Prefix Alternatives (for ambiguous consonants)

Consonants that share ASCII transliterations with others can be typed using a `/` prefix:

| ASCII | Default | /-prefix alt | //-escape lit |
|-------|---------|-------------|---------------|
| `s`   | `se` ሰ  | `/se` ሠ     | `//` → `/`   |
| `h`   | `he` ሀ  | `/he` ኀ     |               |
| `d`   | `de` ደ  | `/de` ጸ     |               |
| `z`   | `ze` ዘ  | `/ze` ዠ     |               |
| `t`   | `te` ተ  | `/te` ጠ     |               |
| `c`   | `ce` ቸ  | `/ce` ጨ     |               |
| `f`   | `fe` ፈ  | `/fe` ፀ     |               |
| `p`   | `pe` ፐ  | `/pe` ጰ     |               |

#### Double-Consonant Alternatives

The same alternate syllables can also be typed with doubled initial consonant keys (no `/` needed):

| Input | Output | Input | Output |
|-------|--------|-------|--------|
| `sse` | ሠ (ś) | `hhe` | ኀ (ḫ) |
| `dde` | ጸ (ṣ) | `zze` | ዠ (ž) |
| `tte` | ጠ (ṭ) | `cce` | ጨ (č') |
| `ffe` | ፀ (ś́) | `ppe` | ጰ (p̣) |

#### Standalone Vowels

Vowels are typed directly. Forms a–ä (አ family) use lowercase; glottal/ə (እ family) use uppercase:

| Input | Output | Input | Output |
|-------|--------|-------|--------|
| `a`   | አ     | `A`   | ኣ     |
| `u`   | ኡ     | `U`   | ኡ     |
| `i`   | ኢ     | `E`   | ኤ     |
| `e`   | እ     | `I`   | እ     |
| `o`   | ኦ     | `ea`  | ኧ     |

#### Aynu (Pharyngeal) Forms

Pharyngeal vowel-initial syllables use slash-prefix or doubled-vowel notation:

| Input | Output | Input | Output |
|-------|--------|-------|--------|
| `/e`  | ዐ     | `ae`  | ዐ     |
| `/u`  | ዑ     | `uu`  | ዑ     |
| `/i`  | ዒ     | `ii`  | ዒ     |
| `/a`  | ዓ     | `aa`  | ዓ     |
| `/E`  | ዔ     | `EE`  | ዔ     |
| `/I`  | ዕ     | `ee`  | ዕ     |
| `/o`  | ዖ     | `oo`  | ዖ     |

#### Punctuation and Symbols

| Input   | Output | Description          |
|---------|--------|----------------------|
| `:`     | ፡     | Word separator       |
| `::`    | ።     | Full stop / period   |
| `:::`   | `:`    | Literal colon        |
| `,`     | ፣     | Comma                |
| `;;`    | `;`    | Literal semicolon    |
| `-:`    | ፥     | Preface colon        |
| `:-`    | ፦     | Question colon       |
| `**`    | ፨     | Section mark         |
| `??`    | ፧     | Question mark prefix |

#### Ethiopic Numerals

Numbers are typed with a backtick prefix:

| Input     | Output | Input      | Output  |
|-----------|--------|------------|---------|
| `` `1 ``  | ፩      | `` `10 ``   | ፲       |
| `` `2 ``  | ፪      | `` `20 ``   | ፳       |
| `` `5 ``  | ፭      | `` `50 ``   | ፶       |
| `` `9 ``  | ፱      | `` `90 ``   | ፺       |
| `` `100 ``| ፻      | `` `10000 ``| ፼      |
| `` `1000 `` | ፲፻  | `` `1000000 ``| ፻፼   |

#### Commit Delimiter and Escape Sequences

| Input | Behavior |
|-------|----------|
| `'`   | Flushes pending composition (commit delimiter). On mismatch, commits whatever is composing and starts a new raw `'` segment |
| `''`  | Produces a literal apostrophe `'` |
| `//`  | Produces a literal slash `/` |
| `:::` | Produces a literal colon `:` |

#### Input Engine Behavior

The trie engine handles several cases automatically:

- **Auto-commit**: When a key sequence reaches a leaf node with no further children, output is committed immediately. Example: `bE` → ቤ (no space needed).
- **Pending on branch**: When a sequence reaches a node that has children, output stays in preedit until a mismatch, space, or delimiter commits it. Example: `he` shows ሀ in preedit; typing space or a next syllable's first key commits it.
- **5th order disambiguation**: The 5th order vowel `e` is typed as uppercase `E` to distinguish from the 1st order suffix `e`. Example: `be` = በ (1st), `bE` = ቤ (5th).
- **Prefix priority**: Longer key sequences win over shorter ones. Example: `hWa` → ኋ (not `ህዋ`), `hee` → ሄ (not `ሀእ`).
- **Raw keys in preedit**: When no trie action is pending, raw key characters are shown in preedit. Example: typing `hW` shows `hW` until the vowel completes the labiovelar form.

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
