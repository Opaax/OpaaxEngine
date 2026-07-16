#include "World.h"

#include "Core/World/Entity.h"
#include "Core/World/EntityMeta.h"

namespace Opaax
{
    // =========================================================================
    // CTOR - DTOR
    // =========================================================================
    World::World(OpaaxString InName)
        : m_Id(Guid::New())
        , m_Name(std::move(InName))
    {
        OPAAX_LOG(LogWorld, Info, "World '{}' created", m_Name.CStr())
    }

    World::~World()
    {
        OPAAX_LOG(LogWorld, Info, "World '{}' destroyed ({} entity(ies))", m_Name.CStr(), m_EntityCount)
    }

    // =========================================================================
    // Entity lifecycle
    // =========================================================================
    Entity World::CreateEntity(OpaaxString InName)
    {
        const EntityID lEnt  = m_Registry.create();
        EntityMeta&    lMeta = m_Registry.emplace<EntityMeta>(lEnt, EntityMeta{ Guid::New(), std::move(InName) });
        m_Guids.Register(lMeta.Id, lEnt);
        ++m_EntityCount;

        OPAAX_LOG(LogWorld, Trace, "CreateEntity '{}' in world '{}'", lMeta.Name.CStr(), m_Name.CStr())
        return Entity{ lEnt, this };
    }

    void World::DestroyEntity(EntityID InEntity)
    {
        if (!m_Registry.valid(InEntity))
        {
            OPAAX_LOG(LogWorld, Warn, "DestroyEntity — invalid entity ignored")
            return;
        }

        if (const EntityMeta* lMeta = m_Registry.try_get<EntityMeta>(InEntity))
        {
            m_Guids.Unregister(lMeta->Id);
        }

        m_Registry.destroy(InEntity);
        if (m_EntityCount > 0)
        {
            --m_EntityCount;
        }
    }

    Entity World::FindByGuid(const Guid& InGuid)
    {
        // ENTITY_NONE handle -> an invalid Entity (null-safe lookup).
        return Entity{ m_Guids.Resolve(InGuid), this };
    }

    void World::Clear() noexcept
    {
        m_Registry.clear();
        m_Guids.Clear();
        m_EntityCount = 0;

        OPAAX_LOG(LogWorld, Info, "World '{}' cleared", m_Name.CStr())
    }
}
