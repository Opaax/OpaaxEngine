#include "Engine/Input/InputActionEvaluator.h"

#include <algorithm>

#include "Engine/Input/InputModifiers.h"
#include "Engine/Subsystems/Input/InputManager.h"

namespace Opaax
{
    namespace
    {
        /** @return The dense index for InKey, or KEY_STATE_COUNT when out of range */
        Uint16 ToKeyIndex(EKeyCode InKey) noexcept
        {
            const Uint16 lRaw = static_cast<Uint16>(InKey);
            return lRaw < InputManager::KEY_STATE_COUNT ? lRaw : InputManager::KEY_STATE_COUNT;
        }

        /**
         * Held, or pressed and released within this frame (a quick tap).
         */
        bool IsActuated(const InputManager& InInput, EKeyCode InKey) noexcept
        {
            return InInput.IsKeyDown(InKey) || InInput.WasPressedThisFrame(InKey);
        }
    }

    // =========================================================================
    // Actions
    // =========================================================================
    bool InputActionEvaluator::RegisterAction(const InputAction& InAction)
    {
        if (!InAction.Name.IsValid())
        {
            OPAAX_LOG(LogInputEvaluator, Error, "RegisterAction — refused an action with an empty name.");
            return false;
        }

        if (FindAction(InAction.Name) != nullptr)
        {
            OPAAX_LOG(LogInputEvaluator, Error,
                      "RegisterAction '{}' — that name is already taken. Two actions answering to one name is a binding that drives the wrong thing.",
                      InAction.Name);
            return false;
        }

        m_Actions.emplace_back(ActionEntry{InAction, InputActionState{}});
        return true;
    }

    const InputAction* InputActionEvaluator::FindAction(OpaaxStringID InName) const noexcept
    {
        for (const ActionEntry& lEntry : m_Actions)
        {
            if (lEntry.Action.Name == InName)
            {
                return &lEntry.Action;
            }
        }

        return nullptr;
    }

    InputActionEvaluator::ActionEntry* InputActionEvaluator::FindEntry(OpaaxStringID InName) noexcept
    {
        for (ActionEntry& lEntry : m_Actions)
        {
            if (lEntry.Action.Name == InName)
            {
                return &lEntry;
            }
        }

        return nullptr;
    }

    // =========================================================================
    // Contexts
    // =========================================================================
    bool InputActionEvaluator::AddContext(const InputMappingContext& InContext)
    {
        if (!InContext.Name.IsValid())
        {
            OPAAX_LOG(LogInputEvaluator, Error, "AddContext — refused a context with an empty name.");
            return false;
        }

        for (const InputMappingContext& lExisting : m_Contexts)
        {
            if (lExisting.Name == InContext.Name)
            {
                // Already active: nothing to do (adding it twice would double every binding).
                // Happens every level change (both worlds add the same context), so no log.
                return true;
            }
        }

        InputMappingContext lAccepted;
        lAccepted.Name     = InContext.Name;
        lAccepted.Priority = InContext.Priority;

        for (const InputKeyBinding& lBinding : InContext.Bindings)
        {
            if (ToKeyIndex(lBinding.Key) == InputManager::KEY_STATE_COUNT)
            {
                // Gamepad codes (10000+) are reserved but not supported yet: refuse with a warning.
                OPAAX_LOG(LogInputEvaluator, Warn,
                          "AddContext '{}' — binding for action '{}' uses key code {} which has no feed (gamepad is not wired yet). Skipped.",
                          InContext.Name, lBinding.Action, static_cast<Uint16>(lBinding.Key));
                continue;
            }

            if (FindAction(lBinding.Action) == nullptr)
            {
                OPAAX_LOG(LogInputEvaluator, Warn,
                          "AddContext '{}' — binding names action '{}', which is not registered. Skipped.",
                          InContext.Name, lBinding.Action);
                continue;
            }

            lAccepted.Bindings.emplace_back(lBinding);
        }

        // Count before moving and sorting (sorting changes which context is at back()).
        const Uint64 lAcceptedCount = static_cast<Uint64>(lAccepted.Bindings.size());

        m_Contexts.emplace_back(Move(lAccepted));

        // Highest priority first; stable_sort keeps insertion order for equal priorities.
        std::stable_sort(m_Contexts.begin(), m_Contexts.end(),
                         [](const InputMappingContext& InLeft, const InputMappingContext& InRight)
                         {
                             return InLeft.Priority > InRight.Priority;
                         });

        OPAAX_LOG(LogInputEvaluator, Trace, "Context '{}' added at priority {} — {} of {} binding(s) accepted, {} context(s) active",
                  InContext.Name, InContext.Priority,
                  lAcceptedCount, static_cast<Uint64>(InContext.Bindings.size()),
                  static_cast<Uint64>(m_Contexts.size()));

        return true;
    }

    bool InputActionEvaluator::RemoveContext(OpaaxStringID InName)
    {
        for (auto lIt = m_Contexts.begin(); lIt != m_Contexts.end(); ++lIt)
        {
            if (lIt->Name == InName)
            {
                m_Contexts.erase(lIt);
                OPAAX_LOG(LogInputEvaluator, Trace, "Context '{}' removed", InName);
                return true;
            }
        }

        return false;
    }

    const InputMappingContext* InputActionEvaluator::GetContextAt(Uint64 InIndex) const noexcept
    {
        return InIndex < m_Contexts.size() ? &m_Contexts[InIndex] : nullptr;
    }

    // =========================================================================
    // Per frame
    // =========================================================================
    void InputActionEvaluator::Evaluate(const InputManager& InInput, double InDeltaTime, const InputKeyMask* InPreConsumed)
    {
        // Captured before clearing: Started/Completed compare with the previous frame.
        TDynArray<bool> lWasActuated;
        TDynArray<bool> lWasMaskSuppressed;
        lWasActuated.reserve(m_Actions.size());
        lWasMaskSuppressed.reserve(m_Actions.size());

        for (ActionEntry& lEntry : m_Actions)
        {
            lWasActuated.emplace_back(lEntry.State.Value.AsBool());
            lWasMaskSuppressed.emplace_back(lEntry.State.bMaskSuppressed);

            lEntry.State.Value.Value      = Vector2F{0.f, 0.f};
            lEntry.State.Value.Type       = lEntry.Action.ValueType;
            lEntry.State.bStarted         = false;
            lEntry.State.bTriggered       = false;
            lEntry.State.bCompleted       = false;
            lEntry.State.bHold            = false;
            lEntry.State.bMaskSuppressed  = false;
        }

        // Consumed keys (per key, this frame only), starting with the keys the UI used.
        InputKeyMask lConsumed{};
        if (InPreConsumed != nullptr)
        {
            lConsumed = *InPreConsumed;
        }

        for (const InputMappingContext& lContext : m_Contexts)
        {
            for (const InputKeyBinding& lBinding : lContext.Bindings)
            {
                const Uint16 lIndex = ToKeyIndex(lBinding.Key);
                if (lIndex == InputManager::KEY_STATE_COUNT)
                {
                    continue;
                }

                const bool bActuated = IsActuated(InInput, lBinding.Key);

                ActionEntry* lEntry = FindEntry(lBinding.Action);
                if (lEntry == nullptr)
                {
                    continue;
                }

                // A masked binding gives no value, but remember the key is down so unmasking
                // does not produce a false press.
                if (lConsumed[lIndex])
                {
                    if (bActuated) { lEntry->State.bMaskSuppressed = true; }
                    continue;
                }

                // The raw value goes in x; Negate and Swizzle move it (WASD -> Axis2D).
                const Vector2F lRaw = Vector2F{bActuated ? 1.f : 0.f, 0.f};

                lEntry->State.Value.Value += InputModifiers::ApplyAll(lRaw, lBinding.Modifiers);

                // Consume only pressed keys.
                if (lBinding.bConsume && bActuated)
                {
                    lConsumed[lIndex] = true;
                }
            }
        }

        for (Uint64 lIdx = 0; lIdx < m_Actions.size(); ++lIdx)
        {
            ActionEntry&     lEntry = m_Actions[lIdx];
            InputActionState& lState = lEntry.State;

            // Action modifiers apply to the sum (e.g. Normalize clamps the WASD diagonal to 1).
            lState.Value.Value = InputModifiers::ApplyAll(lState.Value.Value, lEntry.Action.Modifiers);

            lState.bTriggered = lState.Value.AsBool();
            // A key held through an unmask is not a new press: no Started.
            lState.bStarted   = lState.bTriggered && !lWasActuated[lIdx] && !lWasMaskSuppressed[lIdx];
            lState.bCompleted = !lState.bTriggered && lWasActuated[lIdx];

            if (lState.bTriggered)
            {
                lState.HeldSeconds += static_cast<float>(InDeltaTime);

                if (!lState.bHoldLatched && lState.HeldSeconds >= lEntry.Action.HoldSeconds)
                {
                    lState.bHold       = true;
                    lState.bHoldLatched = true;
                }
            }
            else
            {
                // Reset rather than kept, so an interruption never leaves a partial hold.
                lState.HeldSeconds  = 0.f;
                lState.bHoldLatched = false;
            }
        }
    }

    // =========================================================================
    // Query
    // =========================================================================
    const InputActionState* InputActionEvaluator::FindState(OpaaxStringID InName) const noexcept
    {
        for (const ActionEntry& lEntry : m_Actions)
        {
            if (lEntry.Action.Name == InName)
            {
                return &lEntry.State;
            }
        }

        return nullptr;
    }

    InputActionValue InputActionEvaluator::GetValue(OpaaxStringID InName) const noexcept
    {
        const InputActionState* lState = FindState(InName);
        return lState != nullptr ? lState->Value : InputActionValue{};
    }
}
