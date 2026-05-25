# AGENTS.md — Ethiopic Keyboard IME

## Project summary

Multi-platform Ethiopic input method: a platform-independent C++ core library (`libethio`) with IBus (Linux) and TSF (Windows) wrappers. Amharic first, then other Ethiopic-script languages. Android next; iOS and macOS wrappers planned later.

Full design rationale, directory layout, and roadmap are in `plan_ethiopic-keyboard.md`.

## Architecture

```
ibus-ethiopic (GObject C++ glue, ~600 lines) ─┐
                                                ├── libethio (platform-independent C++ core: trie engine + JSON mappings)
ethiopic-tsf   (COM DLL, ~2,200 lines)        ─┘
android        (InputMethodService + JNI)     ← next
```

Modeled on ibus-libpinyin / ibus-chewing (separate core lib + thin platform wrapper).
Core input engine mirrors m17n-lib's `MIMMap` trie structure, but in clean C++ with JSON mapping files instead of S-expressions.

## Branching strategy

Platform-specific branches track from `master` (common code):

| Branch | Contents |
|--------|----------|
| `master` | Common code: `libethio/`, `data/`, `tests/`, `python-helper/`, `docs/`, top-level CMake |
| `linux` | `master` + `ibus-ethiopic/`, `packaging/`, `install/`, `build-run.sh` |
| `windows` | `master` + `windows/`, `win-tests/`, `build-tsf.sh`, `build-msi.bat` |
| `android` | `master` + `android/` (planned) |

`master` tracks `origin/master` (stable). Platform branches are 17–18 commits ahead.

## Reference implementations (study before contributing)

| Project | Clone / URL | Notes |
|---------|-------------|-------|
| ibus-tmpl | `https://github.com/ibus/ibus-tmpl` | Canonical IBus engine skeleton |
| ibus-chewing | `https://github.com/chewing/ibus-chewing` | Production C engine with separate core lib |
| ibus-libpinyin | `https://github.com/libpinyin/ibus-libpinyin` | C++ core lib + IBus wrapper |
| m17n (am-sera.mim) | `https://git.savannah.nongnu.org/cgit/m17n/m17n-db.git` — `MIM/am-sera.mim` | Canonical Amharic SERA mapping (757 lines) |
| ibus-m17n bridge | `https://github.com/ibus/ibus-m17n` | IBus ↔ m17n glue |
| Google Mozc (TSF) | `https://github.com/google/mozc` — `src/win32/tip/` | Production COM-based TSF IME |
| Microsoft TSF sample | `https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/TSF/TextService` | Canonical TSF TextService skeleton |

## Key technical facts

### Core engine (libethio)

- **Ethiopic syllable formula:** `codepoint = BASE_CONSONANT + vowel_offset` where offset ∈ {0..6} for 7 vowels (ä,u,i,a,e,ə,o), with labiovelar offsets {0,1,2,5,8} for kw/gw/qw/hw families.
- **Input mode:** SERA transliteration (default).
- **Trie engine:** auto-commits on leaf nodes; branches keep text in preedit until mismatch or delimiter. Longest-prefix-first disambiguation.
- **Mapping files:** JSON under `data/<language>/<mode>.json` — loaded at runtime by `libethio`. Scriptable auto-generation from Unicode tables.

### Linux (IBus)

- **Key event contract:** `process_key_event(keyval, keycode, state)` — return `TRUE` if consumed, `FALSE` to pass through.
- **Always skip:** release events (`IBUS_RELEASE_MASK`), Super-key combos (`IBUS_MOD4_MASK`), password/PIN fields.
- **Debug:** `G_MESSAGES_DEBUG=all /usr/libexec/ibus-engine-ethiopic --ibus`

### Windows (TSF)

- **COM interfaces implemented:** `ITfTextInputProcessorEx`, `ITfKeyEventSink`, `ITfTextEditSink`, `ITfThreadMgrEventSink`, `ITfCompositionSink`, `ITfDisplayAttributeProvider`, `ITfCandidateListUIElementBehavior`.
- **Key event flow:** `OnTestKeyDown` (pre-claim) → `OnKeyDown` (convert vk→UTF-8 via `ToUnicodeEx`, feed to `Engine::filter()`) → dispatch `CEthiopicEditSession` (TSF edit cookie) → `StartComposition`/`EndComposition`.
- **Candidate UI:** dual approach — custom HWND popup (`WS_EX_TOPMOST | WS_EX_NOACTIVATE`, GDI-painted) for visual display, plus `ITfCandidateListUIElementBehavior` for TSF-aware app integration.
- **Passthrough toggle:** `Ctrl+Shift` toggles between Ethiopic and plain English passthrough.
- **Registration:** self-registering COM DLL — `DllRegisterServer` writes CLSID `{7A5B3C1D-9E2F-4A6B-8C3D-1E5F7A9B2C4D}`, registers Amharic (0x045E) language profile, and TSF category support.
- **Data file search:** `find_data_file()` resolves `am-sera.json` relative to DLL path — `./data/amharic/` (deployed) or `../../data/amharic/` (build tree) or `MAPPING_SOURCE_DIR` (compile-time fallback).
- **Architecture:** x64 only. DLL compiled with `ethio` static lib built directly into the TSF DLL (no external libethio dependency on Windows).

### Android (planned)

- **Stack:** Java `InputMethodService` + JNI bridge (C++) → `libethio` cross-compiled via NDK.
- **Key lifecycle:** `onCreateInputView()` → keyboard, `onKeyDown()` → JNI → `Engine::filter()` → `commitText()` / `setComposingText()`.

## Build reference

### Linux
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

### Windows
```bash
# MSYS2 mingw64 shell
./build-tsf.sh                    # Build DLL + run tests
./build-tsf.sh /c/Windows/System32  # Build + copy DLL
```
```cmd
REM Command prompt (admin)
windows\build-msi.bat             # Build MSI (requires WiX v4+)
regsvr32 msys-ethiopic-tsf.dll    # Manual registration
```

## Windows installers

| Method | File | Notes |
|--------|------|-------|
| MSI (recommended) | `windows/wix/product.wxs` + `windows/build-msi.bat` | WiX v4, major upgrades, silent install, GPO/SCCM |
| PowerShell | `windows/install.ps1` | Quick single-command install/uninstall |
| NSIS | `windows/installer.nsi` | Standalone .exe with license page |
| Manual | `regsvr32` | Direct COM registration |

All installers enforce 64-bit Windows requirement.

## Implementation order (phases)

1. Generate `data/amharic/am-sera.json` from Unicode syllable math + m17n `am-sera.mim` reference — **Done**
2. Build `libethio` (trie engine + JSON loader + unit tests) — **Done**
3. Wire `ibus-ethiopic` wrapper (GObject subclass, key event processing, preedit/commit) — **Done**
4. Build `ethiopic-tsf` Windows TSF wrapper (COM DLL, key events, composition, candidate UI) — **Done**
5. Windows installers (PowerShell, NSIS, WiX MSI) — **Done**
6. Branch restructuring (linux / windows / master) — **Done**
7. Android `InputMethodService` + JNI bridge — **Next**
8. Linux packaging (RPM, DEB, PKGBUILD)
9. iOS `UIInputViewController` + Swift/C++ bridging
10. macOS `IMKInputController` + Objective-C++

## Style / conventions

- **C++ standard:** C++17
- **Build system:** CMake (3.16+)
- **JSON library:** bundled recursive-descent parser (`ethio/json.hpp`, ~255 lines)
- **IBus version:** ibus-1.0 >= 1.5
- **GObject macro:** `G_DECLARE_FINAL_TYPE` (modern GLib style)
- **Windows TSF:** COM object ref-counting via `InterlockedIncrement`/`InterlockedDecrement`; DLL load-count guard (`g_dllRefCount`, `g_dllUnloading`) prevents re-entry during shutdown.
- **Testing:** all 6 suites passing (mapping, engine, features, wordlist, IBus integration, TSF integration). Core lib tests are platform-independent.
- **Comments:** defaults to no comments. Add only when the WHY is non-obvious (TSF event pipeline ordering, trie fallback-on-mismatch, HWND popup rendering approach). Avoid "what" comments.
- **License:** GPL-3.0-or-later with SPDX headers. Bundled JSON parser is MIT.
