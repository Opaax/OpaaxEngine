#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Engine/Subsystems/Resources/Types/Input/InputActionData.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The steps the input editors' verbs record. UN1's shape, and the mover editors' path guard:
    // every step carries its document's PATH, so an undo after opening a second asset is a NO-OP
    // WITH A WARNING rather than a write into the wrong file.
    // =============================================================================

    /**
     * An ACTION was edited — the whole struct, before and after.
     *
     * One step for every field, like MoveModeEdit and for its reason: an action is a flat fold
     * plus a short modifier list, so "which field changed" is not a distinction the Edit menu
     * could usefully make. The LABEL varies so add/remove of a modifier still reads correctly.
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
     * The whole MAPPING list was replaced — add, remove, rebind, reorder and the composite verb
     * are all this.
     *
     * One type for every list verb because they are one edit to a reader: the list before, the
     * list after. MoverEntriesEdit set the precedent.
     */
    struct InputMappingsEdit
    {
        OpaaxString                  MapPath;
        TDynArray<InputMappingEntry> Before;
        TDynArray<InputMappingEntry> After;

        /** What the Edit menu shows — "Add Mapping", "Remove Mapping", "Add 2D Composite"… */
        const char* LabelText = "Edit Mappings";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /** Which priority the whole context is pushed at. Its own step: it is not a list edit. */
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
