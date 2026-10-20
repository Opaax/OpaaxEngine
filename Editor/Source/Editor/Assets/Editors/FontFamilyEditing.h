#pragma once

#include "Core/OpaaxTypes.h"
#include "Renderer/Text/FontFamily.h"

namespace Opaax::Editor
{
    // =============================================================================
    // FamilyEditing — what the font family editor does to a family's faces, as pure functions so tests
    //   can check them without a window. A family holds each style once: of two equal styles the first
    //   is drawn, so the second could never be.
    // =============================================================================
    namespace FamilyEditing
    {
        /** The index of the entry with exactly InStyle, or -1. */
        inline Int32 FindEntry(const TDynArray<FontFamilyEntry>& InEntries, const FontStyleKey& InStyle)
        {
            for (Uint32 lIndex = 0; lIndex < InEntries.size(); ++lIndex)
            {
                if (InEntries[lIndex].Style == InStyle)
                {
                    return static_cast<Int32>(lIndex);
                }
            }

            return -1;
        }

        /** Whether an entry other than the one at InSkip has InStyle. */
        inline bool IsStyleTaken(const TDynArray<FontFamilyEntry>& InEntries, const FontStyleKey& InStyle,
                                 const Uint32 InSkip)
        {
            for (Uint32 lIndex = 0; lIndex < InEntries.size(); ++lIndex)
            {
                if (lIndex != InSkip && InEntries[lIndex].Style == InStyle)
                {
                    return true;
                }
            }

            return false;
        }

        /**
         * Adds an entry for InStyle with no face yet: an empty cell of the matrix, clicked.
         * @return Its index, or -1 when the family already has that style (nothing added)
         */
        inline Int32 AddEntry(TDynArray<FontFamilyEntry>& InOutEntries, const FontStyleKey& InStyle)
        {
            if (FindEntry(InOutEntries, InStyle) >= 0)
            {
                return -1;
            }

            FontFamilyEntry lEntry;
            lEntry.Style = InStyle;
            InOutEntries.emplace_back(Move(lEntry));

            return static_cast<Int32>(InOutEntries.size()) - 1;
        }

        /**
         * Puts the style of the entry at InIndex back to InStyleBefore when an edit gave it a style
         * another entry has.
         * @return True when it was put back
         */
        inline bool RevertTakenStyle(TDynArray<FontFamilyEntry>& InOutEntries, const Uint32 InIndex,
                                     const FontStyleKey& InStyleBefore)
        {
            if (InIndex >= InOutEntries.size() || !IsStyleTaken(InOutEntries, InOutEntries[InIndex].Style, InIndex))
            {
                return false;
            }

            InOutEntries[InIndex].Style = InStyleBefore;
            return true;
        }
    }
}
