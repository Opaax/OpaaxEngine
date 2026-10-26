#pragma once

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
#include "Resources/Asset/Asset.h"

namespace Opaax
{
    // =============================================================================
    // InputActionAsset — one action, like Unreal's UInputAction. Keys are not listed here: mapping
    //   contexts bind keys to actions. Gameplay binds its name (OPAAX_ID("Jump")). InputAction is
    //   the runtime form the evaluator keeps.
    // =============================================================================
    class InputActionAsset final : public Asset
    {
        OPAAX_OBJECT(InputActionAsset, Asset)

    public:
        /** The name gameplay binds to. Empty: the file's name. */
        OpaaxStringID Name;

        /** The value type. */
        EInputValueType ValueType = EInputValueType::Bool;

        /** How long the action must be held for the Hold trigger. */
        float HoldSeconds = 0.5f;

        /** Applied to the sum of all bindings (e.g. Normalize for a WASD diagonal). */
        TDynArray<InputModifierData> Modifiers;

        /** Free text. Not used by the engine. */
        OpaaxString Description;

        OPAAX_PROPERTIES(InputActionAsset,
                         OPAAX_PROP(Name).SetTooltip("What gameplay binds, like \"Jump\".\n"
                                                     "Empty: the file's name."),
                         OPAAX_PROP(ValueType).SetTooltip("Bool for a button, Axis1D/Axis2D for a direction."),
                         OPAAX_PROP(HoldSeconds).SetRange(0.f, 5.f)
                                                .SetTooltip("Seconds of continuous input before the Hold trigger fires."),
                         OPAAX_PROP(Modifiers).SetTooltip("Applied in order to the sum of every key's value."),
                         OPAAX_PROP(Description).SetTooltip("Notes for a human. The engine never reads this."))

        /** Name, or the object's name (its file's) when Name is empty. */
        OpaaxStringID GetActionName() const noexcept { return Name.IsValid() ? Name : GetName(); }

        /** The runtime action the evaluator registers. */
        InputAction ToAction() const
        {
            InputAction lAction;
            lAction.Name        = GetActionName();
            lAction.ValueType   = ValueType;
            lAction.HoldSeconds = HoldSeconds;
            lAction.Modifiers   = Modifiers;
            return lAction;
        }
    };
}
