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

    void World::DestroyEntity(EntityID InEntity, bool bDestroyChildren)
    {
        if (!IsValid(InEntity))
        {
            OPAAX_CORE_WARN("World::DestroyEntity — invalid entity, ignored.");
            return;
        }

        // Snapshot direct children — we cannot mutate the registry while iterating its view.
        TDynArray<EntityID> lChildren;
        {
            auto lView = m_Registry.view<ECS::ParentComponent>();
            for (auto lEnt : lView)
            {
                if (lView.get<ECS::ParentComponent>(lEnt).Parent == InEntity)
                {
                    lChildren.push_back(lEnt);
                }
            }
        }

        if (bDestroyChildren)
        {
            for (EntityID lChild : lChildren)
            {
                DestroyEntity(lChild, true);
            }
        }
        else
        {
            // Promote children to root — preserves their data.
            for (EntityID lChild : lChildren)
            {
                if (m_Registry.valid(lChild) && m_Registry.all_of<ECS::ParentComponent>(lChild))
                {
                    m_Registry.remove<ECS::ParentComponent>(lChild);
                }
            }
        }

        m_Registry.destroy(InEntity);
        m_EntityCount.fetch_sub(1, std::memory_order_relaxed);
    }

    EntityID World::FindByUuid(Uint64 InUuid) const noexcept
    {
        if (InUuid == 0) { return ENTITY_NONE; }

        auto lView = m_Registry.view<const ECS::UuidComponent>();
        for (auto lEnt : lView)
        {
            if (lView.get<const ECS::UuidComponent>(lEnt).Id == InUuid)
            {
                return lEnt;
            }
        }
        return ENTITY_NONE;
    }
} // namespace Opaax::ECS
