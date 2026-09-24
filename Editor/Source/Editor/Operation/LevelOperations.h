#pragma once

#include "Core/OpaaxTypes.h"            // TFunction
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // LevelOps — the verbs that act on the OPEN LEVEL as a whole, as MapOps does for one named
    //   map of it. Both exist for the same reason: more than one call site needs the body, and a
    //   verb duplicated per call site is a verb that drifts.
    //
    //   Neither of these is a user command, which is why they are here and not in the command
    //   registry: AdoptOpen is boot plumbing (EditorService adopts what the engine already opened)
    //   and ConfirmDiscardingEdits answers a question rather than performing one.
    // =============================================================================
    namespace LevelOps
    {
        /**
         * Adopt the ACTIVE world's Level as the open document, and one of its mounted maps for
         * editing. The shared tail of boot, Open Level and Open Map, so none of them can disagree.
         *
         * THE FIRST NON-PERSISTENT MAP is the one edited, falling back to the persistent map when
         * that is the only one mounted: the persistent map is the shared backdrop authored once,
         * so the session opens on the content composed over it instead. Asked of the MOUNTED maps
         * rather than the manifest — a map that failed to load is not editable.
         *
         * @param InLevelAbsPath The `.opaaxlevel` behind it, or EMPTY for a world whose maps
         *   belong to no manifest (a standalone map). Empty is not "no document": a standalone
         *   map still gets a record, which is what keeps Save Map working without a manifest.
         */
        void AdoptOpen(EditorContext& InContext, const OpaaxString& InLevelAbsPath);

        /**
         * Confirm before an action that DESTROYS the world — the whole level's unsaved work, not
         * just the focused map's, since Save Level writes all of it (**MP9**). Modal because the
         * action is not undoable.
         *
         * Not needed for merely changing which map is focused: the baselines are per map and
         * survive a focus change (**MP5**), so nothing is at risk there.
         *
         * @param InOnConfirmed Runs when it is safe to proceed — the user said yes, or there was
         *   nothing at risk and no dialog was shown at all. It does NOT run on a refusal.
         *
         *   A CONTINUATION rather than a bool return, because the question goes through
         *   IEditorDialogs and an answer there is not required to arrive before the call returns.
         *   With the native backend it always does, so this still reads as a guard.
         */
        void ConfirmDiscardingEdits(EditorContext& InContext, TFunction<void()> InOnConfirmed);
    }
}
