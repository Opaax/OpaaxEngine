#include "World.h"

#include <Entt/entt/src/entt/entity/registry.hpp>

namespace Opaax::ECS
{
    Uint32 World::GetEntityCount() const noexcept
    {
        return m_EntityCount;
    }
} // namespace Opaax::ECS
