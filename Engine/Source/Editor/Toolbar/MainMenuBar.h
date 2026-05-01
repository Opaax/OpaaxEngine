#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Editor/IEditorPanel.h"

namespace Opaax
{
    class EditorSubsystem;
}

namespace Opaax::Editor
{
    /**
     * @class MainMenuBar
     * Top-level ImGui main menu bar (File / Edit / View / Window / Help).
     * Drawn first per frame from EditorSubsystem::DrawPanels.
     *
     * NOTE: Save/Open/SaveAs/New are stubbed in M2 Step 1 — wired in M2 Step 3.
     */
    class OPAAX_API MainMenuBar : public IEditorPanel
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        MainMenuBar()  = default;
        ~MainMenuBar() = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void Draw(EditorSubsystem& Owner);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IEditorPanel interface
    public:
        const char* GetPanelName() const override { return "MainMenuBar"; }
        //~End IEditorPanel interface

        // =============================================================================
        // Internals
        // =============================================================================
    private:
        void DrawFileMenu  (EditorSubsystem& Owner);
        void DrawEditMenu  ();
        void DrawViewMenu  (EditorSubsystem& Owner);
        void DrawWindowMenu();
        void DrawHelpMenu  ();
        void DrawAboutModal();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool m_bOpenAboutModal = false;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
