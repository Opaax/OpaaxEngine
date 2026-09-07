#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;
    struct WorldContext;
}

namespace Sandbox
{
    // =============================================================================
    // PlayerControlSubsystem — turns keys into a MoverComponent's INTENT.
    //
    //   THE GAME'S SIDE OF THE MOVER SEAM. A mode reads intent and decides how to move; nothing
    //   in the engine decides that A and D are how a human asks. That split is why MoverInput
    //   exists at all — an AI, a cutscene or a replay writes the same fields and every mode works
    //   unchanged.
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
        bool Startup() override;

        /**
         * Read in UPDATE, not FixedUpdate. Input is a per-FRAME thing (**IN2**) and a frame runs
         * 0..N fixed steps — sampling it per step would read the same keypress several times, or
         * none at all.
         */
        void Update(double InDeltaTime) override;

        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Opaax::WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        Opaax::Uint64 m_LastDriven      = 0;
        bool          m_bLoggedFirstTick = false;
    };
}
