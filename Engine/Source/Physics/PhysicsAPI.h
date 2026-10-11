#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsBackend.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // PhysicsAPI — creates the IPhysicsWorld for an EPhysicsBackend
    //   (implemented in PhysicsBackendFactory.cpp, the only file that includes a backend).
    // =============================================================================
    class PhysicsAPI
    {
        // =============================================================================
        // Creation
        // =============================================================================
    public:
        /** Creates the world for InBackend. Null if the backend is not available (logged). */
        static TUniquePtr<IPhysicsWorld> Create(EPhysicsBackend InBackend,
                                                const PhysicsWorldDesc& InDesc);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The backend used in this run. Set by Create. */
        static EPhysicsBackend GetBackend() noexcept { return s_Backend; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Defined in the .cpp, so there is one value for the whole program. */
        static EPhysicsBackend s_Backend;
    };
}
