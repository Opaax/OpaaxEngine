#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    /** The world a host boots into when its startup level is missing, empty or unreadable. */
    inline constexpr const char* NULL_LEVEL_WORLD_NAME = "NullLevel";
    
    inline constexpr const char* WORLD_MODE_EDIT_AS_CHARS = "Edit";
    
    inline constexpr const char* WORLD_MODE_PLAY_AS_CHARS = "Play";
    
    // =============================================================================
    // EWorldMode — what a World was created for. Set at construction, never changed:
    //   Play In Editor makes a Play copy, so the edit world is never touched.
    // =============================================================================
    enum class EWorldMode : Uint8
    {
        /** Editing. The editor's world: gameplay does not run. */
        Edit,

        /** Simulating. A game's world, and the Play In Editor copy. */
        Play
    };

    /** Label for logs and UI. Not for saving. */
    constexpr const char* ToString(EWorldMode InMode) noexcept
    {
        return InMode == EWorldMode::Edit ? WORLD_MODE_EDIT_AS_CHARS : WORLD_MODE_PLAY_AS_CHARS;
    }

    // =============================================================================
    // WorldSpec — which level to open, in which mode. The world is named after the level.
    // =============================================================================
    struct WorldSpec
    {
        /**
         * Asset-relative level path ("Levels/Main.opaaxlevel"), or empty. Empty or unreadable
         * gives an empty world (not an error).
         */
        OpaaxString LevelPath;

        /** Play by default. */
        EWorldMode Mode = EWorldMode::Play;
    };
}
