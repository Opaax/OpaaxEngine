#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Input/Mapping/InputTypes.h"

namespace Opaax
{
    // =============================================================================
    // JSON for InputModifierData. Kept apart so the evaluator does not include json.
    // Missing keys keep their defaults.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const InputModifierData& InValue)
    {
        InJson = nlohmann::json{
            {"Type",          InValue.Type},
            {"Scale",         InValue.Scale},
            {"DeadZoneLower", InValue.DeadZoneLower},
            {"DeadZoneUpper", InValue.DeadZoneUpper}};
    }

    inline void from_json(const nlohmann::json& InJson, InputModifierData& InValue)
    {
        const InputModifierData lDefaults{};

        InValue.Type          = InJson.value("Type", lDefaults.Type);
        InValue.Scale         = InJson.value("Scale", lDefaults.Scale);
        InValue.DeadZoneLower = InJson.value("DeadZoneLower", lDefaults.DeadZoneLower);
        InValue.DeadZoneUpper = InJson.value("DeadZoneUpper", lDefaults.DeadZoneUpper);
    }
}
