#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // MoverSubsystem
    // =============================================================================
    /**
     * @class MoverSubsystem
     *
     * Drives every entity carrying a MoverComponent through its active IMoverMode each fixed step,
     * applying any queued mode switch (MoverComponent::QueueNextMode) first. Play-only; registered
     * AFTER PhysicsSubsystem so it sweeps against settled geometry, reusing that subsystem's
     * IPhysicsWorld (it owns no world of its own).
     *
     * Modes live in the process-global MoverModeRegistry (the editor reads the same catalog). This
     * subsystem registers the built-ins (GroundMove, Fly) at Startup and clears the registry at
     * Shutdown; game/engine code adds more via MoverModeRegistry::Register — new movement never
     * touches MoverComponent or this subsystem (the anti-monolith seam).
     */
    class OPAAX_API MoverSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(MoverSubsystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        MoverSubsystem() = default;
        explicit MoverSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        ~MoverSubsystem() override = default;

        MoverSubsystem(const MoverSubsystem&)            = delete;
        MoverSubsystem& operator=(const MoverSubsystem&) = delete;
        MoverSubsystem(MoverSubsystem&&)                 = default;
        MoverSubsystem& operator=(MoverSubsystem&&)      = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()                          override;
        void FixedUpdate(double FixedDeltaTime) override;
        void Shutdown()                         override;

        bool IsPlayOnly() const noexcept        override { return true; }
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Mode ids already warned about as unknown — so a misconfigured component warns once,
        // not every entity every step.
        UnorderedSet<Uint32> m_WarnedUnknownModes;
    };

} // namespace Opaax
