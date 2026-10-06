#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Core/String/OpaaxStringJson.h"
#include "Input/Mapping/InputActionValue.h"
#include "Input/Mapping/InputTypes.h"
#include "Input/Mapping/InputTypesJson.h"

namespace Opaax
{
    // =============================================================================
    // InputActionData — one action (.opaaxaction), like Unreal's UInputAction.
    //   Keys are not listed here: mapping contexts bind keys to actions.
    //   The name is what gameplay binds to (OPAAX_ID("Jump")).
    // =============================================================================
    struct InputActionData
    {
        /** The name gameplay binds to. The editor pre-fills it from the file name. */
        OpaaxStringID Name;

        /** The value type. */
        EInputValueType ValueType = EInputValueType::Bool;

        /**
         * How long the action must be held for the Hold trigger.
         */
        float HoldSeconds = 0.5f;

        /**
         * Applied to the sum of all bindings (e.g. Normalize for a WASD diagonal).
         */
        TDynArray<InputModifierData> Modifiers;

        /** Free text. Not used by the engine. */
        OpaaxString Description;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(InputActionData, Name, ValueType, HoldSeconds,
                                                    Modifiers, Description)

        OPAAX_PROPERTIES(InputActionData,
                         OPAAX_PROP(Name).SetTooltip("What gameplay binds, like \"Jump\"."),
                         OPAAX_PROP(ValueType).SetTooltip("Bool for a button, Axis1D/Axis2D for a direction."),
                         OPAAX_PROP(HoldSeconds).SetRange(0.f, 5.f)
                                                .SetTooltip("Seconds of continuous input before the Hold trigger fires."),
                         OPAAX_PROP(Description).SetTooltip("Notes for a human. The engine never reads this."))
    };
}
