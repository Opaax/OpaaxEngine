#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Input/InputTypes.h"

namespace Opaax
{
    class InputManager;

    inline constexpr LogCategory LogInputEvaluator{"InputEvaluator"};

    // =============================================================================
    // InputActionEvaluator — turns held keys into named action values, once per frame.
    //
    //   THE WHOLE TRANSFORM HALF OF INPUT MAPPING, with no subsystem, no world and no
    //   resources: it takes a const InputManager& and answers per-action state. That is what
    //   makes every rule below testable headlessly — priority, consumption, the four trigger
    //   phases, dead zones and the same-frame tap.
    //
    //   IT RE-DERIVES EVERYTHING EVERY FRAME and keeps only HeldSeconds, which is zeroed the
    //   moment an action is not actuated. That is what makes IN5 self-healing: when the route
    //   closes, InputManager::ResetState reports everything up, so no action stays held and a
    //   Hold cannot fire on re-entry. There is no state here to clear.
    //
    //   CONTEXTS ARE A STACK sorted by priority, highest first. A binding whose KEY a
    //   higher-priority context already consumed is skipped — per key, not per action, which
    //   is what makes a menu context stop Jump from firing rather than merely outrank it.
    // =============================================================================
    class OPAAX_API InputActionEvaluator
    {
        // =========================================================================
        // Actions
        // =========================================================================
    public:
        /**
         * Declare an action: its name, its value shape, and how long its Hold takes.
         *
         * Refused (and logged) for an invalid name or a name already taken — two actions
         * answering to one name is a binding that silently drives the wrong thing.
         *
         * @return true when the evaluator accepted it.
         */
        bool RegisterAction(const InputAction& InAction);

        const InputAction* FindAction(OpaaxStringID InName) const noexcept;

        Uint64 GetActionCount() const noexcept { return static_cast<Uint64>(m_Actions.size()); }

        // =========================================================================
        // Contexts
        // =========================================================================
    public:
        /**
         * Push a mapping context, keeping the stack sorted by priority (highest first).
         *
         * Bindings are VALIDATED here rather than per frame: one naming an unregistered action
         * is skipped with a warning, and so is one naming a GAMEPAD code — those are reserved
         * in EKeyCode but have no feed (IN7), and a binding that silently never fires is the
         * failure this engine refuses.
         *
         * IDEMPOTENT by name. Two Play worlds coexist during a level swap, so each one's control
         * subsystem adds the same context and the second call is routine, not a mistake.
         *
         * @return true when the context is active — whether this call is what added it or not.
         */
        bool AddContext(const InputMappingContext& InContext);

        /** @return true when a context of that name was removed. */
        bool RemoveContext(OpaaxStringID InName);

        Uint64 GetContextCount() const noexcept { return static_cast<Uint64>(m_Contexts.size()); }

        /** Priority of the context at InIndex in evaluation order (highest first). For tests. */
        const InputMappingContext* GetContextAt(Uint64 InIndex) const noexcept;

        // =========================================================================
        // Per frame
        // =========================================================================
    public:
        /**
         * Recompute every action's value and trigger phases from InInput.
         *
         * Runs BEFORE any world subsystem reads it — GameInstanceManager is registered ahead of
         * WorldManager, so UpdateAll reaches this first (BO4d).
         */
        void Evaluate(const InputManager& InInput, double InDeltaTime);

        // =========================================================================
        // Query
        // =========================================================================
    public:
        const InputActionState* FindState(OpaaxStringID InName) const noexcept;

        /** Zero for an action that does not exist — a query, not a mistake worth logging. */
        InputActionValue GetValue(OpaaxStringID InName) const noexcept;

        // =========================================================================
        // Members
        // =========================================================================
    private:
        /**
         * The action and its live state together, found by a LINEAR scan.
         *
         * A flat array, like every other name lookup in the engine (MoverData::FindExact,
         * WorldSubsystemRegistry::FindByName). A game has a dozen actions; hashing that would
         * cost more than the scan and would need a std::hash for OpaaxStringID that nothing
         * else in the tree has wanted yet.
         */
        struct ActionEntry
        {
            InputAction      Action;
            InputActionState State;
        };

        ActionEntry* FindEntry(OpaaxStringID InName) noexcept;

        TDynArray<ActionEntry>         m_Actions;

        /** Sorted by Priority DESCENDING — evaluation order and consumption order are one list. */
        TDynArray<InputMappingContext> m_Contexts;
    };
}
