#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
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
        return InMode == EWorldMode::Edit ? "Edit" : "Play";
    }

    // =============================================================================
    // WorldSpec — WHICH world to open, in WHICH mode. The host's answer, not its action.
    //
    // This is the app -> engine seam type (BO4): `OpaaxApplication::GetStartupWorldSpec()`
    // returns one, `IEngine::FinishStartup()` consumes it. Splitting the *what* from the
    // *doing* is what lets a host state policy without reaching through the engine to drive
    // a subsystem — the same smell MR0 removed for registries.
    //
    // WHY THIS LIVES IN Application/ AND NOT World/.
    //   No Application header may include from `World/` or `Engine/` (MR1) — a discipline the
    //   tree has never broken — and a by-value return needs the complete type. That is the
    //   mechanical reason, but the layering reason is the better one: Edit-vs-Play is a HOST
    //   mode. The editor host edits, a game host plays; the World merely records the label it
    //   was born with. Policy belongs beside the host that decides it, so this is not I4's
    //   "game concept leaking into Application" — nothing here knows what a world DOES.
    //   `World.h` includes THIS (engine -> application is the legal direction, and it already
    //   includes `Application/Services/ILogger.h`).
    // =============================================================================
    struct WorldSpec
    {
        /** The world's name. Runtime hosts take it from `IProjectManager::StartupLevel()`. */
        OpaaxString Name;

        /** Play unless a host says otherwise — a bare host boots into something runnable. */
        EWorldMode Mode = EWorldMode::Play;
    };
}
