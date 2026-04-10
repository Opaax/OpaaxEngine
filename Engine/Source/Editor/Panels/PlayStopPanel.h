#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"

namespace Opaax
{
    class SceneManager;
}

namespace Opaax::Editor
{
    // =============================================================================
    // PlayStopPanel
    //
    // Toolbar with Play / Stop buttons.
    //
    // Play  — saves current scene state, sets editor to "playing" mode.
    //         Scene Update/FixedUpdate are driven normally.
    // Stop  — restores saved scene state, returns to "editing" mode.
    //         Scene is deserialized from the temp save.
    //
    // NOTE: We serialize to a temp file on Play and restore on Stop.
    //   This is the simplest correct approach — no need for a deep copy of the World.
    // =============================================================================
    class PlayStopPanel
    {
    public:
        PlayStopPanel()  = default;
        ~PlayStopPanel() = default;

        void Draw(SceneManager* InSceneManager);

        FORCEINLINE bool IsPlaying() const noexcept { return m_bPlaying; }

    private:
        void OnPlay (SceneManager* InSceneManager);
        void OnStop (SceneManager* InSceneManager);

        bool m_bPlaying = false;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR