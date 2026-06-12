// PhysicsBackendFactory.cpp
//
// The single neutral TU that maps EPhysicsBackend -> a concrete IPhysicsWorld. It is
// the ONLY place above the backend directories that includes a backend header, mirroring
// RHI/BackendFactory.cpp. Adding a backend = a new enum value + a new case here.

#include "Physics/PhysicsAPI.h"

#include "Core/Log/OpaaxLog.h"

#include "Physics/Box2D/Box2DPhysicsWorld.h"

namespace Opaax
{
    EPhysicsBackend PhysicsAPI::s_Backend = EPhysicsBackend::Box2D;

    UniquePtr<IPhysicsWorld> PhysicsAPI::Create(EPhysicsBackend InBackend, const PhysicsWorldDesc& InDesc)
    {
        s_Backend = InBackend;

        switch (InBackend)
        {
            case EPhysicsBackend::Box2D: return MakeUnique<Box2DPhysicsWorld>(InDesc);
            default: break;
        }

        OPAAX_CORE_ERROR("PhysicsAPI::Create — backend '{}' not available.", BackendToString(InBackend));
        return nullptr;
    }

    EPhysicsBackend PhysicsAPI::BackendFromString(const OpaaxString& InName)
    {
        if (InName == "Box2D") { return EPhysicsBackend::Box2D; }

        OPAAX_CORE_WARN("PhysicsAPI — unknown physics backend '{}' — falling back to Box2D.", InName);
        return EPhysicsBackend::Box2D;
    }

    const char* PhysicsAPI::BackendToString(EPhysicsBackend InBackend) noexcept
    {
        switch (InBackend)
        {
            case EPhysicsBackend::Box2D: return "Box2D";
        }
        return "Unknown";
    }

} // namespace Opaax
