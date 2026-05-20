#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# Locate CMake (not always on PATH)
for candidate in /c/Program\ Files/CMake/bin/cmake.exe /mingw64/bin/cmake.exe /usr/bin/cmake.exe cmake; do
    if command -v "$candidate" &> /dev/null; then
        CMAKE="$candidate"
        break
    fi
done
if [ -z "${CMAKE:-}" ]; then
    echo "ERROR: cmake not found"
    exit 1
fi

BUILD_DIR="build-win"
TEST_DIR="${BUILD_DIR}/tests"
MSVC_MODE=0

# Detect toolchain — try MSVC first (via vswhere), then GCC
VSW="${PROGRAMFILES_X86:-"C:/Program Files (x86)"}/Microsoft Visual Studio/Installer/vswhere.exe"
if [ -x "$VSW" ] && "$VSW" -latest -property installationPath &> /dev/null; then
    MSVC_MODE=1
    GENERATOR="Visual Studio 17 2022"
    CONFIG="Release"
    DLL_SRC="${BUILD_DIR}/${CONFIG}/ethiopic-tsf.dll"
    TEST_EXE_DIR="${TEST_DIR}/${CONFIG}"
elif command -v g++.exe &> /dev/null; then
    MSVC_MODE=0
    GENERATOR="Unix Makefiles"
    CONFIG=""
    DLL_SRC="${BUILD_DIR}/msys-ethiopic-tsf.dll"
    TEST_EXE_DIR="${TEST_DIR}"
else
    echo "ERROR: No C++ compiler found (VS Build Tools or MSYS2 GCC)"
    exit 1
fi

INSTALL="${1:-}"
if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    echo "Usage: $0 [install-path]"
    echo ""
    echo "  install-path   Optional directory to copy the DLL into."
    echo "                 Example: $0 /c/Windows/System32"
    exit 0
fi

echo "=== Toolchain: $([ $MSVC_MODE -eq 1 ] && echo MSVC || echo GCC) ==="
echo ""

# ── TSF DLL ──────────────────────────────────────────────────
echo "=== Configuring TSF DLL ==="
"$CMAKE" -G "$GENERATOR" -A x64 \
    -S windows/ethiopic-tsf \
    -B "$BUILD_DIR" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1

echo "=== Building TSF DLL ==="
"$CMAKE" --build "$BUILD_DIR" --config "$CONFIG"

echo "  DLL: $DLL_SRC"

# ── Tests ────────────────────────────────────────────────────
echo ""
echo "=== Building tests ==="

mkdir -p "$TEST_DIR"

cat > "${TEST_DIR}/CMakeLists.txt" << 'CMAKE_EOF'
cmake_minimum_required(VERSION 3.16)
project(ethiopic-tests-win LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../../libethio ${CMAKE_CURRENT_BINARY_DIR}/libethio)

add_executable(test_mapping ../../tests/test_mapping.cpp)
target_link_libraries(test_mapping PRIVATE ethio)
target_compile_definitions(test_mapping PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/../../data")

add_executable(test_engine ../../tests/test_engine.cpp)
target_link_libraries(test_engine PRIVATE ethio)
target_compile_definitions(test_engine PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/../../data")

add_executable(test_features ../../tests/test_features.cpp)
target_link_libraries(test_features PRIVATE ethio)
target_compile_definitions(test_features PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/../../data")

add_executable(test_wordlist ../../tests/test_wordlist.cpp)
target_link_libraries(test_wordlist PRIVATE ethio)
target_compile_definitions(test_wordlist PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/../../data")

add_executable(test_tsf_engine ../../tests/test_tsf_engine.cpp)
target_sources(test_tsf_engine PRIVATE ../../windows/ethiopic-tsf/engine.cpp)
target_include_directories(test_tsf_engine PRIVATE
    ../../windows/ethiopic-tsf
    ${CMAKE_SOURCE_DIR}/../../libethio/include
)
target_link_libraries(test_tsf_engine PRIVATE ethio ole32 oleaut32 uuid)
target_compile_definitions(test_tsf_engine PRIVATE
    DATA_DIR="${CMAKE_SOURCE_DIR}/../../data"
    MAPPING_SOURCE_DIR="${CMAKE_SOURCE_DIR}/../../data"
    ETHIOPIC_TEST_STUBS
)

enable_testing()
foreach(test_name test_mapping test_engine test_features test_wordlist test_tsf_engine)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
endforeach()
CMAKE_EOF

"$CMAKE" -G "$GENERATOR" -A x64 \
    -S "$TEST_DIR" \
    -B "$TEST_DIR"
"$CMAKE" --build "$TEST_DIR" --config "$CONFIG"

# ── Run tests ────────────────────────────────────────────────
echo ""
echo "=== Running tests ==="
cd "$TEST_DIR"
FAILED=0
if [ "$MSVC_MODE" -eq 1 ]; then
    EXE_PREFIX="${CONFIG}/"
else
    EXE_PREFIX=""
fi
for exe in test_mapping test_engine test_features test_wordlist test_tsf_engine; do
    echo -n "  $exe ... "
    if "${EXE_PREFIX}${exe}.exe" > /dev/null 2>&1; then
        echo "PASS"
    else
        echo "FAIL (exit $?)"
        FAILED=1
    fi
done

if [ "$FAILED" -eq 0 ]; then
    echo "All tests passed."
else
    echo ""
    echo "Some tests FAILED. Re-run with:"
    echo "  cd ${TEST_DIR} && ${CONFIG}/test_tsf_engine.exe"
    exit 1
fi

cd ../..

# ── Install ──────────────────────────────────────────────────
if [ -n "$INSTALL" ]; then
    echo ""
    echo "=== Installing DLL to $INSTALL ==="
    mkdir -p "$INSTALL"
    cp "$DLL_SRC" "$INSTALL/"
    echo "  Copied $(basename "$DLL_SRC")"
    echo ""
    echo "To register the IME, run from an elevated command prompt:"
    echo "  regsvr32 $INSTALL/$(basename "$DLL_SRC")"
fi

echo ""
echo "Done."
