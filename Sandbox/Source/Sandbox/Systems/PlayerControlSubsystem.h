#pragma once

#include "Application/Services/ILogger.h"
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
    // PlayerControlSubsystem — turns ACTIONS into a MoverComponent's INTENT.
    //
    //   THE GAME'S SIDE OF THE MOVER SEAM. A mode reads intent and decides how to move; nothing
    //   in the engine decides that A and D are how a human asks. That split is why MoverInput
    //   exists at all — an AI, a cutscene or a replay writes the same fields and every mode works
    //   unchanged.
    //
    //   IT NAMES NO KEYS (⑦-B B2). It adds `Input/Gameplay.opaaxinputmap` and binds Move, Jump
    //   and SwitchMode; which keys reach those is the asset's business, so rebinding never
    //   recompiles anything. There is deliberately no EKeyCode anywhere in this game module.
    //
    //   THE HANDLERS RUN BEFORE THIS SUBSYSTEM TICKS. InputMappingSubsystem lives on the
    //   GameInstance, which is registered ahead of WorldManager, so by the time Update runs the
    //   frame's callbacks have already written the intent below (BO4d).
    //
    //   It also restores the GAME-MODULE world-subsystem route, which has had no caller in either
    //   host since QuadOscillatorSubsystem was unregistered — so **MR4**'s "a game module and an
    //   editor module land in one candidate list" is dogfooded again rather than asserted.
    //
    //   Play worlds only: it moves things.
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

        /** Adds the gameplay mapping context and binds the three actions it drives. */
        bool Startup() override;

        /**
         * Write the intent the handlers gathered onto every mover.
         *
         * In UPDATE, not FixedUpdate. Input is a per-FRAME thing (**IN2**) and a frame runs 0..N
         * fixed steps — sampling it per step would read the same press several times, or none.
         */
        void Update(double InDeltaTime) override;

        /**
         * UNBINDS. This is the lifetime contract of the bind surface, not housekeeping: this
         * subsystem dies with its world at PIE Stop while the GameInstance is still alive for one
         * more step, so a binding left behind is a dangling call on the next Broadcast.
         */
        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Action handlers
        // =============================================================================
    private:
        /**
         * Whether this subsystem's world is the one currently being played.
         *
         * EVERY handler below asks first, and that is not defensive coding — it is required.
         * Bindings live on the GameInstance and outlive any single world, so a callback reaches
         * EVERY bound listener, while only the ACTIVE world ticks. Two Play worlds coexist during
         * a level swap (OpenLevel creates the new one before destroying the old), and without
         * this the outgoing world's control subsystem would also react to the player's keys.
         *
         * Harmless for the handlers below, which only write members their own Update reads — and
         * NOT harmless for the first handler that spawns something. The rule belongs at the top
         * of the handler, once, rather than in the memory of whoever writes the next one.
         *
         * World MODE cannot answer this: both worlds in a level swap are Play.
         */
        bool IsOwningWorldActive() const;

        /** Triggered: fires every frame the stick/keys are off-centre. */
        void OnMove(const Opaax::InputActionValue& InValue);

        /** Completed: fires the frame it returns to centre. Without it the last direction sticks. */
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

        // This frame's intent, written by the handlers above and spent in Update. The two bools
        // are LATCHES: a press that arrives between ticks must not be lost.
        Opaax::Vector2F m_MoveDir{0.f, 0.f};
        bool            m_bJumpQueued   = false;
        bool            m_bSwitchQueued = false;

        Opaax::Uint64 m_LastDriven       = 0;
        bool          m_bLoggedFirstTick = false;
    };
}
