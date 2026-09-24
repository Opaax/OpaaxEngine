#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"

#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    // =============================================================================
    // EPhysicsBackend
    // =============================================================================
    /**
     * @enum EPhysicsBackend
     *
     * Physics backend selector. Box2D exists today; a custom solver or a second
     * library are the intended future entries. Selection happens once, in
     * PhysicsSubsystem::Startup.
     */
    enum class EPhysicsBackend
    {
        Box2D
    };

    // =============================================================================
    // PhysicsAPI
    // =============================================================================
    /**
     * @class PhysicsAPI
     *
     * Backend factory + registry — the single place that maps EPhysicsBackend to a
     * concrete IPhysicsWorld. Defined in the neutral PhysicsBackendFactory.cpp (the
     * only TU that includes a backend header). Adding a backend means a new enum value
     * + a new case there — no consumer above Physics/ changes. Mirrors RHI's RenderAPI.
     */
    class OPAAX_API PhysicsAPI
    {
    public:
        static UniquePtr<IPhysicsWorld> Create(EPhysicsBackend InBackend, const PhysicsWorldDesc& InDesc);

        // Map a config string ("Box2D") to EPhysicsBackend. Unknown -> Box2D (logged).
        static EPhysicsBackend          BackendFromString(const OpaaxString& InName);
        static const char*              BackendToString(EPhysicsBackend InBackend) noexcept;

        // The backend chosen for this run. Set by Create.
        static EPhysicsBackend          GetBackend() noexcept { return s_Backend; }

    private:
        static EPhysicsBackend s_Backend;
    };

} // namespace Opaax
