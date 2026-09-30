#pragma once

#include "Editor/Imgui/Configs/EditorImguiConfigData.h"

namespace Opaax::Editor
{
    class IEditorWidgets;

    // =============================================================================
    // EditorImguiConfigDrawer — the hand-written UI for EditorImguiConfigData: groups (Text, Window...)
    //   over a flat data type, so the .config file stays flat. A field added to the data does not
    //   appear here automatically: add it to a group.
    //   No base class: default-constructible, callable as Draw(IEditorWidgets&, EditorImguiConfigData&).
    // =============================================================================
    struct EditorImguiConfigDrawer
    {
        // =============================================================================
        // Functions
        // =============================================================================
        /** Draws the fields, then the block-level actions. */
        void Draw(IEditorWidgets& InWidgets, EditorImguiConfigData& InData);
    };
}
