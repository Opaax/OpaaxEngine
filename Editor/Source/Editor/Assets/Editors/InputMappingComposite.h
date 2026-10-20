#pragma once

#include "Core/OpaaxTypes.h"
#include "Input/Assets/InputMappingContextAsset.h"
#include "Input/InputCodes.h"   // EKeyCode

namespace Opaax::Editor
{
    // =============================================================================
    // MakeComposite2D — the four mappings of a WASD-style composite. A pure, header-only function
    //   so tests can check it (a swapped pair walks the character sideways).
    //   The raw value is in x: +X is plain, -X negates, +Y swizzles, -Y does both.
    //
    //   @param InTemplate The row whose action and consume flag the four copy.
    //   @return Right, Left, Up, Down, in that order.
    // =============================================================================
    inline TDynArray<InputMappingEntry> MakeComposite2D(const InputMappingEntry& InTemplate,
                                                        const EKeyCode InUp,   const EKeyCode InDown,
                                                        const EKeyCode InLeft, const EKeyCode InRight)
    {
        auto lMake = [&InTemplate](const EKeyCode InKey, const bool bInNegate, const bool bInSwizzle)
        {
            InputMappingEntry lEntry;
            lEntry.Action   = InTemplate.Action;
            lEntry.Key      = InKey;
            lEntry.bConsume = InTemplate.bConsume;

            if (bInNegate)
            {
                InputModifierData lNegate;
                lNegate.Type = EInputModifier::Negate;
                lEntry.Modifiers.emplace_back(lNegate);
            }

            if (bInSwizzle)
            {
                InputModifierData lSwizzle;
                lSwizzle.Type = EInputModifier::Swizzle;
                lEntry.Modifiers.emplace_back(lSwizzle);
            }

            return lEntry;
        };

        return { lMake(InRight, false, false),
                 lMake(InLeft,  true,  false),
                 lMake(InUp,    false, true),
                 lMake(InDown,  true,  true) };
    }

    /**
     * InMappings with the row at InIndex replaced by the four rows of a composite on its action.
     * Replaced, not kept: keeping the template row would bind a fifth key.
     * @return False when InIndex is out of range or its row names no action (nothing changes)
     */
    inline bool ReplaceWithComposite2D(TDynArray<InputMappingEntry>& InOutMappings, const Uint32 InIndex,
                                       const EKeyCode InUp, const EKeyCode InDown,
                                       const EKeyCode InLeft, const EKeyCode InRight)
    {
        if (InIndex >= InOutMappings.size() || InOutMappings[InIndex].Action.IsEmpty())
        {
            return false;
        }

        const TDynArray<InputMappingEntry> lComposite =
            MakeComposite2D(InOutMappings[InIndex], InUp, InDown, InLeft, InRight);

        InOutMappings.erase(InOutMappings.begin() + InIndex);
        InOutMappings.insert(InOutMappings.begin() + InIndex, lComposite.begin(), lComposite.end());
        return true;
    }
}
