#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // EInputValueType — what shape an action's value has. Declared by the action, not by
    //   the keys bound to it: "Move" is an Axis2D whether it is driven by WASD or a stick.
    // =============================================================================
    enum class EInputValueType : Uint8
    {
        Bool,
        Axis1D,
        Axis2D
    };

    OPAAX_API const char* ToString(EInputValueType InType) noexcept;

    // =============================================================================
    // InputActionValue — one action's value this frame.
    //
    //   A TAGGED VECTOR, not a variant, and that is deliberate. Unreal's FInputActionValue
    //   is the same shape for the same reason: every modifier is then plain Vector2F
    //   arithmetic, and binding a bool key into an Axis2D action needs no conversion at the
    //   call site. A std::variant would put a std::visit in every modifier — roughly three
    //   times the code — to express something the vector already expresses.
    //
    //   The TYPE is carried so a reader can assert what it asked for; it never changes how
    //   the value is stored. Bool is "x or y is non-zero", Axis1D is x, Axis2D is both.
    // =============================================================================
    struct InputActionValue
    {
        Vector2F        Value{0.f, 0.f};
        EInputValueType Type = EInputValueType::Bool;

        /** Non-zero in ANY component. What "is this action actuated" means for every type. */
        bool AsBool() const noexcept { return Value.x != 0.f || Value.y != 0.f; }

        float    AsAxis1D() const noexcept { return Value.x; }
        Vector2F AsAxis2D() const noexcept { return Value; }
    };
}
