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
    // LibraryOps — the animation library editor's actions. Each changes the open document and
    //   records its own undo step. The panel draws entries with DrawProperties; CommitEntryEdit
    //   checks the result against the rest of the library.
    // =============================================================================
    namespace LibraryOps
    {
        /** Adds an empty entry, ready for a clip drop. @return False when nothing is open */
        bool AddEntry(EditorContext& InContext);

        /** Removes the entry at InIndex. @return False when nothing is open or the index is out of range */
        bool RemoveEntry(EditorContext& InContext, Uint32 InIndex);

        /**
         * Moves the entry at InIndex by InDelta places, clamped to the list. Order matters: an animator
         * with no clip named plays the first entry.
         * @return False when it would not move
         */
        bool MoveEntry(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Closes an in-place edit of the entry at InIndex (the panel's DrawProperties).
         * - A name another entry already has is reverted (names resolve by first match).
         * - An entry that gained a clip but has no name is named from the file stem, unless taken.
         * @param InBefore The entry when the edit started
         * @return True if a step was recorded. False if nothing changed or the edit was reverted
         */
        bool CommitEntryEdit(EditorContext& InContext, Uint32 InIndex, const AnimationLibraryEntry& InBefore);

        /** The clip played when a component names none. @return False when nothing changed */
        bool SetDefaultClip(EditorContext& InContext, OpaaxStringID InName);

        /**
         * Writes the open library to its file, updates the dirty baseline, and reloads the resource.
         * @return False when nothing is open or the write failed
         */
        bool Save(EditorContext& InContext);
    }
}
