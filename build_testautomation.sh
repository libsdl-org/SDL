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
BUILD_DIR="${SDL_ROOT}/build"
TEST_SRC_DIR="${SDL_ROOT}"

echo "=== SDL3 SVE2 testautomation Build ==="
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

# ============================================================
# Step 1: Clean and create build directory
# ============================================================
echo "[1/4] Cleaning build directory..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# ============================================================
# Step 2: Configure CMake (pure-C software renderer)
# ============================================================
echo ""
echo "[2/4] Configuring CMake (pure-C software renderer)..."
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSDL_UNIX_CONSOLE_BUILD=ON \
    -DSDL_TESTS=ON \
    -DSDL_ARMNEON=OFF \
    -DSDL_ARMSVE2=ON \
    ${CMAKE_EXTRA}

# ============================================================
# Step 3: Build Testautomation
# ============================================================
echo ""
echo "[3/4] Building Testautomation..."
ninja


# ============================================================
# Step 4: Run baseline tests
# ============================================================
echo ""
echo "[4/4] Running testautomation tests..."

echo ""
echo "--- Generating Reference Images ---"
"${BUILD_DIR}/test/testautomation"

