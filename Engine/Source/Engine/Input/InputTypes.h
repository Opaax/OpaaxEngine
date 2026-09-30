#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Input/InputActionValue.h"
#include "Engine/Subsystems/Input/InputCodes.h"

namespace Opaax
{
    // =============================================================================
    // EInputTrigger — when a bound handler is called. Chosen when binding, not in the mapping.
    // =============================================================================
    enum class EInputTrigger : Uint8
    {
        /** The frame the value became non-zero (press). */
        Started,

        /** Every frame the value is non-zero. */
        Triggered,

        /** The frame the value returned to zero (release). */
        Completed,

        /** Once, after HoldSeconds of continuous input. */
        Hold
    };

    /** Enum to string. */
    inline const char* ToString(const EInputTrigger InTrigger) noexcept
    {
        switch (InTrigger)
        {
        case EInputTrigger::Started:   return "Started";
        case EInputTrigger::Triggered: return "Triggered";
        case EInputTrigger::Completed: return "Completed";
        case EInputTrigger::Hold:      return "Hold";
        }

        return "Triggered";
    }

    /** Every trigger phase, for iterating the four delegate slots. */
    inline constexpr Uint8 INPUT_TRIGGER_COUNT = 4;
}

OPAAX_ENUM_VALUES(Opaax::EInputTrigger, Started, Triggered, Completed, Hold)

namespace Opaax
{
    // =============================================================================
    // EInputModifier — how a binding's raw value is transformed. Applied in the listed order.
    // =============================================================================
    enum class EInputModifier : Uint8
    {
        /** Flips the sign (S and A in a WASD setup). */
        Negate,

        /** Moves x into y (W and S in a WASD setup, after Negate). */
        Swizzle,

        /** Below Lower gives zero, above Upper gives one, rescaled in between. */
        DeadZone,

        /** Multiply per component. */
        Scalar,

        /** Clamps the length to 1, so diagonals are not faster. */
        Normalize
    };

    /** Enum to string. */
    inline const char* ToString(const EInputModifier InModifier) noexcept
    {
        switch (InModifier)
        {
        case EInputModifier::Negate:    return "Negate";
        case EInputModifier::Swizzle:   return "Swizzle";
        case EInputModifier::DeadZone:  return "DeadZone";
        case EInputModifier::Scalar:    return "Scalar";
        case EInputModifier::Normalize: return "Normalize";
        }

        return "Scalar";
    }
}

OPAAX_ENUM_VALUES(Opaax::EInputModifier, Negate, Swizzle, DeadZone, Scalar, Normalize)

namespace Opaax
{
    // =============================================================================
    // InputModifierData — one modifier and its settings. Unused settings are ignored.
    // =============================================================================
    struct InputModifierData
    {
        EInputModifier Type = EInputModifier::Scalar;

        /** Scalar only. Per component. */
        Vector2F Scale{1.f, 1.f};

        /** DeadZone only. Below Lower gives zero, above Upper gives one, rescaled in between. */
        float DeadZoneLower = 0.25f;
        float DeadZoneUpper = 1.0f;

        // Reflected, so the input panels draw it without custom code.
        OPAAX_PROPERTIES(InputModifierData,
                         OPAAX_PROP(Type).SetTooltip("Which transform. Applied in list order."),
                         OPAAX_PROP(Scale).SetTooltip("Scalar only: multiplied per component."),
                         OPAAX_PROP(DeadZoneLower).SetRange(0.f, 1.f)
                                                  .SetTooltip("DeadZone only: below this reads zero."),
                         OPAAX_PROP(DeadZoneUpper).SetRange(0.f, 1.f)
                                                  .SetTooltip("DeadZone only: at or above this reads one."))
    };

    // =============================================================================
    // InputAction — a resolved action: name, value type and hold duration.
    // =============================================================================
    struct InputAction
    {
        OpaaxStringID   Name;
        EInputValueType ValueType   = EInputValueType::Bool;
        float           HoldSeconds = 0.5f;

        /**
         * Applied to the sum of all bindings (e.g. Normalize to clamp the WASD diagonal).
         */
        TDynArray<InputModifierData> Modifiers;
    };

    // =============================================================================
    // InputKeyBinding — one key feeding one action, through modifiers.
    //   The action is referenced by name (resolved once when the context is added).
    // =============================================================================
    struct InputKeyBinding
    {
        OpaaxStringID Action;
        EKeyCode      Key = EKeyCode::None;

        TDynArray<InputModifierData> Modifiers;

        /**
         * The key is consumed: lower-priority contexts ignore it.
         */
        bool bConsume = true;
    };

    // =============================================================================
    // InputMappingContext — a named set of bindings, added with a priority.
    //   Higher priority is evaluated first and consumes first.
    // =============================================================================
    struct InputMappingContext
    {
        OpaaxStringID Name;
        Int32         Priority = 0;

        TDynArray<InputKeyBinding> Bindings;
    };

    // =============================================================================
    // InputActionState — one action's state this frame.
    //   Recomputed each frame from InputManager, except HeldSeconds.
    // =============================================================================
    struct InputActionState
    {
        InputActionValue Value;

        /** The frame the value became non-zero. */
        bool bStarted = false;

        /** Every frame the value is non-zero. */
        bool bTriggered = false;

        /** The frame the value returned to zero. */
        bool bCompleted = false;

        /** True only on the frame HeldSeconds is reached. */
        bool bHold = false;

        /** How long the action has been active. Zero when it is not. */
        float HeldSeconds = 0.f;

        /**
         * Internal: Hold already fired for this press (so it fires once).
         */
        bool bHoldLatched = false;

        /**
         * Internal: a binding was consumed last frame while its key was down, so unmasking does not
         * produce a false Started.
         */
        bool bMaskSuppressed = false;

        /** Whether it fires for InTrigger this frame. */
        bool FiresFor(EInputTrigger InTrigger) const noexcept
        {
            switch (InTrigger)
            {
            case EInputTrigger::Started:   return bStarted;
            case EInputTrigger::Triggered: return bTriggered;
            case EInputTrigger::Completed: return bCompleted;
            case EInputTrigger::Hold:      return bHold;
            }

            return false;
        }
    };
}
