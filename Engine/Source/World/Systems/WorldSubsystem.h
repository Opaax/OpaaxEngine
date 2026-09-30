#pragma once

#include "Core/Systems/Subsystem.h"

namespace Opaax
{
    // =============================================================================
    // IWorldSubsystem — interface for world subsystems. Lives as long as its world.
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
    // WorldSubsystemMgr — owns and ticks a world's subsystems.
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
