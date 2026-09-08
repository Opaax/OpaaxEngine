#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathsJson.hpp"          // Vector2F
#include "Core/Reflection/OpaaxEnumJson.h"   // EInputModifier, by LABEL
#include "Engine/Input/InputTypes.h"

namespace Opaax
{
    // =============================================================================
    // The nlohmann bridge for InputModifierData, SPLIT from the type the way
    // ResourcePathJson.h and MathsJson.hpp are.
    //
    //   Split rather than intrusive because InputTypes.h is what the EVALUATOR includes, and the
    //   evaluator has no business knowing json exists. A mapping asset is the only thing that
    //   serializes a modifier, so the bridge lives on the asset's side of the line.
    //
    //   WITH DEFAULTS on the way in: a modifier authored before a knob existed still loads, and a
    //   Scalar entry has no reason to spell out dead-zone bounds it never reads.
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
        const InputModifierData lDefaults;

        InValue.Type          = InJson.value("Type", lDefaults.Type);
        InValue.Scale         = InJson.value("Scale", lDefaults.Scale);
        InValue.DeadZoneLower = InJson.value("DeadZoneLower", lDefaults.DeadZoneLower);
        InValue.DeadZoneUpper = InJson.value("DeadZoneUpper", lDefaults.DeadZoneUpper);
    }
}
