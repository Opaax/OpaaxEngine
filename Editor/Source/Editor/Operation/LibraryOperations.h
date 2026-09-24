#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    struct AnimationLibraryEntry;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // LibraryOps — the animation library editor's VERBS, beside ClipOps and SheetOps.
    //
    //   Each one mutates the open document and records its own undo step (**UN1**).
    //
    //   THE POLICY LIVES HERE, not in the undo steps and not in the panel: an
    //   AnimationLibraryEntry is CReflected, so the panel draws it with DrawProperties and gets the
    //   name field and the typed drop target for free — but a property drawer writes straight
    //   through a reference and knows nothing about the rest of the library. CommitEntryEdit is
    //   where that edit is judged.
    // =============================================================================
    namespace LibraryOps
    {
        /** Append an empty entry, ready to have a clip dropped on it. @return false when none is open. */
        bool AddEntry(EditorContext& InContext);

        /** Remove the entry at InIndex. @return false when nothing is open or the index is past the end. */
        bool RemoveEntry(EditorContext& InContext, Uint32 InIndex);

        /**
         * Move the entry at InIndex by InDelta places, clamped to the list.
         *
         * Order is not cosmetic here: an animator with no clip named falls back to the FIRST entry.
         *
         * @return false when the move would leave it where it is.
         */
        bool MoveEntry(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Close an in-place edit of the entry at InIndex — the panel's DrawProperties gesture.
         *
         * Two policies, both of which a property drawer cannot know:
         * - A name that ANOTHER entry already answers to is REVERTED, because the runtime resolves
         *   a clip name by FIRST match, so a duplicate makes one clip permanently unreachable.
         * - An entry that gained a clip but has no name is NAMED FROM THE FILE STEM (**I13**), so
         *   dropping a clip in is one action rather than two. Skipped if that name is taken.
         *
         * @param InBefore The entry as it was when the gesture opened.
         * @return true when a step was recorded — false when nothing changed, or the edit was reverted.
         */
        bool CommitEntryEdit(EditorContext& InContext, Uint32 InIndex, const AnimationLibraryEntry& InBefore);

        /** Which clip a component with no opinion plays. @return false when nothing changed. */
        bool SetDefaultClip(EditorContext& InContext, OpaaxStringID InName);

        /**
         * Write the open library back to its file, rebase the dirty marker and publish it.
         *
         * @return false when nothing is open or the write failed.
         */
        bool Save(EditorContext& InContext);
    }
}
