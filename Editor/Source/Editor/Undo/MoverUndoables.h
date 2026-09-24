#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The steps the mover editors' verbs record. UN1's shape, and the library's path guard: every
    // step carries its document's PATH, so an undo after opening a second asset is a NO-OP WITH A
    // WARNING rather than a write into the wrong file.
    // =============================================================================

    /**
     * The whole ENTRY list was replaced — add, remove, rename, repoint and reorder are all this.
     *
     * One type for five verbs because they are one edit to a reader: the list before, the list
     * after. The LABEL is the field that varies, so the Edit menu still names what happened.
     */
    struct MoverEntriesEdit
    {
        OpaaxString           MoverPath;
        TDynArray<MoverEntry> Before;
        TDynArray<MoverEntry> After;

        /**
         * The default rides in the SAME step, because the edits that dangle it are the edits to the
         * list: renaming the default entry orphans it, removing it deletes it. One Ctrl+Z has to
         * put BOTH back — `LibraryEntriesEdit` set this precedent and `SheetOps::Slice` before it.
         */
        OpaaxStringID BeforeDefault;
        OpaaxStringID AfterDefault;

        /** What the Edit menu shows — "Add Mode", "Remove Mode", "Rename Mode"… */
        const char* LabelText = "Edit Mode List";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /** Which mode a mover with no opinion starts in. */
    struct MoverDefaultMode
    {
        OpaaxString   MoverPath;
        OpaaxStringID Before;
        OpaaxStringID After;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Set Default Mode"; }
    };

    /**
     * A tuning was edited — the WHOLE struct, before and after.
     *
     * One step for every knob, unlike the library's five verbs, because a tuning has no structure
     * to edit: it is a flat fold of floats, so "which field changed" is not a distinction the Edit
     * menu could usefully make. The data is small enough that carrying two copies costs nothing.
     */
    struct MoveModeEdit
    {
        OpaaxString  ModePath;
        MoveModeData Before;
        MoveModeData After;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Edit Move Mode"; }
    };
}
