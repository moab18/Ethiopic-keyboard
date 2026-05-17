# Ethiopic Keyboard

ፈጣን የአማርኛ መጻፊያ ኪቦርድ

A two-layer Ethiopic input method for Linux: a platform-independent C++ core library (`libethio`) and an IBus engine wrapper (`ibus-ethiopic`). The JSON mapping covers all Ethiopic Unicode characters — the same engine and mapping work for every Ethiopic-script language (Amharic, Tigrinya, Oromo, Guragigna, etc.). Windows TSF and mobile wrappers planned.

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
│   │   ├── wordlist.h                 # Word list with prefix/tag suggestions
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
│       └── ethiopic.xml               # IBus component registration
├── data/amharic/                      # Amharic SERA mapping + wordlist (template for other languages)
├── tests/                             # Standalone test programs
│   ├── test_mapping.cpp               # Trie construction + JSON loading
│   ├── test_engine.cpp                # Core engine: key→Ethiopic sequences
│   ├── test_features.cpp              # Feature-level behavior + edge cases
│   ├── test_wordlist.cpp              # Word list loading + suggestions
│   └── test_ibus_engine.cpp           # IBus integration (key events, preedit, commit)
└── build*/                            # Build directories (gitignored)
```

## Requirements

- **CMake** 3.16+
- **C++17** compiler (tested with GCC 13+ and Clang 18+)
- **IBus** 1.5+ (`ibus-1.0` pkg-config package)
- **GLib** 2.0 (`glib-2.0`, `gio-2.0` pkg-config packages)

## Build & Install

```bash
# Configure
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr

# Build
cmake --build build

# Install (requires root for /usr prefix)
sudo cmake --install build
```

### Custom prefix

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

The install step places:
- `${LIBEXECDIR}/ibus-engine-ethiopic` — the engine executable (e.g. `/usr/libexec/ibus-engine-ethiopic`)
- `${DATADIR}/ibus/component/ethiopic.xml` — IBus component registration
- `${DATADIR}/ibus-ethiopic/` — mapping data (`*.json`), engine defaults

### Running tests

Tests are configured and built separately from the main project:

```bash
# Configure and build tests
cmake -B tests/build -S tests
cmake --build tests/build

# Run all tests
cd tests/build && ctest --output-on-failure && cd ../..
```

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

The trie engine is language-agnostic — the same `am-sera.json` mapping covers the entire Ethiopic Unicode syllabary and works for any Ethiopic-script language.

### SERA Transliteration (Default)

Type using the SERA (System for Ethiopic Representation in ASCII) scheme, supported out of the box for Amharic. The trie engine handles complex prefix disambiguation automatically — longer key sequences take priority over shorter ones, and leaf nodes auto-commit without requiring a delimiter.

#### Design Principles

The character mapping is built for productivity, guided by a few core decisions:

**Leverage existing Latin-to-Ethiopic tradition.** There is a long-established practice of writing Ethiopic words with Latin letters in emails, chats, and social media. The mapping follows the conventions users already know — SERA being the most widely used standard in academia and online communities.

**Provide multiple input paths for the same character.** Users come from different transliteration backgrounds. The engine accepts several equivalent key sequences for many characters — no single convention is forced. For example, the pharyngeal character ዐ can be typed as `/a`, `aa`, or `Xe`. The ḫ-family (ኀ) can be reached via `/he`, `hhe`, or capital `Ke`. Cross-family examples:

| Target | Path 1 | Path 2 | Path 3 |
|--------|--------|--------|--------|
| አ (ʾä) | `a` | `xe` | |
| ዐ (ʿä) | `/a` | `aa` | `Xe` |
| ኀ (ḫä) | `/he` | `hhe` | |
| ሸ (šä) | `she` | `ve` | |
| ጨ (č̣ä) | `Ce` | `cce` | |

**Include Latin-to-Ethiopic word-building shortcuts.** Beyond character-by-character mapping, common Latin-spelled word fragments are recognized and expanded directly — matching how Ethiopians naturally construct words in Latin script. This lets users type familiar English-spelled patterns and get correct Ethiopic output:

| Input | Output | Example usage |
|-------|--------|---------------|
| `hai` | ሃይ | `hailu` → ሃይሉ (or `haylu` → ሃይሉ), `hhaile` → ኃይሌ |
| `mai` | ማይ | `maik` → ማይክ |
| `sai` | ሳይ | `saint` → ሳይንት (Saint) |
| `gai` | ጋይ | `gaint` → ጋይንት (giant) |
| `tai` | ታይ | `taitai` → ታይታይ |
| `bai` | ባይ | `baibel` → ባይብል |
| `wai` | ዋይ | `wai` → ዋይ |
| `kai` | ካይ | `kelkai` → ከልካይ |
| `lai` | ላይ | `lai` → ላይ |
| `ria` | ርያ | `mariam` → ማርያም |
| `pia` | ጵያ | `etiyopia` → ኢትዮጵያ |
| `eth` | ኢትዮጵያ | (recognizes Ethiopia directly) |
| `axum` | አክሱም |

**Provide word completion and prediction.** A small curated dictionary of common words, names, and bigrams powers inline suggestions. After typing two or more Ethiopic characters, completions appear automatically. This is sample to show the possibilities of expanding in the future.
For example, after typing `እንደምን` the engine suggests: `ነህ`, `ነሽ`, `አለህ`, `አለሽ`, `ናችሁ`, `አደሩ`. Tab or number keys select the desired completion.

**Use structural characters innovatively.** A small set of ASCII punctuation characters serve double duty as IME controls:

| Character | Role |
|-----------|------|
| `'` (single quote) | Commit delimiter — flushes pending composition and starts fresh. `h'h'h` produces ህህህ. `''` escapes to a literal `'`. |
| `/` (forward slash) | Prefix for alternate consonant families and aynu vowels. `/he`→ኀ, `/a`→ዐ. `//` escapes to literal `/`. |
| Doubled consonant | Alternative for ḫ-family (`hh`→ኀ) and č̣-family (`cc`→ጨ). |
| `` ` `` (backtick) | Prefix for Geez numerals: `` `1 ``→፩, `` `10 ``→፲, `` `100 ``→፻. |

**Indian (ASCII) numerals pass through unchanged.** Since 1–9 are far more common in modern Ethiopic writing than Geez numerals (፩–፱), plain digits are not consumed by the IME. Geez numerals are accessed with the backtick prefix when needed.

**Toggle between Ethiopic and English with Ctrl+Shift.** No need to open IBus preferences or cycle through input methods. Pressing `Ctrl+Shift` instantly switches between Ethiopic SERA transliteration mode and plain English passthrough, giving fast bilingual typing without leaving the keyboard.

**Weight character frequency.** Common characters get the shortest, most ergonomic sequences. Rare characters use longer sequences or capital letters. For example, `v` maps to the common š-family (ሸ ሹ ሺ…), while capital `V` maps to the rare v-family (ቨ ቩ ቪ…). The ቨ sound is largely restricted to spoken forms and loanwords — in writing it is almost always replaced with the በ (b) family, making it low-frequency and a natural fit for the capital-letter path.

**Choose capital-letter alternates by shape similarity and home-row convenience.** Alternate consonants are accessed via capital letters that sit on the home row and are visually linked to their lowercase base forms. For instance, `D` maps to ጸ — the ጸ glyph visibly resembles the `d`-based ደ family and the `D` key is under the typist's resting middle finger. Likewise `F` maps to ፀ, echoing the `f`-based ፈ family from the home-row index-finger position. This pattern holds across the board — `S`→ሠ (looks like s-based ሰ), `T`→ጠ (looks like t-based ተ), `Z`→ዠ (looks like z-based ዘ), and so on. The capital key is both easy to reach and easy to remember.

**Avoid non-alphabetic keys for primary input.** Characters like `[`, `]`, `\`, `{`, `}` live at the periphery of the keyboard and require hand movement. They are reserved for their literal passthrough use in programming and technical contexts. The mapping uses only standard QWERTY letter keys, common punctuation, and backtick — everything within the typist's home reach.

**Keep ASCII modifiers minimal.** The mapping is designed for a single keyboard layout with no layer switching. Capital letters serve a deliberate role — they visually signal alternate consonant series (e.g. `S`→ሠ for ś-family vs `s`→ሰ for s-family), acting as a mnemonic rather than an arbitrary modifier. The `'` commit delimiter, `/` prefix, and backtick are the only non-letter keys used structurally.

**Respect literal passthrough.** Any ASCII key not claimed by the mapping (e.g. `#`, `&`, `@`, `~`) falls through to the application unchanged. Users typing code, URLs, or mixed-language text are never blocked by the IME.

The examples below use Amharic; other Ethiopic languages follow the same pattern with their own consonant inventories.

#### Consonant Families

Each consonant has 7 vowel forms (1st–7th order: ä, u, i, a, e, ə, o). The 6th order (ə) uses the bare key. The 5th order (e) uses uppercase `E`. The alias `ie` works for 5th order and `I` for 6th order on most families.

| Family      | 1st (ä) | 2nd (u) | 3rd (i) | 4th (a) | 5th (e) | 6th (ə) | 7th (o) |
|-------------|---------|---------|---------|---------|---------|---------|---------|
| h (ሀ)      | `he` ሀ  | `hu` ሁ  | `hi` ሂ  | `ha` ሃ  | `hE` ሄ  | `h` ህ   | `ho` ሆ  |
| l (ለ)      | `le` ለ  | `lu` ሉ  | `li` ሊ  | `la` ላ  | `lE` ሌ  | `l` ል   | `lo` ሎ  |
| m (መ)      | `me` መ  | `mu` ሙ  | `mi` ሚ  | `ma` ማ  | `mE` ሜ  | `m` ም   | `mo` ሞ  |
| r (ረ)      | `re` ረ  | `ru` ሩ  | `ri` ሪ  | `ra` ራ  | `rE` ሬ  | `r` ር   | `ro` ሮ  |
| s (ሰ)      | `se` ሰ  | `su` ሱ  | `si` ሲ  | `sa` ሳ  | `sE` ሴ  | `s` ስ   | `so` ሶ  |
| sh (ሸ)     | `she` ሸ | `shu` ሹ | `shi` ሺ | `sha` ሻ | `shE` ሼ | `sh` ሽ  | `sho` ሾ |
| q (ቀ)      | `qe` ቀ  | `qu` ቁ  | `qi` ቂ  | `qa` ቃ  | `qE` ቄ  | `q` ቅ   | `qo` ቆ  |
| b (በ)      | `be` በ  | `bu` ቡ  | `bi` ቢ  | `ba` ባ  | `bE` ቤ  | `b` ብ   | `bo` ቦ  |
| t (ተ)      | `te` ተ  | `tu` ቱ  | `ti` ቲ  | `ta` ታ  | `tE` ቴ  | `t` ት   | `to` ቶ  |
| c (ቸ)      | `ce` ቸ  | `cu` ቹ  | `ci` ቺ  | `ca` ቻ  | `cE` ቼ  | `c` ች   | `co` ቾ  |
| n (ነ)      | `ne` ነ  | `nu` ኑ  | `ni` ኒ  | `na` ና  | `nE` ኔ  | `n` ን   | `no` ኖ  |
| k (ከ)      | `ke` ከ  | `ku` ኩ  | `ki` ኪ  | `ka` ካ  | `kE` ኬ  | `k` ክ   | `ko` ኮ  |
| w (ወ)      | `we` ወ  | `wu` ዉ  | `wi` ዊ  | `wa` ዋ  | `wE` ዌ  | `w` ው   | `wo` ዎ  |
| z (ዘ)      | `ze` ዘ  | `zu` ዙ  | `zi` ዚ  | `za` ዛ  | `zE` ዜ  | `z` ዝ   | `zo` ዞ  |
| y (የ)      | `ye` የ  | `yu` ዩ  | `yi` ዪ  | `ya` ያ  | `yE` ዬ  | `y` ይ   | `yo` ዮ  |
| d (ደ)      | `de` ደ  | `du` ዱ  | `di` ዲ  | `da` ዳ  | `dE` ዴ  | `d` ድ   | `do` ዶ  |
| j (ጀ)      | `je` ጀ  | `ju` ጁ  | `ji` ጂ  | `ja` ጃ  | `jE` ጄ  | `j` ጅ   | `jo` ጆ  |
| g (ገ)      | `ge` ገ  | `gu` ጉ  | `gi` ጊ  | `ga` ጋ  | `gE` ጌ  | `g` ግ   | `go` ጎ  |
| f (ፈ)      | `fe` ፈ  | `fu` ፉ  | `fi` ፊ  | `fa` ፋ  | `fE` ፌ  | `f` ፍ   | `fo` ፎ  |
| p (ጰ)      | `pe` ጰ  | `pu` ጱ  | `pi` ጲ  | `pa` ጳ  | `pE` ጴ  | `p` ጵ   | `po` ጶ  |

Note: `sh` can also be typed as `v` (e.g. `ve`→ሸ). Uppercase `p` maps to the p family (`Pe`→ፔ, ፑ, ፒ …). See alternate consonants below.

#### Capital Consonant Alternates

Several Ethiopic consonants share the same ASCII base letter (e.g. both ሰ and ሠ  use 's|S'). Capital-initial keys access the alternate series:

| Key | Output | Description |
|-----|--------|-------------|
| `Se` | ሠ (ś) | ś-family |
| `He` | ሐ (ḥ) | ḥ-family |
| `De` | ጸ (ṣ) | ṣ-family |
| `Ze` | ዠ (ž) | ž-family |
| `Te` | ጠ (ṭ) | ṭ-family |
| `Ce` | ጨ (č') | č'-family (same as `cce`) |
| `Fe` | ፀ (ś́) | ś́-family |
| `Pe` | ፐ (p) | standard p (lowercase `p` yields pharyngeal ጰ) |
| `Qe` | ቐ (ḳ) | ḳ-family + labiovelars `QWe` ቘ etc. |
| `Ke` | ኸ (ḫ) | ḫ-family + labiovelars `KWe` ዀ etc. |
| `Ne` | ኘ (ñ) | ñ-family |
| `Ge` | ጘ (ŋ) | ŋ-family |
| `Je` | ዸ (ḍ) | ḍ-family |
| `Ve` | ቨ (v) | v-family |

All capital families support full 7-vowel conjugation (`Se Su Si Sa SE S So`) and labialized `-Wa` forms where applicable.

#### Slash-Prefix and Double-Consonant

The `/h` prefix and `hh` doubling both access the ḫ-family (ኀ ኁ ኂ ኃ ኄ ኅ ኆ), including labiovelars (`/hWe` ኈ, `hhWe` ኈ, etc.). These are the only consonant families using `/`-prefix or double-consonant notation. `cc` doubles for the č'-family (`cce`→ጨ).

`//` escapes to a literal `/`.

#### Labiovelar Forms (W-infix)

|       | q-family | k-family | g-family | /h-family |
|-------|----------|----------|----------|-----------|
| 1st   | `qWe` ቈ  | `kWe` ኰ  | `gWe` ጐ  | `/hWe` ኈ  |
| 2nd   |          | `kWu` ኵ  | `gWu` ጕ  | `/hWu` ኍ  |
| 3rd   | `qWi` ቊ  | `kWi` ኲ  | `gWi` ጒ  | `/hWi` ኊ  |
| 4th   | `qWa` ቋ  | `kWa` ኳ  | `gWa` ጓ  | `hWa` ኋ   |
| 5th   | `qWE` ቌ  | `kWE` ኴ  | `gWE` ጔ  | `/hWE` ኌ  |

Note: plain `hWa`→ኋ (4th only) works directly. Other h-family labiovelars require `/hW` or `hhW` prefix. Q-family and K-family labiovelars also exist via capital initials (`QWe`→ቘ, `KWe`→ዀ).

#### Standalone Vowels

Vowels typed directly. The `x` prefix is an alternative for the same vowel forms:

| Input | Output | Input | Output | x-prefix | Output |
|-------|--------|-------|--------|----------|--------|
| `a`   | አ     | `A`   | ኣ     | `xa`     | ኣ     |
| `u`   | ኡ     | `U`   | ኡ     | `xu`     | ኡ     |
| `i`   | ኢ     | `E`   | ኤ     | `xE`     | ኤ     |
| `e`   | እ     | `I`   | እ     | `x`      | እ     |
| `o`   | ኦ     | `ea`  | ኧ     | `xea`    | ኧ     |
|       |        | `ae`  | ኤ     | `xe`     | አ     |

#### Aynu (Pharyngeal) Forms

Three input styles: slash-prefix, doubled-vowel, or capital `X`:

| Slash  | Output | Double | Output | X-form | Output |
|--------|--------|--------|--------|--------|--------|
| `/a`   | ዐ     | `aa`   | ዐ     | `Xe`   | ዐ     |
| `/u`   | ዑ     | `uu`   | ዑ     | `Xu`   | ዑ     |
| `/i`   | ዒ     | `ii`   | ዒ     | `Xi`   | ዒ     |
| `/A`   | ዓ     | `AA`   | ዓ     | `Xa`   | ዓ     |
| `/E`   | ዔ     | `EE`   | ዔ     | `XE`   | ዔ     |
| `/e`   | ዕ     | `ee`   | ዕ     | `X`    | ዕ     |
| `/o`   | ዖ     | `oo`   | ዖ     | `Xo`   | ዖ     |

#### Punctuation and Symbols

| Input    | Output | Description          |
|----------|--------|----------------------|
| `:`      | ፡     | Word separator       |
| `::`     | ።     | Full stop / period   |
| `:::`    | `:`    | Literal colon        |
| `.`      | ።     | Period (alternative) |
| `,`      | ፣     | Comma                |
| `;`      | ፤     | Semicolon            |
| `;;`     | `;`    | Literal semicolon    |
| `-:`     | ፥     | Preface colon        |
| `:-`     | ፦     | Question colon       |
| `**`     | ፨     | Section mark         |
| `:|:`    | ፨     | Section mark (alt)   |
| `/?`     | ፧     | Question mark prefix |
| `??`     | ፧     | Question mark (alt)  |

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
- **Prefix priority**: Longer key sequences win over shorter ones. Example: `hWa` → ኋ (not `ህዋ`), `hie` → ሄ (not `ሂእ`).
- **Raw keys in preedit**: When no trie action is pending, raw key characters are shown in preedit. Example: typing `hW` shows `hW` until the vowel completes the labiovelar form.

### Planned

- Word prediction and suggestions (static + dynamic word pools) — **in progress**
- Windows TSF wrapper
- Mobile (Android/iOS) wrappers

## Word Suggestions

The engine supports word completion based on two word pools:

### Static Word Pool
A curated JSON dictionary of common words, place names, people, and organizations (`data/amharic/wordlist.json`). Organized into categories:

| Tag | Category |
|-----|----------|
| `common` | Frequently used words |
| `places` | Cities, towns, regions |
| `people` | Famous musicians, authors, public figures |
| `organizations` | Institutions, companies, NGOs |

The word list is loaded once at engine init. Prefix-based suggestions use binary search over a sorted, deduplicated merged list.

### Auto-Suggest Behavior
- After typing **2 or more** Ethiopic characters of a word, suggestions appear automatically
- **Tab** cycles through suggestions and accepts the selected completion

### Dynamic Word Pool (Planned)
Words harvested from the current document's surrounding text, providing context-aware completions.

## License

GPL-3.0-or-later. The bundled JSON parser (`libethio/include/ethio/json.hpp`) is MIT-licensed.

## References

| Project | Purpose |
|---------|---------|
| [ibus-libpinyin](https://github.com/libpinyin/ibus-libpinyin) | C++ core lib + IBus wrapper (architecture model) |
| [ibus-chewing](https://github.com/chewing/ibus-chewing) | Production C engine with separate core lib |
| [ibus-tmpl](https://github.com/ibus/ibus-tmpl) | Canonical IBus engine skeleton |
| [m17n-db](https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git) | Reference SERA mappings (Amharic, Tigrinya, etc.) |
| [ibus-m17n](https://github.com/ibus/ibus-m17n) | IBus ↔ m17n bridge |
| [IBus DevGuide](https://github.com/ibus/ibus/wiki/DevGuide) | Official IBus developer guide |
