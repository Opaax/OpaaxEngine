#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // PlayToolbarPanel — Play In Editor controls: Play / Pause / Step / Stop, plus the current state
    //   and which world is live. Holds no state: the buttons dispatch to EditorContext::PIE, the same
    //   object the F-keys use.
    // =============================================================================
    class PlayToolbarPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Play Controls);
        
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit PlayToolbarPanel(EditorContext& InContext);
        ~PlayToolbarPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        PlayToolbarPanel(const PlayToolbarPanel&)            = delete;
        PlayToolbarPanel& operator=(const PlayToolbarPanel&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        /** The four buttons; each is disabled when its action would be refused. */
        void DrawContents() override;

        /** Nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 360.f, 90.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;
    };
}
