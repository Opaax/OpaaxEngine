#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Movement/Assets/MoveModeData.h"
#include "Movement/Assets/MoverData.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The undo steps recorded by the mover editors' actions.
    // Every step carries the asset's path: undoing after opening another asset does nothing and warns,
    // instead of writing into the wrong file.
    // =============================================================================

    /**
     * The whole entry list was replaced (add, remove, rename, repoint, reorder). The label says which.
     */
    struct MoverEntriesEdit
    {
        OpaaxString           MoverPath;
        TDynArray<MoverEntry> Before;
        TDynArray<MoverEntry> After;

        /**
         * The default is in the same step: renaming or removing the default entry changes it, and one
         * Ctrl+Z must restore both.
         */
        OpaaxStringID BeforeDefault;
        OpaaxStringID AfterDefault;

        /** What the Edit menu shows: "Add Mode", "Remove Mode", "Rename Mode"... */
        const char* LabelText = "Edit Mode List";

        void        Undo(EditorContext& InContext);
        void        Redo(EditorContext& InContext);
        const char* Label() const noexcept { return LabelText; }
    };

    /** The mode a mover starts in when none is named. */
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
     * A tuning was edited: the whole struct, before and after (a flat set of floats).
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
