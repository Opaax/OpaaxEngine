#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/Types/AnimationLibraryData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The steps the library editor's verbs record. UN1's shape, and SpriteSheetUndoables' path
    // guard: every step carries the library's PATH, so an undo after opening a second library is a
    // NO-OP WITH A WARNING rather than a write into the wrong file.
    // =============================================================================

    /**
     * The whole ENTRY list was replaced — add, remove, rename, repoint and reorder are all this.
     *
     * One type for five verbs because they are one edit to a reader: the list before, the list
     * after. The LABEL is the field that varies, so the Edit menu still names what happened.
     */
    struct LibraryEntriesEdit
    {
        OpaaxString                      LibraryPath;
        TDynArray<AnimationLibraryEntry> Before;
        TDynArray<AnimationLibraryEntry> After;

        /**
         * The default rides in the SAME step, because the edits that dangle it are the edits to the
         * list: renaming the default entry orphans it, removing it deletes it. `SheetOps::Slice`
         * set this precedent for `DefaultFrame` — one Ctrl+Z has to put BOTH back, or undoing a
         * rename leaves a default naming something that is no longer there.
         */
        OpaaxStringID BeforeDefault;
        OpaaxStringID AfterDefault;

        /** What the Edit menu shows — "Add Clip", "Remove Clip", "Rename Clip"… */
        const char* LabelText = "Edit Clip List";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /** Which clip a component with no opinion plays. */
    struct LibraryDefaultClip
    {
        OpaaxString   LibraryPath;
        OpaaxStringID Before;
        OpaaxStringID After;

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return "Set Default Clip"; }
    };
}
