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
BUILD_DIR="${SDL_ROOT}/build-baseline"
TEST_SRC_DIR="${SDL_ROOT}"

echo "=== SDL3 SVE2 Baseline Build ==="
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


# ============================================================
# Step 3: Build SDL3 static library
# ============================================================
echo ""
echo "[3/5] Building SDL3 static library..."
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
    -o "${BUILD_DIR}/test_sve2_baseline" \
    "${TEST_SRC_DIR}/test_sve2_baseline.c" \
    "${BUILD_DIR}/libSDL3.a" \
    "${LINK_FLAGS[@]}"

echo "OK: test_sve2_baseline compiled"

clang -O3 \
    -I"${SDL_ROOT}/include" \
    -I"${BUILD_DIR}" \
    -o "${BUILD_DIR}/test_sve2_visual_ref" \
    "${TEST_SRC_DIR}/test_sve2_visual_ref.c" \
    "${BUILD_DIR}/libSDL3.a" \
    "${LINK_FLAGS[@]}"

echo "OK: test_sve2_visual_ref compiled"

# ============================================================
# Step 5: Run baseline tests
# ============================================================
echo ""
echo "[5/5] Running baseline tests..."
#echo ""
#echo "--- Performance Baseline 1 ---"
#taskset -c 10 "${BUILD_DIR}/test_sve2_baseline"

echo ""
echo "--- Generating Reference Images ---"
"${BUILD_DIR}/test_sve2_visual_ref"

mv /tmp/sve2_ref_01_gradient_alpha_blend.bmp /tmp/sve2_new.bmp
vbindiff /tmp/sw.bmp /tmp/sve2_new.bmp

echo ""
echo "=== Baseline Complete ==="
echo "Build dir: ${BUILD_DIR}"
echo "Reference images: /tmp/sve2_ref_*.bmp"
echo ""
echo "Next steps:"
echo "  1. Inspect images (macOS): open /tmp/sve2_ref_*.bmp"
echo "  2. Inspect images (Linux):  xdg-open /tmp/sve2_ref_*.bmp"
