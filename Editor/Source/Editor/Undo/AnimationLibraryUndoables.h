#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo steps recorded by the library editor's actions.
    // Every step carries the library's path: undoing after opening another library does nothing and warns,
    // instead of writing into the wrong file.
    // =============================================================================

    /**
     * The whole entry list was replaced (add, remove, rename, repoint, reorder). The label says which.
     */
    struct LibraryEntriesEdit
    {
        OpaaxString                      LibraryPath;
        TDynArray<AnimationLibraryEntry> Before;
        TDynArray<AnimationLibraryEntry> After;

        /**
         * The default is in the same step: renaming or removing the default entry changes it, and one
         * Ctrl+Z must restore both.
         */
        OpaaxStringID BeforeDefault;
        OpaaxStringID AfterDefault;

        /** What the Edit menu shows: "Add Clip", "Remove Clip", "Rename Clip"... */
        const char* LabelText = "Edit Clip List";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /** The clip played when a component names none. */
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
