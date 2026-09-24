#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The step the UI canvas editor's verbs record (**UI15**). UN1's shape, with
    // SpriteSheetUndoables' path guard.
    // =============================================================================

    /**
     * The whole TREE was replaced — add, remove, reparent and a property edit are all this.
     *
     * ONE type for every verb, and for a TREE that is not merely convenient: a step that restored
     * PART of a tree could leave a parent pointing at a child that no longer exists. Text before,
     * text after, and the reader rebuilds from scratch — the only shape that cannot half-restore.
     * The LABEL is the field that varies, so the Edit menu still names what happened.
     *
     * It carries the canvas's PATH: an undo after opening a second `.opaaxui` is a NO-OP WITH A
     * WARNING rather than a write into the wrong document.
     */
    struct UITreeEdit
    {
        OpaaxString CanvasPath;
        OpaaxString Before;
        OpaaxString After;

        /** What the Edit menu shows — "Add Widget", "Delete Widget", "Reparent", "Edit Widget". */
        const char* LabelText = "Edit UI";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };
}
