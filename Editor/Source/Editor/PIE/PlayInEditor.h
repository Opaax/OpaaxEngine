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
    // EPlayState — where the editor is in the PIE cycle. Paused is a state, not a flag on
    //   Playing: Step is only legal from it, and the toolbar enables its buttons off this.
    // =============================================================================
    enum class EPlayState : Uint8
    {
        Edit,
        Playing,
        Paused
    };

    const char* ToString(EPlayState InState) noexcept;

    // =============================================================================
    // PlayInEditor — the PIE state machine (Editor.md D6). Owned by EditorService, referenced by
    //   EditorContext, exactly as EditorSelection is.
    //
    //   It is its own type because TWO front-ends drive it — the toolbar's buttons and
    //   EditorService::RouteInput's reserved keys (D5 step 3). Holding the state inside the panel
    //   would force EditorService to reach into a panel to answer a key press.
    //
    //   PIE = capture + re-instantiate, never a registry copy: Play CLONES the edit world into a
    //   Play world (WorldManager::CloneWorld) and activates it; the edit world stays alive and
    //   untouched, so Stop restores it by simply activating it again and destroying the clone.
    //   That is why restore costs nothing and needs no undo.
    //
    //   Every verb REFUSES loudly from a wrong state rather than no-op'ing silently — a Play that
    //   quietly did nothing is the kind of thing that gets debugged twice.
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
         * Start a GAME, then clone the ACTIVE (edit) world into a Play world and activate it.
         *
         * That order is the contract, not a preference: a world subsystem's context is built
         * inside CreateWorld, so the game instance has to exist before the clone is made or every
         * subsystem in it would be handed a session that is not there yet.
         *
         * The source is remembered so Stop can restore it. Refused unless the state is Edit and
         * there is an active world to clone; a failed clone ends the game it just started.
         *
         * @return true when the Play world is live.
         */
        bool Play();

        /** Suspend the world tick. Refused unless Playing. */
        bool Pause();

        /** Resume the world tick. Refused unless Paused. */
        bool Resume();

        /** Pause if playing, resume if paused — what one key can drive. */
        bool TogglePause();

        /** Tick exactly one more frame, then stay paused. Refused unless Paused. */
        bool Step();

        /**
         * Re-activate the edit world, then end the game — that order, so no frame ever runs
         * without an active world. Refused when already in Edit.
         *
         * EndGame is what destroys the clone: it destroys every PLAY world, and the edit world is
         * an Edit world. So the clone and the session go together, in the right order, and this
         * verb never names either of them.
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

        // The game bracket. PIE is one game session, so Play/Stop are StartGame/EndGame with a
        // world clone in between — the same two verbs a runtime host calls around its whole run.
        IEngine& m_Engine;

        // Non-owning: WorldManager owns every world (I5). Both are set by Play and cleared by
        // Stop, and are only ever dereferenced between the two.
        World* m_EditWorld = nullptr;
        World* m_PlayWorld = nullptr;

        EPlayState m_State = EPlayState::Edit;
    };
}
