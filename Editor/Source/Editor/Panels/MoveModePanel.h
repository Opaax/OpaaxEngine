#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"   // the gesture caches the data

namespace Opaax
{
    OPAAX_LOG_CATEGORY(MoveModePanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MoveModePanel — the tuning EDITOR: one mode's knobs, and nothing else.
    //
    //   The simplest document panel in the editor. A tuning has no list, no canvas and no preview,
    //   so this is a property fold over one struct plus a Save — which is exactly what makes the
    //   knobs editable at all, since M9's answer (an ImGui virtual inside the engine) is forbidden
    //   now (**MR2d**).
    //
    //   IT SHOWS ONLY THE KNOBS THE SELECTED MODE READS. MoveModeData carries every mode's fields
    //   because it is ONE resource type — a clip is one type too — and deciding which of them
    //   matter is a presentation question, so it is answered here rather than in the engine.
    // =============================================================================
    class MoveModePanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Move Mode);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit MoveModePanel(EditorContext& InContext);
        ~MoveModePanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        MoveModePanel(const MoveModePanel&)            = delete;
        MoveModePanel& operator=(const MoveModePanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker and Save. */
        void DrawHeader();

        /** The knobs, bracketed for undo and committed through MoveModeOps. */
        void DrawTuning(MoveModeData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — a tuning is floats. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Nothing claimed, so nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 380.f, 400.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // =============================================================================
        // The open edit gesture — the tuning as it was when the first field went active
        // =============================================================================
        MoveModeData m_GestureBefore;
        bool         m_bGestureOpen   = false;
        bool         m_bWasItemActive = false;
    };
}
