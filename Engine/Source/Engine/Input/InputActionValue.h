#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Reflection/OpaaxEnum.h"

namespace Opaax
{
    // =============================================================================
    // EInputValueType — the value type of an action ("Move" is Axis2D for WASD or a stick).
    // =============================================================================
    enum class EInputValueType : Uint8
    {
        Bool,
        Axis1D,
        Axis2D
    };

    /** Enum to string. */
    inline const char* ToString(const EInputValueType InType) noexcept
    {
        switch (InType)
        {
        case EInputValueType::Bool:   return "Bool";
        case EInputValueType::Axis1D: return "Axis1D";
        case EInputValueType::Axis2D: return "Axis2D";
        }

        return "Bool";
    }
}

OPAAX_ENUM_VALUES(Opaax::EInputValueType, Bool, Axis1D, Axis2D)

namespace Opaax
{
    // =============================================================================
    // InputActionValue — one action's value this frame. Always a Vector2F (like Unreal's
    //   FInputActionValue): Bool is "x or y non-zero", Axis1D is x, Axis2D is both.
    // =============================================================================
    struct InputActionValue
    {
        Vector2F        Value{0.f, 0.f};
        EInputValueType Type = EInputValueType::Bool;

        /** Non-zero in any component: the action is active. */
        bool AsBool() const noexcept { return Value.x != 0.f || Value.y != 0.f; }

        float    AsAxis1D() const noexcept { return Value.x; }
        Vector2F AsAxis2D() const noexcept { return Value; }
    };
}
