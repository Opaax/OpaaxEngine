#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Engine/Input/InputTypes.h"

namespace Opaax
{
    // =============================================================================
    // InputModifiers — the pipeline one binding's raw value passes through.
    //
    //   FREE AND PURE, so the whole transform half of input mapping is testable without a
    //   world, a window or an InputManager. Vector2F is glm::vec2, so every step below is
    //   ordinary arithmetic rather than a visit over a variant (see InputActionValue).
    // =============================================================================
    namespace InputModifiers
    {
        /**
         * Apply ONE modifier.
         *
         * @param InValue    The value so far — the raw key value for the first modifier, the
         *                   previous step's output afterwards.
         * @param InModifier Which transform, and its knobs.
         */
        // OPAAX_API on the FUNCTIONS, not on a class: these are free and stateless, and a free
        // function in a namespace is not exported by anything else in its header the way a
        // member of an OPAAX_API class is. Without it OpaaxTests — which links the import lib —
        // gets LNK2019, which is how this was found.
        OPAAX_API Vector2F Apply(Vector2F InValue, const InputModifierData& InModifier) noexcept;

        /**
         * Apply every modifier IN ORDER.
         *
         * Order is authored and load-bearing: DeadZone-then-Scalar is not Scalar-then-DeadZone,
         * because the dead zone's thresholds are in the un-scaled range.
         */
        OPAAX_API Vector2F ApplyAll(Vector2F InValue, const TDynArray<InputModifierData>& InModifiers) noexcept;
    }
}
