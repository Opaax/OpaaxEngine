#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The step the family editor's verbs record. UN1's shape, and SpriteSheetUndoables' path guard:
    // it carries the family's PATH, so an undo after opening a second family is a NO-OP WITH A
    // WARNING rather than a write into the wrong file.
    // =============================================================================

    /**
     * The whole ENTRY list was replaced — add, remove and repoint are all this.
     *
     * ONE type for three verbs because they are one edit to a reader: the list before, the list
     * after. The LABEL is the field that varies, so the Edit menu still names what happened.
     *
     * NO default field, unlike its animation sibling: a family has nothing to dangle. A style key
     * always has a value on all four axes, so there is no "which one stands in" to keep valid.
     */
    struct FontFamilyEntriesEdit
    {
        OpaaxString                 FamilyPath;
        TDynArray<FontFamilyEntry>  Before;
        TDynArray<FontFamilyEntry>  After;

        /** What the Edit menu shows — "Add Face", "Remove Face", "Edit Face". */
        const char* LabelText = "Edit Face List";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };
}
