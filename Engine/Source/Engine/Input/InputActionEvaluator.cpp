#include "Engine/Input/InputActionEvaluator.h"

#include <algorithm>

#include "Engine/Input/InputModifiers.h"
#include "Engine/Subsystems/Input/InputManager.h"

namespace Opaax
{
    namespace
    {
        /** @return the dense index for InKey, or KEY_STATE_COUNT when it is out of range. */
        Uint16 ToKeyIndex(EKeyCode InKey) noexcept
        {
            const Uint16 lRaw = static_cast<Uint16>(InKey);
            return lRaw < InputManager::KEY_STATE_COUNT ? lRaw : InputManager::KEY_STATE_COUNT;
        }

        /**
         * Held, OR pressed-and-released inside this frame.
         *
         * The second half is IN3: a tap that starts and ends between two reads leaves IsKeyDown
         * false, and dropping it is exactly the input loss the edge latches exist to prevent.
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
                OPAAX_LOG(LogInputEvaluator, Warn, "AddContext '{}' — already added.", InContext.Name);
                return false;
            }
        }

        InputMappingContext lAccepted;
        lAccepted.Name     = InContext.Name;
        lAccepted.Priority = InContext.Priority;

        for (const InputKeyBinding& lBinding : InContext.Bindings)
        {
            if (ToKeyIndex(lBinding.Key) == InputManager::KEY_STATE_COUNT)
            {
                // Gamepad codes live at 10000+. They are RESERVED but unfed (IN7), so accepting
                // one would look supported and never fire — refused loudly instead.
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

        m_Contexts.emplace_back(Move(lAccepted));

        // Highest priority first. stable_sort so two contexts at one priority keep the order they
        // were added in, which makes "who consumes first" answerable rather than arbitrary.
        std::stable_sort(m_Contexts.begin(), m_Contexts.end(),
                         [](const InputMappingContext& InLeft, const InputMappingContext& InRight)
                         {
                             return InLeft.Priority > InRight.Priority;
                         });

        OPAAX_LOG(LogInputEvaluator, Info, "Context '{}' added at priority {} — {} of {} binding(s) accepted",
                  InContext.Name, InContext.Priority,
                  static_cast<Uint64>(m_Contexts.back().Bindings.size()),
                  static_cast<Uint64>(InContext.Bindings.size()));

        return true;
    }

    bool InputActionEvaluator::RemoveContext(OpaaxStringID InName)
    {
        for (auto lIt = m_Contexts.begin(); lIt != m_Contexts.end(); ++lIt)
        {
            if (lIt->Name == InName)
            {
                m_Contexts.erase(lIt);
                OPAAX_LOG(LogInputEvaluator, Info, "Context '{}' removed", InName);
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
    void InputActionEvaluator::Evaluate(const InputManager& InInput, double InDeltaTime)
    {
        // Captured BEFORE the values are cleared: Started and Completed are edges against the
        // PREVIOUS frame's actuation, and that is the only thing carried across.
        TDynArray<bool> lWasActuated;
        lWasActuated.reserve(m_Actions.size());

        for (ActionEntry& lEntry : m_Actions)
        {
            lWasActuated.emplace_back(lEntry.State.Value.AsBool());

            lEntry.State.Value.Value = Vector2F{0.f, 0.f};
            lEntry.State.Value.Type  = lEntry.Action.ValueType;
            lEntry.State.bStarted    = false;
            lEntry.State.bTriggered  = false;
            lEntry.State.bCompleted  = false;
            lEntry.State.bHold       = false;
        }

        // Consumption is per KEY and lives for one frame only.
        TFixedArray<bool, InputManager::KEY_STATE_COUNT> lConsumed{};

        for (const InputMappingContext& lContext : m_Contexts)
        {
            for (const InputKeyBinding& lBinding : lContext.Bindings)
            {
                const Uint16 lIndex = ToKeyIndex(lBinding.Key);
                if (lIndex == InputManager::KEY_STATE_COUNT || lConsumed[lIndex])
                {
                    continue;
                }

                const bool bActuated = IsActuated(InInput, lBinding.Key);

                ActionEntry* lEntry = FindEntry(lBinding.Action);
                if (lEntry == nullptr)
                {
                    continue;
                }

                // The raw value rides in x; Negate and Swizzle are what move it elsewhere, which
                // is how four keys become one Axis2D without a composite concept in the format.
                const Vector2F lRaw = Vector2F{bActuated ? 1.f : 0.f, 0.f};

                lEntry->State.Value.Value += InputModifiers::ApplyAll(lRaw, lBinding.Modifiers);

                // Consume only what is actually pressed: swallowing an untouched key would be a
                // no-op with a confusing name.
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

            lState.bTriggered = lState.Value.AsBool();
            lState.bStarted   = lState.bTriggered && !lWasActuated[lIdx];
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
                // Zeroed rather than remembered, which is what makes a route close self-healing:
                // nothing carries a partial hold across an interruption the reader never saw.
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
