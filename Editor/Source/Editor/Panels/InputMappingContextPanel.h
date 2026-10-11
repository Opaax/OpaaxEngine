#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Input/Assets/InputMappingContextData.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(InputMapPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputMappingContextPanel — the rebinding editor: which keys reach which actions. A list with
    //   add/remove/reorder and the fields of the selected row. The key combo is hand-drawn to hide
    //   gamepad codes (not supported yet). "Add 2D Composite" builds a WASD-style Axis2D in one click.
    // =============================================================================
    class InputMappingContextPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Input Mapping);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit InputMappingContextPanel(EditorContext& InContext);
        ~InputMappingContextPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        InputMappingContextPanel(const InputMappingContextPanel&)            = delete;
        InputMappingContextPanel& operator=(const InputMappingContextPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Name, dirty marker, Save and priority. */
        void DrawHeader(const InputMappingContextData& InData);

        /** Add / Remove / Up / Down / Add 2D Composite, and the rows. */
        void DrawMappingList(const InputMappingContextData& InData);

        /** The selected row: action, key, consume, and its modifiers. */
        void DrawSelectedMapping(InputMappingContextData& InData);

        /** The key dropdown (bindable codes only). @return True if it changed */
        bool DrawKeyCombo(EKeyCode& InOutKey);

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

        PanelWindowStyle GetWindowStyle() const override { return { { 460.f, 520.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        Int32 m_Selected         = -1;
        Int32 m_SelectedModifier = -1;

        // =============================================================================
        // The open edit gesture: the data as it was when the first field became active
        // =============================================================================
        InputMappingEntry m_GestureBefore;
        Uint32            m_GestureIndex   = 0;
        bool              m_bGestureOpen   = false;
        bool              m_bWasItemActive = false;
    };
}
