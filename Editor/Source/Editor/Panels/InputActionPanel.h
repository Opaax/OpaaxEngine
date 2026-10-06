#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Input/Assets/InputActionData.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(InputActionPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputActionPanel — the action editor: the action's fields and its modifiers.
    //   Shows no keys (mapping contexts bind keys). These modifiers apply to the sum of all bindings
    //   (e.g. Normalize clamps the WASD diagonal; on a binding it would do nothing).
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

        /** Name, value type, hold seconds, description, with undo. */
        void DrawFields(InputActionData& InData);

        /** The action's modifiers: add, remove, reorder, and each one's settings. */
        void DrawModifiers(InputActionData& InData);

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

        PanelWindowStyle GetWindowStyle() const override { return { { 380.f, 420.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // =============================================================================
        // The open edit gesture: the data as it was when the first field became active
        // =============================================================================
        InputActionData m_GestureBefore;
        bool            m_bGestureOpen   = false;
        bool            m_bWasItemActive = false;

        Int32 m_SelectedModifier = -1;
    };
}
