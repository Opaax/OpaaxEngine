#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Input/Assets/InputActionData.h"
#include "Input/Assets/InputMappingContextData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo steps recorded by the input editors' actions.
    // Every step carries the asset's path: undoing after opening another asset does nothing and warns,
    // instead of writing into the wrong file.
    // =============================================================================

    /**
     * An action was edited: the whole struct, before and after. The label says what changed.
     */
    struct InputActionEdit
    {
        OpaaxString     ActionPath;
        InputActionData Before;
        InputActionData After;

        const char* LabelText = "Edit Input Action";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /**
     * The whole mapping list was replaced (add, remove, rebind, reorder, 2D composite).
     */
    struct InputMappingsEdit
    {
        OpaaxString                  MapPath;
        TDynArray<InputMappingEntry> Before;
        TDynArray<InputMappingEntry> After;

        /** What the Edit menu shows: "Add Mapping", "Remove Mapping", "Add 2D Composite"... */
        const char* LabelText = "Edit Mappings";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /** The priority the whole context is pushed at. */
    struct InputMapPriorityEdit
    {
        OpaaxString MapPath;
        Int32       Before = 0;
        Int32       After  = 0;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Set Context Priority"; }
    };
}
