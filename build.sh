#!/usr/bin/env bash
# =============================================================================
# build.sh  —  Configure and build Binary IDE on Linux and macOS.
#
# Usage:
#   ./build.sh            # Debug build (default)
#   ./build.sh release    # Optimised release build
#   ./build.sh clean      # Remove the build directory and exit
#   ./build.sh run        # Build then launch the app (needs a display)
#
# Windows users: use build.bat instead.
# The script lives next to CMakeLists.txt in binary-ide/.
# Run it from either binary-ide/ or the workspace root.
# =============================================================================

set -e  # Exit on first error

# ------------------------------------------------------------------
# Resolve the project root (directory containing this script).
# ------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${1:-debug}"

case "$BUILD_TYPE" in
    clean)
        echo "[build.sh] Removing build directory..."
        rm -rf "$SCRIPT_DIR/build"
        echo "[build.sh] Done."
        exit 0
        ;;
    release)
        CMAKE_BUILD_TYPE="Release"
        BUILD_DIR="$SCRIPT_DIR/build/release"
        ;;
    run)
        CMAKE_BUILD_TYPE="Debug"
        BUILD_DIR="$SCRIPT_DIR/build/debug"
        ;;
    *)
        CMAKE_BUILD_TYPE="Debug"
        BUILD_DIR="$SCRIPT_DIR/build/debug"
        ;;
esac

echo ""
echo "============================================================"
echo "  Binary IDE  —  $CMAKE_BUILD_TYPE build"
echo "  Platform : $(uname -s)"
echo "  Source   : $SCRIPT_DIR"
echo "  Output   : $BUILD_DIR"
echo "============================================================"
echo ""

# ------------------------------------------------------------------
# Configure (only if CMakeCache.txt doesn't exist yet).
# ------------------------------------------------------------------
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "[build.sh] Configuring with CMake ($CMAKE_BUILD_TYPE)..."
    cmake \
        -S "$SCRIPT_DIR" \
        -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    echo ""
fi

# ------------------------------------------------------------------
# Build
# ------------------------------------------------------------------
echo "[build.sh] Building..."

# Detect the number of logical CPUs — works on Linux and macOS.
if command -v nproc &>/dev/null; then
    NPROC=$(nproc)
elif command -v sysctl &>/dev/null; then
    NPROC=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
else
    NPROC=4
fi

cmake --build "$BUILD_DIR" --config "$CMAKE_BUILD_TYPE" -- -j"$NPROC"

echo ""
echo "============================================================"
echo "  Build succeeded!"
echo "  Binary: $BUILD_DIR/BinaryIDE"
echo "============================================================"
echo ""

# ------------------------------------------------------------------
# Optional: run the application after a successful build.
# Requires a running display server (X11 on Linux, Quartz on macOS).
# ------------------------------------------------------------------
if [ "$BUILD_TYPE" = "run" ]; then
    if [[ "$(uname -s)" == "Darwin" ]]; then
        # macOS: no DISPLAY needed; run directly.
        echo "[build.sh] Launching BinaryIDE on macOS..."
        exec "$BUILD_DIR/BinaryIDE"
    elif [ -z "$DISPLAY" ]; then
        echo "[build.sh] WARNING: \$DISPLAY is not set."
        echo "[build.sh] The app requires an X11 display."
        echo "[build.sh] In Replit: enable the VNC panel before running."
        echo "[build.sh] On a local machine: DISPLAY=:0 ./build.sh run"
    else
        echo "[build.sh] Launching BinaryIDE on DISPLAY=$DISPLAY ..."
        exec "$BUILD_DIR/BinaryIDE"
    fi
fi
