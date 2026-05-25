# Ethiopic TSF — Windows Text Services Framework IME

COM DLL that plugs the `libethio` SERA transliteration engine into the Windows Text Services Framework. Type Amharic (and other Ethiopic-script languages) phonetically using a standard US-English keyboard layout.

```
ethiopic-tsf (COM DLL)
  → libethio   (platform-independent C++ trie engine)
```

## Files

| File | Purpose |
|------|---------|
| `engine.h` | `CEthiopicTextService` COM class — implements `ITfTextInputProcessor`, `ITfKeyEventSink`, `ITfCompositionSink`, `ITfThreadMgrEventSink` |
| `engine.cpp` | Key event processing, SERA→Ethiopic conversion via `libethio`, composition/preeedit management, test-mode support |
| `dllmain.cpp` | COM server entry points: `DllRegisterServer`, `DllUnregisterServer`, `DllGetClassObject`, `DllCanUnloadNow` |
| `ethiopic-tsf.def` | DLL export definitions |
| `CMakeLists.txt` | CMake build (MSVC toolchain) |
| `README.md` | This file |

## Requirements

- **64-bit Windows 10 or later** (x64 only; 32-bit/x86 not supported)
- **Visual Studio 2022** with MSVC toolchain (or VS Build Tools)
- **CMake** 3.16+
- **C++17** compiler

System libraries linked: `ole32`, `oleaut32`, `advapi32`, `uuid` (all standard Windows SDK).

## Build

Run from the project root:

```bash
./build-tsf.sh
```

This script:
1. Configures and builds the TSF DLL with MSVC → `build-win/Release/ethiopic-tsf.dll`
2. Builds and runs all test executables (mapping, engine, features, wordlist, TSF engine)

### Manual build (step by step)

```cmd
cmake -G "Visual Studio 17 2022" -A x64 ^
    -S windows/ethiopic-tsf ^
    -B build-win

cmake --build build-win --config Release
```

Output: `build-win/Release/ethiopic-tsf.dll`

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

Three options: MSI (recommended for production), PowerShell (quickest), or manual.

### Option 1: MSI Installer (recommended)

Build a standard Windows Installer `.msi` package using the [WiX Toolset](https://wixtoolset.org/):

```cmd
windows\build-msi.bat
```

Output: `build-win/EthiopicKeyboard-0.1.0-x64.msi`

Requires WiX v4+ (`dotnet tool install --global wix`).

The MSI supports:
- Interactive install with directory chooser
- Silent install: `msiexec /i EthiopicKeyboard-0.1.0-x64.msi /qn`
- Silent uninstall: `msiexec /x EthiopicKeyboard-0.1.0-x64.msi /qn`
- Major upgrades (replace old version automatically)
- Add/Remove Programs entry with publisher metadata
- Enterprise deployment via Group Policy / SCCM

### Option 2: PowerShell Installer (quickest)

From an **elevated PowerShell** in the project root:

```powershell
powershell -ExecutionPolicy Bypass -File windows\install.ps1
```

This copies the DLL and all data files to `C:\Program Files\Moab\Ethiopic Keyboard\`, registers the IME, and verifies the installation.

To uninstall:

```powershell
powershell -ExecutionPolicy Bypass -File windows\install.ps1 -Uninstall
```

### Option 3: Manual Registration

The DLL self-registers its CLSID and language profile. From an **elevated command prompt**:

```cmd
regsvr32 ethiopic-tsf.dll
```

This runs `DllRegisterServer()`, which:
1. Writes CLSID under `HKCR\CLSID\`
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
regsvr32 /u ethiopic-tsf.dll
```

#### Without regsvr32

```cmd
rundll32 ethiopic-tsf.dll,DllRegisterServer
rundll32 ethiopic-tsf.dll,DllUnregisterServer
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
| Word list / candidate suggestions | Done |
| Display attribute (underline style) | Done |
| Ctrl+Shift toggle (passthrough) | Done |
| Installer (PowerShell, WiX MSI) | Done |

## Architecture notes

- Modeled after Google Mozc's Windows TSF module and Microsoft's TSF sample
- Uses `ITfComposition` for preedit (not custom rendering) — simple underline display
- Edit sessions are asynchronous (`TF_ES_ASYNCDONTCARE`) to avoid blocking the input thread
- The `libethio` core is compiled directly into the DLL via CMake source listing

## References

- [TSF documentation](https://docs.microsoft.com/en-us/windows/win32/tsf/text-services-framework)
- [Microsoft TSF sample](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/TSF/TextService)
- [Google Mozc (Windows TSF module)](https://github.com/google/mozc) — `src/win32/tip/`
- [Ethiopic Unicode block (U+1200–U+137F)](https://unicode.org/charts/PDF/U1200.pdf)
