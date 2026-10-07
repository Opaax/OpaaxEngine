#include "World/World.h"

#include "World/Components/TransformComponent.h"   // added to every entity
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Level.h"   // complete type for TUniquePtr<Level>

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
        // Log the mode: Edit and Play worlds differ only by their subsystems.
        OPAAX_LOG(LogWorld, Trace, "World '{}' created ({})", m_Name.CStr(), ToString(m_Mode));
    }

    World::~World()
    {
        // In case DestroyWorld did not run first. Safe to repeat.
        ShutdownSubsystems();

        OPAAX_LOG(LogWorld, Trace, "World '{}' destroyed ({} entity(ies))", m_Name.CStr(), m_EntityCount);
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
    }

    void World::RemoveEntityCount()
    {
        if (m_EntityCount <= 0 )
        {
            return;
        }

        --m_EntityCount;
        ++m_Revision;
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

        // Two entities with one guid would make FindByGuid ambiguous.
        if (m_Guids.Contains(InGuid))
        {
            OPAAX_LOG(LogWorld, Error, "CreateEntityWithGuid — '{}' refused: that Guid is already live in world '{}'",
                      InName.CStr(), m_Name.CStr());
            return Entity{};
        }

        const EntityID lEnt  = m_Registry.create();
        EntityMeta&    lMeta = m_Registry.emplace<EntityMeta>(lEnt, EntityMeta{ InGuid, Move(InName), InOwnerMap });

        // Every entity has a Transform. Map data fills it (IComponentEntry::Load uses get_or_emplace).
        m_Registry.emplace<TransformComponent>(lEnt);

        m_Guids.Register(lMeta.Id, lEnt);

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

        // Announced while the entity and its children are whole. A listener may destroy it itself.
        m_OnEntityDestroying.Broadcast(InEntity);
        if (!m_Registry.valid(InEntity))
        {
            return;
        }

        if (const EntityMeta* lMeta = m_Registry.try_get<EntityMeta>(InEntity))
        {
            // Cascade: children die with their parent. Collected first (destroying inside the view is unsafe).
            const Guid lId = lMeta->Id;
            m_Guids.Unregister(lId);

            TDynArray<EntityID> lChildren;
            m_Registry.view<EntityMeta>().each([&](const EntityID InId, const EntityMeta& InChild)
            {
                if (InChild.Parent == lId) { lChildren.emplace_back(InId); }
            });

            // A listener may have destroyed a sibling, or this entity, meanwhile.
            for (const EntityID lChild : lChildren)
            {
                if (m_Registry.valid(lChild)) { DestroyEntity(lChild); }
            }
        }

        if (m_Registry.valid(InEntity))
        {
            m_Registry.destroy(InEntity);
            RemoveEntityCount();
        }
    }

    Entity World::FindByGuid(const Guid& InGuid)
    {
        // ENTITY_NONE gives an invalid Entity.
        return Entity{ m_Guids.Resolve(InGuid), this };
    }

    void World::OnActive()
    {
        m_bActive = true;

        OPAAX_LOG(LogWorld, Trace, "World '{}' activated", m_Name.CStr());
    }
    
    void World::OnDesactive()
    {
        m_bActive = false;

        OPAAX_LOG(LogWorld, Trace, "World '{}' deactivated", m_Name.CStr());
    }

    void World::Clear() noexcept
    {
        // Every entity is announced first, in no particular order. Collected first: a listener may
        // create or destroy entities.
        if (m_OnEntityDestroying.IsBound())
        {
            TDynArray<EntityID> lEntities;
            m_Registry.view<EntityMeta>().each([&](const EntityID InId, const EntityMeta&) { lEntities.emplace_back(InId); });

            for (const EntityID lEntity : lEntities)
            {
                if (m_Registry.valid(lEntity)) { m_OnEntityDestroying.Broadcast(lEntity); }
            }
        }

        m_Registry.clear();
        m_Guids.Clear();
        m_EntityCount = 0;
        ++m_Revision;   // every entity removed

        // The Level's mounts describe entities that no longer exist: clear them (a Level still
        // claiming a map would refuse to mount it again).
        if (m_Level != nullptr) { m_Level->OnWorldCleared(); }

        OPAAX_LOG(LogWorld, Trace, "World '{}' cleared", m_Name.CStr());
    }
}
