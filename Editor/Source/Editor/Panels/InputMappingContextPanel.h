#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Panels/IEditorPanel.h"

#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextData.h"   // the gesture caches an entry

namespace Opaax
{
    OPAAX_LOG_CATEGORY(InputMapPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputMappingContextPanel — the REBINDING editor: which keys reach which actions.
    //
    //   MoverPanel's shape: a list with add/remove/reorder, and a property fold over the selected
    //   row. Nothing here opens an action — a key changes, the action it drives does not, which is
    //   why the two are separate assets (**IM9**).
    //
    //   THE KEY COMBO IS HAND-DRAWN rather than left to the generic enum drawer, for one reason:
    //   EKeyCode reserves the gamepad range and nothing feeds it (**IN7**), so offering those
    //   would let an author pick a binding that is refused at load. The generic drawer cannot
    //   filter, so this one does — every other field still comes from DrawProperties.
    //
    //   `Add 2D Composite` is the authoring cost of IM5's design paid once: four bindings plus
    //   Negate/Swizzle is the correct way to build an Axis2D and a miserable thing to type.
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
        /** Name, dirty marker, Save and the context's priority. */
        void DrawHeader(const InputMappingContextData& InData);

        /** Add / Remove / Up / Down / Add 2D Composite, and the rows. */
        void DrawMappingList(const InputMappingContextData& InData);

        /** The selected row: action, key, consume, and its own modifier pipeline. */
        void DrawSelectedMapping(InputMappingContextData& InData);

        /** The filtered key dropdown — bindable codes only. @return true when it changed. */
        bool DrawKeyCombo(EKeyCode& InOutKey);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Nothing claimed, so nothing to release. */
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
        // The open edit gesture — the entry as it was when the first field went active
        // =============================================================================
        InputMappingEntry m_GestureBefore;
        Uint32            m_GestureIndex   = 0;
        bool              m_bGestureOpen   = false;
        bool              m_bWasItemActive = false;
    };
}
