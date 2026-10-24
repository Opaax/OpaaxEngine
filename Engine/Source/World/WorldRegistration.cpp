// The World module's types. Each registers itself; see Engine/Registries/AutoRegistration.h.
#include "Engine/Registries/AutoRegistration.h"
#include "World/Behaviour/BehaviourSubsystem.h"
#include "World/Level.h"
#include "World/World.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Prefab/PrefabAsset.h"
#include "World/Serialization/LevelAsset.h"
#include "World/Serialization/MapAsset.h"

namespace Opaax
{
    // Every entity has one (World::CreateEntity adds it); it cannot be removed.
    OPAAX_REGISTER_ESSENTIAL_COMPONENT(TransformComponent, "Transform");

    // Which prefab an entity comes from. Saved in the map like any component.
    OPAAX_REGISTER_NAMED_COMPONENT(PrefabInstanceComponent, "PrefabInstance");

    OPAAX_REGISTER_OBJECT(LevelAsset);
    OPAAX_REGISTER_OBJECT(MapAsset);

    // A prefab is loaded once, however many instances a level places.
    OPAAX_REGISTER_OBJECT(PrefabAsset);

    // Runs gameplay behaviours in Play worlds, before physics so what they apply is simulated this step.
    OPAAX_REGISTER_OBJECT(BehaviourSubsystem);

    // Made by WorldManager, never by name: registered so tools can name them.
    OPAAX_REGISTER_OBJECT(World);
    OPAAX_REGISTER_OBJECT(Level);
}
