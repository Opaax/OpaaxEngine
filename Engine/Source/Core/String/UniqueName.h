#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/Char.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // UniqueName — the rule every editor document names a new or copied thing by: the name itself when
    //   free, else its base and the smallest free number ("Crate", "Crate 1", "Crate 2"...). Name lookups
    //   (Behaviour::FindEntity, UIWidget::FindByName) then always find what the author meant.
    // =============================================================================
    namespace UniqueName
    {
        /** InName without a trailing " <number>": "Crate 12" gives "Crate"; "R2D2" and "12" stay as they are. */
        inline OpaaxString BaseOf(const OpaaxString& InName)
        {
            Uint32 lDigits = 0;
            while (lDigits < InName.GetLength() && Char::IsDigit(InName[InName.GetLength() - 1 - lDigits]))
            {
                ++lDigits;
            }

            const Uint32 lSpace = InName.GetLength() - lDigits;
            if (lDigits == 0 || lSpace < 2 || InName[lSpace - 1] != ' ')
            {
                return InName;
            }
            return InName.SubString(0, lSpace - 1);
        }

        /**
         * InName when InIsTaken says it is free; otherwise BaseOf(InName) followed by the smallest number from
         * 1 that is free. A copy of "Crate 1" is "Crate 2", not "Crate 1 1".
         * @param InIsTaken bool(const OpaaxString&): whether a name is used in the document
         */
        template<typename TIsTaken>
        OpaaxString Make(const OpaaxString& InName, TIsTaken&& InIsTaken)
        {
            if (!InIsTaken(InName))
            {
                return InName;
            }

            const OpaaxString lBase = BaseOf(InName);
            for (Uint32 lNumber = 1;; ++lNumber)
            {
                OpaaxString lCandidate = lBase + OpaaxString(" ") + OpaaxString::FromUInt(lNumber);
                if (!InIsTaken(lCandidate))
                {
                    return lCandidate;
                }
            }
        }
    }
}
