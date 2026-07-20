#pragma once

#include "Core/Systems/Subsystem.h"

namespace Opaax
{
    // =============================================================================
    // IEngineSubsystem — marker interface for engine-owned subsystems.
    //   Lifetime = engine (up on engine start, down on engine stop). Managed by
    //   EngineSubsystemMgr. No CoreEngineApp coupling — the service-locator world
    //   reaches shared facilities through OpaaxApplication::GetAppService<T>().
    // =============================================================================
    class OPAAX_API IEngineSubsystem : public Opaax::ISubsystem
    {
    };

    // =============================================================================
    // EngineSubsystemBase — ctor/dtor boilerplate for concrete engine subsystems.
    //   Leaves Startup()/Shutdown() pure (each concrete implements them) and inherits
    //   the no-op Update/FixedUpdate/Render from ISubsystem. Stamp the concrete with
    //   OPAAX_SUBSYSTEM_TYPE(ClassName) for GetTypeID/StaticTypeID.
    // =============================================================================
    class OPAAX_API EngineSubsystemBase : public Opaax::IEngineSubsystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        EngineSubsystemBase()          = default;
        ~EngineSubsystemBase() override = default;

        // Heap-owned via UniquePtr in the manager — never copied or moved.
        EngineSubsystemBase(const EngineSubsystemBase&)            = delete;
        EngineSubsystemBase& operator=(const EngineSubsystemBase&) = delete;
        EngineSubsystemBase(EngineSubsystemBase&&)                 = delete;
        EngineSubsystemBase& operator=(EngineSubsystemBase&&)      = delete;
    };

    // =============================================================================
    // EngineSubsystemMgr — owns + drives the engine subsystem list. Inherits
    //   Register/Startup/Update/FixedUpdate/Render/Shutdown from ISubsystemManager
    //   (registration order; reverse order for shutdown).
    // =============================================================================
    class OPAAX_API EngineSubsystemMgr : public ISubsystemManager<IEngineSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~EngineSubsystemMgr() override = default;
    };
}
