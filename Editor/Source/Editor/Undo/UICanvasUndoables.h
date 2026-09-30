#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo step recorded by the UI canvas editor's actions.
    // =============================================================================

    /**
     * The whole tree was replaced (add, remove, reparent, property edit). Stored as text before and
     * after, and rebuilt from scratch, so a tree is never half restored. The label says which edit.
     * Carries the canvas path: undoing after opening another canvas does nothing and warns.
     */
    struct UITreeEdit
    {
        OpaaxString CanvasPath;
        OpaaxString Before;
        OpaaxString After;

        /** What the Edit menu shows: "Add Widget", "Delete Widget", "Reparent", "Edit Widget". */
        const char* LabelText = "Edit UI";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };
}
