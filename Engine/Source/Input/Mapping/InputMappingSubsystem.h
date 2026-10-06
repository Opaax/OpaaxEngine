#pragma once

#include <utility>   // std::forward

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Core/Events/Delegate.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"
#include "Engine/Input/InputActionEvaluator.h"
#include "Engine/Input/InputTypes.h"

namespace Opaax
{
    struct GameInstanceContext;

    inline constexpr LogCategory LogInputMapping{"InputMapping"};

    /** Passed to a bound handler: the action's value when it fired. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnInputAction, const InputActionValue&)

    // =============================================================================
    // InputMappingSubsystem — turns raw keys (InputManager) into named actions.
    //   Game-instance subsystem, so mapping contexts survive level changes.
    //   Updated before the worlds, so action values are ready for gameplay.
    //
    //   Gameplay binds handlers with Bind(action, trigger, this, &T::Handler).
    //   The owner must call UnbindAll(this) in its Shutdown.
    // =============================================================================
    class InputMappingSubsystem final : public GameInstanceSubsystemBase
    {
        // =========================================================================
        // Base Implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(InputMappingSubsystem)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /** @param InContext Stable for the whole game. */
        explicit InputMappingSubsystem(GameInstanceContext& InContext) noexcept;
        ~InputMappingSubsystem() override = default;

        // =========================================================================
        // Actions and contexts
        // =========================================================================
    public:
        /** Declares an action. Refused for an empty or already used name. */
        bool RegisterAction(const InputAction& InAction);

        /**
         * Adds a mapping context. Higher priority is evaluated (and consumes keys) first.
         * Bindings to unknown actions or gamepad codes are skipped with a warning.
         */
        bool AddContext(const InputMappingContext& InContext);

        /**
         * Loads a .opaaxinputmap, registers the actions it uses, and adds it as a context.
         * @param InName      Name to use with RemoveContext
         * @param InAssetPath Asset-relative path ("Input/Gameplay.opaaxinputmap")
         */
        bool AddContextAsset(OpaaxStringID InName, const OpaaxString& InAssetPath);

        /** @return True if a context with that name was removed */
        bool RemoveContext(OpaaxStringID InName);

        // =========================================================================
        // Binding
        // =========================================================================
    public:
        /**
         * Calls InMember on InOwner when InAction fires for InTrigger.
         * @return Handle for Unbind (optional: UnbindAll(this) needs none)
         */
        template<typename T>
        DelegateHandle Bind(OpaaxStringID InAction, EInputTrigger InTrigger, T* InOwner,
                            void (T::*InMember)(const InputActionValue&))
        {
            WarnIfUnknownAction(InAction, InTrigger);
            return DelegateFor(InAction, InTrigger).AddMember(InOwner, InMember);
        }

        /** Lambda version. No owner, so it can only be removed by handle. */
        DelegateHandle Bind(OpaaxStringID InAction, EInputTrigger InTrigger,
                            TFunction<void(const InputActionValue&)> InCallback);

        /** Removes one binding. @return True if removed */
        bool Unbind(OpaaxStringID InAction, EInputTrigger InTrigger, DelegateHandle InHandle);

        /**
         * Removes every member binding owned by InOwner. Call it in the owner's Shutdown.
         * @return Number of bindings removed
         */
        Uint64 UnbindAll(void* InOwner);

        // =========================================================================
        // Query
        // =========================================================================
    public:
        /** This frame's value. Zero for an unknown action. */
        InputActionValue GetValue(OpaaxStringID InAction) const noexcept;

        /** This frame's state, or nullptr. */
        const InputActionState* FindState(OpaaxStringID InAction) const noexcept;

        Uint64 GetContextCount() const noexcept { return m_Evaluator.GetContextCount(); }
        Uint64 GetActionCount()  const noexcept { return m_Evaluator.GetActionCount(); }

        /** Visits every action and its state (editor Input panel). */
        template<typename TFunc>
        void ForEachAction(TFunc&& InFunc) const { m_Evaluator.ForEachAction(std::forward<TFunc>(InFunc)); }

        /** Number of bound handlers. */
        Uint64 GetBindingCount() const noexcept;

        // =========================================================================
        // Keys consumed by the UI
        // =========================================================================
    public:
        /**
         * Marks keys as used this frame (by the UI). They drive no action. Cleared every frame.
         */
        void ConsumeThisFrame(const InputKeyMask& InConsumed);

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin IGameInstanceSubsystem interface
    public:
        bool Startup() override;

        /** Evaluates, then calls the handlers of what fired. Before any world ticks. */
        void Update(double InDeltaTime) override;

        void Shutdown() override;
        //~End IGameInstanceSubsystem interface

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /** The four delegates of an action, found or created. */
        FOnInputAction& DelegateFor(OpaaxStringID InAction, EInputTrigger InTrigger);

        /** Warns once per name when binding an unknown action. */
        void WarnIfUnknownAction(OpaaxStringID InAction, EInputTrigger InTrigger) const;

        // =========================================================================
        // Members
        // =========================================================================
    private:
        struct ActionDelegates
        {
            OpaaxStringID Action;

            /** Indexed by EInputTrigger. */
            TFixedArray<FOnInputAction, INPUT_TRIGGER_COUNT> ByTrigger;
        };

        GameInstanceContext* m_Context = nullptr;

        InputActionEvaluator m_Evaluator;

        TDynArray<ActionDelegates> m_Bindings;

        /** Keys used by the UI this frame. Cleared after Evaluate. */
        InputKeyMask m_PreConsumed{};
    };
}
