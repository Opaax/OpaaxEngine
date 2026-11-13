#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"

namespace Opaax
{
    /** How a sliced frame fills its edges and centre. */
    enum class ESliceMode : Uint8
    {
        Stretch,   // each edge and the centre stretched to its length
        Tile       // repeated at their size on screen, the last tile cut
    };

    /** Enum to string. */
    inline const char* ToString(const ESliceMode InMode) noexcept
    {
        switch (InMode)
        {
        case ESliceMode::Stretch: return "Stretch";
        case ESliceMode::Tile:    return "Tile";
        }

        return "Stretch";
    }
}

OPAAX_ENUM_VALUES(Opaax::ESliceMode, Stretch, Tile)
