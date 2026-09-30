#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;
    struct WorldContext;
    struct InputActionValue;
}

namespace Sandbox
{
    // =============================================================================
    // PlayerControlSubsystem — turns input actions into a MoverComponent's intent (MoverInput). The
    //   modes decide how to move; an AI or a replay could write the same intent.
    //   Names no keys: it adds Input/Gameplay.opaaxinputmap and binds Move, Jump and SwitchMode; the
    //   asset decides the keys, so rebinding needs no recompile.
    //   The handlers run before this subsystem ticks (input lives on the GameInstance, registered
    //   before WorldManager). Play worlds only.
    // =============================================================================
    class PlayerControlSubsystem final : public Opaax::WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(PlayerControlSubsystem)

        static bool ShouldCreate(const Opaax::World& InWorld);

        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        explicit PlayerControlSubsystem(Opaax::WorldContext& InContext) : m_Context(&InContext) {}

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface

        /** Adds the gameplay mapping context and binds its three actions. */
        bool Startup() override;

        /**
         * Writes the gathered intent onto every mover. In Update, not FixedUpdate: input is per frame, and
         * a frame runs 0..N fixed steps.
         */
        void Update(double InDeltaTime) override;

        /**
         * Unbinds. Required: this subsystem dies with its world at Stop while the GameInstance lives on,
         * so a leftover binding would call into a dead object.
         */
        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Action handlers
        // =============================================================================
    private:
        /**
         * Whether this subsystem's world is the one being played. Every handler checks it first: bindings
         * outlive worlds, and during a level swap two Play worlds exist, so the outgoing world would also
         * react to the keys.
         */
        bool IsOwningWorldActive() const;

        /** Triggered: fires every frame the stick/keys are off-centre. */
        void OnMove(const Opaax::InputActionValue& InValue);

        /** Completed: fires the frame it returns to centre (otherwise the last direction sticks). */
        void OnMoveCompleted(const Opaax::InputActionValue& InValue);

        /** Started: the press edge, so holding the key does not re-jump. */
        void OnJump(const Opaax::InputActionValue& InValue);

        /** Started: one switch per press. */
        void OnSwitchMode(const Opaax::InputActionValue& InValue);


        // =============================================================================
        // Members
        // =============================================================================
    private:
        Opaax::WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        // This frame's intent, written by the handlers and used in Update. The two bools are latches: a
        // press between ticks is not lost.
        Opaax::Vector2F m_MoveDir{0.f, 0.f};
        bool            m_bJumpQueued   = false;
        bool            m_bSwitchQueued = false;

        Opaax::Uint64 m_LastDriven       = 0;
        bool          m_bLoggedFirstTick = false;
    };
}
