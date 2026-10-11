#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Input/Mapping/InputTypes.h"
#include "Input/InputManager.h"   // KEY_STATE_COUNT

namespace Opaax
{
    inline constexpr LogCategory LogInputEvaluator{"InputEvaluator"};

    /** Per-key "already consumed this frame" flags (e.g. keys the UI used). */
    using InputKeyMask = TFixedArray<bool, InputManager::KEY_STATE_COUNT>;

    // =============================================================================
    // InputActionEvaluator — turns held keys into named action values, once per frame.
    //   No world or resources needed, so it is fully testable.
    //   Everything is recomputed each frame (only HeldSeconds is kept).
    //   Contexts are sorted by priority, highest first; a key consumed by a higher
    //   context is skipped by lower ones.
    // =============================================================================
    class InputActionEvaluator
    {
        // =========================================================================
        // Actions
        // =========================================================================
    public:
        /**
         * Declares an action: name, value type and hold duration.
         * Refused (and logged) if the name is invalid or already used.
         * @return True if added
         */
        bool RegisterAction(const InputAction& InAction);

        const InputAction* FindAction(OpaaxStringID InName) const noexcept;

        Uint64 GetActionCount() const noexcept { return static_cast<Uint64>(m_Actions.size()); }

        /**
         * Visits every action and its state, in registration order (editor Input panel).
         */
        template<typename TFunc>
        void ForEachAction(TFunc&& InFunc) const
        {
            for (const ActionEntry& lEntry : m_Actions)
            {
                InFunc(lEntry.Action, lEntry.State);
            }
        }

        // =========================================================================
        // Contexts
        // =========================================================================
    public:
        /**
         * Adds a mapping context, keeping the stack sorted by priority (highest first).
         * Bindings to unknown actions or gamepad codes are skipped with a warning.
         * Adding a context that is already active does nothing.
         * @return True if the context is active
         */
        bool AddContext(const InputMappingContext& InContext);

        /** @return True if a context with that name was removed */
        bool RemoveContext(OpaaxStringID InName);

        Uint64 GetContextCount() const noexcept { return static_cast<Uint64>(m_Contexts.size()); }

        /** Priority of the context at InIndex (highest first). For tests. */
        const InputMappingContext* GetContextAt(Uint64 InIndex) const noexcept;

        // =========================================================================
        // Per frame
        // =========================================================================
    public:
        /**
         * Recomputes every action's value and trigger phases from InInput.
         * @param InPreConsumed Keys the UI already used this frame (they drive no action). May be null.
         */
        void Evaluate(const InputManager& InInput, double InDeltaTime, const InputKeyMask* InPreConsumed = nullptr);

        // =========================================================================
        // Query
        // =========================================================================
    public:
        const InputActionState* FindState(OpaaxStringID InName) const noexcept;

        /** Zero for an unknown action. */
        InputActionValue GetValue(OpaaxStringID InName) const noexcept;

        // =========================================================================
        // Members
        // =========================================================================
    private:
        /**
         * An action and its state (linear search: there are few actions).
         */
        struct ActionEntry
        {
            InputAction      Action;
            InputActionState State;
        };

        ActionEntry* FindEntry(OpaaxStringID InName) noexcept;

        TDynArray<ActionEntry>         m_Actions;

        /** Sorted by priority, highest first. */
        TDynArray<InputMappingContext> m_Contexts;
    };
}
