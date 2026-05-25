# Ethiopic TSF — Windows Text Services Framework IME

COM DLL that plugs the `libethio` SERA transliteration engine into the Windows Text Services Framework. Type Amharic (and other Ethiopic-script languages) phonetically using a standard US-English keyboard layout.

```
ethiopic-tsf (COM DLL, ~650 lines C++)
  → libethio   (platform-independent C++ trie engine)
```

## Files

| File | Purpose |
|------|---------|
| `engine.h` | `CEthiopicTextService` COM class — implements `ITfTextInputProcessor`, `ITfKeyEventSink`, `ITfCompositionSink`, `ITfThreadMgrEventSink` |
| `engine.cpp` | Key event processing, SERA→Ethiopic conversion via `libethio`, composition/preeedit management, test-mode support |
| `dllmain.cpp` | COM server entry points: `DllRegisterServer`, `DllUnregisterServer`, `DllGetClassObject`, `DllCanUnloadNow` |
| `ethiopic-tsf.def` | DLL export definitions |
| `CMakeLists.txt` | CMake build (MinGW / MSYS2 toolchain) |
| `README.md` | This file |

## Requirements

- **MSYS2** with mingw-w64-x86_64 toolchain (GCC 13+)
- **CMake** 3.16+
- **C++17** compiler
- Windows 10+ (TSF is available on Windows 2000+, tested on 10+)

System libraries linked: `ole32`, `oleaut32`, `advapi32`, `uuid` (all standard Windows SDK).

## Build

Run from an **MSYS2 mingw64 shell** in the project root:

```bash
./build-tsf.sh
```

This script:
1. Configures and builds the TSF DLL → `build-win/ethiopic-tsf/msys-ethiopic-tsf.dll`
2. Builds and runs all test executables (mapping, engine, features, wordlist, TSF engine)

### Build with install

```bash
./build-tsf.sh /c/Windows/System32
```

Copies the built DLL to the given directory after a successful build + test run.

### Manual build (step by step)

```bash
# Configure
cmake -G "Unix Makefiles" \
    -DCMAKE_CXX_COMPILER=/mingw64/bin/g++.exe \
    -S windows/ethiopic-tsf \
    -B build-win/ethiopic-tsf

# Build
make -C build-win/ethiopic-tsf -j$(nproc)
```

Output: `build-win/ethiopic-tsf/msys-ethiopic-tsf.dll`

## Test

### Automated tests

The build script runs five test executables automatically. To re-run them manually:

```bash
cd build-win/tests

./test_mapping.exe       # Trie construction + JSON loading
./test_engine.exe        # Core engine: key→Ethiopic sequences
./test_features.exe      # Feature-level behavior + edge cases
./test_wordlist.exe      # Word list loading + suggestions
./test_tsf_engine.exe    # TSF integration (key events, preedit, commit)
```

All tests output `PASS` / `FAIL` per test case and return non-zero exit code on failure.

### Test mode

The engine class (`CEthiopicTextService`) includes a test mode that bypasses TSF COM interfaces entirely:

```cpp
auto *svc = new CEthiopicTextService();
svc->SetTestMode(true);

svc->ProcessKeyUtf8("h");
svc->ProcessKeyUtf8("e");
// svc->TestCommittedText() == "ሀ"
// svc->TestPreeditText()   == ""

svc->ResetTest();
```

Test inputs are UTF-8 strings (e.g., `"selam "` → `"ሰላም "`). The test mode accumulates committed text and tracks the current preedit string — no real TSF context or edit session needed. See `tests/test_tsf_engine.cpp` for 17 test cases covering single keys, multi-syllable words, labiovelars, punctuation, numerals, and UTF-8 byte integrity.

### Manual testing in Notepad

After registering the DLL (see Deploy below), switch to the IME in the language bar and type in any text application.

## Deploy

Three options: a one-command PowerShell installer, an NSIS-based setup wizard, or manual registration.

### Option 1: PowerShell Installer (quickest)

From an **elevated PowerShell** in the project root:

```powershell
powershell -ExecutionPolicy Bypass -File windows\install.ps1
```

This copies the DLL and all data files to `C:\Program Files\Moab\Ethiopic Keyboard\`, registers the IME, and verifies the installation.

To uninstall:

```powershell
powershell -ExecutionPolicy Bypass -File windows\install.ps1 -Uninstall
```

### Option 2: NSIS Setup Wizard

Build an `.exe` installer with license page, directory chooser, and uninstaller:

```bash
makensis windows/installer.nsi
```

Output: `build-win/EthiopicKeyboard-0.1.0-setup.exe`

Requires [NSIS 3.0+](https://nsis.sourceforge.io/Download).

### Option 3: Manual Registration

The DLL self-registers its CLSID and language profile. From an **elevated command prompt**:

```cmd
regsvr32 msys-ethiopic-tsf.dll
```

This runs `DllRegisterServer()`, which:
1. Writes CLSID under `HKCU\Software\Classes\CLSID\`
2. Registers an "Ethiopic (SERA)" language profile for Amharic (`0x045E`) via `ITfInputProcessorProfileMgr`

Data files must be placed alongside the DLL:
```
<install-dir>\
├── ethiopic-tsf.dll
└── data\
    └── amharic\
        ├── am-sera.json
        ├── wordlist.json
        ├── bigrams.json
        └── names.json
```

The DLL locates mapping files via `./data/amharic/` relative to its own path.

#### Unregistration

```cmd
regsvr32 /u msys-ethiopic-tsf.dll
```

#### Without regsvr32

```cmd
rundll32 msys-ethiopic-tsf.dll,DllRegisterServer
rundll32 msys-ethiopic-tsf.dll,DllUnregisterServer
```

### Activation

After installation:
1. Open **Settings → Time & Language → Language → Amharic → Options → Keyboards**
2. Add **Ethiopic (SERA)**
3. Switch to it via the language bar or `Win+Space`

The IME will now intercept keystrokes in any TSF-aware application (Notepad, Word, browsers, VS Code, etc.).

## How it works

### Data flow

```
User types 'h'  →  OnKeyDown  →  vk_to_utf8('h')  →  m_core.filter("h")
    → composing = "ህ" (U+1205)
    → TSF shows preedit underline

User types 'e'  →  OnKeyDown  →  vk_to_utf8('e')  →  m_core.filter("e")
    → composing = "ሀ" (U+1200), leaf node reached
    → TSF commits "ሀ" via ITfEditSession, clears preedit
```

### Key classes

- **`CEthiopicTextService`** — The main COM object. Activated by TSF when the user switches to this IME. Owns the `libethio::Engine` instance and routes all keystrokes through it.
- **`CEthiopicEditSession`** — A transient edit session that applies text changes (commit + preedit) inside a TSF edit cookie. Created per keystroke that produces output.

### Key event handling

- `OnTestKeyDown` — Always claims keys (`*pfEaten = TRUE`) to ensure TSF delivers keystrokes
- `OnKeyDown` — Converts virtual key to UTF-8 via `ToUnicodeEx()`, feeds to `libethio::Engine::filter()`, then dispatches an edit session to apply commit/preedit
- `OnKeyUp` — Passes through (`*pfEaten = FALSE`)
- Release events are skipped (TSF delivers both press and release)

### Implementation status

| Feature | Status |
|---------|--------|
| SERA→Ethiopic keystroke conversion | Done |
| Preedit / composition display | Done |
| Multi-syllable word input | Done |
| Labiovelar support (hW, kW, etc.) | Done |
| Punctuation + numerals | Done |
| Word list / candidate suggestions | Pending |
| Display attribute (underline style) | Pending |
| Ctrl+Shift toggle (passthrough) | Pending |
| Installer (WiX MSI) | Pending |

## Architecture notes

- Modeled after Google Mozc's Windows TSF module and Microsoft's TSF sample
- Uses `ITfComposition` for preedit (not custom rendering) — simple underline display
- Edit sessions are asynchronous (`TF_ES_ASYNCDONTCARE`) to avoid blocking the input thread
- The `libethio` core is compiled directly into the DLL via CMake source listing (no `add_subdirectory` — avoids a CMake 4.3 path-mangling bug with MSYS2/MinGW)

## References

- [TSF documentation](https://docs.microsoft.com/en-us/windows/win32/tsf/text-services-framework)
- [Microsoft TSF sample](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/TSF/TextService)
- [Google Mozc (Windows TSF module)](https://github.com/google/mozc) — `src/win32/tip/`
- [Ethiopic Unicode block (U+1200–U+137F)](https://unicode.org/charts/PDF/U1200.pdf)
