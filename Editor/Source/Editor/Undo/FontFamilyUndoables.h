#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo step recorded by the font family editor's actions.
    // Every step carries the family's path: undoing after opening another family does nothing and warns,
    // instead of writing into the wrong file.
    // =============================================================================

    /**
     * The whole entry list was replaced (add, remove, repoint). The label says which.
     */
    struct FontFamilyEntriesEdit
    {
        OpaaxString                 FamilyPath;
        TDynArray<FontFamilyEntry>  Before;
        TDynArray<FontFamilyEntry>  After;

        /** What the Edit menu shows: "Add Face", "Remove Face", "Edit Face". */
        const char* LabelText = "Edit Face List";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };
}
