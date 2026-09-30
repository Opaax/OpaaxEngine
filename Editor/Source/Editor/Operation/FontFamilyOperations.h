#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    struct FontFamilyEntry;
    struct FontStyleKey;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // FamilyOps — the font family editor's actions. Each changes the open document and records its
    //   own undo step. The panel draws entries with DrawProperties; CommitEntryEdit checks the result
    //   against the rest of the family. No MoveEntry: order does not matter (Find picks the exact
    //   style match, and duplicate styles are refused).
    // =============================================================================
    namespace FamilyOps
    {
        /**
         * Adds an entry with InStyle, ready for a .ttf drop.
         * @return False when nothing is open, or the style is already in the family
         */
        bool AddEntry(EditorContext& InContext, const FontStyleKey& InStyle);

        /** Removes the entry at InIndex. @return False when nothing is open or the index is out of range */
        bool RemoveEntry(EditorContext& InContext, Uint32 InIndex);

        /**
         * Closes an in-place edit of the entry at InIndex (the panel's DrawProperties). A style another
         * entry already has is reverted (Find uses the first match, so a duplicate would be unreachable).
         * @param InBefore The entry when the edit started
         * @return True if a step was recorded. False if nothing changed or the edit was reverted
         */
        bool CommitEntryEdit(EditorContext& InContext, Uint32 InIndex, const FontFamilyEntry& InBefore);

        /**
         * Writes the open family to its file, updates the dirty baseline, and reloads the resource.
         * @return False when nothing is open or the write failed
         */
        bool Save(EditorContext& InContext);
    }
}
