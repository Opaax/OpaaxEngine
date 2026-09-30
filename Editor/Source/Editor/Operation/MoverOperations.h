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
    // MoverOps — the mover editor's actions. Each changes the open document and records its own
    //   undo step. The panel draws entries with DrawProperties; CommitEntryEdit checks the result
    //   against the rest of the mover.
    // =============================================================================
    namespace MoverOps
    {
        /** Adds an empty entry, ready for a tuning drop. @return False when nothing is open */
        bool AddEntry(EditorContext& InContext);

        /** Removes the entry at InIndex. @return False when nothing is open or the index is out of range */
        bool RemoveEntry(EditorContext& InContext, Uint32 InIndex);

        /**
         * Moves the entry at InIndex by InDelta places, clamped to the list. Order matters: a mover with
         * no mode named uses the first entry.
         * @return False when it would not move
         */
        bool MoveEntry(EditorContext& InContext, Uint32 InIndex, Int32 InDelta);

        /**
         * Closes an in-place edit of the entry at InIndex (the panel's DrawProperties).
         * - A name another entry already has is reverted (names resolve by first match).
         * - An entry that gained a tuning but has no name is named from the file stem, unless taken.
         * @param InBefore The entry when the edit started
         * @return True if a step was recorded. False if nothing changed or the edit was reverted
         */
        bool CommitEntryEdit(EditorContext& InContext, Uint32 InIndex, const MoverEntry& InBefore);

        /** The mode a mover starts in when none is named. @return False when nothing changed */
        bool SetDefaultMode(EditorContext& InContext, OpaaxStringID InName);

        /**
         * Writes the open mover to its file, updates the dirty baseline, and reloads the resource.
         * @return False when nothing is open or the write failed
         */
        bool Save(EditorContext& InContext);
    }

    // =============================================================================
    // MoveModeOps — the tuning editor's actions.
    // =============================================================================
    namespace MoveModeOps
    {
        /**
         * Closes an in-place edit of the open tuning (the panel's DrawProperties).
         * @param InBefore The data when the edit started
         * @return True if a step was recorded, false if nothing changed
         */
        bool CommitEdit(EditorContext& InContext, const MoveModeData& InBefore);

        /**
         * Writes the open tuning to its file, updates the dirty baseline, and reloads the resource.
         * @return False when nothing is open or the write failed
         */
        bool Save(EditorContext& InContext);
    }
}
