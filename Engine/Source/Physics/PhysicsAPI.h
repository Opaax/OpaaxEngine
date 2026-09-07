#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsBackend.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // PhysicsAPI — the backend factory, mirroring RHI's device creation.
    //
    //   The single place mapping EPhysicsBackend to a concrete IPhysicsWorld, defined in the
    //   neutral PhysicsBackendFactory.cpp — the only TU above Physics/<Backend>/ that includes
    //   a backend header. Adding a backend is a new enum value plus a new case there; no
    //   consumer above Physics/ changes.
    //
    //   There is no BackendFromString: the config field is a real EPhysicsBackend, so the enum
    //   json bridge parses it and a typo is not expressible (the same thing that retired
    //   RHI's BackendFromString).
    // =============================================================================
    class OPAAX_API PhysicsAPI
    {
        // =============================================================================
        // Creation
        // =============================================================================
    public:
        /** Build the world for InBackend. Null when the backend is not available (logged). */
        static TUniquePtr<IPhysicsWorld> Create(EPhysicsBackend InBackend,
                                                const PhysicsWorldDesc& InDesc);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The backend chosen for this run. Set by Create. */
        static EPhysicsBackend GetBackend() noexcept { return s_Backend; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Defined out-of-line in the DLL, so every module agrees on the one value (I1/I2). */
        static EPhysicsBackend s_Backend;
    };
}
