#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/Input/InputActionValue.h"
#include "Engine/Input/InputTypes.h"
#include "Engine/Input/InputTypesJson.h"

namespace Opaax
{
    // =============================================================================
    // InputActionData — ONE action, as DATA. The `.opaaxaction` payload.
    //
    //   ONE ASSET PER ACTION, like Unreal's UInputAction, and that was the user's call: an
    //   action is the thing a game names and grows (a description today, a category or an
    //   icon later), and per-action files are what make it addressable from the browser.
    //
    //   IT DOES NOT NAME ITS KEYS. Which key drives it is a MAPPING CONTEXT's business, which
    //   is the whole reason rebinding never touches this file — and why one action can be
    //   driven by four keys in one context and one key in another.
    //
    //   THE NAME IS THE IDENTITY gameplay uses (Bind(OPAAX_ID("Jump"), ...)), and it lives
    //   HERE rather than in the mapping entry that references it: two contexts pointing at one
    //   action must agree on what it is called, and the only way to guarantee that is for the
    //   action to own its own name. The editor pre-fills it from the file stem (I13).
    // =============================================================================
    struct InputActionData
    {
        /** What gameplay binds. Pre-filled from the file stem; renaming it breaks nothing else. */
        OpaaxStringID Name;

        /** The shape of its value. Keys feed x; Negate and Swizzle are what move it (IM). */
        EInputValueType ValueType = EInputValueType::Bool;

        /**
         * How long continuous actuation takes to fire the Hold trigger.
         *
         * Here and NOT on the binding: "Crouch is a half-second hold" is a property of the action, and
         * two places to author it is the two-sources trap (L30).
         */
        float HoldSeconds = 0.5f;

        /**
         * Applied to the SUM of every binding that feeds this action.
         *
         * The level a binding's own modifiers cannot reach, and the reason an Axis2D built from
         * four keys needs one: normalizing each contribution individually changes nothing, so
         * without a Normalize HERE the diagonal is 1.41x too fast.
         */
        TDynArray<InputModifierData> Modifiers;

        /** Free text for whoever opens this in a year. Never read by the engine. */
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
