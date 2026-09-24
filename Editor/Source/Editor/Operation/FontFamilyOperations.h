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
    // FamilyOps — the font family editor's VERBS, beside LibraryOps, ClipOps and SheetOps.
    //
    //   Each one mutates the open document and records its own undo step (**UN1**).
    //
    //   THE POLICY LIVES HERE, not in the undo steps and not in the panel: a FontFamilyEntry is
    //   CReflected, so the panel draws it with DrawProperties and gets four dropdowns and a typed
    //   drop target for free — but a property drawer writes straight through a reference and knows
    //   nothing about the rest of the family. CommitEntryEdit is where that edit is judged.
    //
    //   NO MoveEntry, unlike LibraryOps. Order is not meaningful here: FontFamilyData::Find scores
    //   every candidate and stops on an exact match, so which entry comes first only decides a tie —
    //   and a tie is two entries with the SAME style, which CommitEntryEdit refuses outright.
    // =============================================================================
    namespace FamilyOps
    {
        /**
         * Append an entry at InStyle, ready to have a `.ttf` dropped on it.
         *
         * @return false when nothing is open, or when that style is already in the family — adding a
         *   second entry for one style would make the first unreachable.
         */
        bool AddEntry(EditorContext& InContext, const FontStyleKey& InStyle);

        /** Remove the entry at InIndex. @return false when nothing is open or the index is past the end. */
        bool RemoveEntry(EditorContext& InContext, Uint32 InIndex);

        /**
         * Close an in-place edit of the entry at InIndex — the panel's DrawProperties gesture.
         *
         * ONE policy a property drawer cannot know: a style that ANOTHER entry already answers to is
         * REVERTED, because Find resolves by first match, so a duplicate makes one face permanently
         * unreachable. The library's duplicate-NAME rule, one axis wider.
         *
         * @param InBefore The entry as it was when the gesture opened.
         * @return true when a step was recorded — false when nothing changed, or the edit was reverted.
         */
        bool CommitEntryEdit(EditorContext& InContext, Uint32 InIndex, const FontFamilyEntry& InBefore);

        /**
         * Write the open family back to its file, rebase the dirty marker and publish it.
         *
         * @return false when nothing is open or the write failed.
         */
        bool Save(EditorContext& InContext);
    }
}
