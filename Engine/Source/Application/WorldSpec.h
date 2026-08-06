#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    /** The world a host boots into when its startup level is absent, empty or unreadable. */
    inline constexpr const char* NULL_LEVEL_WORLD_NAME = "NullLevel";
    
    /***/
    inline constexpr const char* WORLD_MODE_EDIT_AS_CHARS = "Edit";
    
    /***/
    inline constexpr const char* WORLD_MODE_PLAY_AS_CHARS = "Play";
    
    // =============================================================================
    // EWorldMode — what a World was created FOR.
    //
    // Set at construction and never mutated (World exposes no setter). Changing mode means
    // creating another world, which is exactly what makes PIE-by-clone coherent: the edit
    // world is never disturbed by Play, so Stop restores it for free rather than undoing
    // anything. A settable mode would quietly re-introduce the "restore the world after
    // playing" problem that cloning exists to avoid.
    //
    // Scoped enum by X2 — enumerators this generic (Edit/Play) must not enter the namespace.
    // =============================================================================
    enum class EWorldMode : Uint8
    {
        /** Authoring. The editor's world: edit-only overlays live here, gameplay does not run. */
        Edit,

        /** Simulating. A game host's world, and the clone PIE runs. */
        Play
    };

    /** Log/UI label. Not for serialization — a map file should write its own stable token. */
    constexpr const char* ToString(EWorldMode InMode) noexcept
    {
        return InMode == EWorldMode::Edit ? WORLD_MODE_EDIT_AS_CHARS : WORLD_MODE_PLAY_AS_CHARS;
    }

    // =============================================================================
    // WorldSpec — WHICH level to open, in WHICH mode. The host's answer, not its action (BO4).
    //
    // Lives in Application/ rather than World/ because Edit-vs-Play is a HOST mode, and because
    // no Application header may include from World/ or Engine/ (MR1) while a by-value return
    // needs the complete type.
    //
    // There is deliberately no Name: the world is named by the LEVEL it opens (LevelData::Name),
    // so a host that cannot supply a level cannot invent a name for one either.
    // =============================================================================
    struct WorldSpec
    {
        /**
         * WHICH level to open, ASSET-RELATIVE ("Levels/Main.opaaxlevel"), or EMPTY for none.
         *
         * Empty is a supported answer, not a misconfiguration: a test host, or a game that
         * populates its world in code, boots into the NullLevel world. So is a path that does
         * not resolve — `IEngine::FinishStartup` falls back rather than refusing to boot.
         */
        OpaaxString LevelPath;

        /** Play unless a host says otherwise — a bare host boots into something runnable. */
        EWorldMode Mode = EWorldMode::Play;
    };
}
