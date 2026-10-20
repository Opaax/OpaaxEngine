#include "Renderer/Text/FontFamily.h"

namespace Opaax
{
    namespace
    {
        /** Unsigned |a - b|. */
        constexpr Uint32 Distance(const Uint32 InLeft, const Uint32 InRight) noexcept
        {
            return (InLeft > InRight) ? (InLeft - InRight) : (InRight - InLeft);
        }
    }

    const FontFamilyEntry* FontFamily::FindExact(const FontStyleKey& InKey) const noexcept
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

    const FontFamilyEntry* FontFamily::Find(const FontStyleKey& InKey) const noexcept
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

    Uint32 FontFamily::MatchCost(const FontStyleKey& InCandidate, const FontStyleKey& InWanted) noexcept
    {
        constexpr Uint32 WIDTH_PENALTY = 100000u;
        constexpr Uint32 SLANT_PENALTY = 10000u;

        const Uint32 lWidthSteps     = Distance(static_cast<Uint32>(InCandidate.Width),
                                                static_cast<Uint32>(InWanted.Width));
        const Uint32 lWeightDistance = Distance(WeightValue(InCandidate.Weight),
                                                WeightValue(InWanted.Weight));

        return (lWidthSteps * WIDTH_PENALTY)
             + ((InCandidate.Slant != InWanted.Slant) ? SLANT_PENALTY : 0u)
             + lWeightDistance;
    }

    void FontFamily::PostEditChange()
    {
        // Of two equal styles, Find takes the first: the later one is never drawn.
        for (Uint64 lIndex = 1; lIndex < Entries.size(); ++lIndex)
        {
            for (Uint64 lEarlier = 0; lEarlier < lIndex; ++lEarlier)
            {
                const FontStyleKey& lStyle = Entries[lIndex].Style;
                if (lStyle != Entries[lEarlier].Style)
                {
                    continue;
                }

                OPAAX_LOG(LogFontFamily, Warn, "Font family '{}': entry {} repeats entry {}'s style "
                          "({}/{}/{}/{}) — it is never drawn", GetName(), lIndex, lEarlier,
                          ToString(lStyle.Subset), ToString(lStyle.Weight), ToString(lStyle.Width), ToString(lStyle.Slant));
                break;
            }
        }
    }
}
