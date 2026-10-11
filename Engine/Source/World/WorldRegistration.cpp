// The World module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Serialization/LevelResource.hpp"
#include "World/Serialization/MapResource.hpp"

namespace Opaax
{
    // Every entity has one (World::CreateEntity adds it); it cannot be removed.
    OPAAX_REGISTER_ESSENTIAL_COMPONENT(TransformComponent, "Transform");

    // Which prefab an entity comes from. Saved in the map like any component.
    OPAAX_REGISTER_NAMED_COMPONENT(PrefabInstanceComponent, "PrefabInstance");

    OPAAX_REGISTER_NAMED_RESOURCE(LevelResource, "Level");
    OPAAX_REGISTER_NAMED_RESOURCE(MapResource, "Map");

    // A prefab is loaded once, however many instances a level places.
    OPAAX_REGISTER_NAMED_RESOURCE(PrefabResource, "Prefab");

    // Runs gameplay behaviours in Play worlds, before physics so what they apply is simulated this step.
    OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM(BehaviourSubsystem, "Behaviours", WorldSubsystemOrder::Gameplay);
}
