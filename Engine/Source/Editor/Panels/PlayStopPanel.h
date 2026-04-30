#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Editor/IEditorPanel.h"

namespace Opaax
{
    class SceneManager;
}

namespace Opaax::Editor
{
    /**
     * @class PlayStopPanel
     * Toolbar with Play / Stop buttons.
     * Play  — serialises the scene to a temp file and enters play mode.
     * Stop  — deserializes from the temp file and returns to edit mode.
     *
     * NOTE: Draw(SceneManager*) is called directly by EditorSubsystem.
     */
    class PlayStopPanel : public IEditorPanel
    {
        // =============================================================================
        // CTRO - DTOR
        // =============================================================================
    public:
        PlayStopPanel()  = default;
        ~PlayStopPanel() = default;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void OnPlay (SceneManager* InSceneManager);
        void OnStop (SceneManager* InSceneManager);

        //------------------------------------------------------------------------------
    public:
        void Draw(SceneManager* InSceneManager);

        //------------------------------------------------------------------------------
        //Get-Set
        
        FORCEINLINE bool IsPlaying() const noexcept { return m_bPlaying; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IEditorPanel interface
    public:
        const char* GetPanelName() const override { return "Play/Stop"; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool m_bPlaying = false;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR