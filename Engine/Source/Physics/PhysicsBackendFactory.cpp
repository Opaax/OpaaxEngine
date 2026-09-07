#include "Physics/PhysicsAPI.h"

#include "Application/Services/ILogger.h"

#include "Physics/Box2D/Box2DPhysicsWorld.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(PhysicsBackend);

    EPhysicsBackend PhysicsAPI::s_Backend = EPhysicsBackend::Box2D;

    const char* ToString(EPhysicsBackend InBackend) noexcept
    {
        switch (InBackend)
        {
            case EPhysicsBackend::Box2D: return "Box2D";
        }
        return "Unknown";
    }

    TUniquePtr<IPhysicsWorld> PhysicsAPI::Create(EPhysicsBackend InBackend, const PhysicsWorldDesc& InDesc)
    {
        s_Backend = InBackend;

        switch (InBackend)
        {
            case EPhysicsBackend::Box2D: return MakeUnique<Box2DPhysicsWorld>(InDesc);
        }

        OPAAX_LOG(LogPhysicsBackend, Error, "Backend '{}' is not available.", ToString(InBackend));
        return nullptr;
    }
}
