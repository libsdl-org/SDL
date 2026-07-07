#!/bin/bash
# SDL3 SVE2 Baseline Build Script
# Cross-platform: macOS (Darwin) and Debian 12 (Linux)
# Purpose: Build SDL3 with pure-C software blitters and run baseline tests

set -e

# ============================================================
# Detect platform
# ============================================================
OS="$(uname -s)"
ARCH="$(uname -m)"
SDL_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SDL_ROOT}/build_colour_keying"
TEST_SRC_DIR="${SDL_ROOT}"

echo "=== SDL3 SVE2 Colour Keying Build ==="
echo "Platform: ${OS}"
echo "Arch: ${ARCH}"
echo "SDL Root: ${SDL_ROOT}"
echo "Build Dir: ${BUILD_DIR}"
echo ""

# ============================================================
# Platform-specific settings
# ============================================================
if [ "${OS}" = "Darwin" ]; then
    CMAKE_EXTRA="-DCMAKE_OSX_ARCHITECTURES=arm64"
    LINK_FLAGS=(
        -framework Cocoa
        -framework CoreVideo
        -framework IOKit
        -framework CoreAudio
        -framework AudioToolbox
        -framework ForceFeedback
        -framework CoreHaptics
        -framework GameController
        -framework Carbon
        -framework AVFoundation
        -framework CoreMedia
        -framework UniformTypeIdentifiers
        -framework UserNotifications
        -framework Security
        -liconv
    )
elif [ "${OS}" = "Linux" ]; then
    CMAKE_EXTRA=""
    LINK_FLAGS=(
        -ldl
        -lpthread
        -lm
        -lrt
    )
else
    echo "Unsupported platform: ${OS}"
    exit 1
fi

cd "${BUILD_DIR}"

echo ""
echo "[3/5] Building SDL3 static library..."
ninja clean
ninja

if [ ! -f "${BUILD_DIR}/libSDL3.a" ]; then
    echo "Error: libSDL3.a was not generated"
    exit 1
fi
echo "OK: libSDL3.a generated"

# ============================================================
# Step 4: Compile baseline test programs
# ============================================================
echo ""
echo "[4/5] Compiling baseline test programs..."

clang -O3 \
    -I"${SDL_ROOT}/include" \
    -I"${BUILD_DIR}" \
    -o "${BUILD_DIR}/colour_keying_test" \
    "${TEST_SRC_DIR}/colour_keying_test.c" \
    "${BUILD_DIR}/libSDL3.a" \
    "${LINK_FLAGS[@]}"

echo "OK: colour_keying_test compiled"

# ============================================================
# Step 5: Run Colour Keying tests
# ============================================================
echo ""
echo "[5/5] Running Colour Keying tests..."
echo ""
echo "--- Performance Baseline ---"
"${BUILD_DIR}/colour_keying_test"


echo ""
echo "=== Baseline Complete ==="
echo "Build dir: ${BUILD_DIR}"
