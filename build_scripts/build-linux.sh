#!/usr/bin/env bash
# ─── InfinityAudio – Linux build script ────────────────────────────────────
# Prerequisites:
#   sudo apt install cmake ninja-build qt6-base-dev qt6-multimedia-dev
#
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-linux"

echo "=== InfinityAudio – Linux Release Build ==="
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" --parallel
echo ""
echo "Binary: $BUILD_DIR/InfinityAudio"
