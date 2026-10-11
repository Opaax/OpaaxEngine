#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"
#include "Renderer/Text/FontStyle.h"

namespace Opaax
{
    struct FontFaceResource;

    // =============================================================================
    // FontFamilyData — every face of a typeface, by style (.opaaxfont).
    //   Lets a component ask for "Roboto, greek, medium, italic" instead of a .ttf path.
    //   A component can also name a single .ttf directly.
    // =============================================================================

    /** One face: its style, and its file. */
    struct FontFamilyEntry
    {
        FontStyleKey Style;

        /** Asset-relative ("Fonts/Roboto/roboto-latin-400-normal.ttf") or a mount. */
        TResourcePath<FontFaceResource> Face;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(FontFamilyEntry, Style, Face)

        // Reflected for the family editor (Style shows as four dropdowns).
        OPAAX_PROPERTIES(FontFamilyEntry,
                         OPAAX_PROP(Style),
                         OPAAX_PROP(Face).SetTooltip("The .ttf this cut resolves to.\n"
                                                     "Drag one from the Resource Browser."))
    };

    struct FontFamilyData
    {
        TDynArray<FontFamilyEntry> Entries;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(FontFamilyData, Entries)

        Uint32 EntryCount() const noexcept { return static_cast<Uint32>(Entries.size()); }

        /** The entry matching InKey exactly, or nullptr. */
        const FontFamilyEntry* FindExact(const FontStyleKey& InKey) const noexcept
        {
            for (const FontFamilyEntry& lEntry : Entries)
            {
                if (lEntry.Style == InKey)
                {
                    return &lEntry;
                }
            }

            return nullptr;
        }

        /**
         * The best face for InKey, or nullptr if the family has nothing for that script.
         * The script never falls back; the other axes do, in CSS order: width, then slant, then weight.
         * Compare the returned Style with InKey to know if the match was exact.
         */
        const FontFamilyEntry* Find(const FontStyleKey& InKey) const noexcept
        {
            const FontFamilyEntry* lBest     = nullptr;
            Uint32                 lBestCost = 0u;

            for (const FontFamilyEntry& lEntry : Entries)
            {
                if (lEntry.Style.Subset != InKey.Subset)
                {
                    continue;
                }

                const Uint32 lCost = MatchCost(lEntry.Style, InKey);
                if (lBest == nullptr || lCost < lBestCost)
                {
                    lBest     = &lEntry;
                    lBestCost = lCost;

                    if (lCost == 0u)
                    {
                        break;   // exact match
                    }
                }
            }

            return lBest;
        }

        /**
         * Distance between two styles in the same script. Lower is better; zero is exact.
         * Width outweighs slant, which outweighs weight.
         */
        static Uint32 MatchCost(const FontStyleKey& InCandidate, const FontStyleKey& InWanted) noexcept
        {
            constexpr Uint32 WIDTH_PENALTY = 100000u;
            constexpr Uint32 SLANT_PENALTY = 10000u;

            const Uint32 lWidthSteps      = Distance(static_cast<Uint32>(InCandidate.Width),
                                                     static_cast<Uint32>(InWanted.Width));
            const Uint32 lWeightDistance  = Distance(WeightValue(InCandidate.Weight),
                                                     WeightValue(InWanted.Weight));

            return (lWidthSteps * WIDTH_PENALTY)
                 + ((InCandidate.Slant != InWanted.Slant) ? SLANT_PENALTY : 0u)
                 + lWeightDistance;
        }

    private:
        /** Unsigned |a - b|. */
        static constexpr Uint32 Distance(const Uint32 InLeft, const Uint32 InRight) noexcept
        {
            return (InLeft > InRight) ? (InLeft - InRight) : (InRight - InLeft);
        }
    };
}
