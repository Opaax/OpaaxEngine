#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Movement/Assets/MoveModeData.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(MoveModePanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MoveModePanel — the tuning editor: the fields of one movement mode. Only the fields the
    //   selected mode uses are shown (MoveModeData holds every mode's fields).
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

        /** The fields, with undo, committed through MoveModeOps. */
        void DrawTuning(MoveModeData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 380.f, 400.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // =============================================================================
        // The open edit gesture: the data as it was when the first field became active
        // =============================================================================
        MoveModeData m_GestureBefore;
        bool         m_bGestureOpen   = false;
        bool         m_bWasItemActive = false;
    };
}
