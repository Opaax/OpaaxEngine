#!/usr/bin/env bash
# =============================================================================
# MakeOpaax.sh — create a new Opaax game project (MakeOpaax.bat on Windows).
#
# Usage:   ./MakeOpaax.sh <ProjectName> [<Folder>]
#
# Generates <Folder>/<ProjectName>/ mirroring Sandbox (game module + runtime + editor + tests)
# and registers it in <Folder>/CMakeLists.txt. Without a folder, the game goes in this repository.
# Another folder is a game repository of its own: when it has no CMakeLists.txt yet, it gets one
# that adds this engine as a subdirectory, with presets (Docs/GameRepository.md). The skeleton
# lives in OpaaxCreator/Templates/ and OpaaxCreator/Workspace/; OpaaxCreator instantiates it and
# is built here when missing or out of date.
# =============================================================================
set -u

if [ $# -lt 1 ] || [ -z "$1" ]; then
    echo "Usage: ./MakeOpaax.sh <ProjectName> [<Folder>]"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CREATOR_DIR="$SCRIPT_DIR/OpaaxCreator"
WORKSPACE="${2:-$SCRIPT_DIR}"

# Single-config generators put it in bin/, multi-config ones in bin/Release/.
find_creator() {
    for CANDIDATE in "$CREATOR_DIR/build/bin/OpaaxCreator" "$CREATOR_DIR/build/bin/Release/OpaaxCreator" \
                     "$CREATOR_DIR/build/bin/Release/OpaaxCreator.exe"; do
        if [ -x "$CANDIDATE" ]; then echo "$CANDIDATE"; return 0; fi
    done
    return 1
}

# Configured once, rebuilt when its source changed.
if [ ! -f "$CREATOR_DIR/build/CMakeCache.txt" ]; then
    echo "[Opaax] Configuring OpaaxCreator once..."
    cmake -S "$CREATOR_DIR" -B "$CREATOR_DIR/build" -DCMAKE_BUILD_TYPE=Release || { echo "[Opaax] ERROR: OpaaxCreator configure failed."; exit 1; }
fi
cmake --build "$CREATOR_DIR/build" --config Release || { echo "[Opaax] ERROR: OpaaxCreator build failed."; exit 1; }
CREATOR_EXE="$(find_creator)" || { echo "[Opaax] ERROR: OpaaxCreator was built but not found."; exit 1; }

"$CREATOR_EXE" "$1" "$WORKSPACE" "$SCRIPT_DIR"
