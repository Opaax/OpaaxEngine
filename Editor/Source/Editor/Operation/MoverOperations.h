#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    struct MoverEntry;
    struct MoveModeData;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MoverOps — the mover editor's VERBS, beside LibraryOps whose shape they take.
    //
    //   Each one mutates the open document and records its own undo step (**UN1**).
    //
    //   THE POLICY LIVES HERE, not in the undo steps and not in the panel: a MoverEntry is
    //   CReflected, so the panel draws it with DrawProperties and gets the name field and the typed
    //   drop target for free — but a property drawer writes straight through a reference and knows
    //   nothing about the rest of the mover. CommitEntryEdit is where that edit is judged.
    // =============================================================================
    namespace MoverOps
    {
        /** Append an empty entry, ready to have a tuning dropped on it. @return false when none is open. */
        bool AddEntry(EditorContext& InContext);

        /** Remove the entry at InIndex. @return false when nothing is open or the index is past the end. */
        bool RemoveEntry(EditorContext& InContext, Uint32 InIndex);

        /**
         * Move the entry at InIndex by InDelta places, clamped to the list.
         *
         * Order is not cosmetic here: a mover with no mode named falls back to the FIRST entry.
         *
         * @return false when the move would leave it where it is.
         */
        bool MoveEntry(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Close an in-place edit of the entry at InIndex — the panel's DrawProperties gesture.
         *
         * Two policies, both of which a property drawer cannot know:
         * - A name that ANOTHER entry already answers to is REVERTED, because the runtime resolves
         *   a mode name by FIRST match, so a duplicate makes one mode permanently unreachable.
         * - An entry that gained a tuning but has no name is NAMED FROM THE FILE STEM (**I13**), so
         *   dropping one in is one action rather than two. Skipped if that name is taken.
         *
         * @param InBefore The entry as it was when the gesture opened.
         * @return true when a step was recorded — false when nothing changed, or the edit was reverted.
         */
        bool CommitEntryEdit(EditorContext& InContext, Uint32 InIndex, const MoverEntry& InBefore);

        /** Which mode a mover with no opinion starts in. @return false when nothing changed. */
        bool SetDefaultMode(EditorContext& InContext, OpaaxStringID InName);

        /**
         * Write the open mover back to its file, rebase the dirty marker and publish it.
         *
         * @return false when nothing is open or the write failed.
         */
        bool Save(EditorContext& InContext);
    }

    // =============================================================================
    // MoveModeOps — the tuning editor's verbs. Two, because a tuning has no structure to edit.
    // =============================================================================
    namespace MoveModeOps
    {
        /**
         * Close an in-place edit of the open tuning — the panel's DrawProperties gesture.
         *
         * @param InBefore The data as it was when the gesture opened.
         * @return true when a step was recorded; false when the fold changed nothing.
         */
        bool CommitEdit(EditorContext& InContext, const MoveModeData& InBefore);

        /**
         * Write the open tuning back to its file, rebase the dirty marker and publish it.
         *
         * @return false when nothing is open or the write failed.
         */
        bool Save(EditorContext& InContext);
    }
}
