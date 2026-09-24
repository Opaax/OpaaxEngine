#pragma once

#include "Editor/Imgui/Configs/EditorImguiConfigData.h"

namespace Opaax::Editor
{
    class IEditorWidgets;

    // =============================================================================
    // EditorImguiConfigDrawer — the hand-written UI for EditorImguiConfigData.
    //
    //   WHAT IT BUYS: grouping (Text / Window / …) over a FLAT data type. Nesting the fields into
    //   structs would group them too, but it would nest the .config file with them — and how a
    //   theme reads on screen is not a reason to change what it looks like on disk. Presentation is
    //   exactly what a custom drawer is for; the generic fold cannot express it because the group
    //   headers it emits come from the data's own shape.
    //
    //   It costs the fold's other half: a field added to EditorImguiConfigData does NOT appear here
    //   on its own. Add it to a group below, or it is invisible.
    //
    //   DUCK-TYPED, no base class (D7): default-constructible, and callable as
    //   Draw(IEditorWidgets&, EditorImguiConfigData&). The registry only ever CALLS it, so the body
    //   lives in the .cpp and this header names no backend.
    // =============================================================================
    struct EditorImguiConfigDrawer
    {
        // =============================================================================
        // Functions
        // =============================================================================
        /** Registry contract — the fields, then the block-level actions. */
        void Draw(IEditorWidgets& InWidgets, EditorImguiConfigData& InData);
    };
}
