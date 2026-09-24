#pragma once

#include "Core/Systems/Subsystem.h"

namespace Opaax
{
    // =============================================================================
    // IWorldSubsystem — marker interface for world-owned subsystems.
    //   Lifetime = world
    // =============================================================================
    class OPAAX_API IWorldSubsystem : public Opaax::ISubsystem
    {
    };

	class OPAAX_API WorldSubsystemBase : public Opaax::IWorldSubsystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        WorldSubsystemBase()          = default;
        ~WorldSubsystemBase() override = default;

   		// =============================================================================
        // Copy - Move Delete
        // =============================================================================
        WorldSubsystemBase(const WorldSubsystemBase&)            = delete;
        WorldSubsystemBase& operator=(const WorldSubsystemBase&) = delete;
        WorldSubsystemBase(WorldSubsystemBase&&)                 = delete;
        WorldSubsystemBase& operator=(WorldSubsystemBase&&)      = delete;
    };

	// =============================================================================
    // EngineSubsystemMgr — owns + drives the world subsystem list.
    // =============================================================================
    class OPAAX_API WorldSubsystemMgr : public ISubsystemManager<IWorldSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~WorldSubsystemMgr() override = default;
    };
}
