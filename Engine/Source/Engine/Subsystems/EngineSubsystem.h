#pragma once

#include "Core/Systems/Subsystem.h"

namespace Opaax
{
    // =============================================================================
    // IEngineSubsystem — interface for engine subsystems. Live from engine start to engine stop.
    // =============================================================================
    class IEngineSubsystem : public Opaax::ISubsystem
    {
    };

    // =============================================================================
    // EngineSubsystemBase — base for concrete engine subsystems.
    //   Add OPAAX_SUBSYSTEM_TYPE(ClassName) to the concrete class.
    // =============================================================================
    class EngineSubsystemBase : public Opaax::IEngineSubsystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        EngineSubsystemBase()          = default;
        ~EngineSubsystemBase() override = default;

        // Owned by the manager, never copied or moved.
        EngineSubsystemBase(const EngineSubsystemBase&)            = delete;
        EngineSubsystemBase& operator=(const EngineSubsystemBase&) = delete;
        EngineSubsystemBase(EngineSubsystemBase&&)                 = delete;
        EngineSubsystemBase& operator=(EngineSubsystemBase&&)      = delete;
    };

    // =============================================================================
    // EngineSubsystemMgr — owns and ticks the engine subsystems (registration order;
    //   reverse order for shutdown).
    // =============================================================================
    class EngineSubsystemMgr : public ISubsystemManager<IEngineSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~EngineSubsystemMgr() override = default;
    };
}
