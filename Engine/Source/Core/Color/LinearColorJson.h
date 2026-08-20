#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Maths/MathsJson.hpp"

namespace Opaax
{
    // =============================================================================
    // The nlohmann bridge for LinearColor, SPLIT from the type the way OpaaxTagJson.h is split from
    // OpaaxTag.h: the renderer wants a colour, not a json library.
    //
    // It forwards to the VECTOR's bridge, so a colour is written {x,y,z,w} exactly as it was before
    // the type existed. That is load-bearing rather than lazy — every .opaaxmap already on disk
    // holds a Vector4F under that key, and with _WITH_DEFAULT a key it could not read would revert
    // silently to white instead of failing.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const LinearColor& InColor)
    {
        InJson = static_cast<const Vector4F&>(InColor);
    }

    inline void from_json(const nlohmann::json& InJson, LinearColor& InColor)
    {
        InJson.get_to(static_cast<Vector4F&>(InColor));
    }
}
