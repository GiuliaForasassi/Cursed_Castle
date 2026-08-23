#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

EXE=$(grep -m1 '^project(' CMakeLists.txt | sed 's/project(\([^ )]*\).*/\1/')

if [ ! -f build/CMakeCache.txt ]; then
    echo "[1/3] Configuring..."
    cmake --preset default
else
    echo "[1/3] Already configured, skipping."
fi

echo "[2/3] Building..."
cmake --build --preset default

echo "[3/3] Running $EXE..."
cd build && ./"$EXE"
