#include "World.h"

namespace Opaax
{
    void World::Clear() noexcept
    {
        m_Registry.clear();
        m_EntityCount.store(0, std::memory_order_relaxed);

        OPAAX_CORE_TRACE("World::Clear() — all entities destroyed.");
    }

    Uint32 World::GetEntityCount() const noexcept
    {
        return m_EntityCount;
    }
} // namespace Opaax::ECS
