# Windows TSF Ethiopic Keyboard — Plan

## Architecture

```
ethiopic-tsf (COM DLL, ~1200 lines C++)
  → libethio   (unchanged platform-independent C++ core)
```

Same `libethio` static library. No IBus, no GLib, no D-Bus. The wrapper is a COM DLL that plugs into the Windows Text Services Framework.

## TSF Concepts (minimum to implement)

| Interface | Role |
|-----------|------|
| `ITfTextInputProcessorEx` | Entry point — activated/deactivated when user switches to the IME |
| `ITfKeyEventSink` | Receives keystrokes (`OnKeyDown`, `OnKeyUp`) |
| `ITfCompositionSink` | Tracks composition lifecycle |
| `ITfThreadMgrEventSink` | Session activation / deactivation notifications |
| `ITfDisplayAttributeProvider` | Preedit underline / highlight styling |
| `ITfCandidateListUIElement` | Candidate/suggestion popup (optional, for word completion) |
| `ITfFnSearchCandidateProvider` / `ITfIntegratableCandidateListUIElement` | Optional: richer candidate UI |

The minimum viable product needs only `ITfTextInputProcessorEx` + `ITfKeyEventSink` + `ITfCompositionSink`.

## Data Flow

```
User presses key (e.g., 'h')
  → TSF delivers OnKeyDown
    → ethiopic-tsf converts to char
      → libethio::Engine::filter("h")
        → returns handled=true, composing="ህ"
    → TSF sets preedit (composition) via ITfComposition
    → TSF renders underline
User presses 'e'
  → filter("e") → composing="ሀ", produced="ሀ" (leaf node auto-commit)
    → TSF ends composition, inserts "ሀ" via ITfInsertAtSelection
    → TSF clears preedit
```

## Project Structure

```
ethiopic-keyboard/
├── libethio/                  # unchanged
├── ibus-ethiopic/             # unchanged (Linux)
├── data/amharic/              # unchanged
├── windows/
│   └── ethiopic-tsf/
│       ├── CMakeLists.txt     # MSVC/MinGW CMake toolchain
│       ├── dllmain.cpp        # DllRegisterServer, DllUnregisterServer
│       ├── engine.cpp         # TSF COM class implementing all interfaces
│       ├── engine.h           # Class declaration
│       ├── preedit.cpp        # Composition/preeedit display
│       ├── candidate.cpp      # Candidate list UI (word suggestions)
│       ├── registry.cpp       # Register language profile + CLSID
│       ├── ethiopic-tsf.def   # DLL exports
│       └── ethiopic-tsf.rc    # Version info resource
├── tests/
│   └── test_tsf_engine.cpp    # Windows-specific tests (mock TSF)
└── packaging/
    └── windows/
        └── ethiopic-tsf.wxs   # WiX installer source
```

## Key Implementation Details

### Registration (DllRegisterServer)

The DLL must register:
1. **CLSID** — COM class GUID under `HKCR\CLSID\{...}`
2. **Language profile** — `ITfInputProcessorProfileMgr::RegisterProfile` associating:
   - `CLSID` of the text service
   - Language: `am` (Amharic), `ti` (Tigrinya), etc.
   - `GUID_TFCAT_TIP_KEYBOARD` category
   - Display name: "Ethiopic (SERA)"
   - Icon

### Keystroke Processing (OnKeyDown)

```cpp
STDMETHODIMP OnKeyDown(ITfContext *ctx, WPARAM wParam, LPARAM lParam, BOOL *eaten) {
    // 1. Skip release events, Super key, Ctrl+C/V/X, password fields
    // 2. Handle control keys (Esc=reset, Backspace=reset, Space=commit)
    // 3. Convert virtual key → Unicode char
    // 4. m_core.filter(utf8_char)
    // 5. Update preedit or commit based on result
    *eaten = handled;
    return S_OK;
}
```

Key differences from IBus:
- No `ibus_keyval_to_unicode` — use `ToUnicodeEx()` with the active keyboard layout
- No D-Bus — all text insertion goes through `ITfRange` + `ITfInsertAtSelection`
- Modifier handling: check `GetKeyState(VK_CONTROL)` / `GetAsyncKeyState()`
- Password detection: Check `ITfContext::GetProperty(GUID_PROP_INPUTSCOPE)` for `IS_PASSWORD`

### Preedit / Composition

TSF compositions are per-context objects created via `ITfContextComposition`. The IME:
1. Calls `ITfContextComposition::StartComposition()` to begin
2. Sets text and display attributes via `ITfRange::SetText()` + `ITfProperty`
3. Calls `ITfComposition::EndComposition()` to commit and clear

Display attributes (underline style) are registered in the TSF category manager under `GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER`.

### Commit Text

```cpp
void Commit(ITfContext *ctx, const std::string &text) {
    // 1. End any active composition
    // 2. Get insertion point via ITfContext::GetSelection
    // 3. Create range at insertion point
    // 4. range->SetText(text)
    // 5. Advance insertion point past committed text
}
```

### Candidates / Word Suggestions

If implementing the word suggestion feature from `libethio`:
- Build `ITfCandidateListUIElement` populated from `wordlist.suggest()`
- Show/hide via `ITfUIElementMgr::BeginUIElement()` / `EndUIElement()`
- Arrow keys and number keys select candidates
- On selection: delete current preedit, insert candidate + space

For a simpler first version, skip the candidate UI and just commit word suggestions inline.

### Ctrl+Shift Toggle

Monitor `VK_SHIFT` + `VK_CONTROL` in `OnKeyDown`. Call `ITfInputProcessorProfileMgr::DeactivateProfile()` to disable or `ActivateProfile()` a different language profile. Alternatively, use an internal passthrough flag (like the IBus engine does with `m_core.toggle_passthrough()`).

## Build System

Use CMake with the MSVC toolchain. The DLL links statically against `libethio`:

```cmake
add_library(ethiopic-tsf SHARED
    dllmain.cpp
    engine.cpp
    preedit.cpp
    candidate.cpp
    registry.cpp
    ethiopic-tsf.def
)
target_link_libraries(ethiopic-tsf PRIVATE ethio)
```

Dependencies: `ole32`, `oleaut32`, `advapi32` (all system libs). No external packages needed beyond a C++17 compiler.

## Testing

Windows TSF testing is harder than IBus (no test-mode shortcut). Options:

1. **Unit test `libethio`** — the existing C++ test suite already covers core logic. Run it on Windows first.
2. **Mock TSF interfaces** — write tests that construct mock `ITfContext` and feed key events, asserting `ITfRange` receives correct text. Use Google Test or Catch2 on Windows.
3. **Integration test** — build a small Win32 app with an edit control, activate the IME, inject keystrokes via `SendInput()`, and read the edit control's text.
4. **Manual test in Notepad** — after registration, switch to the IME in the language bar and type.

## Roadmap

| Step | Duration | Deliverable |
|------|----------|-------------|
| 1. Build `libethio` on Windows | 1 day | CMake MSVC build passes, existing C++ tests pass |
| 2. COM DLL skeleton | 2–3 days | `DllRegisterServer`, CLSID, language profile registration working in TSF |
| 3. Keystroke → text insertion | 3–4 days | `OnKeyDown` → `filter()` → commit + preedit working in Notepad |
| 4. Preeedit display | 1–2 days | Underlined composition text while typing |
| 5. Control keys & toggle | 1 day | Escape, Backspace, Space, Ctrl+Shift toggle |
| 6. Candidate list | 2–3 days | Word suggestion popup with arrow/number selection |
| 7. Installer | 1 day | WiX MSI package |
| 8. Testing & polish | 2–3 days | Mock tests + manual testing |

**Total: ~2–3 weeks for a working IME.**

## References

| Resource | URL |
|----------|-----|
| TSF overview | `https://docs.microsoft.com/en-us/windows/win32/tsf/text-services-framework` |
| TSF sample (Microsoft) | `https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/TSF/TextService` |
| Google Japanese Input TSF | `https://github.com/google/mozc` — `src/win32/tip/` |
| OpenVanilla TSF | `https://github.com/openvanilla/openvanilla` — Windows TSF module |
| TSF Aware blog | `https://blogs.msdn.microsoft.com/tsfaware/` |
