#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Maths/MathsJson.hpp"

namespace Opaax
{
    // =============================================================================
    // JSON for LinearColor. Written as a Vector4F ({x,y,z,w}).
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
