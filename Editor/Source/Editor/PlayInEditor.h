#pragma once

#include "Core/OpaaxTypes.h"   // Uint8

namespace Opaax
{
    class World;
    class WorldManager;
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
        explicit PlayInEditor(WorldManager& InWorlds) noexcept : m_Worlds(InWorlds) {}

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
         * Clone the ACTIVE (edit) world into a Play world and activate the clone.
         *
         * The source is remembered so Stop can restore it. Refused unless the state is Edit and
         * there is an active world to clone.
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
         * Re-activate the edit world, then destroy the Play clone — that order, so no frame ever
         * runs without an active world. Refused when already in Edit.
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

        /** The world being authored — non-null only while a PIE session is live. */
        World* GetEditWorld() const noexcept { return m_EditWorld; }

        /** The Play clone — non-null only while a PIE session is live. */
        World* GetPlayWorld() const noexcept { return m_PlayWorld; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldManager& m_Worlds;

        // Non-owning: WorldManager owns every world (I5). Both are set by Play and cleared by
        // Stop, and are only ever dereferenced between the two.
        World* m_EditWorld = nullptr;
        World* m_PlayWorld = nullptr;

        EPlayState m_State = EPlayState::Edit;
    };
}
