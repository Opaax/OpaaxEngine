#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "Engine/Subsystems/Resources/Types/Font/FontStyle.h"

namespace Opaax
{
    struct FontFaceResource;   // only NAMED — TResourcePath never completes its parameter

    // =============================================================================
    // FontFamilyData — every cut of one typeface, under the four axes that pick between them. The
    //   `.opaaxfont` payload.
    //
    //   ITS WHOLE JOB IS THE ALIAS, exactly as AnimationLibraryData's is. A face is its own asset —
    //   one file, individually loadable — and this is what lets a component say "Roboto, greek,
    //   medium, italic" instead of naming `Fonts/Roboto/roboto-greek-500-italic.ttf`. A component
    //   may skip it and name one `.ttf` directly, because a single label must not need a family
    //   asset beside it.
    //
    //   NO Name FIELD and no default entry, unlike its animation sibling. A clip alias is a string
    //   that can be absent, so a library needs to say which one stands in; a style key cannot be
    //   absent — the four axes always have a value — so the ladder below always has a real question
    //   to answer.
    //
    //   EVERY MEMBER IS INLINE: the struct carries no OPAAX_API, so a member defined in the DLL's
    //   .cpp is unresolvable from the exe (**I6**).
    // =============================================================================

    /** One cut: the style it answers to, and the file that draws it. */
    struct FontFamilyEntry
    {
        FontStyleKey Style;

        /** Asset-relative ("Fonts/Roboto/roboto-latin-400-normal.ttf") or a mount. */
        TResourcePath<FontFaceResource> Face;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(FontFamilyEntry, Style, Face)

        // What the family editor folds over for the SELECTED entry — Style becomes a group of four
        // dropdowns and Face a typed drop target, with no drawer written for either (I15). The list
        // itself is the panel's own UI, which is what a 162-row matrix has to be.
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

        /** The entry matching InKey on all four axes, or nullptr. */
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
         * The best face this family has for InKey, or nullptr when it has nothing in that SCRIPT.
         *
         * THE SUBSET IS NEVER CROSSED. Every other axis falls back, because asking for Medium in a
         * family that stops at Regular is a style request rather than a typo and CSS has resolved it
         * that way for twenty years. Asking for Greek and getting Latin is a different thing
         * entirely: it does not degrade, it returns a screenful of tofu boxes, so the honest miss is
         * the better answer and the caller can say which script it lacks.
         *
         * Priority follows CSS's own font-matching order — WIDTH, then SLANT, then WEIGHT — which is
         * why the penalties are orders of magnitude apart rather than added up.
         *
         * The caller decides whether it cares that the answer was inexact: compare the returned
         * entry's Style against the one asked for. Nothing is logged here, because a family is
         * consulted once per draw and a per-frame line is not a diagnostic.
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
                        break;   // an exact match cannot be beaten
                    }
                }
            }

            return lBest;
        }

        /**
         * How far InCandidate is from InWanted, given both are in the right script. Lower is better;
         * zero is exact.
         *
         * The three terms cannot trade against each other: one step of width outranks any slant
         * mismatch, which outranks the whole 800-point weight range.
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
        /** Unsigned |a - b|, written out so the header needs no <cstdlib> and no signed round trip. */
        static constexpr Uint32 Distance(const Uint32 InLeft, const Uint32 InRight) noexcept
        {
            return (InLeft > InRight) ? (InLeft - InRight) : (InRight - InLeft);
        }
    };
}
