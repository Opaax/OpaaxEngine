#pragma once

#include "Core/OpaaxTypes.h" // Uint8 / Uint32

#include <algorithm> // std::lower_bound

namespace Opaax
{
    // =============================================================================
    // Font kerning lookup (pure)
    // =============================================================================

    /**
     * Pack a glyph pair into a single sortable key: (First << 8) | Second. A kerning
     * table is kept sorted by this key so a lookup is a binary search.
     */
    //------------------------------------------------------------------------------
    constexpr Uint32 PackKerningKey(Uint8 InFirst, Uint8 InSecond) noexcept
    {
        return (static_cast<Uint32>(InFirst) << 8) | static_cast<Uint32>(InSecond);
    }

    /**
     * Binary-search a kerning table (sorted ascending by PackKerningKey) for the
     * (InFirstCp, InSecondCp) pair and return its signed pixel Advance, or 0 when the
     * pair is absent, the table is empty, or either codepoint falls outside
     * [InMinCp, InMaxCp]. Pure — no FontAsset state.
     *
     * TPair is any POD exposing `Uint8 First`, `Uint8 Second`, `float Advance` (e.g.
     * FontAsset::KerningPair). Keeping this generic lets it be unit-tested with a
     * hand-built table, with no TTF and no FontAsset instance.
     */
    //------------------------------------------------------------------------------
    template <typename TPair>
    float KerningLookup(const TPair* InPairs, Uint32 InCount,
                        Uint32 InFirstCp, Uint32 InSecondCp,
                        Uint32 InMinCp, Uint32 InMaxCp) noexcept
    {
        if (InCount == 0u) { return 0.f; }
        if (InFirstCp  < InMinCp || InFirstCp  > InMaxCp) { return 0.f; }
        if (InSecondCp < InMinCp || InSecondCp > InMaxCp) { return 0.f; }

        const Uint32 lKey = PackKerningKey(static_cast<Uint8>(InFirstCp),
                                           static_cast<Uint8>(InSecondCp));

        const TPair* lEnd = InPairs + InCount;
        const TPair* lIt  = std::lower_bound(InPairs, lEnd, lKey,
            [](const TPair& InP, Uint32 InTarget) noexcept
            {
                return PackKerningKey(InP.First, InP.Second) < InTarget;
            });

        if (lIt == lEnd) { return 0.f; }
        return (PackKerningKey(lIt->First, lIt->Second) == lKey) ? lIt->Advance : 0.f;
    }
}
