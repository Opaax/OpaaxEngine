#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // PlayToolbarPanel — the PIE controls (Editor.md D6). Play / Pause / Step / Stop, plus the
    //   current state and which world is live.
    //
    //   A PANEL, registered through Panels() like any other — NOT a menu (that route is M5) and not
    //   a privileged widget drawn by EditorService. It holds no state of its own: every button
    //   forwards to EditorContext::PIE, which is the same object the reserved keys drive, so the
    //   two front-ends can never disagree about what "playing" means.
    // =============================================================================
    class PlayToolbarPanel final : public IEditorPanel
    {
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
        /** Nothing to acquire — the panel reaches PIE through the context. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        /** The four buttons; each is disabled in the states where its verb would be refused. */
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
