#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"     // OPAAX_ENUM_VALUES — the two enums below stamp it
#include "Core/Reflection/OpaaxProperty.h" // OPAAX_PROPERTIES — InputModifierData draws itself
#include "Core/Maths/MathTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Input/InputActionValue.h"
#include "Engine/Subsystems/Input/InputCodes.h"

namespace Opaax
{
    // =============================================================================
    // EInputTrigger — WHEN a binding fires. Named on the BIND call, not on the mapping.
    //
    //   Unreal splits UInputTrigger (on the mapping) from ETriggerEvent (on the binding).
    //   Collapsing them to one concept on the binding side is what the user's
    //   `Bind(this, trigger, callback)` asks for, and it makes the mapping asset smaller:
    //   a key mapping carries no trigger and no hold time at all.
    // =============================================================================
    enum class EInputTrigger : Uint8
    {
        /** The frame the value became non-zero. A press. */
        Started,

        /** Every frame the value is non-zero. What a Move binding wants. */
        Triggered,

        /** The frame the value returned to zero. A release. */
        Completed,

        /** ONCE, after the action's HoldSeconds of continuous actuation. */
        Hold
    };

    /** I11: the mapping lives with the enum, found by ADL. */
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
    // EInputModifier — how one binding's raw value is transformed before it reaches the
    //   action. A CLOSED set, applied in the order the mapping lists them: DeadZone-then-
    //   Scale is not Scale-then-DeadZone, which is why this is an ordered list and not a
    //   set of flags.
    // =============================================================================
    enum class EInputModifier : Uint8
    {
        /** Flip the sign. The `S` and `A` halves of a WASD composite. */
        Negate,

        /** Move x into y. The `W`/`S` halves of a WASD composite, after Negate. */
        Swizzle,

        /** Below Lower reads zero; above Upper reads one; between, rescaled. */
        DeadZone,

        /** Multiply per component. */
        Scalar,

        /** Clamp magnitude to 1, so a diagonal is not faster than a straight line. */
        Normalize
    };

    /** I11: the mapping lives with the enum, found by ADL. */
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
    // InputModifierData — one step of a binding's modifier pipeline.
    //
    //   ONE struct with the union of knobs rather than a type per modifier: the set is
    //   closed and tiny, and the editor already knows how to hide the fields a Type does
    //   not use (MoveModePanel's fold, ⑦-A P5a-2). A registry of modifier types would buy
    //   extensibility nobody has asked for and cost a resource or a variant for the params.
    // =============================================================================
    struct InputModifierData
    {
        EInputModifier Type = EInputModifier::Scalar;

        /** Scalar only. Per component. */
        Vector2F Scale{1.f, 1.f};

        /** DeadZone only. Below Lower is zero, above Upper is one, between is rescaled. */
        float DeadZoneLower = 0.25f;
        float DeadZoneUpper = 1.0f;

        // REFLECTED, so both input panels draw a modifier with no per-type editor code — the Type
        // field becomes a dropdown on its own, because TPropertyDrawer specialises for any enum
        // that declared its values. The knobs a given Type ignores are still shown; which ones
        // matter is a presentation question and this struct is deliberately not the place for it.
        OPAAX_PROPERTIES(InputModifierData,
                         OPAAX_PROP(Type).SetTooltip("Which transform. Applied in list order."),
                         OPAAX_PROP(Scale).SetTooltip("Scalar only: multiplied per component."),
                         OPAAX_PROP(DeadZoneLower).SetRange(0.f, 1.f)
                                                  .SetTooltip("DeadZone only: below this reads zero."),
                         OPAAX_PROP(DeadZoneUpper).SetRange(0.f, 1.f)
                                                  .SetTooltip("DeadZone only: at or above this reads one."))
    };

    // =============================================================================
    // InputAction — what an action IS, resolved. Name, shape, and how long a Hold takes.
    //
    //   HoldSeconds lives HERE and not on the binding: "Crouch is a half-second hold" is a property
    //   of the action, and putting it in both places is the two-sources trap (L30).
    // =============================================================================
    struct InputAction
    {
        OpaaxStringID   Name;
        EInputValueType ValueType   = EInputValueType::Bool;
        float           HoldSeconds = 0.5f;

        /**
         * Applied to the SUM of every binding that fed this action, after they are added together.
         *
         * The level a binding's own modifiers cannot reach. Normalize is the reason it exists: a
         * WASD composite is four bindings each contributing a unit vector, so normalizing them
         * INDIVIDUALLY changes nothing and the diagonal still comes out 1.41x too fast. Only the
         * total can be clamped, and only here.
         */
        TDynArray<InputModifierData> Modifiers;
    };

    // =============================================================================
    // InputKeyBinding — one key feeding one action, through a modifier pipeline.
    //
    //   RESOLVED: the action is named, not pathed. The `.opaaxinputmap` asset references an
    //   action by TResourcePath (that is what makes the editor field a resource picker), and
    //   resolution happens ONCE when the context is added — resolving a path per key per
    //   frame would be absurd. This is the form the evaluator actually runs on.
    // =============================================================================
    struct InputKeyBinding
    {
        OpaaxStringID Action;
        EKeyCode      Key = EKeyCode::None;

        TDynArray<InputModifierData> Modifiers;

        /**
         * Whether this key is swallowed from every LOWER-priority context.
         *
         * Per KEY, not per action — that is what makes a menu context stop `Jump` from
         * firing rather than merely outrank it.
         */
        bool bConsume = true;
    };

    // =============================================================================
    // InputMappingContext — a named set of bindings, pushed onto the stack at a priority.
    //   Higher priority is evaluated first and consumes first.
    // =============================================================================
    struct InputMappingContext
    {
        OpaaxStringID Name;
        Int32         Priority = 0;

        TDynArray<InputKeyBinding> Bindings;
    };

    // =============================================================================
    // InputActionState — one action's answer for this frame, and the little history the
    //   trigger phases need. Everything except HeldSeconds is re-derived every frame from
    //   InputManager, which is what makes a route close (IN5) self-healing.
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

        /** The ONE frame HeldSeconds is crossed. Not "is held" — that is bTriggered. */
        bool bHold = false;

        /** How long the action has been continuously actuated. Zeroed when it is not. */
        float HeldSeconds = 0.f;

        /**
         * INTERNAL to the evaluator: this actuation has already fired its Hold.
         *
         * Separate from bHold because they answer different questions — bHold is "fire this
         * frame" and must be true exactly once, while this must stay true until the action is
         * released, or a held key would re-fire Hold every frame after the threshold.
         */
        bool bHoldLatched = false;

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
