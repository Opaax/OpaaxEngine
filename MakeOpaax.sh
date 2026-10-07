#!/usr/bin/env bash
# =============================================================================
# MakeOpaax.sh — create a new Opaax game project (MakeOpaax.bat on Windows).
#
# Usage:   ./MakeOpaax.sh <ProjectName>
#
# Generates <repo>/<ProjectName>/ mirroring Sandbox (game module + runtime + editor)
# and registers it in the root CMakeLists.txt. The skeleton lives in
# OpaaxCreator/Templates/; OpaaxCreator instantiates it and is built here on demand.
# =============================================================================
set -u

if [ $# -lt 1 ] || [ -z "$1" ]; then
    echo "Usage: ./MakeOpaax.sh <ProjectName>"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CREATOR_DIR="$SCRIPT_DIR/OpaaxCreator"

# Single-config generators put it in bin/, multi-config ones in bin/Release/.
find_creator() {
    for CANDIDATE in "$CREATOR_DIR/build/bin/OpaaxCreator" "$CREATOR_DIR/build/bin/Release/OpaaxCreator" \
                     "$CREATOR_DIR/build/bin/Release/OpaaxCreator.exe"; do
        if [ -x "$CANDIDATE" ]; then echo "$CANDIDATE"; return 0; fi
    done
    return 1
}

if ! CREATOR_EXE="$(find_creator)"; then
    echo "[Opaax] OpaaxCreator not found - building it once..."
    cmake -S "$CREATOR_DIR" -B "$CREATOR_DIR/build" -DCMAKE_BUILD_TYPE=Release || { echo "[Opaax] ERROR: OpaaxCreator configure failed."; exit 1; }
    cmake --build "$CREATOR_DIR/build" --config Release || { echo "[Opaax] ERROR: OpaaxCreator build failed."; exit 1; }
    CREATOR_EXE="$(find_creator)" || { echo "[Opaax] ERROR: OpaaxCreator was built but not found."; exit 1; }
fi

"$CREATOR_EXE" "$1" "$SCRIPT_DIR"
