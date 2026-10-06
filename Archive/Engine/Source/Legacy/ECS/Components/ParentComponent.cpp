#include "ParentComponent.h"

namespace Opaax::ECS
{
    json ParentComponent::Serialize() const
    {
        // The scene serializer writes parent_uuid at the entity level — runtime EntityIDs
        // are not stable across save/load, so nothing useful goes here.
        return json::object();
    }

    void ParentComponent::DeserializeImplementation(const json& /*Json*/)
    {
        // Intentionally empty: parent links are restored by SceneSerializer after a UUID lookup.
    }
}
