#include "World.h"

#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"

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

    void World::AddEntityCount()
    {
        ++m_EntityCount;
        LogEntityCount();
    }
    
    void World::RemoveEntityCount()
    {
        if (m_EntityCount <= 0 )
        {
            return;
        }
        
        --m_EntityCount;
        LogEntityCount();
    }

    void World::LogEntityCount()
    {
        OPAAX_LOG(LogWorld, Info, "Entity count in world '{}' = {}", m_Name.CStr(), m_EntityCount)
    }

    // =========================================================================
    // Entity lifecycle
    // =========================================================================
    Entity World::CreateEntity(OpaaxString InName)
    {
        const EntityID lEnt  = m_Registry.create();
        EntityMeta&    lMeta = m_Registry.emplace<EntityMeta>(lEnt, EntityMeta{ Guid::New(), Move(InName) });
        m_Guids.Register(lMeta.Id, lEnt);
        OPAAX_LOG(LogWorld, Trace, "CreateEntity '{}' in world '{}'", lMeta.Name.CStr(), m_Name.CStr())
        
        AddEntityCount();
        
        return Entity{ lEnt, this };
    }

    void World::DestroyEntity(Entity InEntity)
    {
        if (InEntity.IsValid())
        {
            DestroyEntity(InEntity.GetHandle());
        }
    }

    void World::DestroyEntity(EntityID InEntity)
    {
        if (!m_Registry.valid(InEntity))
        {
            OPAAX_LOG(LogWorld, Warn, "DestroyEntity — invalid entity ignored")
            return;
        }
        
        const EntityMeta* lMeta = m_Registry.try_get<EntityMeta>(InEntity);

        if (lMeta != nullptr)
        {
            OPAAX_LOG(LogWorld, Trace, "DestroyEntity — {}", lMeta->Name.CStr())
            m_Guids.Unregister(lMeta->Id);
        }else
        {
            OPAAX_LOG(LogWorld, Trace, "DestroyEntity — Unknown Entity destroy")
        }

        m_Registry.destroy(InEntity);
        RemoveEntityCount();
    }

    Entity World::FindByGuid(const Guid& InGuid)
    {
        // ENTITY_NONE handle -> an invalid Entity (null-safe lookup).
        return Entity{ m_Guids.Resolve(InGuid), this };
    }

    void World::OnActive()
    {
        OPAAX_LOG(LogWorld, Info, "World '{}' activated", m_Name.CStr())
    }
    
    void World::OnDesactive()
    {
        OPAAX_LOG(LogWorld, Info, "World '{}' deactivated", m_Name.CStr());
    }

    void World::Clear() noexcept
    {
        m_Registry.clear();
        m_Guids.Clear();
        m_EntityCount = 0;

        OPAAX_LOG(LogWorld, Info, "World '{}' cleared", m_Name.CStr())
    }
}
