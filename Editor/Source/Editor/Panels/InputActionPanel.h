#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Engine/Subsystems/Resources/Types/Input/InputActionData.h"   // the gesture caches the data

namespace Opaax
{
    OPAAX_LOG_CATEGORY(InputActionPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputActionPanel — the ACTION editor: what an action is, and what its total is scaled by.
    //
    //   MoveModePanel's shape — a property fold over one struct plus a Save — with one list, the
    //   action-level modifiers.
    //
    //   IT SHOWS NO KEYS, deliberately. Which keys reach an action is a mapping context's
    //   business (**IM9**), which is exactly why rebinding never opens this file.
    //
    //   THE MODIFIER LIST IS NOT THE SAME LIST THE MAPPINGS HAVE. These apply to the SUM of every
    //   binding that fed the action, which is the only level a diagonal can be clamped at
    //   (**IM5**) — a `Normalize` here is what stops WASD being 1.41x faster on the diagonal, and
    //   the same modifier on a binding would do nothing at all.
    // =============================================================================
    class InputActionPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Input Action);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit InputActionPanel(EditorContext& InContext);
        ~InputActionPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        InputActionPanel(const InputActionPanel&)            = delete;
        InputActionPanel& operator=(const InputActionPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker and Save. */
        void DrawHeader();

        /** Name, value type, hold seconds, description — bracketed for undo. */
        void DrawFields(InputActionData& InData);

        /** The action-level modifier pipeline: add, remove, reorder, and each one's knobs. */
        void DrawModifiers(InputActionData& InData);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — an action is a name and a few numbers. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Nothing claimed, so nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 380.f, 420.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // =============================================================================
        // The open edit gesture — the action as it was when the first field went active
        // =============================================================================
        InputActionData m_GestureBefore;
        bool            m_bGestureOpen   = false;
        bool            m_bWasItemActive = false;

        Int32 m_SelectedModifier = -1;
    };
}
