#!/bin/bash
set -e

PREFIX="${1:-/usr}"
INSTALL="${2:-0}"

cmake -B build -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cmake --build build

cmake -B build/tests -S tests
cmake --build build/tests
cd build/tests && ctest --output-on-failure && cd ../..

if [ "$INSTALL" = "1" ]; then
    sudo cmake --install build
    echo "Installed to $PREFIX."
    exit 0
fi

if pgrep -f ibus-engine-ethiopic > /dev/null; then
    echo "Killing existing ibus-engine-ethiopic processes..."
    pkill -f ibus-engine-ethiopic
    sleep 0.5
fi

echo "Starting engine in debug mode (IBus will connect on input method switch)..."
G_MESSAGES_DEBUG=all build/ibus-ethiopic/ibus-engine-ethiopic --ibus
