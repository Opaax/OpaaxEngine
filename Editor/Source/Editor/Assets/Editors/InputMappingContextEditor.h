#pragma once

#include "Core/OpaaxTypes.h"
#include "Editor/Assets/IAssetEditor.h"
#include "Input/InputCodes.h"   // EKeyCode

namespace Opaax
{
    class InputMappingContextAsset;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputMappingContextEditor — the rebinding view of an input mapping context: one row per mapping
    //   (its key, then its action), add/remove/reorder, and the fields of the selected one. The key
    //   combo leaves out gamepad codes (no gamepad input yet); "2D Composite" turns the selected
    //   mapping into a WASD-style Axis2D. The asset window records each change as an undo step.
    // =============================================================================
    class InputMappingContextEditor final : public IAssetEditor
    {
    public:
        explicit InputMappingContextEditor(EditorContext& InContext) : m_Context(InContext) {}

        //~Begin IAssetEditor interface
        void DrawContents(AssetDocument& InDocument) override;

        Vector2F GetDefaultSize() const override { return { 520.f, 600.f }; }
        //~End IAssetEditor interface

    private:
        /** Every property but the mappings (the priority). */
        void DrawHeader(InputMappingContextAsset& InAsset);

        /** Add / Remove / Up / Down / 2D Composite, and the rows. */
        void DrawMappingList(InputMappingContextAsset& InAsset);

        /** The selected mapping's fields, in their declared order. */
        void DrawSelectedMapping(InputMappingContextAsset& InAsset);

        /** The key dropdown, bindable codes only. */
        void DrawKeyCombo(EKeyCode& InOutKey);

        EditorContext& m_Context;

        /** The selected row, or -1. */
        Int32 m_Selected = -1;
    };
}
