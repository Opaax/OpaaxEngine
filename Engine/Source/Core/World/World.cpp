#include "World.h"

#include "ECS/BaseComponents.hpp"

namespace Opaax::ECS
{
    EntityID World::CreateEntity(const char* InTag)
    {
        Uint32 lIndex = 0;
        Uint32 lGeneration = 0;

        if (!m_FreeList.empty())
        {
            // Recycle a destroyed slot
            lIndex = m_FreeList.back();
            m_FreeList.pop_back();
            lGeneration = m_EntitySlots[lIndex].Generation;
        }
        else
        {
            // New slot
            lIndex = static_cast<Uint32>(m_EntitySlots.size());

            if (lIndex > ENTITY_INDEX_MASK)
            {
                OPAAX_CORE_ERROR("World::CreateEntity — entity pool exhausted ({} max).", ENTITY_INDEX_MASK);
                return ENTITY_NONE;
            }

            m_EntitySlots.push_back({ 0, false });
        }

        m_EntitySlots[lIndex].bAlive = true;
        ++m_EntityCount;

        const EntityID lID = EntityID::Make(lIndex, lGeneration);

        // Every entity gets a tag — makes debugging and editor hierarchy trivial.
        GetStorage<TagComponent>().Add(lID, InTag);

        return lID;
    }

    void World::DestroyEntity(EntityID InEntity)
    {
        if (!IsValid(InEntity))
        {
            OPAAX_CORE_WARN("World::DestroyEntity — invalid entity, ignored.");
            return;
        }

        const Uint32 lIndex = InEntity.GetIndex();

        // Remove all known component types for this entity.
        // Only storages that exist are iterated — no cost for unused types.
        for (auto& [lKey, lStorage] : m_Storages)
        {
            // We can't call Remove<T> without the type — use a virtual trampoline.
            // FIXME: Add virtual Remove(EntityID) to IStorageBase to avoid this loop cost.
            //   For now we rely on each StorageWrapper knowing its type via the template.
            //   Acceptable until entity count * component type count becomes large.
            (void)lKey;
            (void)lStorage;
        }

        // NOTE: Component removal on destroy is handled below — each storage exposes
        //   a type-erased Remove via the virtual interface we'll add in the FIXME above.
        //   For now, game code is responsible for removing components before destroying.
        //   This is flagged as a known limitation.
        //
        // FIXME: Implement type-erased Remove(EntityID) on IStorageBase.
        //   Until then, components are leaked in their storages when an entity is destroyed.
        //   They become orphaned (no entity points to them) but consume memory.
        //   Fix this before the scene serialization milestone.

        m_EntitySlots[lIndex].bAlive = false;
        m_EntitySlots[lIndex].Generation = (m_EntitySlots[lIndex].Generation + 1) & ENTITY_GEN_MASK;
        m_FreeList.push_back(lIndex);
        --m_EntityCount;
    }

    bool World::IsValid(EntityID InEntity) const noexcept
    {
        if (!InEntity.IsValid()) { return false; }

        const Uint32 lIndex = InEntity.GetIndex();

        if (lIndex >= static_cast<Uint32>(m_EntitySlots.size())) { return false; }

        const EntitySlot& lSlot = m_EntitySlots[lIndex];

        return lSlot.bAlive && (lSlot.Generation == InEntity.GetGeneration());
    }

} // namespace Opaax::ECS
