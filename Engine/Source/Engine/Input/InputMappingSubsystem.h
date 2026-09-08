#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Core/Events/Delegate.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"
#include "Engine/Input/InputActionEvaluator.h"
#include "Engine/Input/InputTypes.h"

namespace Opaax
{
    struct GameInstanceContext;

    inline constexpr LogCategory LogInputMapping{"InputMapping"};

    /** What a bound handler receives: the action's value at the moment it fired. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnInputAction, const InputActionValue&)

    // =============================================================================
    // InputMappingSubsystem — the layer that turns KEYS into MEANING, and the first tenant
    //   of the GameInstance tier.
    //
    //   The other half of the input chain. Engine/Subsystems/Input/InputManager is the raw
    //   end — what is held, what changed this frame, physical codes only, and its header says
    //   outright that "there is no 'Jump' in here". This is where there is one.
    //
    //   SESSION-SCOPED, not world-scoped, and that is the requirement that chose the tier: a
    //   pushed mapping context must survive level travel, and a UI context must outlive any
    //   single world. It also means the stack cannot leak across a PIE cycle — Stop destroys
    //   the whole game instance rather than resetting anything (GI6).
    //
    //   TICKS BEFORE EVERY WORLD (GameInstanceManager is registered before WorldManager), so
    //   this frame's action values are published before any gameplay subsystem reads them.
    //
    //   GAMEPLAY BINDS; it does not poll. GetValue exists and reads the same table, but the
    //   primary surface is Bind(action, trigger, this, &T::Handler) — and the ONE rule that
    //   comes with it is that the owner must UnbindAll(this) in its Shutdown. A world
    //   subsystem dies at PIE Stop while this is still alive for one more step (BO4d's
    //   teardown order), so an un-removed binding is a dangling call on the next Broadcast —
    //   which is exactly what TMulticastDelegate's own header warns about.
    // =============================================================================
    class OPAAX_API InputMappingSubsystem final : public GameInstanceSubsystemBase
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
        /** @param InContext BORROWED, and stable for the whole game (GameInstance owns it). */
        explicit InputMappingSubsystem(GameInstanceContext& InContext) noexcept;
        ~InputMappingSubsystem() override = default;

        // =========================================================================
        // Actions and contexts
        // =========================================================================
    public:
        /** Declare an action. Refused for an empty or already-taken name. */
        bool RegisterAction(const InputAction& InAction);

        /**
         * Push a mapping context. Higher priority is evaluated — and consumes — first.
         *
         * Bindings naming an unregistered action, or a gamepad code (IN7, no feed), are
         * skipped with a warning rather than accepted and silently dead.
         */
        bool AddContext(const InputMappingContext& InContext);

        /**
         * Load a `.opaaxinputmap`, resolve every action it references, and push it under InName.
         *
         * THE ASSET ROUTE, and where the two forms of a mapping meet: on disk an entry names its
         * action by PATH (so the editor field is a resource picker), and the evaluator needs it by
         * NAME. Resolution happens HERE, once per AddContext — never per key per frame.
         *
         * Each referenced action is registered on first sight, so a context is self-sufficient:
         * nothing has to declare the actions before adding the map that uses them.
         *
         * @param InName      What RemoveContext will ask for. The asset carries no name of its own.
         * @param InAssetPath Asset-relative ("Input/Gameplay.opaaxinputmap").
         */
        bool AddContextAsset(OpaaxStringID InName, const OpaaxString& InAssetPath);

        /** @return true when a context of that name was pushed and is now gone. */
        bool RemoveContext(OpaaxStringID InName);

        // =========================================================================
        // Binding
        // =========================================================================
    public:
        /**
         * Call InMember on InOwner whenever InAction fires for InTrigger.
         *
         * @return the handle for a targeted Unbind. Keeping it is OPTIONAL — UnbindAll(this)
         *   in the owner's Shutdown is the contract, and it needs no handle.
         */
        template<typename T>
        DelegateHandle Bind(OpaaxStringID InAction, EInputTrigger InTrigger, T* InOwner,
                            void (T::*InMember)(const InputActionValue&))
        {
            WarnIfUnknownAction(InAction, InTrigger);
            return DelegateFor(InAction, InTrigger).AddMember(InOwner, InMember);
        }

        /** Lambda form. It has NO owner, so it can only be removed by handle. */
        DelegateHandle Bind(OpaaxStringID InAction, EInputTrigger InTrigger,
                            TFunction<void(const InputActionValue&)> InCallback);

        /** Remove one registration. @return true when one was removed. */
        bool Unbind(OpaaxStringID InAction, EInputTrigger InTrigger, DelegateHandle InHandle);

        /**
         * Remove EVERY member-binding owned by InOwner, across every action and trigger.
         *
         * THE LIFETIME CONTRACT. One call in the owner's Shutdown; without it a destroyed
         * subsystem is still in a delegate list and the next Broadcast calls into freed memory.
         *
         * @return how many registrations were removed.
         */
        Uint64 UnbindAll(void* InOwner);

        // =========================================================================
        // Query
        // =========================================================================
    public:
        /** This frame's value. Zero for an action that does not exist. */
        InputActionValue GetValue(OpaaxStringID InAction) const noexcept;

        /** This frame's full state, or nullptr. */
        const InputActionState* FindState(OpaaxStringID InAction) const noexcept;

        Uint64 GetContextCount() const noexcept { return m_Evaluator.GetContextCount(); }
        Uint64 GetActionCount()  const noexcept { return m_Evaluator.GetActionCount(); }

        /** How many handlers are registered, across every action and trigger. */
        Uint64 GetBindingCount() const noexcept;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin IGameInstanceSubsystem interface
    public:
        bool Startup() override;

        /** Evaluate, then broadcast whatever fired. Both happen before any world ticks. */
        void Update(double InDeltaTime) override;

        void Shutdown() override;
        //~End IGameInstanceSubsystem interface

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /** The four delegate slots for one action, found or created. Out-of-line for Bind<T>. */
        FOnInputAction& DelegateFor(OpaaxStringID InAction, EInputTrigger InTrigger);

        /** A bind naming an action nothing registered will never fire. Say so, once per name. */
        void WarnIfUnknownAction(OpaaxStringID InAction, EInputTrigger InTrigger) const;

        // =========================================================================
        // Members
        // =========================================================================
    private:
        struct ActionDelegates
        {
            OpaaxStringID Action;

            /** Indexed by EInputTrigger. Four slots, so dispatch is an index, not a search. */
            TFixedArray<FOnInputAction, INPUT_TRIGGER_COUNT> ByTrigger;
        };

        GameInstanceContext* m_Context = nullptr;

        InputActionEvaluator m_Evaluator;

        TDynArray<ActionDelegates> m_Bindings;
    };
}
