# Ethiopic keyboard design plan for IBus

**Session ID:** ses_2306d2b24ffeEDAagm5gJ47Ab7
**Created:** 4/27/2026, 11:33:19 AM
**Updated:** 4/27/2026, 12:06:20 PM

---

## User

Give me a plan to design to implement ethiopic keyboard based on linux ibus engine. It would be advisable to follow the m17n implementation of Anthny and others. Please check their open source codes as well. I want to use c++ programming. Once I tested the implementation in my fedora linux box then I will plan to implement the same for windows even for mobile phones. So give me a draft plan and then let us work on that before we go for implementation. You can also give me any better suggestion of alternatives to implement this keyboard. The keyboard will be focused for amharic language first, then we can expand it for other ethiopic language varities.  It also should be based on ISO unicode standard. Please check the details of Anthy and Zhuyin chinese and other asian language keyboard implementation in IME   

---

## Assistant (Build · DeepSeek V4 Pro · 549.6s)

_Thinking:_

The user wants a comprehensive plan for implementing an Ethiopic keyboard based on the Linux IBus engine, following existing implementations like m17n, Anthy, and other Asian language IMEs. Let me first research the codebase they have, then provide a detailed plan.

Let me explore:
1. The current working directory to see what's already there
2. Research m17n implementation
3. Research Anthy/Zhuyin IME implementations
4. Understand IBus engine architecture

This is a research and planning task, so I should use the explore agent and web fetch tools to gather information before creating a plan.

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/moab/sources/ethiopic-keyboard"
}
```

**Output:**
```
<path>/home/moab/sources/ethiopic-keyboard</path>
<type>directory</type>
<entries>


(0 entries)
</entries>
```

**Tool: task**

**Input:**
```json
{
  "description": "Research m17n Ethiopic implementation",
  "subagent_type": "general",
  "prompt": "Research how m17n library (m17n-lib) implements Ethiopic keyboard input methods. Specifically:\n\n1. Search the web for \"m17n ethiopic keyboard implementation\" and \"m17n-lib ethiopic input method\"\n2. Look at the m17n-db and m17n-lib source repositories (they're on Savannah or GitHub mirrors):\n   - How does m17n define keyboard layouts? (Look at .mim files - m17n input method files)\n   - What's the structure of an .mim file for Ethiopic scripts (am-ethio.mim or similar)?\n   - How does m17n-lib process these files?\n   - What's the IBus-m17n integration like? (ibus-m17n engine)\n\n3. Search for \"ibus-m17n source code\" to understand how it bridges IBus and m17n\n\nReturn a detailed summary of:\n- The m17n input method file format and structure\n- Key concepts (maps, states, rules, variables)\n- How Ethiopic syllabic mapping is done in m17n\n- The ibus-m17n bridge architecture\n- Repository URLs for the actual source code"
}
```

**Output:**
```
task_id: ses_2306cf4ccffeewVFP6e1XQUaBP (for resuming to continue this task if needed)

<task_result>
Here is the comprehensive summary:

---

## m17n Ethiopic Keyboard Input Method Implementation

### Repository URLs

| Component | URL |
|-----------|-----|
| **m17n-lib** (core library) | `https://git.savannah.nongnu.org/cgit/m17n/m17n-lib.git` |
| **m17n-db** (input method data) | `https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git` |
| **m17n-docs** | `https://git.savannah.nongnu.org/cgit/m17n/m17n-docs.git` |
| **ibus-m17n** (IBus bridge) | `https://github.com/ibus/ibus-m17n` |
| Savannah project page | `https://savannah.nongnu.org/projects/m17n` |
| Manual (Japanese, frames) | `https://www.nongnu.org/m17n/manual-ja/index.html` |

---

### 1. MIM File Format & Structure

`.mim` files are **S-expression (Lisp-like)** files defining input methods. They live in `/usr/share/m17n/` and are loaded by the m17n library at runtime via `mdatabase_load()` in `database.c`. The MIM filename convention is `<language>-<name>.mim` (e.g., `am-sera.mim`).

**Top-level structure of a `.mim` file:**

```lisp
(input-method <language-symbol> <name-symbol>)    ;; identifies the IM

(title "Display Title")                            ;; optional UI title

(description "Human-readable description...")       ;; optional

(map ...)                                           ;; key translation maps

(state ...)                                         ;; state machine definitions

(variable ...)                                      ;; configurable variables

(command ...)                                       ;; keybindings for commands
```

**Key files:**
- **`global.mim`** (`Mt global`) — defines global variables (`candidates-group-size`, `fallback-input-method`), and default command keybindings (Return=commit, Space=convert, Escape=revert, arrow keys for candidate navigation, etc.)
- **`am-sera.mim`** (`am sera`) — Amharic input using the SERA (System for Ethiopic Representation in ASCII) transliteration scheme. Contains 757 lines mapping Latin keystrokes to Ethiopic syllables.

---

### 2. Key Concepts: Maps, States, Rules, Variables

#### Maps (`input.c:380-395`)
The core data structure is `MIMMap`:

```c
struct MIMMap {
    MPlist *map_actions;     // Actions executed when this map is reached
    MPlist *submaps;         // List of deeper maps keyed by MSymbol (NULL = terminal)
    MPlist *branch_actions;  // Actions executed when leaving this map successfully
};
```

Maps form a **trie (prefix tree)**. Each map entry is a key sequence mapped to actions. In the `.mim` file:

```lisp
(map
  (map                          ;; root map
    ("he" ?ሀ)                   ;; keyseq -> insert (shorthand for (insert "ሀ"))
    ("hu" ?ሁ)                   ;; ?X means Unicode character X
    ("h" ?ህ)
    (branch                      ;; named branch with actions
      (nil (delete @-) (insert "something"))
    )
  ))
```

**How keys are translated:**
When a map is a terminal (no submaps), reaching it triggers `map_actions`. Each `(KEYSEQ ACTION*)` entry creates a path through submaps. The key sequence can be:
- A string: `"he"` — each character becomes a symbol
- A plist of symbols: `(C-a B)` — explicit modifier keys

#### States (`input.c:408-418`)
```c
struct MIMState {
    MSymbol name;       // Name of the state (e.g., "init")
    MText *title;       // Optional display title
    MIMMap *map;        // Key translation map (root of the map trie)
};
```

States define *groups of maps*. A `.mim` file defines a state machine:

```lisp
(state
  (init                          ;; state name
    (map                          ;; root map for this state
      ...                         ;; branches within this state
    )
  )
)
```

The initial state is always named `init`. States can be switched via the `shift` action in map/branch actions.

#### Branches (`input.c:1144-1174`)
Branches are named submaps within a state. The `branch` keyword in a map defines actions for when a specific submap is reached or exited:

```lisp
(map
  (nil (action1) (action2))      ;; nil = default branch_actions for this map
  ((submap-name) (action1) ...)  ;; named branch = map_actions for submaps
)
```

- `nil` → sets `branch_actions` (executed on success OR failure to find next key)
- `t` → sets `map_actions` (executed when this map is reached)
- Other symbols → creates named branches with specific actions

#### Actions
Common actions parsed by `take_action_list()` in `input.c:3102`:
- **`insert`** — Insert text into preedit buffer (the default when no action name is given)
- **`delete`** — Delete characters from preedit
- **`commit`** — Force commit of current preedit
- **`pushback`** — Push a key back into the input queue
- **`undo`** — Undo last action
- **`shift`** — Switch to a different state
- **`set`** — Set a variable
- **`cond`** — Conditional branching based on variables
- **`unhandle`** — Mark key as unhandled (fall through to fallback IM)
- **Arithmetic**: `add`, `sub`, `mul`, `div`, `=`, `<`, `>`, `<=`, `>=`

#### Variables (`input.c:528-532`)
Defined via `(variable (NAME DESCRIPTION DEFAULT-VALUE))`. Global variables from `global.mim`:
- `candidates-group-size` (default 10) — max candidates per group
- `candidates-charset` — restrict candidates to a charset
- `fallback-input-method` (default `"lsymbol, unicode"`) — chained fallback IMs

#### Commands (`input.c:1868-1906`)
Defined via `(command (NAME DESCRIPTION KEYSEQ...))`. Key sequences can be strings or plists of symbols. Global commands include: `commit` (Return/Linefeed), `convert` (Space/Henkan), `revert` (Escape), candidate navigation (arrows/numbers), preedit navigation, delete, reload (M-Return).

#### Macros & External Modules
- **`(macro (NAME ACTION*))`** — defines reusable action sequences
- **`(module NAME SYMBOL*)`** — loads shared libraries (`.so`) for custom C functions (e.g., Anthy for Japanese)
- **`(include FILENAME)`** — includes another `.mim` file

---

### 3. How Ethiopic Syllabic Mapping Is Done in m17n

The file `am-sera.mim` (Amharic SERA) uses a **single flat map** (no sub-states) inside the `init` state. Each Ethiopic consonant has 7 vowel forms (ä, u, i, a, e, ə, o) plus labialized variants. The mapping is purely phonetic ASCII → Unicode:

```lisp
(map
 (map
  ;; h-family (ሀ ሁ ሂ ሃ ሄ ህ ሆ)
  ("he" ?ሀ)  ("hu" ?ሁ)  ("hi" ?ሂ)  ("ha" ?ሃ)  ("hE" ?ሄ)  ("h" ?ህ)  ("ho" ?ሆ)
  ;; hW-family with labialization (ኋ ኈ ኍ ኊ ኌ)
  ("hW" ?ኋ)  ("hWa" ?ኋ)  ("hWe" ?ኈ)  ("hWu" ?ኍ)  ("hWi" ?ኊ)  ("hWE" ?ኌ)

  ;; l-family (ለ ሉ ሊ ላ ሌ ል ሎ)
  ("le" ?ለ)  ("lu" ?ሉ)  ("li" ?ሊ)  ("la" ?ላ)  ("lE" ?ሌ)  ("l" ?ል)  ("lo" ?ሎ)
  ;; ... same for ~35 consonant families
))
```

**Key design decisions in `am-sera.mim`:**
- Vowel suffixes: `e`=ä, `u`=u, `i`=i, `a`=a, `E`/`ee`=e, (no vowel)=ə, `o`=o
- Labialization: `W` suffix (e.g., `hWe` → ኈ)
- Case variations for emphatic consonants: lowercase for plain, uppercase for emphatic (e.g., `se`=ሰ vs `Se`=ሸ, `he`=ሀ vs `He`=ሐ) — though note in SERA, some mappings differ from this pattern
- Multiple aliases: e.g., `hhe`/`h2e`/`` `he `` all map to ኀ (providing alternative input conventions)
- Special forms: `ea`=ኧ, vowel-only forms (`a`=አ, `u`=ኡ, `i`=ኢ, `e`=እ, `o`=ኦ)
- Punctuation: `:`=፡, `::`=።, `,`=፣, etc.
- Ethiopic numerals: `` `1 `` through `` `1000000 ``

**There is no `am-ethio.mim` in the current m17n-db**. The available Ethiopic-related files are:
- `am-sera.mim` — Amharic SERA transliteration
- `gez.lnm` — Ge'ez language name map (for font/language tagging)

Other Amharic input methods exist outside m17n (e.g., GeezIME, Keyman layouts) but are not part of m17n-db.

---

### 4. The m17n-lib Input Processing Engine

The heart of the engine is in `m17n-lib/src/input.c` (~7300 lines). Processing flow:

1. **`minput_open_im(language, name, arg)`** → loads the `.mim` file from the m17n database, parses S-expressions, builds `MInputMethodInfo` with parsed states, maps, commands, variables.

2. **`minput_create_ic(im, arg)`** → creates `MInputContext` with initialized state (starts at `init` state's root map).

3. **`minput_filter(ic, key, arg)`** → calls the driver's `filter()` function (`input.c:4175`). The filter:
   - Calls `handle_key()` which traverses the map trie
   - Looks up `key` in `map->submaps` (with alias resolution via `M_key_alias`)
   - If a submap is found, descends into it and executes `map_actions`
   - If a terminal map is reached or the key isn't found, executes `branch_actions`
   - Handles fallback IMs, IM push/pop, state shifting
   - Produces text in `ic->produced`

4. **`minput_lookup(ic, key, arg, mt)`** → retrieves produced text into `mt`.

**Key alias system** (`input.c:580-700`):
- Single characters (0-255) get aliases: char → `M-char` → `A-char` → `C-M-char`
- This allows maps to handle modified keys by following alias chains

**State shifting** (`shift_state` in `input.c`):
- `(shift STATE-NAME)` action changes the current active state
- The `init` state is the default; shifting to `nil` resets to initial state

**Fallback input methods**:
- If the current IM can't handle a key, `check_fallback()` iterates through `fallback-input-method` (e.g., `lsymbol, unicode`) to handle special symbol input

**Error handling**:
- If a key reaches the root map of the initial state unhandled, it's marked `key_unhandled = 1`
- If 100 iterations occur without progress, the engine resets (sanity check)

---

### 5. The ibus-m17n Bridge Architecture

**Repository**: `https://github.com/ibus/ibus-m17n` (C, GObject-based)

**Architecture** (`src/engine.c`):

The bridge works by **dynamically creating GObject types for each m17n input method** at runtime:

```
IBus Engine Name:  "m17n:<lang>:<name>"
GObject Type Name:  "IBusM17N<Lang><Name>Engine"
```

**Key structures:**

- **`IBusM17NEngine`** (instance): holds `MInputContext *context`, `IBusLookupTable *table`, preedit/property state
- **`IBusM17NEngineClass`** (class, shared per IM): holds `MInputMethod *im`, title, icon, language, name, preedit colors, GSettings

**Initialization flow:**
1. `ibus_m17n_engine_class_init()` — loads `.mim` via `minput_open_im()`, reads title/icon via `minput_get_title_icon()`, connects GSettings
2. `ibus_m17n_engine_constructor()` — calls `minput_create_ic()`, registers callbacks for all `Minput_*` events (preedit, status, candidates, surrounding text)

**Key event processing** (`ibus_m17n_engine_process_key_event`):
1. Checks for password/PIN fields → skip processing
2. Optional US layout translation (`use-us-layout` setting)
3. Falls through to parent `IBusEngineSimple` for Compose key sequences
4. Converts IBus key event to m17n key symbol via `ibus_m17n_key_event_to_symbol()`:
   - Maps IBus modifiers to m17n prefixes: `C-` (Control), `S-` (Shift), `A-` (Alt/Mod1), `G-` (AltGr/Mod5), `M-` (Meta), `s-` (Super), `H-` (Hyper)
   - Uses `ibus_keyval_name()` for non-ASCII keysyms
5. Calls `minput_filter()` then `minput_lookup()` via `ibus_m17n_engine_process_key()`

**Callback system** (`ibus_m17n_engine_callback`):
- `Minput_preedit_draw` → updates IBus preedit text with colors/underline
- `Minput_status_draw` → updates IBus property (input method indicator)
- `Minput_candidates_draw` → populates IBus lookup table from m17n candidate list
- `Minput_get_surrounding_text` → fetches surrounding text from IBus for context-sensitive IMs
- `Minput_delete_surrounding_text` → deletes surrounding text (used by some IMs)

**Special handling:**
- **Compose key**: Inherits from `IBusEngineSimple`, so Compose sequences work alongside m17n
- **Ordering fix**: A 0.1s `nanosleep` after commits helps Mutter get event ordering right (issue #72)
- **Shift handling**: Only adds `S-` prefix for non-graphical shift results (issue #90)
- **Password fields**: Emulates focus-out to discard pending input state

---

### Summary of Data Flow

```
User types key
  → IBus delivers key event
    → ibus-m17n engine converts to MSymbol (e.g., "h", "e", "S-Return")
      → minput_filter(ic, key)
        → m17n-lib traverses MIMMap trie
          → finds ("he" ?ሀ) at terminal map
            → executes (insert "ሀ") → stores in ic->produced
      → minput_lookup(ic, key) → retrieves produced text
    → ibus-m17n commits "ሀ" via IBus
```
</task_result>
```

**Tool: task**

**Input:**
```json
{
  "description": "Research Anthy IBus engine architecture",
  "subagent_type": "general",
  "prompt": "Research how Anthy (Japanese input method) implements its IBus engine. This is a model for building a custom IBus engine in C/C++.\n\nSearch and gather information about:\n\n1. The Anthy IBus engine (ibus-anthy) source code structure:\n   - How does it implement the IBusEngine interface?\n   - Key files: engine.cc, engine.h, etc.\n   - How does it subclass IBusEngine?\n   - How does it handle key events (process_key_event)?\n   - How does it manage preedit (composing) text?\n   - How does it commit text?\n\n2. The IBus C API for creating engines:\n   - IBusEngine class structure\n   - Key virtual methods to override\n   - Signal handling\n   - Lookup table management\n\n3. Also briefly look at ibus-table or ibus-libzhuyin for comparison\n\n4. The IBus development documentation and API reference\n\nReturn:\n- Anthy's IBus engine architecture overview\n- Key classes and their responsibilities\n- The event flow (key press → process → commit/preedit)\n- Repository URLs\n- Best practices for writing a custom IBus engine in C++"
}
```

**Output:**
```
task_id: ses_2306cd67dffeJQ7isXvryotRlD (for resuming to continue this task if needed)

<task_result>
Here is a comprehensive research summary on how IBus engines are built in C/C++, based on ibus-anthy, ibus-chewing, ibus-tmpl, and the IBus C API.

---

## Repository URLs

| Repo | URL | Language |
|------|-----|----------|
| **IBus core** | `https://github.com/ibus/ibus` | C (GLib/GObject) |
| **ibus-tmpl** (template) | `https://github.com/ibus/ibus-tmpl` | C + Python |
| **ibus-chewing** (production C) | `https://github.com/chewing/ibus-chewing` | C |
| **ibus-anthy** (production Python) | `https://github.com/ibus/ibus-anthy` | Python (SWIG binds C lib) |
| **ibus-libpinyin** (C++ production) | `https://github.com/libpinyin/ibus-libpinyin` | C++ |
| **IBus 1.5 API docs** | `http://ibus.github.io/docs/ibus-1.5/index.html` | HTML docs |
| **Dev wiki** | `https://github.com/ibus/ibus/wiki/Develop` | Guide |

---

## IBusEngine C++ Class Hierarchy (GObject-based)

IBus uses GLib/GObject for its C API. The class hierarchy is:

```
GObject
  ⮕ IBusObject
      ⮕ IBusService
          ⮕ IBusEngine          ← you subclass this
```

The `IBusEngine` struct (`ibus/src/ibusengine.h`) defines the engine instance:

```c
struct _IBusEngine {
    IBusService parent;
    /* public fields: */
    gboolean enabled;
    gboolean has_focus;
    IBusRectangle cursor_area;
    guint client_capabilities;
};
```

### Virtual method table (what you override):

```c
struct _IBusEngineClass {
    IBusServiceClass parent;

    /* === KEY VIRTUAL METHODS === */
    gboolean (*process_key_event)(IBusEngine *engine,
                                  guint keyval, guint keycode, guint state);
    void     (*focus_in)        (IBusEngine *engine);
    void     (*focus_out)       (IBusEngine *engine);
    void     (*reset)           (IBusEngine *engine);
    void     (*enable)          (IBusEngine *engine);
    void     (*disable)         (IBusEngine *engine);
    void     (*set_cursor_location)(IBusEngine *engine, gint x, gint y,
                                    gint w, gint h);
    void     (*set_capabilities)(IBusEngine *engine, guint caps);

    /* Lookup table navigation */
    void     (*page_up)         (IBusEngine *engine);
    void     (*page_down)       (IBusEngine *engine);
    void     (*cursor_up)       (IBusEngine *engine);
    void     (*cursor_down)     (IBusEngine *engine);

    /* Property / candidate interaction */
    void     (*property_activate)(IBusEngine *engine,
                                  const gchar *prop_name, guint prop_state);
    void     (*candidate_clicked)(IBusEngine *engine,
                                  guint index, guint button, guint state);

    /* Surrounding text, handwriting, content-type */
    void     (*set_surrounding_text)(IBusEngine *engine,
                                     IBusText *text,
                                     guint cursor_index, guint anchor_pos);
    void     (*set_content_type)(IBusEngine *engine,
                                 guint purpose, guint hints);
    /* ... focus_in_id, focus_out_id, process_hand_writing_event ... */
};
```

---

## How to Subclass IBusEngine in C (ibus-tmpl pattern)

### 1. Define your engine struct and use `G_DEFINE_TYPE`

From `ibus-tmpl/src/engine.h`:
```c
#define IBUS_TYPE_ENCHANT_ENGINE (ibus_enchant_engine_get_type())
GType ibus_enchant_engine_get_type(void);
```

From `ibus-tmpl/src/engine.c`:
```c
struct _IBusEnchantEngine {
    IBusEngine parent;          // MUST be first field

    GString *preedit;           // composing buffer
    gint cursor_pos;            // cursor in preedit
    IBusLookupTable *table;     // candidate lookup table
};

struct _IBusEnchantEngineClass {
    IBusEngineClass parent;
};

G_DEFINE_TYPE(IBusEnchantEngine, ibus_enchant_engine, IBUS_TYPE_ENGINE)
```

### 2. Class init: connect virtual methods

```c
static void
ibus_enchant_engine_class_init(IBusEnchantEngineClass *klass)
{
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS(klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

    // This is the one method you MUST implement
    engine_class->process_key_event =
        ibus_enchant_engine_process_key_event;

    // Destroy for cleanup
    ibus_object_class->destroy =
        (IBusObjectDestroyFunc) ibus_enchant_engine_destroy;
}
```

### 3. Instance init: allocate resources

```c
static void
ibus_enchant_engine_init(IBusEnchantEngine *enchant)
{
    enchant->preedit = g_string_new("");
    enchant->cursor_pos = 0;

    enchant->table = ibus_lookup_table_new(9, 0, TRUE, TRUE);
    g_object_ref_sink(enchant->table);
}
```

---

## The Event Flow: Key Press → Process → Commit/Preedit

### ibus-anthy (Python) and ibus-chewing (C) follow identical patterns:

```
[User presses key]
      │
      ▼
process_key_event(keyval, keycode, state)
      │
      ├── Check modifiers (RELEASE_MASK, CTRL, MOD1, MOD4)
      │   └── If modifier-only → maybe pass through
      │
      ├── Check for password field (InputPurpose.PASSWORD) → return FALSE
      │
      ├── Look up keybinding table (configurable commands)
      │   └── If command found → execute & return TRUE
      │
      ├── Classify key:
      │   ├── Backspace/Delete → edit preedit buffer
      │   ├── Space/Return → commit preedit + reset
      │   ├── Escape → clear preedit + reset
      │   ├── Arrow keys → move cursor / navigate candidates
      │   ├── Number keys (with lookup table visible) → select candidate
      │   └── Character keys → append to preedit
      │
      └── Call engine_update() to refresh display
          ├── update_preedit_text()   ← ibus_engine_update_preedit_text()
          ├── update_auxiliary_text() ← ibus_engine_update_auxiliary_text()
          ├── update_lookup_table()   ← ibus_engine_update_lookup_table()
          └── commit_text()           ← ibus_engine_commit_text()
```

### The `process_key_event` signature (C):

```c
static gboolean
engine_process_key_event(IBusEngine *engine,
                         guint       keyval,    // GDK keysym
                         guint       keycode,   // hardware scancode
                         guint       state)     // modifier mask
```

**Return value:**
- `TRUE` → key consumed by engine (don't pass to application)
- `FALSE` → key not handled (pass through to application)

### Key event processing in ibus-chewing (`ibus-chewing-engine.c`):

```c
gboolean ibus_chewing_engine_process_key_event(IBusEngine *engine,
        KSym keySym, guint keycode, KeyModifiers unmaskedMod)
{
    // 1. Ignore Super-key combos (pass through)
    if (unmaskedMod & IBUS_MOD4_MASK) return FALSE;

    // 2. Ignore password fields
    if (is_password(self)) return FALSE;

    // 3. Translate keycode → keysym (system layout mapping)
    KSym kSym = ibus_chewing_pre_edit_key_code_to_key_sym(
        self->icPreEdit, keySym, keycode, unmaskedMod);

    // 4. Delegate to libchewing core
    gboolean result = ibus_chewing_pre_edit_process_key(
        self->icPreEdit, kSym, unmaskedMod);

    // 5. Update UI: commit → preedit → aux → lookup table
    ibus_chewing_engine_update(self);

    return result;
}
```

### ibus-anthy (Python) key processing (`engine.py:1958`):

```python
def __process_key_event_internal2(self, keyval, keycode, state):
    # 1. Check password field
    if self.__input_purpose == IBus.InputPurpose.PASSWORD:
        return False

    # 2. Strip RELEASE_MASK, handle only press events
    is_press = (state & IBus.ModifierType.RELEASE_MASK) == 0
    if not is_press: return False

    # 3. Look up keybindings
    key = self._mk_key(keyval, state)
    for cmd in self.__keybind.get(key, []):
        if getattr(self, cmd)(keyval, state):
            return True

    # 4. Handle Ctrl+Shift+u (ISO 14755) in Japanese mode
    if keyval == IBus.KEY_U and state == (SHIFT | CTRL):
        return True  # eats the key

    # 5. Process printable characters
    if 0x21 <= keyval <= 0x7e:
        ret = self.__on_key_common(keyval, state)
        # Handle trigger periods (、。）for conversion
        return ret
    else:
        # If preedit not empty, eat non-printable keys
        if not self.__preedit_ja_string.is_empty():
            return True
        return False
```

---

## Managing Preedit (Composing) Text

### C API:

```c
// Update preedit with attributes (underline, cursor)
IBusText *text = ibus_text_new_from_static_string(preedit_str);
text->attrs = ibus_attr_list_new();
ibus_attr_list_append(text->attrs,
    ibus_attr_underline_new(IBUS_ATTR_UNDERLINE_SINGLE, 0, len));

// Show preedit text at cursor position
ibus_engine_update_preedit_text((IBusEngine *)engine, text,
                                cursor_pos, TRUE);

// Or use mode variant (commit preedit on focus out)
ibus_engine_update_preedit_text_with_mode(engine, text,
    cursor_pos, TRUE, IBUS_ENGINE_PREEDIT_COMMIT);

// Hide preedit
ibus_engine_hide_preedit_text((IBusEngine *)engine);
```

### Python API (ibus-anthy):

```python
def update_preedit(self, string, attrs, cursor_pos, visible):
    text = IBus.Text.new_from_string(string)
    for attr in attrs:
        text.append_attribute(attr.type, attr.value,
                              attr.start, attr.end)
    self.update_preedit_text(text, cursor_pos, visible)

# Or with focus-out mode:
self.update_preedit_text_with_mode(text, cursor_pos, visible,
    IBus.PreeditFocusMode.COMMIT)
```

---

## Committing Text

### C API:

```c
// Commit text to the application
IBusText *text = ibus_text_new_from_static_string(string);
ibus_engine_commit_text((IBusEngine *)engine, text);
// text is unreffed by ibus if floating

// Delete surrounding text
ibus_engine_delete_surrounding_text(engine, offset, nchars);
```

### ibus-chewing commit flow (production pattern):

```c
void refresh_outgoing_text(IBusChewingEngine *self) {
    gchar *outgoingStr = ibus_chewing_pre_edit_get_outgoing(self->icPreEdit);
    self->outgoingText = g_object_ref_sink(
        ibus_text_new_from_string(outgoingStr));
}

void commit_text(IBusChewingEngine *self) {
    refresh_outgoing_text(self);
    if (!ibus_text_is_empty(self->outgoingText))
        ibus_engine_commit_text(IBUS_ENGINE(self), self->outgoingText);
    ibus_chewing_pre_edit_clear_outgoing(self->icPreEdit);
}
```

---

## Lookup Table (Candidate) Management

### C API:

```c
// Create table: page_size, cursor_pos, cursor_visible, round
IBusLookupTable *table = ibus_lookup_table_new(9, 0, TRUE, TRUE);

// Set orientation
ibus_lookup_table_set_orientation(table, TRUE); // vertical/horizontal

// Add candidates
ibus_lookup_table_clear(table);
ibus_lookup_table_append_candidate(table,
    ibus_text_new_from_string("candidate1"));

// Set select-key labels (1. 2. 3. ...)
ibus_lookup_table_set_label(table, i,
    ibus_text_new_from_printf("%c.", '1' + i));

// Show/hide
ibus_engine_update_lookup_table(engine, table, TRUE);
ibus_engine_show_lookup_table(engine);
ibus_engine_hide_lookup_table(engine);

// Fast update for large tables (> page_size * 4)
ibus_engine_update_lookup_table_fast(engine, table, TRUE);

// Navigation helpers
ibus_lookup_table_cursor_up(table);
ibus_lookup_table_page_down(table);
```

### Python API (ibus-anthy):

```python
self.__lookup_table = IBus.LookupTable.new(
    page_size=9, cursor_pos=0, cursor_visible=True, round=True)

self.__lookup_table.append_candidate(IBus.Text.new_from_string(candidate))
self.update_lookup_table(self.__lookup_table, visible)
self.show_lookup_table()
```

---

## Forwarding Key Events

```c
// Pass a key through to the application
ibus_engine_forward_key_event(engine, keyval, keycode, state);
```

Used when the engine partially handles a key but wants the original key also sent to the app (e.g., Shift+Space for fullwidth/halfwidth toggle).

---

## Property List (Language Bar / IME Options)

### C pattern (ibus-chewing):

```c
// Create properties
self->InputMode = ibus_property_new(
    "InputMode", PROP_TYPE_NORMAL,
    ibus_text_new_from_static_string("中"),  // label
    NULL,                                    // icon
    ibus_text_new_from_static_string("Toggle"), // tooltip
    TRUE, TRUE,                              // sensitive, visible
    PROP_STATE_UNCHECKED,
    NULL);                                   // sub_props

// Build prop list
self->prop_list = ibus_prop_list_new();
ibus_prop_list_append(self->prop_list, self->InputMode);

// Register with IBus (called on focus_in)
ibus_engine_register_properties(engine, self->prop_list);

// Update a property (reflect state change)
ibus_property_set_label(prop, new_label);
ibus_engine_update_property(engine, prop);

// Handle property clicks via:
// engine_class->property_activate = ibus_chewing_engine_property_activate;
```

---

## Main Program Setup (C)

From `ibus-tmpl/src/main.c` and `ibus-chewing/src/main.c`:

```c
int main(int argc, char **argv) {
    // 1. Parse options, set locale
    g_option_context_parse(context, &argc, &argv, &error);

    // 2. Initialize IBus
    ibus_init();

    // 3. Create bus connection
    bus = ibus_bus_new();
    g_signal_connect(bus, "disconnected", G_CALLBACK(ibus_disconnected_cb), NULL);

    // 4. Create factory and register engine type
    factory = ibus_factory_new(ibus_bus_get_connection(bus));
    ibus_factory_add_engine(factory, "engine-name", IBUS_TYPE_YOUR_ENGINE);

    // 5. Register component or request name
    if (exec_by_ibus) {
        ibus_bus_request_name(bus, "org.freedesktop.IBus.YOUR_NAME", 0);
    } else {
        IBusComponent *comp = ibus_component_new(...);
        ibus_component_add_engine(comp, ibus_engine_desc_new(...));
        ibus_bus_register_component(bus, comp);
    }

    // 6. Enter main loop
    ibus_main();
    return 0;
}
```

---

## Summary: Key Responsibilities by Class

| Class / File | Responsibility |
|---|---|
| `main.c` | Entry point: bus, factory, component registration, GLib main loop |
| `engine.h` | GType declaration, struct with `IBusEngine parent` + engine-specific data |
| `engine.c` | Core logic: `process_key_event`, preedit/commit/lookup update, focus/reset/enable |
| `preedit.c` (optional) | Preedit buffer management (ibus-chewing pattern) |
| `lookup-table.c` (optional) | Candidate table construction and update |

---

## Best Practices for Writing a Custom IBus Engine in C++

1. **Start from `ibus-tmpl`** (`https://github.com/ibus/ibus-tmpl`) -- clone it and replace the engine name/type.

2. **Use `G_DEFINE_TYPE`** or `G_DECLARE_FINAL_TYPE` (modern GLib) to subclass `IBusEngine`:
   ```c
   G_DECLARE_FINAL_TYPE(MyEngine, my_engine, MY, ENGINE, IBusEngine)
   ```

3. **Keep preedit state in your engine struct**: `GString *preedit`, cursor position, lookup table pointer.

4. **`process_key_event` must be fast** -- return quickly. Use `GLib.idle_add()` for async work (see ibus-anthy's `__invalidate()`).

5. **Always check password fields** via `set_content_type` (ignore keys for passwords):
   ```c
   if (purpose == IBUS_INPUT_PURPOSE_PASSWORD) return FALSE;
   ```

6. **Filter modifiers correctly**:
   - Strip `IBUS_RELEASE_MASK` (only handle press, not release)
   - Pass through `IBUS_MOD4_MASK` (Super/Win key combos)
   - Check `IBUS_CONTROL_MASK | IBUS_MOD1_MASK`

7. **Use GSettings** for engine configuration (see ibus-chewing's GSettigs bindings).

8. **Separate core IME logic** from IBus glue -- keep the linguistic engine (conversion, dictionary) in its own library, and have the IBus engine be a thin wrapper (libchewing, libanthy, libpinyin pattern).

9. **Test with**: `G_MESSAGES_DEBUG=all /usr/libexec/ibus-engine-your-engine --ibus`

10. **Build with**: autotools (`autogen.sh` + `make`) or meson/CMake (ibus-chewing uses CMake and meson).
</task_result>
```

**Tool: task**

**Input:**
```json
{
  "description": "Research Ethiopic Unicode standard",
  "subagent_type": "general",
  "prompt": "Research the Unicode standard for Ethiopic script, specifically for Amharic language input.\n\nSearch for:\n1. Ethiopic Unicode block (U+1200–U+137F) structure\n2. Ethiopic Supplement (U+1380–U+139F)\n3. Ethiopic Extended blocks\n4. How Ethiopic syllables are organized - the base character + vowel modification pattern\n5. The standard Amharic keyboard layout (both phonetic/typewriter and SERA/transliteration approaches)\n6. ISO standards related to Ethiopic keyboard layout (if any)\n7. Common transliteration schemes for Amharic (SERA - System for Ethiopic Representation in ASCII, EAE transliteration, etc.)\n8. The Geez script structure: consonants × 7 vowels (ä, u, i, a, e, ə, o) + labialized variants\n\nReturn:\n- Unicode range details for Ethiopic syllables\n- The mathematical structure of Ethiopic syllables (how to compute syllable from consonant + vowel)\n- Common keyboard layouts and transliteration schemes\n- Any ISO standards for Ethiopic keyboard"
}
```

**Output:**
```
task_id: ses_2306cb93effemKeHnykkNxWmM5 (for resuming to continue this task if needed)

<task_result>
Here is the compiled research:

---

## 1. Unicode Range Details

| Block | Range | Code Points | Content |
|-------|-------|-------------|---------|
| **Ethiopic** | `U+1200–U+137F` | 358 assigned (384 total) | Core syllabary: consonants × 7 vowels, labialized variants, punctuation, numerals |
| **Ethiopic Supplement** | `U+1380–U+139F` | 26 assigned | Additional syllables for Sebatbeit, Me'en, Bilen, etc. |
| **Ethiopic Extended** | `U+2D80–U+2DDF` | 79 assigned | Additional syllables for Gurage, Basketo, Gumuz languages |
| **Ethiopic Extended-A** | `U+AB00–U+AB2F` | 32 assigned | Gamo-Gofa-Dawro, Basketo, Gumuz supplements |
| **Ethiopic Extended-B** | `U+1E7E0–U+1E7FF` | 28 assigned | Additional Gurage and extended characters |

**ISO 15924 code**: `Ethi` (430) — "Ethiopic (Geʻez)"

---

## 2. Mathematical Structure of Ethiopic Syllables

The Geʻez script is an **abugida** (alphasyllabary). Each base consonant is modified in a systematic way to indicate the following vowel.

### Core formula: 26 consonants × 7 vowels + labiovelar variants + additional consonants for modern languages

**The 7 vowel orders** (traditional numbering 1st–7th):

| Order | Traditional name | Amharic vowel | Unicode offset |
|-------|-----------------|---------------|----------------|
| 1st | **gəʽəz** (ግዕዝ) | **ä** [ə] | +0 — base form |
| 2nd | **kaʽəb** (ካዕብ) | **u** [u] | +1 |
| 3rd | **saləs** (ሣልስ) | **i** [i] | +2 |
| 4th | **rabʽ** (ራብዕ) | **a** [aː] | +3 |
| 5th | **haməs** (ኃምስ) | **e** [e] | +4 |
| 6th | **sadəs** (ሳድስ) | **ə** [ɨ] | +5 |
| 7th | **sabʽ** (ሳብዕ) | **o** [o] | +6 |

### Syllable computation rule

Each consonant family occupies a contiguous block in Unicode. The code point is:

```
code_point = BASE_CONSONANT + vowel_offset
```

Where `vowel_offset ∈ {0, 1, 2, 3, 4, 5, 6}`.

**Example** — consonant **b** (በ) family:

```
በ = U+1260  (bä)    BASE + 0
ቡ = U+1261  (bu)    BASE + 1
ቢ = U+1262  (bi)    BASE + 2
ባ = U+1263  (ba)    BASE + 3
ቤ = U+1264  (be)    BASE + 4
ብ = U+1265  (bə)    BASE + 5
ቦ = U+1266  (bo)    BASE + 6
```

### Labiovelar (8th order) variants

Four velar consonants have labialized forms (`kw`, `gw`, `qw`, `hw`) that use vowel offsets at `+8, +0, +1, +2, +5` (5 forms, not 7):

```
ከ (kä) → ኰ (kʷä) at offset +8, ኲ (kʷi), ኳ (kʷa), ኴ (kʷe), ኵ (kʷə)
```

### Additional modern-language consonants

Derived by adding a horizontal line to a phonetically-similar consonant: `በ (b)` → `ቨ (v)`, `ተ (t)` → `ቸ (č/t͡ʃ)`, `ሰ (s)` → `ሸ (š/ʃ)`, `ነ (n)` → `ኘ (ñ/ɲ)`, etc.

---

## 3. Keyboard Layouts and Transliteration Schemes

### A. Physical/typewriter layout (most common in Ethiopia)

An overlay on QWERTY where each consonant base character maps to a single key, and vowel modification is done by typing the **vowel number** (1–7) after the consonant. No formal ISO standard — this is a *de facto* standard used in operating systems (Windows Amharic IME, Linux, etc.).

### B. SERA (System for Ethiopic Representation in ASCII)

A standardized **transliteration** system, not a typing method. Maps each Ethiopic character to 2–3 ASCII characters:

- **consonant letter** + **vowel letter**
- Examples: `be` = በ (bä), `bu` = ቡ, `bi` = ቢ, `ba` = ባ, `bE` = ቤ (be), `bI` = ብ (bə), `bo` = ቦ
- Capital vowel letters represent the 5th and 6th orders
- Labiovelars: `kWa` = ኳ (kʷa)
- Ejectives: capital consonant (e.g., `Q` = ቀ /q'/, `S` = ጸ /ts'/)

**Widely used in** academic linguistics, websites (e.g., Amharic Wikipedia uses a SERA-based input system), and software libraries (libethio, abyssinica).

### C. EAE (Ethiopian Authoring Engine)

Used in SIL software (Keyman, FieldWorks). Similar concept: type Latin transcription and get Ethiopic output. Vowel mapping: a/ā position differs from SERA.

### D. BGN/PCGN 1967 system

Official US/UK government romanization for Amharic geographic names (geonames.nga.mil). Uses diacritic marks for ejectives (underdot), same as academic convention.

### E. Common transliteration conventions

| Convention | ejective marking | vowel order notation | Used by |
|-----------|-----------------|---------------------|---------|
| Linguist standard | underdot (ṭ, č̣, ḳ) | ä u i a e ə o | Academic papers |
| SERA | capital (T, C, Q) | e u i a E I o | Online input |
| BGN/PCGN | underdot | — | Maps, geographic names |

---

## 4. ISO Standards Related to Ethiopic Script

| Standard | Description |
|----------|-------------|
| **ISO 15924** | Code `Ethi` (430) — registers the Geʻez/Ethiopic script |
| **ISO/IEC 10646** | Universal Coded Character Set — contains all Ethiopic Unicode codepoints |
| **ISO 639** | Language codes: `am`/`amh` (Amharic), `gez` (Geʻez), `ti`/`tir` (Tigrinya) |
| **ISO/IEC 9995-9:2026** | Keyboard layouts — *Groups and mechanisms for multilingual and multiscript input* — provides the framework for implementing multiple script groups on a keyboard (relevant but not Ethiopic-specific) |

**No ISO standard exists for a specific Ethiopic keyboard layout.** The layout is covered by national practices (Ethiopian Standards Agency) and operating system manufacturers' implementations, not by a dedicated ISO/IEC standard. The keyboard layout standardization for Ethiopic remains a *de facto* rather than *de jure* arrangement.
</task_result>
```

---

## Assistant (Build · DeepSeek V4 Pro · 27.9s)

_Thinking:_

Now I have comprehensive research on:

1. **m17n** implementation - the .mim file format, how m17n-lib processes keyboard definitions, the ibus-m17n bridge architecture
2. **IBus engine architecture** - how to subclass IBusEngine in C/C++, the event flow, preedit/commit management
3. **Ethiopic Unicode** - Unicode ranges, syllable structure, transliteration schemes (SERA, EAE, typewriter)

The user wants:
1. A plan for implementing an Ethiopic keyboard based on IBus
2. Following m17n implementation patterns
3. Using C++ programming
4. Test on Fedora Linux first, then expand to Windows and mobile
5. Focus on Amharic first, then expand to other Ethiopic varieties
6. Based on ISO Unicode standard
7. Check Anthy and Zhuyin implementations

Let me also note that the current working directory is empty - this is a fresh project.

Now I should provide a comprehensive plan. Let me think about the best approach:

### Key Design Decisions:

**Option A: Write a native C++ IBus engine (from scratch)**
- Pros: Full control, performance, no m17n dependency
- Cons: Must reimplement all the trie-based mapping engine, harder to extend

**Option B: Write .mim files for m17n + contribute to ibus-m17n**
- Pros: Leverages existing mature engine, minimal code
- Cons: Limited to m17n's feature set, S-expression config, less customizability

**Option C: Write a C++ core library + thin IBus wrapper (best)**
- Pros: 
  - Core library reusable across platforms (IBus on Linux, TSF on Windows, custom on mobile)
  - C++ for better architecture, type safety, OOP design
  - Can support multiple keyboard layouts (SERA, typewriter, phonetic)
  - Easier to test independently
- Cons: More initial work, but pays off for multi-platform

**Option D: Use libzhuyin/ibus-libpinyin pattern as reference**
- They use C++ core + IBus wrapper - excellent reference

I think Option C is the best approach, and I should model it after:
- **ibus-libpinyin** (C++ core library + IBus engine wrapper)
- **ibus-chewing** (separate C core lib + IBus wrapper)
- **ibus-anthy** (separate anthy library + IBus wrapper)

### Draft Plan:

The plan should cover:

1. **Project Structure**
2. **Core Library (libethiopic)** - platform-independent
   - Mapping data structures
   - Syllable computation engine
   - Multiple input modes (SERA, typewriter, phonetic, etc.)
   - Dictionary/word prediction (future)
3. **IBus Engine (ibus-ethiopic)** 
   - IBusEngine subclass
   - Key event processing
   - Preedit management
   - Candidate/lookup table
4. **Build System** (CMake or Meson)
5. **Testing Strategy**
6. **Roadmap** (Amharic first → other languages → Windows → mobile)

Let me also think about whether the user might benefit from just using m17n directly vs building everything from scratch. The user did say "It would be advisable to follow the m17n implementation" - so maybe hybrid: write .mim files for m17n first to get something working quickly, then build the custom engine.

But the user also explicitly says they want C++ and plan to go to Windows/mobile, which m17n doesn't support. So a native C++ solution makes more sense.

Let me structure the plan now.

**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Present comprehensive design plan for Ethiopic keyboard IME",
      "status": "in_progress",
      "priority": "high"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Present comprehensive design plan for Ethiopic keyboard IME",
    "status": "in_progress",
    "priority": "high"
  }
]
```

---

## Assistant (Build · DeepSeek V4 Pro · 123.3s)

_Thinking:_

Let me present a comprehensive plan now.

Here is the comprehensive design plan based on my research of m17n, ibus-anthy, ibus-chewing, ibus-libpinyin, and the Unicode Ethiopic standard.

---

## Ethiopic Keyboard IME — Design Plan

### 1. Architecture Overview: Two-Layer Design

Following the pattern of **ibus-libpinyin** and **ibus-chewing**, we split into:

```
┌─────────────────────────────────────────────┐
│                  IBus Engine                  │  Linux/Fedora
│              (ibus-ethiopic)                 │  GObject C++ glue
│     key_event → forward → commit/preedit     │
└──────────────┬──────────────────────────────┘
               │ calls
┌──────────────▼──────────────────────────────┐
│            Core Library (libethio)           │  Platform-independent C++
│  • Mapping engine (trie-based, like m17n)    │  Reusable on:
│  • Syllable computation from Unicode tables  │    Linux, Windows (TSF),
│  • Multiple input modes                      │    macOS, iOS, Android
│  • Word completion / dictionary (future)     │
└─────────────────────────────────────────────┘
```

**Rationale:** The core library is tested independently and compiles anywhere. The IBus wrapper is thin (~500 lines). Later, swap the IBus wrapper for a Windows TSF wrapper or Android InputMethodService — the core stays identical.

---

### 2. Project Directory Structure

```
ethiopic-keyboard/
├── CMakeLists.txt                    # Top-level build
├── README.md
├── data/                             # Keyboard definition files
│   ├── amharic/
│   │   ├── sera.json                 # SERA transliteration mapping
│   │   ├── typewriter.json           # Physical typewriter layout
│   │   └── phonetic.json             # Alternative phonetic layout
│   ├── tigrinya/
│   │   └── sera.json
│   └── oromo/
│       └── ...
├── libethio/                         # Core library (C++)
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── ethio/
│   │       ├── ethio.h               # Public API header
│   │       ├── mapping.h             # Mapping data structures
│   │       ├── syllabary.h           # Unicode syllabary tables
│   │       └── engine.h              # Input engine (trie processor)
│   └── src/
│       ├── engine.cpp                # Core input processing
│       ├── mapping.cpp               # Mapping loader (JSON → trie)
│       ├── syllabary.cpp             # Unicode-based syllable compute
│       └── transliterator.cpp        # Optional: Latin ↔ Ethiopic
├── ibus-ethiopic/                    # IBus engine wrapper (C++)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp                  # Entry point, bus registration
│   │   ├── engine.h                  # IBusEthiopicEngine class
│   │   ├── engine.cpp                # process_key_event, preedit, commit
│   │   └── keymap.cpp                # Key translation helpers
│   └── data/
│       └── org.freedesktop.IBus.Ethiopic.xml  # D-Bus component desc
├── tests/                            # Unit + integration tests
│   ├── test_syllabary.cpp
│   ├── test_mapping.cpp
│   ├── test_engine.cpp
│   └── test_ibus_engine.cpp
└── docs/
    └── DESIGN.md
```

---

### 3. Core Library Design (`libethio`)

#### 3a. Syllabary Module (`syllabary.cpp`)

Encodes the mathematical structure of Ethiopic:

```cpp
namespace ethio {

// The 26 base consonants in Unicode order (U+1208–U+1296)
enum class Consonant : uint32_t {
    // Traditional Ge'ez order
    H   = 0x1200,  // ሀ
    L   = 0x1208,  // ለ
    HH  = 0x1210,  // ሐ
    M   = 0x1218,  // መ
    SZ  = 0x1220,  // ሠ
    R   = 0x1228,  // ረ
    S   = 0x1230,  // ሰ
    SH  = 0x1238,  // ሸ
    Q   = 0x1240,  // ቀ
    B   = 0x1260,  // በ
    V   = 0x1268,  // ቨ
    T   = 0x1270,  // ተ
    CH  = 0x1278,  // ቸ
    HH2 = 0x1280,  // ኀ
    N   = 0x1290,  // ነ
    NY  = 0x1298,  // ኘ
    A   = 0x12A0,  // አ
    K   = 0x12A8,  // ከ
    KW  = 0x12B0,  // ኰ  (labiovelar series offset +8)
    // ... continue for all consonant families
};

// 7 vowel orders: ä=0, u=1, i=2, a=3, e=4, ə=5, o=6
enum class Vowel : uint8_t { AE = 0, U = 1, I = 2, A = 3, E = 4, Y = 5, O = 6 };

// Compute codepoint: base + vowel offset
constexpr char32_t syllable(Consonant base, Vowel v) {
    return static_cast<char32_t>(base) + static_cast<uint8_t>(v);
}

// Lookup: given Latin key sequence, return Ethiopic codepoint
char32_t lookup(const std::string &keyseq);

} // namespace ethio
```

This reflects the exact m17n `am-sera.mim` logic but in C++ constants.

#### 3b. Mapping Engine (`engine.cpp`)

The core trie-based state machine, modeled directly on m17n-lib's `MIMMap`:

```
       root
      /  |  \
    'h' 'l' 'b' ...     (consonant prefixes)
    /|\
  'e''u''i'...           (vowel suffixes → terminal nodes → commit)
```

Data structures:

```cpp
struct MapNode {
    std::unordered_map<std::string, std::unique_ptr<MapNode>> submaps;
    std::vector<Action> actions;  // on terminal: insert/delete/commit/shift
    std::vector<Action> branch_actions; // on exit/key-not-found
};

struct InputEngine {
    MapNode root;
    std::string buffer;       // current composing buffer (preedit)
    MapNode* current_map;     // pointer into trie
    std::vector<char32_t> produced; // text to commit

    // Called per keypress
    bool filter(const std::string &key);

    // Load mapping from JSON
    void load_mapping(const std::string &json_path);

    // Reset state
    void reset();
};
```

This mirrors m17n's `MInputContext` + `MIMMap` in essence, but in clean C++ with no S-expression parser.

#### 3c. Mapping File Format (JSON)

Instead of m17n's S-expression `.mim` files, we use JSON for readability:

```json
{
  "input_method": "am-sera",
  "title": "Amharic (SERA)",
  "description": "Amharic using SERA transliteration",
  "states": {
    "init": {
      "map": {
        "he": "ሀ", "hu": "ሁ", "hi": "ሂ", "ha": "ሃ", "hE": "ሄ", "h": "ህ", "ho": "ሆ",
        "le": "ለ", "lu": "ሉ", "li": "ሊ", "la": "ላ", "lE": "ሌ", "l": "ል", "lo": "ሎ",
        "be": "በ", "bu": "ቡ", "bi": "ቢ", "ba": "ባ", "bE": "ቤ", "b": "ብ", "bo": "ቦ",
        "...": "..."
      }
    }
  }
}
```

A Python/JS script can auto-generate these from the Unicode tables (since the syllable structure is mathematically regular).

#### 3d. Alternative: Typewriter Layout Mode

The "typewriter" layout (used in Ethiopian keyboards) uses vowel-number suffixes:

```json
{
  "input_method": "am-typewriter",
  "map": {
    "h1": "ሀ", "h2": "ሁ", "h3": "ሂ", "h4": "ሃ", "h5": "ሄ", "h6": "ህ", "h7": "ሆ",
    "l1": "ለ", "l2": "ሉ", "l3": "ሊ", "l4": "ላ", "l5": "ሌ", "l6": "ል", "l7": "ሎ"
  }
}
```

Same engine, different JSON mapping. The core library supports switching modes at runtime (via `shift` action or IBus property toggle).

---

### 4. IBus Engine Wrapper (`ibus-ethiopic`)

Following the **ibus-tmpl** skeleton and **ibus-chewing** patterns:

#### 4a. Engine Class

```cpp
// engine.h
G_DECLARE_FINAL_TYPE(IBusEthiopicEngine,
                     ibus_ethiopic_engine,
                     IBUS, ETHIOPIC_ENGINE,
                     IBusEngine)

struct _IBusEthiopicEngine {
    IBusEngine parent;

    // Core state
    ethio::InputEngine *core_engine;

    // IBus display state
    IBusLookupTable *table;
    std::string preedit_string;
    guint cursor_pos;

    // Settings
    GSettings *settings;
    std::string input_mode;   // "sera" or "typewriter"
};
```

#### 4b. Key Event Processing

```cpp
gboolean
ibus_ethiopic_engine_process_key_event(IBusEngine *engine,
                                        guint keyval,
                                        guint keycode,
                                        guint state)
{
    auto *ethio = IBUS_ETHIOPIC_ENGINE(engine);

    // 1. Skip release events
    if (state & IBUS_RELEASE_MASK) return FALSE;

    // 2. Pass through Super-key combos
    if (state & IBUS_MOD4_MASK) return FALSE;

    // 3. Skip password fields (check set_content_type)
    if (ethio->is_password_field) return FALSE;

    // 4. Handle control keys
    switch (keyval) {
    case IBUS_KEY_BackSpace:
        return handle_backspace(ethio);
    case IBUS_KEY_Return:
        return handle_commit(ethio);
    case IBUS_KEY_Escape:
        return handle_reset(ethio);
    case IBUS_KEY_space:
        return handle_space(ethio);
    }

    // 5. Convert keysym to string key
    std::string key = keysym_to_string(keyval, state);

    // 6. Pass to core engine
    bool handled = ethio->core_engine->filter(key);

    // 7. Update IBus display
    update_preedit(ethio);
    update_lookup_table(ethio);

    return handled;
}
```

#### 4c. Preedit & Commit Flow

```
User types "h" → core_engine->filter("h")
    → buffer = "h", produced = []         → update_preedit("h")
User types "e" → core_engine->filter("e")
    → trie matches "he" → terminal
    → actions: insert "ሀ"
    → produced = ["ሀ"]
    → commit_text("ሀ")
    → update_preedit(""), reset buffer
```

Preedit update:
```cpp
void update_preedit(IBusEthiopicEngine *ethio) {
    if (ethio->preedit_string.empty()) {
        ibus_engine_hide_preedit_text(IBUS_ENGINE(ethio));
        return;
    }
    IBusText *text = ibus_text_new_from_static_string(
        ethio->preedit_string.c_str());
    // Add underline attribute for composing text
    text->attrs = ibus_attr_list_new();
    ibus_attr_list_append(text->attrs,
        ibus_attr_underline_new(IBUS_ATTR_UNDERLINE_SINGLE, 0,
                                ethio->preedit_string.length()));
    ibus_engine_update_preedit_text(IBUS_ENGINE(ethio), text,
                                    ethio->cursor_pos, TRUE);
}
```

#### 4d. Main Entry Point

```cpp
// main.cpp
int main(int argc, char **argv) {
    ibus_init();
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

    IBusFactory *factory = ibus_factory_new(conn);
    ibus_factory_add_engine(factory, "ethiopic",
                            IBUS_TYPE_ETHIOPIC_ENGINE);

    if (g_getenv("IBUS_COMPONENT_EXEC")) {
        ibus_bus_request_name(ibus_bus_new(), "org.freedesktop.IBus.Ethiopic", 0);
    } else {
        IBusComponent *comp = ibus_component_new(
            "org.freedesktop.IBus.Ethiopic",
            "Ethiopic Input Method",
            "1.0.0",
            "GPL-3.0+",
            "Moab",
            "https://github.com/...",
            "", "ibus-engine-ethiopic");
        ibus_component_add_engine(comp,
            ibus_engine_desc_new("ethiopic", "Ethiopic", "Amharic/SERA",
                                 "am", "GPL-3.0+", "Moab",
                                 PKGDATADIR "/icons/ibus-ethiopic.svg",
                                 "us"));
        ibus_bus_register_component(ibus_bus_new(), comp);
    }

    ibus_main();
}
```

---

### 5. Build System (CMake)

```cmake
cmake_minimum_required(VERSION 3.16)
project(ethiopic-keyboard VERSION 0.1.0 LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(IBUS REQUIRED ibus-1.0>=1.5)
pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0)
pkg_check_modules(JSON REQUIRED nlohmann_json)

# Core library (platform-independent)
add_library(ethio STATIC
    libethio/src/engine.cpp
    libethio/src/mapping.cpp
    libethio/src/syllabary.cpp
)
target_include_directories(ethio PUBLIC libethio/include)
target_link_libraries(ethio PUBLIC nlohmann_json::nlohmann_json)

# IBus engine (Linux only)
add_library(ibus-ethiopic-engine SHARED
    ibus-ethiopic/src/engine.cpp
)
target_link_libraries(ibus-ethiopic-engine
    ethio ${IBUS_LIBRARIES} ${GLIB_LIBRARIES})

# Executable
add_executable(ibus-engine-ethiopic
    ibus-ethiopic/src/main.cpp)
target_link_libraries(ibus-engine-ethiopic
    ibus-ethiopic-engine)

# Tests
enable_testing()
add_executable(test_engine tests/test_engine.cpp)
target_link_libraries(test_engine ethio)
add_test(NAME engine COMMAND test_engine)
```

---

### 6. Development Roadmap

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| **Phase 1: Core Library** | 2–3 weeks | `libethio` with SERA mapping for Amharic, trie engine, unit tests |
| **Phase 2: IBus Engine** | 1–2 weeks | `ibus-ethiopic` wrapper, Fedora RPM packaging, D-Bus registration |
| **Phase 3: Typewriter Layout** | 1 week | Physical typewriter layout JSON, mode switching via IBus property |
| **Phase 4: Expand Languages** | 2 weeks | Tigrinya, Oromo, and other Ethiopic varieties |
| **Phase 5: Windows TSF** | 3–4 weeks | TSF TextService wrapper around same `libethio` |
| **Phase 6: Mobile** | 3–4 weeks | Android InputMethodService / iOS custom keyboard extension |
| **Phase 7: Advanced** | Ongoing | Word prediction, user dictionary, fuzzy matching, candidate ranking |

**Phase 1 and 2 are what you can test on Fedora now.**

---

### 7. Alternatives Considered

| Approach | Pros | Cons | Verdict |
|----------|------|------|---------|
| **A. Write `.mim` file for m17n only** | Instant result, zero code | No Windows/mobile support, stuck with S-expression config, limited customizability | **Good for rapid prototype** |
| **B. C++ lib + IBus wrapper (proposed)** | Multi-platform, full control, clean architecture | More initial code | **Best long-term** |
| **C. Keyman keyboard + Keyman runtime** | Cross-platform out of box, GUI editor | Keyman license restrictions, binary format, heavy runtime | Viable but less flexible |

**My recommendation:** Do **Approach A first** (write `am-ethio.mim` and use ibus-m17n) as a **1-day spike** to validate the mapping. Then build the C++ solution (Approach B) for the real product. The m17n `.mim` file serves as the canonical reference for your C++ mapping data.

---

### 8. Next Steps

If you agree with this direction, I suggest we:

1. **First:** Create the `am-sera.json` mapping file (auto-generate from Unicode math) — ~1 hour of work
2. **Then:** Build `libethio` with the trie engine and unit tests — ~2 days
3. **Then:** Wire up the `ibus-ethiopic` wrapper — ~1 day
4. **Then:** Package as RPM and test on your Fedora box

Shall I start with #1 — generating the complete Amharic SERA mapping file?

---

## User

Create or update `AGENTS.md` for this repository.

The goal is a compact instruction file that helps future OpenCode sessions avoid mistakes and ramp up quickly. Every line should answer: "Would an agent likely miss this without help?" If not, leave it out.

User-provided focus or constraints (honor these):


## How to investigate

Read the highest-value sources first:
- `README*`, root manifests, workspace config, lockfiles
- build, test, lint, formatter, typecheck, and codegen config
- CI workflows and pre-commit / task runner config
- existing instruction files (`AGENTS.md`, `CLAUDE.md`, `.cursor/rules/`, `.cursorrules`, `.github/copilot-instructions.md`)
- repo-local OpenCode config such as `opencode.json`

If architecture is still unclear after reading config and docs, inspect a small number of representative code files to find the real entrypoints, package boundaries, and execution flow. Prefer reading the files that explain how the system is wired together over random leaf files.

Prefer executable sources of truth over prose. If docs conflict with config or scripts, trust the executable source and only keep what you can verify.

## What to extract

Look for the highest-signal facts for an agent working in this repo:
- exact developer commands, especially non-obvious ones
- how to run a single test, a single package, or a focused verification step
- required command order when it matters, such as `lint -> typecheck -> test`
- monorepo or multi-package boundaries, ownership of major directories, and the real app/library entrypoints
- framework or toolchain quirks: generated code, migrations, codegen, build artifacts, special env loading, dev servers, infra deploy flow
- repo-specific style or workflow conventions that differ from defaults
- testing quirks: fixtures, integration test prerequisites, snapshot workflows, required services, flaky or expensive suites
- important constraints from existing instruction files worth preserving

Good `AGENTS.md` content is usually hard-earned context that took reading multiple files to infer.

## Questions

Only ask the user questions if the repo cannot answer something important. Use the `question` tool for one short batch at most.

Good questions:
- undocumented team conventions
- branch / PR / release expectations
- missing setup or test prerequisites that are known but not written down

Do not ask about anything the repo already makes clear.

## Writing rules

Include only high-signal, repo-specific guidance such as:
- exact commands and shortcuts the agent would otherwise guess wrong
- architecture notes that are not obvious from filenames
- conventions that differ from language or framework defaults
- setup requirements, environment quirks, and operational gotchas
- references to existing instruction sources that matter

Exclude:
- generic software advice
- long tutorials or exhaustive file trees
- obvious language conventions
- speculative claims or anything you could not verify
- content better stored in another file referenced via `opencode.json` `instructions`

When in doubt, omit.

Prefer short sections and bullets. If the repo is simple, keep the file simple. If the repo is large, summarize the few structural facts that actually change how an agent should work.

If `AGENTS.md` already exists at `/`, improve it in place rather than rewriting blindly. Preserve verified useful guidance, delete fluff or stale claims, and reconcile it with the current codebase.

---

## Assistant (Build · DeepSeek V4 Pro · 8.9s)

_Thinking:_

The user wants me to create an `AGENTS.md` file for the ethiopic-keyboard repository. Let me first check what's in the repository currently and if there are any existing instruction files.

**Tool: glob**

**Input:**
```json
{
  "pattern": "**/*",
  "path": "/home/moab/sources/ethiopic-keyboard"
}
```

**Output:**
```
/home/moab/sources/ethiopic-keyboard/plan_ethiopic-keyboard.md
```

**Tool: glob**

**Input:**
```json
{
  "pattern": "**/README*",
  "path": "/home/moab/sources/ethiopic-keyboard"
}
```

**Output:**
```
No files found
```

**Tool: glob**

**Input:**
```json
{
  "pattern": "**/{AGENTS,CLAUDE,CONTRIBUTING,DEVELOPMENT}*",
  "path": "/home/moab/sources/ethiopic-keyboard"
}
```

**Output:**
```
No files found
```

**Tool: glob**

**Input:**
```json
{
  "pattern": "**/{CMakeLists,Makefile,meson.build}*",
  "path": "/home/moab/sources/ethiopic-keyboard"
}
```

**Output:**
```
No files found
```

---

## Assistant (Build · DeepSeek V4 Pro · 6.0s)

_Thinking:_

The directory only has one file: `plan_ethiopic-keyboard.md`. There's no README, no build system, no existing AGENTS.md, no code yet. This is a pre-implementation stage - just the plan document from the previous conversation.

Let me read the plan file to understand what's been discussed, then create a concise AGENTS.md based on the design decisions already made.

**Tool: read**

**Input:**
```json
{
  "filePath": "/home/moab/sources/ethiopic-keyboard/plan_ethiopic-keyboard.md"
}
```

**Output:**
```
<path>/home/moab/sources/ethiopic-keyboard/plan_ethiopic-keyboard.md</path>
<type>file</type>
<content>
1: # Ethiopic keyboard design plan for IBus
2: 
3: **Session ID:** ses_2306d2b24ffeEDAagm5gJ47Ab7
4: **Created:** 4/27/2026, 11:33:19 AM
5: **Updated:** 4/27/2026, 11:45:00 AM
6: 
7: ---
8: 
9: ## User
10: 
11: Give me a plan to design to implement ethiopic keyboard based on linux ibus engine. It would be advisable to follow the m17n implementation of Anthny and others. Please check their open source codes as well. I want to use c++ programming. Once I tested the implementation in my fedora linux box then I will plan to implement the same for windows even for mobile phones. So give me a draft plan and then let us work on that before we go for implementation. You can also give me any better suggestion of alternatives to implement this keyboard. The keyboard will be focused for amharic language first, then we can expand it for other ethiopic language varities.  It also should be based on ISO unicode standard. Please check the details of Anthy and Zhuyin chinese and other asian language keyboard implementation in IME   
12: 
13: ---
14: 
15: ## Assistant (Build · DeepSeek V4 Pro · 549.6s)
16: 
17: _Thinking:_
18: 
19: The user wants a comprehensive plan for implementing an Ethiopic keyboard based on the Linux IBus engine, following existing implementations like m17n, Anthy, and other Asian language IMEs. Let me first research the codebase they have, then provide a detailed plan.
20: 
21: Let me explore:
22: 1. The current working directory to see what's already there
23: 2. Research m17n implementation
24: 3. Research Anthy/Zhuyin IME implementations
25: 4. Understand IBus engine architecture
26: 
27: This is a research and planning task, so I should use the explore agent and web fetch tools to gather information before creating a plan.
28: 
29: **Tool: read**
30: 
31: **Input:**
32: ```json
33: {
34:   "filePath": "/home/moab/sources/ethiopic-keyboard"
35: }
36: ```
37: 
38: **Output:**
39: ```
40: <path>/home/moab/sources/ethiopic-keyboard</path>
41: <type>directory</type>
42: <entries>
43: 
44: 
45: (0 entries)
46: </entries>
47: ```
48: 
49: **Tool: task**
50: 
51: **Input:**
52: ```json
53: {
54:   "description": "Research m17n Ethiopic implementation",
55:   "subagent_type": "general",
56:   "prompt": "Research how m17n library (m17n-lib) implements Ethiopic keyboard input methods. Specifically:\n\n1. Search the web for \"m17n ethiopic keyboard implementation\" and \"m17n-lib ethiopic input method\"\n2. Look at the m17n-db and m17n-lib source repositories (they're on Savannah or GitHub mirrors):\n   - How does m17n define keyboard layouts? (Look at .mim files - m17n input method files)\n   - What's the structure of an .mim file for Ethiopic scripts (am-ethio.mim or similar)?\n   - How does m17n-lib process these files?\n   - What's the IBus-m17n integration like? (ibus-m17n engine)\n\n3. Search for \"ibus-m17n source code\" to understand how it bridges IBus and m17n\n\nReturn a detailed summary of:\n- The m17n input method file format and structure\n- Key concepts (maps, states, rules, variables)\n- How Ethiopic syllabic mapping is done in m17n\n- The ibus-m17n bridge architecture\n- Repository URLs for the actual source code"
57: }
58: ```
59: 
60: **Output:**
61: ```
62: task_id: ses_2306cf4ccffeewVFP6e1XQUaBP (for resuming to continue this task if needed)
63: 
64: <task_result>
65: Here is the comprehensive summary:
66: 
67: ---
68: 
69: ## m17n Ethiopic Keyboard Input Method Implementation
70: 
71: ### Repository URLs
72: 
73: | Component | URL |
74: |-----------|-----|
75: | **m17n-lib** (core library) | `https://git.savannah.nongnu.org/cgit/m17n/m17n-lib.git` |
76: | **m17n-db** (input method data) | `https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git` |
77: | **m17n-docs** | `https://git.savannah.nongnu.org/cgit/m17n/m17n-docs.git` |
78: | **ibus-m17n** (IBus bridge) | `https://github.com/ibus/ibus-m17n` |
79: | Savannah project page | `https://savannah.nongnu.org/projects/m17n` |
80: | Manual (Japanese, frames) | `https://www.nongnu.org/m17n/manual-ja/index.html` |
81: 
82: ---
83: 
84: ### 1. MIM File Format & Structure
85: 
86: `.mim` files are **S-expression (Lisp-like)** files defining input methods. They live in `/usr/share/m17n/` and are loaded by the m17n library at runtime via `mdatabase_load()` in `database.c`. The MIM filename convention is `<language>-<name>.mim` (e.g., `am-sera.mim`).
87: 
88: **Top-level structure of a `.mim` file:**
89: 
90: ```lisp
91: (input-method <language-symbol> <name-symbol>)    ;; identifies the IM
92: 
93: (title "Display Title")                            ;; optional UI title
94: 
95: (description "Human-readable description...")       ;; optional
96: 
97: (map ...)                                           ;; key translation maps
98: 
99: (state ...)                                         ;; state machine definitions
100: 
101: (variable ...)                                      ;; configurable variables
102: 
103: (command ...)                                       ;; keybindings for commands
104: ```
105: 
106: **Key files:**
107: - **`global.mim`** (`Mt global`) — defines global variables (`candidates-group-size`, `fallback-input-method`), and default command keybindings (Return=commit, Space=convert, Escape=revert, arrow keys for candidate navigation, etc.)
108: - **`am-sera.mim`** (`am sera`) — Amharic input using the SERA (System for Ethiopic Representation in ASCII) transliteration scheme. Contains 757 lines mapping Latin keystrokes to Ethiopic syllables.
109: 
110: ---
111: 
112: ### 2. Key Concepts: Maps, States, Rules, Variables
113: 
114: #### Maps (`input.c:380-395`)
115: The core data structure is `MIMMap`:
116: 
117: ```c
118: struct MIMMap {
119:     MPlist *map_actions;     // Actions executed when this map is reached
120:     MPlist *submaps;         // List of deeper maps keyed by MSymbol (NULL = terminal)
121:     MPlist *branch_actions;  // Actions executed when leaving this map successfully
122: };
123: ```
124: 
125: Maps form a **trie (prefix tree)**. Each map entry is a key sequence mapped to actions. In the `.mim` file:
126: 
127: ```lisp
128: (map
129:   (map                          ;; root map
130:     ("he" ?ሀ)                   ;; keyseq -> insert (shorthand for (insert "ሀ"))
131:     ("hu" ?ሁ)                   ;; ?X means Unicode character X
132:     ("h" ?ህ)
133:     (branch                      ;; named branch with actions
134:       (nil (delete @-) (insert "something"))
135:     )
136:   ))
137: ```
138: 
139: **How keys are translated:**
140: When a map is a terminal (no submaps), reaching it triggers `map_actions`. Each `(KEYSEQ ACTION*)` entry creates a path through submaps. The key sequence can be:
141: - A string: `"he"` — each character becomes a symbol
142: - A plist of symbols: `(C-a B)` — explicit modifier keys
143: 
144: #### States (`input.c:408-418`)
145: ```c
146: struct MIMState {
147:     MSymbol name;       // Name of the state (e.g., "init")
148:     MText *title;       // Optional display title
149:     MIMMap *map;        // Key translation map (root of the map trie)
150: };
151: ```
152: 
153: States define *groups of maps*. A `.mim` file defines a state machine:
154: 
155: ```lisp
156: (state
157:   (init                          ;; state name
158:     (map                          ;; root map for this state
159:       ...                         ;; branches within this state
160:     )
161:   )
162: )
163: ```
164: 
165: The initial state is always named `init`. States can be switched via the `shift` action in map/branch actions.
166: 
167: #### Branches (`input.c:1144-1174`)
168: Branches are named submaps within a state. The `branch` keyword in a map defines actions for when a specific submap is reached or exited:
169: 
170: ```lisp
171: (map
172:   (nil (action1) (action2))      ;; nil = default branch_actions for this map
173:   ((submap-name) (action1) ...)  ;; named branch = map_actions for submaps
174: )
175: ```
176: 
177: - `nil` → sets `branch_actions` (executed on success OR failure to find next key)
178: - `t` → sets `map_actions` (executed when this map is reached)
179: - Other symbols → creates named branches with specific actions
180: 
181: #### Actions
182: Common actions parsed by `take_action_list()` in `input.c:3102`:
183: - **`insert`** — Insert text into preedit buffer (the default when no action name is given)
184: - **`delete`** — Delete characters from preedit
185: - **`commit`** — Force commit of current preedit
186: - **`pushback`** — Push a key back into the input queue
187: - **`undo`** — Undo last action
188: - **`shift`** — Switch to a different state
189: - **`set`** — Set a variable
190: - **`cond`** — Conditional branching based on variables
191: - **`unhandle`** — Mark key as unhandled (fall through to fallback IM)
192: - **Arithmetic**: `add`, `sub`, `mul`, `div`, `=`, `<`, `>`, `<=`, `>=`
193: 
194: #### Variables (`input.c:528-532`)
195: Defined via `(variable (NAME DESCRIPTION DEFAULT-VALUE))`. Global variables from `global.mim`:
196: - `candidates-group-size` (default 10) — max candidates per group
197: - `candidates-charset` — restrict candidates to a charset
198: - `fallback-input-method` (default `"lsymbol, unicode"`) — chained fallback IMs
199: 
200: #### Commands (`input.c:1868-1906`)
201: Defined via `(command (NAME DESCRIPTION KEYSEQ...))`. Key sequences can be strings or plists of symbols. Global commands include: `commit` (Return/Linefeed), `convert` (Space/Henkan), `revert` (Escape), candidate navigation (arrows/numbers), preedit navigation, delete, reload (M-Return).
202: 
203: #### Macros & External Modules
204: - **`(macro (NAME ACTION*))`** — defines reusable action sequences
205: - **`(module NAME SYMBOL*)`** — loads shared libraries (`.so`) for custom C functions (e.g., Anthy for Japanese)
206: - **`(include FILENAME)`** — includes another `.mim` file
207: 
208: ---
209: 
210: ### 3. How Ethiopic Syllabic Mapping Is Done in m17n
211: 
212: The file `am-sera.mim` (Amharic SERA) uses a **single flat map** (no sub-states) inside the `init` state. Each Ethiopic consonant has 7 vowel forms (ä, u, i, a, e, ə, o) plus labialized variants. The mapping is purely phonetic ASCII → Unicode:
213: 
214: ```lisp
215: (map
216:  (map
217:   ;; h-family (ሀ ሁ ሂ ሃ ሄ ህ ሆ)
218:   ("he" ?ሀ)  ("hu" ?ሁ)  ("hi" ?ሂ)  ("ha" ?ሃ)  ("hE" ?ሄ)  ("h" ?ህ)  ("ho" ?ሆ)
219:   ;; hW-family with labialization (ኋ ኈ ኍ ኊ ኌ)
220:   ("hW" ?ኋ)  ("hWa" ?ኋ)  ("hWe" ?ኈ)  ("hWu" ?ኍ)  ("hWi" ?ኊ)  ("hWE" ?ኌ)
221: 
222:   ;; l-family (ለ ሉ ሊ ላ ሌ ል ሎ)
223:   ("le" ?ለ)  ("lu" ?ሉ)  ("li" ?ሊ)  ("la" ?ላ)  ("lE" ?ሌ)  ("l" ?ል)  ("lo" ?ሎ)
224:   ;; ... same for ~35 consonant families
225: ))
226: ```
227: 
228: **Key design decisions in `am-sera.mim`:**
229: - Vowel suffixes: `e`=ä, `u`=u, `i`=i, `a`=a, `E`/`ee`=e, (no vowel)=ə, `o`=o
230: - Labialization: `W` suffix (e.g., `hWe` → ኈ)
231: - Case variations for emphatic consonants: lowercase for plain, uppercase for emphatic (e.g., `se`=ሰ vs `Se`=ሸ, `he`=ሀ vs `He`=ሐ) — though note in SERA, some mappings differ from this pattern
232: - Multiple aliases: e.g., `hhe`/`h2e`/`` `he `` all map to ኀ (providing alternative input conventions)
233: - Special forms: `ea`=ኧ, vowel-only forms (`a`=አ, `u`=ኡ, `i`=ኢ, `e`=እ, `o`=ኦ)
234: - Punctuation: `:`=፡, `::`=።, `,`=፣, etc.
235: - Ethiopic numerals: `` `1 `` through `` `1000000 ``
236: 
237: **There is no `am-ethio.mim` in the current m17n-db**. The available Ethiopic-related files are:
238: - `am-sera.mim` — Amharic SERA transliteration
239: - `gez.lnm` — Ge'ez language name map (for font/language tagging)
240: 
241: Other Amharic input methods exist outside m17n (e.g., GeezIME, Keyman layouts) but are not part of m17n-db.
242: 
243: ---
244: 
245: ### 4. The m17n-lib Input Processing Engine
246: 
247: The heart of the engine is in `m17n-lib/src/input.c` (~7300 lines). Processing flow:
248: 
249: 1. **`minput_open_im(language, name, arg)`** → loads the `.mim` file from the m17n database, parses S-expressions, builds `MInputMethodInfo` with parsed states, maps, commands, variables.
250: 
251: 2. **`minput_create_ic(im, arg)`** → creates `MInputContext` with initialized state (starts at `init` state's root map).
252: 
253: 3. **`minput_filter(ic, key, arg)`** → calls the driver's `filter()` function (`input.c:4175`). The filter:
254:    - Calls `handle_key()` which traverses the map trie
255:    - Looks up `key` in `map->submaps` (with alias resolution via `M_key_alias`)
256:    - If a submap is found, descends into it and executes `map_actions`
257:    - If a terminal map is reached or the key isn't found, executes `branch_actions`
258:    - Handles fallback IMs, IM push/pop, state shifting
259:    - Produces text in `ic->produced`
260: 
261: 4. **`minput_lookup(ic, key, arg, mt)`** → retrieves produced text into `mt`.
262: 
263: **Key alias system** (`input.c:580-700`):
264: - Single characters (0-255) get aliases: char → `M-char` → `A-char` → `C-M-char`
265: - This allows maps to handle modified keys by following alias chains
266: 
267: **State shifting** (`shift_state` in `input.c`):
268: - `(shift STATE-NAME)` action changes the current active state
269: - The `init` state is the default; shifting to `nil` resets to initial state
270: 
271: **Fallback input methods**:
272: - If the current IM can't handle a key, `check_fallback()` iterates through `fallback-input-method` (e.g., `lsymbol, unicode`) to handle special symbol input
273: 
274: **Error handling**:
275: - If a key reaches the root map of the initial state unhandled, it's marked `key_unhandled = 1`
276: - If 100 iterations occur without progress, the engine resets (sanity check)
277: 
278: ---
279: 
280: ### 5. The ibus-m17n Bridge Architecture
281: 
282: **Repository**: `https://github.com/ibus/ibus-m17n` (C, GObject-based)
283: 
284: **Architecture** (`src/engine.c`):
285: 
286: The bridge works by **dynamically creating GObject types for each m17n input method** at runtime:
287: 
288: ```
289: IBus Engine Name:  "m17n:<lang>:<name>"
290: GObject Type Name:  "IBusM17N<Lang><Name>Engine"
291: ```
292: 
293: **Key structures:**
294: 
295: - **`IBusM17NEngine`** (instance): holds `MInputContext *context`, `IBusLookupTable *table`, preedit/property state
296: - **`IBusM17NEngineClass`** (class, shared per IM): holds `MInputMethod *im`, title, icon, language, name, preedit colors, GSettings
297: 
298: **Initialization flow:**
299: 1. `ibus_m17n_engine_class_init()` — loads `.mim` via `minput_open_im()`, reads title/icon via `minput_get_title_icon()`, connects GSettings
300: 2. `ibus_m17n_engine_constructor()` — calls `minput_create_ic()`, registers callbacks for all `Minput_*` events (preedit, status, candidates, surrounding text)
301: 
302: **Key event processing** (`ibus_m17n_engine_process_key_event`):
303: 1. Checks for password/PIN fields → skip processing
304: 2. Optional US layout translation (`use-us-layout` setting)
305: 3. Falls through to parent `IBusEngineSimple` for Compose key sequences
306: 4. Converts IBus key event to m17n key symbol via `ibus_m17n_key_event_to_symbol()`:
307:    - Maps IBus modifiers to m17n prefixes: `C-` (Control), `S-` (Shift), `A-` (Alt/Mod1), `G-` (AltGr/Mod5), `M-` (Meta), `s-` (Super), `H-` (Hyper)
308:    - Uses `ibus_keyval_name()` for non-ASCII keysyms
309: 5. Calls `minput_filter()` then `minput_lookup()` via `ibus_m17n_engine_process_key()`
310: 
311: **Callback system** (`ibus_m17n_engine_callback`):
312: - `Minput_preedit_draw` → updates IBus preedit text with colors/underline
313: - `Minput_status_draw` → updates IBus property (input method indicator)
314: - `Minput_candidates_draw` → populates IBus lookup table from m17n candidate list
315: - `Minput_get_surrounding_text` → fetches surrounding text from IBus for context-sensitive IMs
316: - `Minput_delete_surrounding_text` → deletes surrounding text (used by some IMs)
317: 
318: **Special handling:**
319: - **Compose key**: Inherits from `IBusEngineSimple`, so Compose sequences work alongside m17n
320: - **Ordering fix**: A 0.1s `nanosleep` after commits helps Mutter get event ordering right (issue #72)
321: - **Shift handling**: Only adds `S-` prefix for non-graphical shift results (issue #90)
322: - **Password fields**: Emulates focus-out to discard pending input state
323: 
324: ---
325: 
326: ### Summary of Data Flow
327: 
328: ```
329: User types key
330:   → IBus delivers key event
331:     → ibus-m17n engine converts to MSymbol (e.g., "h", "e", "S-Return")
332:       → minput_filter(ic, key)
333:         → m17n-lib traverses MIMMap trie
334:           → finds ("he" ?ሀ) at terminal map
335:             → executes (insert "ሀ") → stores in ic->produced
336:       → minput_lookup(ic, key) → retrieves produced text
337:     → ibus-m17n commits "ሀ" via IBus
338: ```
339: </task_result>
340: ```
341: 
342: **Tool: task**
343: 
344: **Input:**
345: ```json
346: {
347:   "description": "Research Anthy IBus engine architecture",
348:   "subagent_type": "general",
349:   "prompt": "Research how Anthy (Japanese input method) implements its IBus engine. This is a model for building a custom IBus engine in C/C++.\n\nSearch and gather information about:\n\n1. The Anthy IBus engine (ibus-anthy) source code structure:\n   - How does it implement the IBusEngine interface?\n   - Key files: engine.cc, engine.h, etc.\n   - How does it subclass IBusEngine?\n   - How does it handle key events (process_key_event)?\n   - How does it manage preedit (composing) text?\n   - How does it commit text?\n\n2. The IBus C API for creating engines:\n   - IBusEngine class structure\n   - Key virtual methods to override\n   - Signal handling\n   - Lookup table management\n\n3. Also briefly look at ibus-table or ibus-libzhuyin for comparison\n\n4. The IBus development documentation and API reference\n\nReturn:\n- Anthy's IBus engine architecture overview\n- Key classes and their responsibilities\n- The event flow (key press → process → commit/preedit)\n- Repository URLs\n- Best practices for writing a custom IBus engine in C++"
350: }
351: ```
352: 
353: **Output:**
354: ```
355: task_id: ses_2306cd67dffeJQ7isXvryotRlD (for resuming to continue this task if needed)
356: 
357: <task_result>
358: Here is a comprehensive research summary on how IBus engines are built in C/C++, based on ibus-anthy, ibus-chewing, ibus-tmpl, and the IBus C API.
359: 
360: ---
361: 
362: ## Repository URLs
363: 
364: | Repo | URL | Language |
365: |------|-----|----------|
366: | **IBus core** | `https://github.com/ibus/ibus` | C (GLib/GObject) |
367: | **ibus-tmpl** (template) | `https://github.com/ibus/ibus-tmpl` | C + Python |
368: | **ibus-chewing** (production C) | `https://github.com/chewing/ibus-chewing` | C |
369: | **ibus-anthy** (production Python) | `https://github.com/ibus/ibus-anthy` | Python (SWIG binds C lib) |
370: | **ibus-libpinyin** (C++ production) | `https://github.com/libpinyin/ibus-libpinyin` | C++ |
371: | **IBus 1.5 API docs** | `http://ibus.github.io/docs/ibus-1.5/index.html` | HTML docs |
372: | **Dev wiki** | `https://github.com/ibus/ibus/wiki/Develop` | Guide |
373: 
374: ---
375: 
376: ## IBusEngine C++ Class Hierarchy (GObject-based)
377: 
378: IBus uses GLib/GObject for its C API. The class hierarchy is:
379: 
380: ```
381: GObject
382:   ⮕ IBusObject
383:       ⮕ IBusService
384:           ⮕ IBusEngine          ← you subclass this
385: ```
386: 
387: The `IBusEngine` struct (`ibus/src/ibusengine.h`) defines the engine instance:
388: 
389: ```c
390: struct _IBusEngine {
391:     IBusService parent;
392:     /* public fields: */
393:     gboolean enabled;
394:     gboolean has_focus;
395:     IBusRectangle cursor_area;
396:     guint client_capabilities;
397: };
398: ```
399: 
400: ### Virtual method table (what you override):
401: 
402: ```c
403: struct _IBusEngineClass {
404:     IBusServiceClass parent;
405: 
406:     /* === KEY VIRTUAL METHODS === */
407:     gboolean (*process_key_event)(IBusEngine *engine,
408:                                   guint keyval, guint keycode, guint state);
409:     void     (*focus_in)        (IBusEngine *engine);
410:     void     (*focus_out)       (IBusEngine *engine);
411:     void     (*reset)           (IBusEngine *engine);
412:     void     (*enable)          (IBusEngine *engine);
413:     void     (*disable)         (IBusEngine *engine);
414:     void     (*set_cursor_location)(IBusEngine *engine, gint x, gint y,
415:                                     gint w, gint h);
416:     void     (*set_capabilities)(IBusEngine *engine, guint caps);
417: 
418:     /* Lookup table navigation */
419:     void     (*page_up)         (IBusEngine *engine);
420:     void     (*page_down)       (IBusEngine *engine);
421:     void     (*cursor_up)       (IBusEngine *engine);
422:     void     (*cursor_down)     (IBusEngine *engine);
423: 
424:     /* Property / candidate interaction */
425:     void     (*property_activate)(IBusEngine *engine,
426:                                   const gchar *prop_name, guint prop_state);
427:     void     (*candidate_clicked)(IBusEngine *engine,
428:                                   guint index, guint button, guint state);
429: 
430:     /* Surrounding text, handwriting, content-type */
431:     void     (*set_surrounding_text)(IBusEngine *engine,
432:                                      IBusText *text,
433:                                      guint cursor_index, guint anchor_pos);
434:     void     (*set_content_type)(IBusEngine *engine,
435:                                  guint purpose, guint hints);
436:     /* ... focus_in_id, focus_out_id, process_hand_writing_event ... */
437: };
438: ```
439: 
440: ---
441: 
442: ## How to Subclass IBusEngine in C (ibus-tmpl pattern)
443: 
444: ### 1. Define your engine struct and use `G_DEFINE_TYPE`
445: 
446: From `ibus-tmpl/src/engine.h`:
447: ```c
448: #define IBUS_TYPE_ENCHANT_ENGINE (ibus_enchant_engine_get_type())
449: GType ibus_enchant_engine_get_type(void);
450: ```
451: 
452: From `ibus-tmpl/src/engine.c`:
453: ```c
454: struct _IBusEnchantEngine {
455:     IBusEngine parent;          // MUST be first field
456: 
457:     GString *preedit;           // composing buffer
458:     gint cursor_pos;            // cursor in preedit
459:     IBusLookupTable *table;     // candidate lookup table
460: };
461: 
462: struct _IBusEnchantEngineClass {
463:     IBusEngineClass parent;
464: };
465: 
466: G_DEFINE_TYPE(IBusEnchantEngine, ibus_enchant_engine, IBUS_TYPE_ENGINE)
467: ```
468: 
469: ### 2. Class init: connect virtual methods
470: 
471: ```c
472: static void
473: ibus_enchant_engine_class_init(IBusEnchantEngineClass *klass)
474: {
475:     IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS(klass);
476:     IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);
477: 
478:     // This is the one method you MUST implement
479:     engine_class->process_key_event =
480:         ibus_enchant_engine_process_key_event;
481: 
482:     // Destroy for cleanup
483:     ibus_object_class->destroy =
484:         (IBusObjectDestroyFunc) ibus_enchant_engine_destroy;
485: }
486: ```
487: 
488: ### 3. Instance init: allocate resources
489: 
490: ```c
491: static void
492: ibus_enchant_engine_init(IBusEnchantEngine *enchant)
493: {
494:     enchant->preedit = g_string_new("");
495:     enchant->cursor_pos = 0;
496: 
497:     enchant->table = ibus_lookup_table_new(9, 0, TRUE, TRUE);
498:     g_object_ref_sink(enchant->table);
499: }
500: ```
501: 
502: ---
503: 
504: ## The Event Flow: Key Press → Process → Commit/Preedit
505: 
506: ### ibus-anthy (Python) and ibus-chewing (C) follow identical patterns:
507: 
508: ```
509: [User presses key]
510:       │
511:       ▼
512: process_key_event(keyval, keycode, state)
513:       │
514:       ├── Check modifiers (RELEASE_MASK, CTRL, MOD1, MOD4)
515:       │   └── If modifier-only → maybe pass through
516:       │
517:       ├── Check for password field (InputPurpose.PASSWORD) → return FALSE
518:       │
519:       ├── Look up keybinding table (configurable commands)
520:       │   └── If command found → execute & return TRUE
521:       │
522:       ├── Classify key:
523:       │   ├── Backspace/Delete → edit preedit buffer
524:       │   ├── Space/Return → commit preedit + reset
525:       │   ├── Escape → clear preedit + reset
526:       │   ├── Arrow keys → move cursor / navigate candidates
527:       │   ├── Number keys (with lookup table visible) → select candidate
528:       │   └── Character keys → append to preedit
529:       │
530:       └── Call engine_update() to refresh display
531:           ├── update_preedit_text()   ← ibus_engine_update_preedit_text()
532:           ├── update_auxiliary_text() ← ibus_engine_update_auxiliary_text()
533:           ├── update_lookup_table()   ← ibus_engine_update_lookup_table()
534:           └── commit_text()           ← ibus_engine_commit_text()
535: ```
536: 
537: ### The `process_key_event` signature (C):
538: 
539: ```c
540: static gboolean
541: engine_process_key_event(IBusEngine *engine,
542:                          guint       keyval,    // GDK keysym
543:                          guint       keycode,   // hardware scancode
544:                          guint       state)     // modifier mask
545: ```
546: 
547: **Return value:**
548: - `TRUE` → key consumed by engine (don't pass to application)
549: - `FALSE` → key not handled (pass through to application)
550: 
551: ### Key event processing in ibus-chewing (`ibus-chewing-engine.c`):
552: 
553: ```c
554: gboolean ibus_chewing_engine_process_key_event(IBusEngine *engine,
555:         KSym keySym, guint keycode, KeyModifiers unmaskedMod)
556: {
557:     // 1. Ignore Super-key combos (pass through)
558:     if (unmaskedMod & IBUS_MOD4_MASK) return FALSE;
559: 
560:     // 2. Ignore password fields
561:     if (is_password(self)) return FALSE;
562: 
563:     // 3. Translate keycode → keysym (system layout mapping)
564:     KSym kSym = ibus_chewing_pre_edit_key_code_to_key_sym(
565:         self->icPreEdit, keySym, keycode, unmaskedMod);
566: 
567:     // 4. Delegate to libchewing core
568:     gboolean result = ibus_chewing_pre_edit_process_key(
569:         self->icPreEdit, kSym, unmaskedMod);
570: 
571:     // 5. Update UI: commit → preedit → aux → lookup table
572:     ibus_chewing_engine_update(self);
573: 
574:     return result;
575: }
576: ```
577: 
578: ### ibus-anthy (Python) key processing (`engine.py:1958`):
579: 
580: ```python
581: def __process_key_event_internal2(self, keyval, keycode, state):
582:     # 1. Check password field
583:     if self.__input_purpose == IBus.InputPurpose.PASSWORD:
584:         return False
585: 
586:     # 2. Strip RELEASE_MASK, handle only press events
587:     is_press = (state & IBus.ModifierType.RELEASE_MASK) == 0
588:     if not is_press: return False
589: 
590:     # 3. Look up keybindings
591:     key = self._mk_key(keyval, state)
592:     for cmd in self.__keybind.get(key, []):
593:         if getattr(self, cmd)(keyval, state):
594:             return True
595: 
596:     # 4. Handle Ctrl+Shift+u (ISO 14755) in Japanese mode
597:     if keyval == IBus.KEY_U and state == (SHIFT | CTRL):
598:         return True  # eats the key
599: 
600:     # 5. Process printable characters
601:     if 0x21 <= keyval <= 0x7e:
602:         ret = self.__on_key_common(keyval, state)
603:         # Handle trigger periods (、。）for conversion
604:         return ret
605:     else:
606:         # If preedit not empty, eat non-printable keys
607:         if not self.__preedit_ja_string.is_empty():
608:             return True
609:         return False
610: ```
611: 
612: ---
613: 
614: ## Managing Preedit (Composing) Text
615: 
616: ### C API:
617: 
618: ```c
619: // Update preedit with attributes (underline, cursor)
620: IBusText *text = ibus_text_new_from_static_string(preedit_str);
621: text->attrs = ibus_attr_list_new();
622: ibus_attr_list_append(text->attrs,
623:     ibus_attr_underline_new(IBUS_ATTR_UNDERLINE_SINGLE, 0, len));
624: 
625: // Show preedit text at cursor position
626: ibus_engine_update_preedit_text((IBusEngine *)engine, text,
627:                                 cursor_pos, TRUE);
628: 
629: // Or use mode variant (commit preedit on focus out)
630: ibus_engine_update_preedit_text_with_mode(engine, text,
631:     cursor_pos, TRUE, IBUS_ENGINE_PREEDIT_COMMIT);
632: 
633: // Hide preedit
634: ibus_engine_hide_preedit_text((IBusEngine *)engine);
635: ```
636: 
637: ### Python API (ibus-anthy):
638: 
639: ```python
640: def update_preedit(self, string, attrs, cursor_pos, visible):
641:     text = IBus.Text.new_from_string(string)
642:     for attr in attrs:
643:         text.append_attribute(attr.type, attr.value,
644:                               attr.start, attr.end)
645:     self.update_preedit_text(text, cursor_pos, visible)
646: 
647: # Or with focus-out mode:
648: self.update_preedit_text_with_mode(text, cursor_pos, visible,
649:     IBus.PreeditFocusMode.COMMIT)
650: ```
651: 
652: ---
653: 
654: ## Committing Text
655: 
656: ### C API:
657: 
658: ```c
659: // Commit text to the application
660: IBusText *text = ibus_text_new_from_static_string(string);
661: ibus_engine_commit_text((IBusEngine *)engine, text);
662: // text is unreffed by ibus if floating
663: 
664: // Delete surrounding text
665: ibus_engine_delete_surrounding_text(engine, offset, nchars);
666: ```
667: 
668: ### ibus-chewing commit flow (production pattern):
669: 
670: ```c
671: void refresh_outgoing_text(IBusChewingEngine *self) {
672:     gchar *outgoingStr = ibus_chewing_pre_edit_get_outgoing(self->icPreEdit);
673:     self->outgoingText = g_object_ref_sink(
674:         ibus_text_new_from_string(outgoingStr));
675: }
676: 
677: void commit_text(IBusChewingEngine *self) {
678:     refresh_outgoing_text(self);
679:     if (!ibus_text_is_empty(self->outgoingText))
680:         ibus_engine_commit_text(IBUS_ENGINE(self), self->outgoingText);
681:     ibus_chewing_pre_edit_clear_outgoing(self->icPreEdit);
682: }
683: ```
684: 
685: ---
686: 
687: ## Lookup Table (Candidate) Management
688: 
689: ### C API:
690: 
691: ```c
692: // Create table: page_size, cursor_pos, cursor_visible, round
693: IBusLookupTable *table = ibus_lookup_table_new(9, 0, TRUE, TRUE);
694: 
695: // Set orientation
696: ibus_lookup_table_set_orientation(table, TRUE); // vertical/horizontal
697: 
698: // Add candidates
699: ibus_lookup_table_clear(table);
700: ibus_lookup_table_append_candidate(table,
701:     ibus_text_new_from_string("candidate1"));
702: 
703: // Set select-key labels (1. 2. 3. ...)
704: ibus_lookup_table_set_label(table, i,
705:     ibus_text_new_from_printf("%c.", '1' + i));
706: 
707: // Show/hide
708: ibus_engine_update_lookup_table(engine, table, TRUE);
709: ibus_engine_show_lookup_table(engine);
710: ibus_engine_hide_lookup_table(engine);
711: 
712: // Fast update for large tables (> page_size * 4)
713: ibus_engine_update_lookup_table_fast(engine, table, TRUE);
714: 
715: // Navigation helpers
716: ibus_lookup_table_cursor_up(table);
717: ibus_lookup_table_page_down(table);
718: ```
719: 
720: ### Python API (ibus-anthy):
721: 
722: ```python
723: self.__lookup_table = IBus.LookupTable.new(
724:     page_size=9, cursor_pos=0, cursor_visible=True, round=True)
725: 
726: self.__lookup_table.append_candidate(IBus.Text.new_from_string(candidate))
727: self.update_lookup_table(self.__lookup_table, visible)
728: self.show_lookup_table()
729: ```
730: 
731: ---
732: 
733: ## Forwarding Key Events
734: 
735: ```c
736: // Pass a key through to the application
737: ibus_engine_forward_key_event(engine, keyval, keycode, state);
738: ```
739: 
740: Used when the engine partially handles a key but wants the original key also sent to the app (e.g., Shift+Space for fullwidth/halfwidth toggle).
741: 
742: ---
743: 
744: ## Property List (Language Bar / IME Options)
745: 
746: ### C pattern (ibus-chewing):
747: 
748: ```c
749: // Create properties
750: self->InputMode = ibus_property_new(
751:     "InputMode", PROP_TYPE_NORMAL,
752:     ibus_text_new_from_static_string("中"),  // label
753:     NULL,                                    // icon
754:     ibus_text_new_from_static_string("Toggle"), // tooltip
755:     TRUE, TRUE,                              // sensitive, visible
756:     PROP_STATE_UNCHECKED,
757:     NULL);                                   // sub_props
758: 
759: // Build prop list
760: self->prop_list = ibus_prop_list_new();
761: ibus_prop_list_append(self->prop_list, self->InputMode);
762: 
763: // Register with IBus (called on focus_in)
764: ibus_engine_register_properties(engine, self->prop_list);
765: 
766: // Update a property (reflect state change)
767: ibus_property_set_label(prop, new_label);
768: ibus_engine_update_property(engine, prop);
769: 
770: // Handle property clicks via:
771: // engine_class->property_activate = ibus_chewing_engine_property_activate;
772: ```
773: 
774: ---
775: 
776: ## Main Program Setup (C)
777: 
778: From `ibus-tmpl/src/main.c` and `ibus-chewing/src/main.c`:
779: 
780: ```c
781: int main(int argc, char **argv) {
782:     // 1. Parse options, set locale
783:     g_option_context_parse(context, &argc, &argv, &error);
784: 
785:     // 2. Initialize IBus
786:     ibus_init();
787: 
788:     // 3. Create bus connection
789:     bus = ibus_bus_new();
790:     g_signal_connect(bus, "disconnected", G_CALLBACK(ibus_disconnected_cb), NULL);
791: 
792:     // 4. Create factory and register engine type
793:     factory = ibus_factory_new(ibus_bus_get_connection(bus));
794:     ibus_factory_add_engine(factory, "engine-name", IBUS_TYPE_YOUR_ENGINE);
795: 
796:     // 5. Register component or request name
797:     if (exec_by_ibus) {
798:         ibus_bus_request_name(bus, "org.freedesktop.IBus.YOUR_NAME", 0);
799:     } else {
800:         IBusComponent *comp = ibus_component_new(...);
801:         ibus_component_add_engine(comp, ibus_engine_desc_new(...));
802:         ibus_bus_register_component(bus, comp);
803:     }
804: 
805:     // 6. Enter main loop
806:     ibus_main();
807:     return 0;
808: }
809: ```
810: 
811: ---
812: 
813: ## Summary: Key Responsibilities by Class
814: 
815: | Class / File | Responsibility |
816: |---|---|
817: | `main.c` | Entry point: bus, factory, component registration, GLib main loop |
818: | `engine.h` | GType declaration, struct with `IBusEngine parent` + engine-specific data |
819: | `engine.c` | Core logic: `process_key_event`, preedit/commit/lookup update, focus/reset/enable |
820: | `preedit.c` (optional) | Preedit buffer management (ibus-chewing pattern) |
821: | `lookup-table.c` (optional) | Candidate table construction and update |
822: 
823: ---
824: 
825: ## Best Practices for Writing a Custom IBus Engine in C++
826: 
827: 1. **Start from `ibus-tmpl`** (`https://github.com/ibus/ibus-tmpl`) -- clone it and replace the engine name/type.
828: 
829: 2. **Use `G_DEFINE_TYPE`** or `G_DECLARE_FINAL_TYPE` (modern GLib) to subclass `IBusEngine`:
830:    ```c
831:    G_DECLARE_FINAL_TYPE(MyEngine, my_engine, MY, ENGINE, IBusEngine)
832:    ```
833: 
834: 3. **Keep preedit state in your engine struct**: `GString *preedit`, cursor position, lookup table pointer.
835: 
836: 4. **`process_key_event` must be fast** -- return quickly. Use `GLib.idle_add()` for async work (see ibus-anthy's `__invalidate()`).
837: 
838: 5. **Always check password fields** via `set_content_type` (ignore keys for passwords):
839:    ```c
840:    if (purpose == IBUS_INPUT_PURPOSE_PASSWORD) return FALSE;
841:    ```
842: 
843: 6. **Filter modifiers correctly**:
844:    - Strip `IBUS_RELEASE_MASK` (only handle press, not release)
845:    - Pass through `IBUS_MOD4_MASK` (Super/Win key combos)
846:    - Check `IBUS_CONTROL_MASK | IBUS_MOD1_MASK`
847: 
848: 7. **Use GSettings** for engine configuration (see ibus-chewing's GSettigs bindings).
849: 
850: 8. **Separate core IME logic** from IBus glue -- keep the linguistic engine (conversion, dictionary) in its own library, and have the IBus engine be a thin wrapper (libchewing, libanthy, libpinyin pattern).
851: 
852: 9. **Test with**: `G_MESSAGES_DEBUG=all /usr/libexec/ibus-engine-your-engine --ibus`
853: 
854: 10. **Build with**: autotools (`autogen.sh` + `make`) or meson/CMake (ibus-chewing uses CMake and meson).
855: </task_result>
856: ```
857: 
858: **Tool: task**
859: 
860: **Input:**
861: ```json
862: {
863:   "description": "Research Ethiopic Unicode standard",
864:   "subagent_type": "general",
865:   "prompt": "Research the Unicode standard for Ethiopic script, specifically for Amharic language input.\n\nSearch for:\n1. Ethiopic Unicode block (U+1200–U+137F) structure\n2. Ethiopic Supplement (U+1380–U+139F)\n3. Ethiopic Extended blocks\n4. How Ethiopic syllables are organized - the base character + vowel modification pattern\n5. The standard Amharic keyboard layout (both phonetic/typewriter and SERA/transliteration approaches)\n6. ISO standards related to Ethiopic keyboard layout (if any)\n7. Common transliteration schemes for Amharic (SERA - System for Ethiopic Representation in ASCII, EAE transliteration, etc.)\n8. The Geez script structure: consonants × 7 vowels (ä, u, i, a, e, ə, o) + labialized variants\n\nReturn:\n- Unicode range details for Ethiopic syllables\n- The mathematical structure of Ethiopic syllables (how to compute syllable from consonant + vowel)\n- Common keyboard layouts and transliteration schemes\n- Any ISO standards for Ethiopic keyboard"
866: }
867: ```
868: 
869: **Output:**
870: ```
871: task_id: ses_2306cb93effemKeHnykkNxWmM5 (for resuming to continue this task if needed)
872: 
873: <task_result>
874: Here is the compiled research:
875: 
876: ---
877: 
878: ## 1. Unicode Range Details
879: 
880: | Block | Range | Code Points | Content |
881: |-------|-------|-------------|---------|
882: | **Ethiopic** | `U+1200–U+137F` | 358 assigned (384 total) | Core syllabary: consonants × 7 vowels, labialized variants, punctuation, numerals |
883: | **Ethiopic Supplement** | `U+1380–U+139F` | 26 assigned | Additional syllables for Sebatbeit, Me'en, Bilen, etc. |
884: | **Ethiopic Extended** | `U+2D80–U+2DDF` | 79 assigned | Additional syllables for Gurage, Basketo, Gumuz languages |
885: | **Ethiopic Extended-A** | `U+AB00–U+AB2F` | 32 assigned | Gamo-Gofa-Dawro, Basketo, Gumuz supplements |
886: | **Ethiopic Extended-B** | `U+1E7E0–U+1E7FF` | 28 assigned | Additional Gurage and extended characters |
887: 
888: **ISO 15924 code**: `Ethi` (430) — "Ethiopic (Geʻez)"
889: 
890: ---
891: 
892: ## 2. Mathematical Structure of Ethiopic Syllables
893: 
894: The Geʻez script is an **abugida** (alphasyllabary). Each base consonant is modified in a systematic way to indicate the following vowel.
895: 
896: ### Core formula: 26 consonants × 7 vowels + labiovelar variants + additional consonants for modern languages
897: 
898: **The 7 vowel orders** (traditional numbering 1st–7th):
899: 
900: | Order | Traditional name | Amharic vowel | Unicode offset |
901: |-------|-----------------|---------------|----------------|
902: | 1st | **gəʽəz** (ግዕዝ) | **ä** [ə] | +0 — base form |
903: | 2nd | **kaʽəb** (ካዕብ) | **u** [u] | +1 |
904: | 3rd | **saləs** (ሣልስ) | **i** [i] | +2 |
905: | 4th | **rabʽ** (ራብዕ) | **a** [aː] | +3 |
906: | 5th | **haməs** (ኃምስ) | **e** [e] | +4 |
907: | 6th | **sadəs** (ሳድስ) | **ə** [ɨ] | +5 |
908: | 7th | **sabʽ** (ሳብዕ) | **o** [o] | +6 |
909: 
910: ### Syllable computation rule
911: 
912: Each consonant family occupies a contiguous block in Unicode. The code point is:
913: 
914: ```
915: code_point = BASE_CONSONANT + vowel_offset
916: ```
917: 
918: Where `vowel_offset ∈ {0, 1, 2, 3, 4, 5, 6}`.
919: 
920: **Example** — consonant **b** (በ) family:
921: 
922: ```
923: በ = U+1260  (bä)    BASE + 0
924: ቡ = U+1261  (bu)    BASE + 1
925: ቢ = U+1262  (bi)    BASE + 2
926: ባ = U+1263  (ba)    BASE + 3
927: ቤ = U+1264  (be)    BASE + 4
928: ብ = U+1265  (bə)    BASE + 5
929: ቦ = U+1266  (bo)    BASE + 6
930: ```
931: 
932: ### Labiovelar (8th order) variants
933: 
934: Four velar consonants have labialized forms (`kw`, `gw`, `qw`, `hw`) that use vowel offsets at `+8, +0, +1, +2, +5` (5 forms, not 7):
935: 
936: ```
937: ከ (kä) → ኰ (kʷä) at offset +8, ኲ (kʷi), ኳ (kʷa), ኴ (kʷe), ኵ (kʷə)
938: ```
939: 
940: ### Additional modern-language consonants
941: 
942: Derived by adding a horizontal line to a phonetically-similar consonant: `በ (b)` → `ቨ (v)`, `ተ (t)` → `ቸ (č/t͡ʃ)`, `ሰ (s)` → `ሸ (š/ʃ)`, `ነ (n)` → `ኘ (ñ/ɲ)`, etc.
943: 
944: ---
945: 
946: ## 3. Keyboard Layouts and Transliteration Schemes
947: 
948: ### A. Physical/typewriter layout (most common in Ethiopia)
949: 
950: An overlay on QWERTY where each consonant base character maps to a single key, and vowel modification is done by typing the **vowel number** (1–7) after the consonant. No formal ISO standard — this is a *de facto* standard used in operating systems (Windows Amharic IME, Linux, etc.).
951: 
952: ### B. SERA (System for Ethiopic Representation in ASCII)
953: 
954: A standardized **transliteration** system, not a typing method. Maps each Ethiopic character to 2–3 ASCII characters:
955: 
956: - **consonant letter** + **vowel letter**
957: - Examples: `be` = በ (bä), `bu` = ቡ, `bi` = ቢ, `ba` = ባ, `bE` = ቤ (be), `bI` = ብ (bə), `bo` = ቦ
958: - Capital vowel letters represent the 5th and 6th orders
959: - Labiovelars: `kWa` = ኳ (kʷa)
960: - Ejectives: capital consonant (e.g., `Q` = ቀ /q'/, `S` = ጸ /ts'/)
961: 
962: **Widely used in** academic linguistics, websites (e.g., Amharic Wikipedia uses a SERA-based input system), and software libraries (libethio, abyssinica).
963: 
964: ### C. EAE (Ethiopian Authoring Engine)
965: 
966: Used in SIL software (Keyman, FieldWorks). Similar concept: type Latin transcription and get Ethiopic output. Vowel mapping: a/ā position differs from SERA.
967: 
968: ### D. BGN/PCGN 1967 system
969: 
970: Official US/UK government romanization for Amharic geographic names (geonames.nga.mil). Uses diacritic marks for ejectives (underdot), same as academic convention.
971: 
972: ### E. Common transliteration conventions
973: 
974: | Convention | ejective marking | vowel order notation | Used by |
975: |-----------|-----------------|---------------------|---------|
976: | Linguist standard | underdot (ṭ, č̣, ḳ) | ä u i a e ə o | Academic papers |
977: | SERA | capital (T, C, Q) | e u i a E I o | Online input |
978: | BGN/PCGN | underdot | — | Maps, geographic names |
979: 
980: ---
981: 
982: ## 4. ISO Standards Related to Ethiopic Script
983: 
984: | Standard | Description |
985: |----------|-------------|
986: | **ISO 15924** | Code `Ethi` (430) — registers the Geʻez/Ethiopic script |
987: | **ISO/IEC 10646** | Universal Coded Character Set — contains all Ethiopic Unicode codepoints |
988: | **ISO 639** | Language codes: `am`/`amh` (Amharic), `gez` (Geʻez), `ti`/`tir` (Tigrinya) |
989: | **ISO/IEC 9995-9:2026** | Keyboard layouts — *Groups and mechanisms for multilingual and multiscript input* — provides the framework for implementing multiple script groups on a keyboard (relevant but not Ethiopic-specific) |
990: 
991: **No ISO standard exists for a specific Ethiopic keyboard layout.** The layout is covered by national practices (Ethiopian Standards Agency) and operating system manufacturers' implementations, not by a dedicated ISO/IEC standard. The keyboard layout standardization for Ethiopic remains a *de facto* rather than *de jure* arrangement.
992: </task_result>
993: ```
994: 
995: ---
996: 
997: ## Assistant (Build · DeepSeek V4 Pro · 27.9s)
998: 
999: _Thinking:_
1000: 
1001: Now I have comprehensive research on:
1002: 
1003: 1. **m17n** implementation - the .mim file format, how m17n-lib processes keyboard definitions, the ibus-m17n bridge architecture
1004: 2. **IBus engine architecture** - how to subclass IBusEngine in C/C++, the event flow, preedit/commit management
1005: 3. **Ethiopic Unicode** - Unicode ranges, syllable structure, transliteration schemes (SERA, EAE, typewriter)
1006: 
1007: The user wants:
1008: 1. A plan for implementing an Ethiopic keyboard based on IBus
1009: 2. Following m17n implementation patterns
1010: 3. Using C++ programming
1011: 4. Test on Fedora Linux first, then expand to Windows and mobile
1012: 5. Focus on Amharic first, then expand to other Ethiopic varieties
1013: 6. Based on ISO Unicode standard
1014: 7. Check Anthy and Zhuyin implementations
1015: 
1016: Let me also note that the current working directory is empty - this is a fresh project.
1017: 
1018: Now I should provide a comprehensive plan. Let me think about the best approach:
1019: 
1020: ### Key Design Decisions:
1021: 
1022: **Option A: Write a native C++ IBus engine (from scratch)**
1023: - Pros: Full control, performance, no m17n dependency
1024: - Cons: Must reimplement all the trie-based mapping engine, harder to extend
1025: 
1026: **Option B: Write .mim files for m17n + contribute to ibus-m17n**
1027: - Pros: Leverages existing mature engine, minimal code
1028: - Cons: Limited to m17n's feature set, S-expression config, less customizability
1029: 
1030: **Option C: Write a C++ core library + thin IBus wrapper (best)**
1031: - Pros: 
1032:   - Core library reusable across platforms (IBus on Linux, TSF on Windows, custom on mobile)
1033:   - C++ for better architecture, type safety, OOP design
1034:   - Can support multiple keyboard layouts (SERA, typewriter, phonetic)
1035:   - Easier to test independently
1036: - Cons: More initial work, but pays off for multi-platform
1037: 
1038: **Option D: Use libzhuyin/ibus-libpinyin pattern as reference**
1039: - They use C++ core + IBus wrapper - excellent reference
1040: 
1041: I think Option C is the best approach, and I should model it after:
1042: - **ibus-libpinyin** (C++ core library + IBus engine wrapper)
1043: - **ibus-chewing** (separate C core lib + IBus wrapper)
1044: - **ibus-anthy** (separate anthy library + IBus wrapper)
1045: 
1046: ### Draft Plan:
1047: 
1048: The plan should cover:
1049: 
1050: 1. **Project Structure**
1051: 2. **Core Library (libethiopic)** - platform-independent
1052:    - Mapping data structures
1053:    - Syllable computation engine
1054:    - Multiple input modes (SERA, typewriter, phonetic, etc.)
1055:    - Dictionary/word prediction (future)
1056: 3. **IBus Engine (ibus-ethiopic)** 
1057:    - IBusEngine subclass
1058:    - Key event processing
1059:    - Preedit management
1060:    - Candidate/lookup table
1061: 4. **Build System** (CMake or Meson)
1062: 5. **Testing Strategy**
1063: 6. **Roadmap** (Amharic first → other languages → Windows → mobile)
1064: 
1065: Let me also think about whether the user might benefit from just using m17n directly vs building everything from scratch. The user did say "It would be advisable to follow the m17n implementation" - so maybe hybrid: write .mim files for m17n first to get something working quickly, then build the custom engine.
1066: 
1067: But the user also explicitly says they want C++ and plan to go to Windows/mobile, which m17n doesn't support. So a native C++ solution makes more sense.
1068: 
1069: Let me structure the plan now.
1070: 
1071: **Tool: todowrite**
1072: 
1073: **Input:**
1074: ```json
1075: {
1076:   "todos": [
1077:     {
1078:       "content": "Present comprehensive design plan for Ethiopic keyboard IME",
1079:       "status": "in_progress",
1080:       "priority": "high"
1081:     }
1082:   ]
1083: }
1084: ```
1085: 
1086: **Output:**
1087: ```
1088: [
1089:   {
1090:     "content": "Present comprehensive design plan for Ethiopic keyboard IME",
1091:     "status": "in_progress",
1092:     "priority": "high"
1093:   }
1094: ]
1095: ```
1096: 
1097: ---
1098: 
1099: ## Assistant (Build · DeepSeek V4 Pro · 123.3s)
1100: 
1101: _Thinking:_
1102: 
1103: Let me present a comprehensive plan now.
1104: 
1105: Here is the comprehensive design plan based on my research of m17n, ibus-anthy, ibus-chewing, ibus-libpinyin, and the Unicode Ethiopic standard.
1106: 
1107: ---
1108: 
1109: ## Ethiopic Keyboard IME — Design Plan
1110: 
1111: ### 1. Architecture Overview: Two-Layer Design
1112: 
1113: Following the pattern of **ibus-libpinyin** and **ibus-chewing**, we split into:
1114: 
1115: ```
1116: ┌─────────────────────────────────────────────┐
1117: │                  IBus Engine                  │  Linux/Fedora
1118: │              (ibus-ethiopic)                 │  GObject C++ glue
1119: │     key_event → forward → commit/preedit     │
1120: └──────────────┬──────────────────────────────┘
1121:                │ calls
1122: ┌──────────────▼──────────────────────────────┐
1123: │            Core Library (libethio)           │  Platform-independent C++
1124: │  • Mapping engine (trie-based, like m17n)    │  Reusable on:
1125: │  • Syllable computation from Unicode tables  │    Linux, Windows (TSF),
1126: │  • Multiple input modes                      │    macOS, iOS, Android
1127: │  • Word completion / dictionary (future)     │
1128: └─────────────────────────────────────────────┘
1129: ```
1130: 
1131: **Rationale:** The core library is tested independently and compiles anywhere. The IBus wrapper is thin (~500 lines). Later, swap the IBus wrapper for a Windows TSF wrapper or Android InputMethodService — the core stays identical.
1132: 
1133: ---
1134: 
1135: ### 2. Project Directory Structure
1136: 
1137: ```
1138: ethiopic-keyboard/
1139: ├── CMakeLists.txt                    # Top-level build
1140: ├── README.md
1141: ├── data/                             # Keyboard definition files
1142: │   ├── amharic/
1143: │   │   ├── sera.json                 # SERA transliteration mapping
1144: │   │   ├── typewriter.json           # Physical typewriter layout
1145: │   │   └── phonetic.json             # Alternative phonetic layout
1146: │   ├── tigrinya/
1147: │   │   └── sera.json
1148: │   └── oromo/
1149: │       └── ...
1150: ├── libethio/                         # Core library (C++)
1151: │   ├── CMakeLists.txt
1152: │   ├── include/
1153: │   │   └── ethio/
1154: │   │       ├── ethio.h               # Public API header
1155: │   │       ├── mapping.h             # Mapping data structures
1156: │   │       ├── syllabary.h           # Unicode syllabary tables
1157: │   │       └── engine.h              # Input engine (trie processor)
1158: │   └── src/
1159: │       ├── engine.cpp                # Core input processing
1160: │       ├── mapping.cpp               # Mapping loader (JSON → trie)
1161: │       ├── syllabary.cpp             # Unicode-based syllable compute
1162: │       └── transliterator.cpp        # Optional: Latin ↔ Ethiopic
1163: ├── ibus-ethiopic/                    # IBus engine wrapper (C++)
1164: │   ├── CMakeLists.txt
1165: │   ├── src/
1166: │   │   ├── main.cpp                  # Entry point, bus registration
1167: │   │   ├── engine.h                  # IBusEthiopicEngine class
1168: │   │   ├── engine.cpp                # process_key_event, preedit, commit
1169: │   │   └── keymap.cpp                # Key translation helpers
1170: │   └── data/
1171: │       └── org.freedesktop.IBus.Ethiopic.xml  # D-Bus component desc
1172: ├── tests/                            # Unit + integration tests
1173: │   ├── test_syllabary.cpp
1174: │   ├── test_mapping.cpp
1175: │   ├── test_engine.cpp
1176: │   └── test_ibus_engine.cpp
1177: └── docs/
1178:     └── DESIGN.md
1179: ```
1180: 
1181: ---
1182: 
1183: ### 3. Core Library Design (`libethio`)
1184: 
1185: #### 3a. Syllabary Module (`syllabary.cpp`)
1186: 
1187: Encodes the mathematical structure of Ethiopic:
1188: 
1189: ```cpp
1190: namespace ethio {
1191: 
1192: // The 26 base consonants in Unicode order (U+1208–U+1296)
1193: enum class Consonant : uint32_t {
1194:     // Traditional Ge'ez order
1195:     H   = 0x1200,  // ሀ
1196:     L   = 0x1208,  // ለ
1197:     HH  = 0x1210,  // ሐ
1198:     M   = 0x1218,  // መ
1199:     SZ  = 0x1220,  // ሠ
1200:     R   = 0x1228,  // ረ
1201:     S   = 0x1230,  // ሰ
1202:     SH  = 0x1238,  // ሸ
1203:     Q   = 0x1240,  // ቀ
1204:     B   = 0x1260,  // በ
1205:     V   = 0x1268,  // ቨ
1206:     T   = 0x1270,  // ተ
1207:     CH  = 0x1278,  // ቸ
1208:     HH2 = 0x1280,  // ኀ
1209:     N   = 0x1290,  // ነ
1210:     NY  = 0x1298,  // ኘ
1211:     A   = 0x12A0,  // አ
1212:     K   = 0x12A8,  // ከ
1213:     KW  = 0x12B0,  // ኰ  (labiovelar series offset +8)
1214:     // ... continue for all consonant families
1215: };
1216: 
1217: // 7 vowel orders: ä=0, u=1, i=2, a=3, e=4, ə=5, o=6
1218: enum class Vowel : uint8_t { AE = 0, U = 1, I = 2, A = 3, E = 4, Y = 5, O = 6 };
1219: 
1220: // Compute codepoint: base + vowel offset
1221: constexpr char32_t syllable(Consonant base, Vowel v) {
1222:     return static_cast<char32_t>(base) + static_cast<uint8_t>(v);
1223: }
1224: 
1225: // Lookup: given Latin key sequence, return Ethiopic codepoint
1226: char32_t lookup(const std::string &keyseq);
1227: 
1228: } // namespace ethio
1229: ```
1230: 
1231: This reflects the exact m17n `am-sera.mim` logic but in C++ constants.
1232: 
1233: #### 3b. Mapping Engine (`engine.cpp`)
1234: 
1235: The core trie-based state machine, modeled directly on m17n-lib's `MIMMap`:
1236: 
1237: ```
1238:        root
1239:       /  |  \
1240:     'h' 'l' 'b' ...     (consonant prefixes)
1241:     /|\
1242:   'e''u''i'...           (vowel suffixes → terminal nodes → commit)
1243: ```
1244: 
1245: Data structures:
1246: 
1247: ```cpp
1248: struct MapNode {
1249:     std::unordered_map<std::string, std::unique_ptr<MapNode>> submaps;
1250:     std::vector<Action> actions;  // on terminal: insert/delete/commit/shift
1251:     std::vector<Action> branch_actions; // on exit/key-not-found
1252: };
1253: 
1254: struct InputEngine {
1255:     MapNode root;
1256:     std::string buffer;       // current composing buffer (preedit)
1257:     MapNode* current_map;     // pointer into trie
1258:     std::vector<char32_t> produced; // text to commit
1259: 
1260:     // Called per keypress
1261:     bool filter(const std::string &key);
1262: 
1263:     // Load mapping from JSON
1264:     void load_mapping(const std::string &json_path);
1265: 
1266:     // Reset state
1267:     void reset();
1268: };
1269: ```
1270: 
1271: This mirrors m17n's `MInputContext` + `MIMMap` in essence, but in clean C++ with no S-expression parser.
1272: 
1273: #### 3c. Mapping File Format (JSON)
1274: 
1275: Instead of m17n's S-expression `.mim` files, we use JSON for readability:
1276: 
1277: ```json
1278: {
1279:   "input_method": "am-sera",
1280:   "title": "Amharic (SERA)",
1281:   "description": "Amharic using SERA transliteration",
1282:   "states": {
1283:     "init": {
1284:       "map": {
1285:         "he": "ሀ", "hu": "ሁ", "hi": "ሂ", "ha": "ሃ", "hE": "ሄ", "h": "ህ", "ho": "ሆ",
1286:         "le": "ለ", "lu": "ሉ", "li": "ሊ", "la": "ላ", "lE": "ሌ", "l": "ል", "lo": "ሎ",
1287:         "be": "በ", "bu": "ቡ", "bi": "ቢ", "ba": "ባ", "bE": "ቤ", "b": "ብ", "bo": "ቦ",
1288:         "...": "..."
1289:       }
1290:     }
1291:   }
1292: }
1293: ```
1294: 
1295: A Python/JS script can auto-generate these from the Unicode tables (since the syllable structure is mathematically regular).
1296: 
1297: #### 3d. Alternative: Typewriter Layout Mode
1298: 
1299: The "typewriter" layout (used in Ethiopian keyboards) uses vowel-number suffixes:
1300: 
1301: ```json
1302: {
1303:   "input_method": "am-typewriter",
1304:   "map": {
1305:     "h1": "ሀ", "h2": "ሁ", "h3": "ሂ", "h4": "ሃ", "h5": "ሄ", "h6": "ህ", "h7": "ሆ",
1306:     "l1": "ለ", "l2": "ሉ", "l3": "ሊ", "l4": "ላ", "l5": "ሌ", "l6": "ል", "l7": "ሎ"
1307:   }
1308: }
1309: ```
1310: 
1311: Same engine, different JSON mapping. The core library supports switching modes at runtime (via `shift` action or IBus property toggle).
1312: 
1313: ---
1314: 
1315: ### 4. IBus Engine Wrapper (`ibus-ethiopic`)
1316: 
1317: Following the **ibus-tmpl** skeleton and **ibus-chewing** patterns:
1318: 
1319: #### 4a. Engine Class
1320: 
1321: ```cpp
1322: // engine.h
1323: G_DECLARE_FINAL_TYPE(IBusEthiopicEngine,
1324:                      ibus_ethiopic_engine,
1325:                      IBUS, ETHIOPIC_ENGINE,
1326:                      IBusEngine)
1327: 
1328: struct _IBusEthiopicEngine {
1329:     IBusEngine parent;
1330: 
1331:     // Core state
1332:     ethio::InputEngine *core_engine;
1333: 
1334:     // IBus display state
1335:     IBusLookupTable *table;
1336:     std::string preedit_string;
1337:     guint cursor_pos;
1338: 

(Output capped at 50 KB. Showing lines 1-1338. Use offset=1339 to continue.)
</content>
```

---

## Assistant (Build · DeepSeek V4 Pro · 37.3s)

_Thinking:_

This is a very large plan document (essentially a chat log/transcript). The repo is empty except for this plan file. There's no code, no build system, no tests yet. This is pre-implementation phase.

Let me now create the AGENTS.md file. It should be concise and high-signal, covering:
1. What this project is (Ethiopic keyboard IME)
2. The two-layer architecture (libethio + ibus-ethiopic)
3. The design plan reference
4. Build commands (once implemented)
5. Test commands
6. Key reference implementations to study
7. Development conventions (C++17+, STL, nlohmann_json, etc.)

Since there's no code yet, the file will be somewhat forward-looking but should capture the decisions already made.

**Tool: write**

**Input:**
```json
{
  "filePath": "/home/moab/sources/ethiopic-keyboard/AGENTS.md",
  "content": "# AGENTS.md — Ethiopic Keyboard IME\n\n## Project summary\n\nTwo-layer Ethiopic input method: a platform-independent C++ core library (`libethio`) and an IBus engine wrapper (`ibus-ethiopic`). Amharic first, then other Ethiopic-script languages. Windows TSF and mobile wrappers follow later.\n\nFull design rationale, directory layout, and roadmap are in `plan_ethiopic-keyboard.md`.\n\n## Architecture\n\n```\nibus-ethiopic (GObject C++ glue, ~500 lines)\n  → libethio   (platform-independent C++ core: trie engine, syllabary tables)\n```\n\nModeled on ibus-libpinyin / ibus-chewing (separate core lib + thin IBus wrapper).\nCore input engine mirrors m17n-lib's `MIMMap` trie structure, but in clean C++ with JSON mapping files instead of S-expressions.\n\n## Reference implementations (study before contributing)\n\n| Project | Clone | Notes |\n|---------|-------|-------|\n| ibus-tmpl | `https://github.com/ibus/ibus-tmpl` | Canonical IBus engine skeleton |\n| ibus-chewing | `https://github.com/chewing/ibus-chewing` | Production C engine with separate core lib |\n| ibus-libpinyin | `https://github.com/libpinyin/ibus-libpinyin` | C++ core lib + IBus wrapper |\n| m17n (am-sera.mim) | `https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git` — `MIM/am-sera.mim` | Canonical Amharic SERA mapping (757 lines) |\n| ibus-m17n bridge | `https://github.com/ibus/ibus-m17n` | IBus ↔ m17n glue |\n\n## Key technical facts\n\n- **Ethiopic syllable formula:** `codepoint = BASE_CONSONANT + vowel_offset` where offset ∈ {0..6} for 7 vowels (ä,u,i,a,e,ə,o), with labiovelar offsets {0,1,2,5,8} for kw/gw/qw/hw families.\n- **Input modes supported:** SERA transliteration (default), typewriter (vowel-number suffixes), phonetic.\n- **Mapping files:** JSON under `data/<language>/<mode>.json` — loaded at runtime by `libethio`. Scriptable auto-generation from Unicode tables.\n- **IBus key event contract:** `process_key_event` receives GDK keysym (e.g., `IBUS_KEY_a`), hardware keycode, modifier mask. Return `TRUE` if consumed, `FALSE` to pass through.\n- **Always skip:** release events (`IBUS_RELEASE_MASK`), Super-key combos (`IBUS_MOD4_MASK`), password fields (`IBUS_INPUT_PURPOSE_PASSWORD`).\n- **IBus engine debug:** `G_MESSAGES_DEBUG=all /usr/libexec/ibus-engine-ethiopic --ibus`\n\n## Implementation order (phases)\n\n1. Generate `data/amharic/sera.json` from Unicode syllable math + m17n `am-sera.mim` reference\n2. Build `libethio` (trie engine + JSON loader + unit tests)\n3. Wire `ibus-ethiopic` wrapper (GObject subclass, key event processing, preedit/commit)\n4. Package as an RPM and test on Fedora\n5. Add typewriter layout, then Tigrinya/Oromo, then Windows/mobile wrappers\n\n## Style / conventions\n\n- **C++ standard:** C++17\n- **Build system:** CMake (3.16+)\n- **JSON library:** nlohmann/json (pkg-config: `nlohmann_json`)\n- **IBus version:** ibus-1.0 >= 1.5\n- **GObject macro:** `G_DECLARE_FINAL_TYPE` (modern GLib style)\n- **Testing:** Catch2 or Google Test — core lib tests are platform-independent, IBus integration tests need a D-Bus session\n- **No comments in code** unless the logic is genuinely non-obvious from the names\n"
}
```

**Output:**
```
Wrote file successfully.
```

---

## Assistant (Build · DeepSeek V4 Pro · 8.4s)

_Thinking:_

The AGENTS.md is written. Let me verify it looks good and then mark the todo as complete.

**Tool: todowrite**

**Input:**
```json
{
  "todos": [
    {
      "content": "Present comprehensive design plan for Ethiopic keyboard IME",
      "priority": "high",
      "status": "completed"
    }
  ]
}
```

**Output:**
```
[
  {
    "content": "Present comprehensive design plan for Ethiopic keyboard IME",
    "status": "completed",
    "priority": "high"
  }
]
```

---

## Assistant (Build · DeepSeek V4 Pro · 7.8s)

_Thinking:_

Done. Let me give a brief summary.

`AGENTS.md` created at the repo root. It covers:

- **Architecture:** libethio (C++ core) + ibus-ethiopic (thin IBus wrapper), modeled on ibus-libpinyin/ibus-chewing
- **Reference implementations:** ibus-tmpl, ibus-chewing, ibus-libpinyin, m17n's am-sera.mim, ibus-m17n bridge — all with clone URLs
- **Key technical facts:** Ethiopic syllable formula, JSON mapping file format, IBus key event contract, what to skip (release events, Super-key, passwords)
- **Style/conventions:** C++17, CMake 3.16+, nlohmann/json, `G_DECLARE_FINAL_TYPE`, no comments unless needed
- **Implementation phases:** sera.json → libethio → ibus wrapper → RPM/test on Fedora → typewriter → other languages → Windows/mobile

---

