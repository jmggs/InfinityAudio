#!/usr/bin/env bash
# ─── InfinityAudio – macOS build script ──────────────────────────────────────
# Works on Apple Silicon (arm64) and Intel (x86_64).
#
# Prerequisites installed automatically via Homebrew:
#   brew install cmake make qt
#
# Usage:
#   chmod +x build_scripts/build-mac.sh
#   ./build_scripts/build-mac.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# ── 1. Homebrew ───────────────────────────────────────────────────────────────
if ! command -v brew &>/dev/null; then
    echo "[ERROR] Homebrew not found."
    echo "        Install it first: https://brew.sh"
    echo "        /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    exit 1
fi

# ── 2. Dependencies ───────────────────────────────────────────────────────────
echo "=== Checking / installing dependencies ==="

for pkg in cmake qt; do
    if ! brew list --formula "$pkg" &>/dev/null; then
        echo "  Installing $pkg ..."
        brew install "$pkg"
    else
        echo "  $pkg: OK"
    fi
done

QT_PREFIX="$(brew --prefix qt)"

# ── 3. Detect architecture and select preset ──────────────────────────────────
ARCH="$(uname -m)"   # arm64 or x86_64

if [[ "$ARCH" == "arm64" ]]; then
    PRESET="macos-arm64-release"
    BUILD_DIR="$PROJECT_DIR/build-mac-arm64"
else
    PRESET="macos-x86_64-release"
    BUILD_DIR="$PROJECT_DIR/build-mac-x86_64"
fi

echo ""
echo "=== InfinityAudio – macOS Release Build (${ARCH}) ==="

# ── 4. Configure + Build ──────────────────────────────────────────────────────
cd "$PROJECT_DIR"

cmake --preset "$PRESET"
cmake --build "$BUILD_DIR" --parallel

# ── 5. Report ─────────────────────────────────────────────────────────────────
echo ""
echo "Build complete:"
echo "  App bundle: $BUILD_DIR/InfinityAudio.app"
echo ""
echo "  To run:"
echo "    open \"$BUILD_DIR/InfinityAudio.app\""
