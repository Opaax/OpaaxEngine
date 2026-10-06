// The Physics module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "Physics/ColliderDebugSubsystem.h"
#include "Physics/Components/ColliderComponent.h"
#include "Physics/Components/RigidbodyComponent.h"
#include "Physics/PhysicsSubsystem.h"

namespace Opaax
{
    // The collider puts an entity in the physics world; the rigidbody (optional) sets the body type.
    OPAAX_REGISTER_NAMED_COMPONENT(ColliderComponent, "Collider");
    OPAAX_REGISTER_NAMED_COMPONENT(RigidbodyComponent, "Rigidbody");

    // Play worlds only: it moves transforms.
    OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM(PhysicsSubsystem, "Physics", WorldSubsystemOrder::Physics);

    // No ShouldCreate: colliders must be visible while editing. The debug channel toggles it.
    OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM(ColliderDebugSubsystem, "ColliderDebug", WorldSubsystemOrder::Debug);
}
