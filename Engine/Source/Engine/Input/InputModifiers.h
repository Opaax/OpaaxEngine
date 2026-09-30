#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Engine/Input/InputTypes.h"

namespace Opaax
{
    // =============================================================================
    // InputModifiers — transforms applied to a binding's raw value. Pure functions.
    // =============================================================================
    namespace InputModifiers
    {
        /**
         * Applies one modifier.
         * @param InValue    The value so far
         * @param InModifier The transform and its settings
         */
        OPAAX_API Vector2F Apply(Vector2F InValue, const InputModifierData& InModifier) noexcept;

        /**
         * Applies every modifier, in order (order matters: DeadZone then Scalar differs from
         * Scalar then DeadZone).
         */
        OPAAX_API Vector2F ApplyAll(Vector2F InValue, const TDynArray<InputModifierData>& InModifiers) noexcept;
    }
}
