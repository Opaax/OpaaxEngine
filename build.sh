#!/usr/bin/env bash
# =============================================================================
# Opaax Engine — build script for Linux and macOS (build.bat on Windows).
#
#   ./build.sh                  -> Debug + Editor (default)     -> SandboxEditor
#   ./build.sh debug            -> Debug, no editor             -> Sandbox
#   ./build.sh release          -> Release, no editor, no imgui -> Sandbox
#
#   ./build.sh run [preset]     -> build the preset, then launch its app
#   ./build.sh fast [target]    -> incremental build of one target in the configured
#                                  debug-editor tree (default target: Sandbox)
#   ./build.sh test             -> build OpaaxTests (debug-editor) + run CTest
#   ./build.sh clean            -> delete all build directories
#   ./build.sh export <Game> <Dir>
#                               -> the game's release build, its content and the engine's,
#                                  installed into <Dir>: a folder that runs on another machine
#
# Ends with one marker line, OPAAX_BUILD_OK or OPAAX_BUILD_FAIL, and exits 0 or 1.
# A launched app's exit code is reported as OPAAX_RUN_EXIT=<n>.
# =============================================================================
set -u

cd "$(dirname "$0")"

ok()   { echo; echo "[Opaax] Build complete."; echo "OPAAX_BUILD_OK"; exit 0; }
fail() { echo; echo "${1:-}"; echo "OPAAX_BUILD_FAIL"; exit 1; }

# Single-config generators put binaries in bin/, multi-config ones in bin/<Config>/.
binary_dir() {
    if [ -d "build/$1/bin/$2" ]; then echo "build/$1/bin/$2"; else echo "build/$1/bin"; fi
}

MODE="${1:-debug-editor}"

case "$MODE" in
    clean)
        echo "Cleaning build directories..."
        rm -rf build
        ok
        ;;
    fast)
        TARGET="${2:-Sandbox}"
        echo "[Opaax] Fast build: target \"$TARGET\" (debug-editor / Debug, no reconfigure)"
        if [ ! -d build/debug-editor ]; then
            cmake --preset debug-editor || fail "[ERROR] CMake configure failed."
        fi
        cmake --build build/debug-editor --config Debug --target "$TARGET" || fail "[ERROR] Fast build failed."
        ok
        ;;
    test)
        cmake --preset debug-editor || fail "[ERROR] CMake configure failed."
        cmake --build build/debug-editor --config Debug --target OpaaxTests || fail "[ERROR] Test build failed."
        ctest --test-dir build/debug-editor -C Debug --output-on-failure || fail "[ERROR] CTest reported failures."
        ok
        ;;
    export)
        # The same steps as the editor's Export Game (File menu); see CMake/OpaaxGame.cmake.
        GAME="${2:-}"
        DEST="${3:-}"
        if [ -z "$GAME" ] || [ -z "$DEST" ]; then
            fail "Usage: ./build.sh export <Game> <Destination folder>"
        fi
        echo "[Opaax] Exporting $GAME into \"$DEST\"..."
        cmake --preset release || fail "[ERROR] CMake configure failed."
        cmake --build build/release --config Release --target "$GAME" || fail "[ERROR] Build failed."
        cmake --install build/release --config Release --component "$GAME" --prefix "$DEST" || fail "[ERROR] Install failed."
        echo "[Opaax] Exported $GAME into \"$DEST\""
        ok
        ;;
esac

RUNAPP=0
PRESET="$MODE"
if [ "$MODE" = "run" ]; then
    RUNAPP=1
    PRESET="${2:-debug-editor}"
fi

case "$PRESET" in
    debug-editor) CONFIG=Debug;   PRIMARY=SandboxEditor ;;
    debug)        CONFIG=Debug;   PRIMARY=Sandbox ;;
    release)      CONFIG=Release; PRIMARY=Sandbox ;;
    *)
        echo "[ERROR] Unknown preset: \"$PRESET\""
        echo "Presets: debug-editor (default), debug, release. Modes: run [preset], fast [target], test, clean,"
        echo "export <Game> <Dir>."
        fail
        ;;
esac

echo "[Opaax] Preset : $PRESET  (config: $CONFIG, app: $PRIMARY)"
cmake --preset "$PRESET" || fail "[ERROR] CMake configure failed."
cmake --build "build/$PRESET" --config "$CONFIG" || fail "[ERROR] Build failed."

BIN_DIR="$(binary_dir "$PRESET" "$CONFIG")"
echo "[Opaax] Output: $BIN_DIR/"

if [ "$RUNAPP" -eq 1 ]; then
    [ -x "$BIN_DIR/$PRIMARY" ] || fail "[ERROR] Not found: $BIN_DIR/$PRIMARY"

    # Run from the binary's folder: relative paths resolve from there, as in the IDE.
    echo "[Opaax] Launching $PRIMARY ..."
    (cd "$BIN_DIR" && "./$PRIMARY")
    echo "OPAAX_RUN_EXIT=$?"
fi

ok
