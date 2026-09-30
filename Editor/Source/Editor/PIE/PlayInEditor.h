#pragma once

#include "Core/OpaaxTypes.h"   // Uint8

namespace Opaax
{
    class World;
    class WorldManager;
    class IEngine;
}

namespace Opaax::Editor
{
    // =============================================================================
    // EPlayState — where the editor is in the Play cycle. Paused is a state (Step is only allowed
    //   from it).
    // =============================================================================
    enum class EPlayState : Uint8
    {
        Edit,
        Playing,
        Paused
    };

    const char* ToString(EPlayState InState) noexcept;

    // =============================================================================
    // PlayInEditor — the Play-in-editor state machine. Owned by EditorService, referenced by
    //   EditorContext. Driven by the toolbar and by EditorService::RouteInput's keys.
    //   Play clones the edit world into a Play world and activates it; the edit world stays untouched,
    //   so Stop just reactivates it and destroys the clone (no undo needed).
    //   Calls from a wrong state are refused with a log.
    // =============================================================================
    class PlayInEditor
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        PlayInEditor(WorldManager& InWorlds, IEngine& InEngine) noexcept
            : m_Worlds(InWorlds), m_Engine(InEngine) {}

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        PlayInEditor(const PlayInEditor&)            = delete;
        PlayInEditor& operator=(const PlayInEditor&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Starts a game, then clones the active (edit) world into a Play world and activates it. The game
         * must exist first: world subsystems get their context when the world is created.
         * Refused unless in Edit with an active world; a failed clone ends the game.
         * @return True when the Play world is live
         */
        bool Play();

        /** Pauses the world tick. Refused unless Playing. */
        bool Pause();

        /** Resumes the world tick. Refused unless Paused. */
        bool Resume();

        /** Pauses if playing, resumes if paused. */
        bool TogglePause();

        /** Ticks one frame, then stays paused. Refused unless Paused. */
        bool Step();

        /**
         * Reactivates the edit world, then ends the game (which destroys the Play clone). In that order,
         * so no frame runs without an active world. Refused when already in Edit.
         */
        bool Stop();

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        EPlayState GetState() const noexcept { return m_State; }

        bool IsEdit()    const noexcept { return m_State == EPlayState::Edit; }
        bool IsPlaying() const noexcept { return m_State == EPlayState::Playing; }
        bool IsPaused()  const noexcept { return m_State == EPlayState::Paused; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldManager& m_Worlds;

        // The game session. Play/Stop are StartGame/EndGame with a world clone in between.
        IEngine& m_Engine;

        // Non-owning: WorldManager owns every world. Set by Play, cleared by Stop.
        World* m_EditWorld = nullptr;
        World* m_PlayWorld = nullptr;

        EPlayState m_State = EPlayState::Edit;
    };
}
