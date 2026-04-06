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

# ── 5. Validate macOS microphone permission key ──────────────────────────────
APP_BUNDLE="$BUILD_DIR/InfinityAudio.app"
INFO_PLIST="$APP_BUNDLE/Contents/Info.plist"
MIC_KEY_VALUE=""
if [[ -f "$INFO_PLIST" ]] && command -v /usr/libexec/PlistBuddy &>/dev/null; then
    MIC_KEY_VALUE="$(/usr/libexec/PlistBuddy -c 'Print :NSMicrophoneUsageDescription' "$INFO_PLIST" 2>/dev/null || true)"
fi

# ── 5. Report ─────────────────────────────────────────────────────────────────
echo ""
echo "Build complete:"
echo "  App bundle : $APP_BUNDLE"
echo "  Size       : ~100 MB (self-contained, includes Qt frameworks)"
if [[ -n "$MIC_KEY_VALUE" ]]; then
    echo "  Mic usage  : OK (Info.plist contains NSMicrophoneUsageDescription)"
else
    echo "  Mic usage  : WARNING (NSMicrophoneUsageDescription not found in Info.plist)"
fi
echo ""
echo "  To run:"
echo "    open \"$APP_BUNDLE\""
echo ""
echo "  macOS note:"
echo "    On first launch, allow microphone access when prompted."
echo "    If denied before, enable it in System Settings > Privacy & Security > Microphone."
echo ""
echo "  To distribute: copy the entire .app folder to any Mac (arm64 or x86_64)."
