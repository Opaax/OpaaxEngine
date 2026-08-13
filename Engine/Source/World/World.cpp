#include "World.h"

#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Level.h"   // complete type for the TUniquePtr<Level> member's destructor

namespace Opaax
{
    // =========================================================================
    // CTOR - DTOR
    // =========================================================================
    World::World(OpaaxString InName, EWorldMode InMode)
        : m_Id(Guid::New())
        , m_Name(std::move(InName))
        , m_Mode(InMode)
    {
        // The mode is in the log because it is otherwise invisible: an Edit and a Play world
        // differ only by which subsystems they get (S3), so an ordered boot log is the only
        // place the distinction shows up before PIE exists.
        OPAAX_LOG(LogWorld, Info, "World '{}' created ({})", m_Name.CStr(), ToString(m_Mode));
    }

    World::~World()
    {
        // Safety net only — DestroyWorld normally got here first, while the engine siblings a
        // subsystem might reach were all still alive. Idempotent, so the normal path costs nothing.
        ShutdownSubsystems();

        OPAAX_LOG(LogWorld, Info, "World '{}' destroyed ({} entity(ies))", m_Name.CStr(), m_EntityCount);
    }

    // =========================================================================
    // Level
    // =========================================================================
    void World::SetLevel(TUniquePtr<Level> InLevel)
    {
        m_Level = Move(InLevel);
    }

    // =========================================================================
    // Subsystems
    // =========================================================================
    void World::SetContext(const WorldContext& InContext)
    {
        m_Context = MakeUnique<WorldContext>(InContext);
    }

    void World::ShutdownSubsystems()
    {
        if (m_bSubsystemsShutdown)
        {
            return;
        }

        m_bSubsystemsShutdown = true;
        m_Subsystems.ShutdownAll();
    }

    void World::AddEntityCount()
    {
        ++m_EntityCount;
        ++m_Revision;
        LogEntityCount();
    }

    void World::RemoveEntityCount()
    {
        if (m_EntityCount <= 0 )
        {
            return;
        }

        --m_EntityCount;
        ++m_Revision;
        LogEntityCount();
    }

    void World::LogEntityCount()
    {
        // TRACE, not Info: this fires on EVERY create and EVERY destroy, so the shmup hot path
        // would put one line in the log per bullet spawned and another per bullet despawned.
        OPAAX_LOG(LogWorld, Trace, "Entity count in world '{}' = {}", m_Name.CStr(), m_EntityCount);
    }

    // =========================================================================
    // Entity lifecycle
    // =========================================================================
    Entity World::CreateEntity(OpaaxString InName, MapId InOwnerMap)
    {
        return CreateEntityWithGuid(Guid::New(), Move(InName), InOwnerMap);
    }

    Entity World::CreateEntityWithGuid(const Guid& InGuid, OpaaxString InName, MapId InOwnerMap)
    {
        if (!InGuid.IsValid())
        {
            OPAAX_LOG(LogWorld, Error, "CreateEntityWithGuid — refused an invalid Guid for '{}'", InName.CStr());
            return Entity{};
        }

        // Two entities under one Guid would make FindByGuid answer arbitrarily, and the
        // second Register would silently evict the first mapping.
        if (m_Guids.Contains(InGuid))
        {
            OPAAX_LOG(LogWorld, Error, "CreateEntityWithGuid — '{}' refused: that Guid is already live in world '{}'",
                      InName.CStr(), m_Name.CStr());
            return Entity{};
        }

        const EntityID lEnt  = m_Registry.create();
        EntityMeta&    lMeta = m_Registry.emplace<EntityMeta>(lEnt, EntityMeta{ InGuid, Move(InName), InOwnerMap });
        m_Guids.Register(lMeta.Id, lEnt);
        OPAAX_LOG(LogWorld, Trace, "CreateEntity '{}' in world '{}'", lMeta.Name.CStr(), m_Name.CStr());

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
            OPAAX_LOG(LogWorld, Warn, "DestroyEntity — invalid entity ignored");
            return;
        }
        
        const EntityMeta* lMeta = m_Registry.try_get<EntityMeta>(InEntity);

        if (lMeta != nullptr)
        {
            OPAAX_LOG(LogWorld, Trace, "DestroyEntity — {}", lMeta->Name.CStr());
            m_Guids.Unregister(lMeta->Id);
        }else
        {
            OPAAX_LOG(LogWorld, Trace, "DestroyEntity — Unknown Entity destroy");
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
        OPAAX_LOG(LogWorld, Info, "World '{}' activated", m_Name.CStr());
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
        ++m_Revision;   // wiping every entity is the largest content change there is

        // The Level's mount records describe entities that no longer exist. Cleared, not
        // unmounted: there is nothing left to destroy, and a Level still claiming a map would
        // refuse to mount it again.
        if (m_Level != nullptr) { m_Level->OnWorldCleared(); }

        OPAAX_LOG(LogWorld, Info, "World '{}' cleared", m_Name.CStr());
    }
}
