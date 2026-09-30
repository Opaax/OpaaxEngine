#include "World/Level.h"

#include <cstddef>   // std::ptrdiff_t

#include "Application/Services/IPaths.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"   // before the resources (completes LoadContext)
#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"
#include "Engine/Subsystems/Resources/ResourceHold.hpp"           // completes IResourceHold
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/HardReferences.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapResource.hpp"
#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/ResourcePrefabResolver.h"
#include "World/World.h"

namespace Opaax
{
    Level::Level(World& InWorld, const ComponentRegistry& InComponents,
                 const IPaths& InPaths, ResourceManager& InResources,
                 const ResourceFormatRegistry& InFormats) noexcept
        : m_World(InWorld), m_Components(InComponents), m_Paths(InPaths), m_Resources(InResources)
        , m_Formats(InFormats)
    {
    }

    Level::~Level() = default;

    // =============================================================================
    // Manifest
    // =============================================================================

    void Level::SetData(LevelData InData)
    {
        m_Data = Move(InData);
    }

    void Level::AdoptMountedFrom(const Level& InSource)
    {
        // Do not mount: the clone's entities are already in the world (copied from the snapshot).
        // Mounting again would duplicate every entity.
        m_Data = InSource.m_Data;

        // Copy ids and paths, not the resource holds: a clone never outlives its source.
        m_Mounted.clear();
        for (const MountedMap& lMap : InSource.m_Mounted)
        {
            m_Mounted.emplace_back(MountedMap{ lMap.Id, lMap.AssetRelPath });
        }
    }

    // =============================================================================
    // Mounting
    // =============================================================================

    Uint64 Level::FindInManifest(const OpaaxString& InAssetRelPath) const noexcept
    {
        for (Uint64 lIndex = 0; lIndex < m_Data.MapCount(); ++lIndex)
        {
            if (m_Data.Maps[lIndex] == InAssetRelPath) { return lIndex; }
        }

        return m_Data.MapCount();
    }

    bool Level::IsMountedPath(const OpaaxString& InAssetRelPath) const noexcept
    {
        for (const MountedMap& lMounted : m_Mounted)
        {
            if (lMounted.AssetRelPath == InAssetRelPath) { return true; }
        }

        return false;
    }

    bool Level::MountOne(const OpaaxString& InAssetRelPath, MountResult& OutResult)
    {
        // Check by path (same file twice) and by id (two files with the same map id).
        if (IsMountedPath(InAssetRelPath))
        {
            OPAAX_LOG(LogLevel, Warn, "Map '{}' is already mounted — skipped", InAssetRelPath.CStr());
            ++OutResult.MapsFailed;
            return false;
        }

        const OpaaxString lAbsPath = m_Paths.AssetToAbsolute(InAssetRelPath);

        // Load through the ResourceManager (shared between worlds). The ref is only needed until
        // the entities exist.
        const ResourceRef<MapResource> lRef = m_Resources.Load<MapResource>(lAbsPath.CStr());
        const MapResource* const       lMap = lRef.Get();

        if (lMap == nullptr)
        {
            // FailFast: a missing map gives null.
            OPAAX_LOG(LogLevel, Error, "Map '{}' failed to load — not mounted", InAssetRelPath.CStr());
            ++OutResult.MapsFailed;
            return false;
        }

        const MapId lMapId = lMap->Data.Id;
        if (IsMounted(lMapId))
        {
            OPAAX_LOG(LogLevel, Warn, "Map '{}' claims map id '{}', which is already mounted — skipped",
                      InAssetRelPath.CStr(), lMapId);
            ++OutResult.MapsFailed;
            return false;
        }

        ++OutResult.MapsMounted;
        OutResult.EntitiesCreated += MapFactory::Instantiate(lMap->Data, m_World, m_Components);

        MountedMap lMounted{ lMapId, InAssetRelPath };

        // The resources the map's entities need loaded up front.
        HoldHardReferences(lMap->Data, lMounted);

        // The map's prefab placements, rebuilt from their prefabs and overrides (second pass).
        if (!lMap->Data.Instances.empty())
        {
            MapData lPlacements;
            lPlacements.Id        = lMapId;
            lPlacements.Instances = lMap->Data.Instances;

            ResourcePrefabResolver lResolver(m_Paths, m_Resources, m_Components);
            const Uint64 lExpanded = PrefabFold::Expand(lPlacements, lResolver, m_Components);

            OutResult.EntitiesCreated += MapFactory::Instantiate(lPlacements, m_World, m_Components);

            OPAAX_LOG(LogLevel, Info, "Map '{}' expanded {} of {} prefab placement(s)",
                      InAssetRelPath.CStr(), lExpanded, lMap->Data.InstanceCount());

            // Prefab placements have hard references too.
            HoldHardReferences(lPlacements, lMounted);
        }

        m_Mounted.emplace_back(Move(lMounted));
        return true;
    }

    // =============================================================================
    // HoldHardReferences — loads every THardResourcePath the entities use, and keeps the holds on
    //   the map's record so they are released with the map.
    // =============================================================================
    void Level::HoldHardReferences(const MapData& InData, MountedMap& OutMounted)
    {
        const TDynArray<HardReference> lRefs = HardReferences::Collect(InData, m_Components);

        if (lRefs.empty())
        {
            return;
        }

        Uint64 lHeld = 0;

        for (const HardReference& lRef : lRefs)
        {
            const ResourceFormatEntry* const lEntry = m_Formats.FindByTypeId(lRef.TypeId);
            if (lEntry == nullptr)
            {
                OPAAX_LOG(LogLevel, Warn, "Map '{}' names a hard reference '{}' of a resource type "
                                          "this build has not registered — not held",
                          OutMounted.AssetRelPath.CStr(), lRef.Path.CStr());
                continue;
            }

            TUniquePtr<IResourceHold> lHold = lEntry->Acquire(m_Resources, m_Paths.AssetToAbsolute(lRef.Path).CStr());

            if (lHold == nullptr || !lHold->IsLoaded())
            {
                // A FailFast type gives null: report it now rather than at first use.
                OPAAX_LOG(LogLevel, Warn, "Map '{}' — hard reference '{}' ({}) did not load",
                          OutMounted.AssetRelPath.CStr(), lRef.Path.CStr(), lEntry->Name);
                continue;
            }

            OutMounted.HardRefs.emplace_back(Move(lHold));
            ++lHeld;
        }

        // Log success, with the count.
        OPAAX_LOG(LogLevel, Info, "Map '{}' holds {} of {} hard reference(s)",
                  OutMounted.AssetRelPath.CStr(), lHeld, static_cast<Uint64>(lRefs.size()));
    }

    Level::MountResult Level::MountAll()
    {
        MountResult lResult;

        // Persistent map first: the other maps are added on top of it.
        if (!m_Data.IsEmpty())
        {
            MountOne(m_Data.PersistentMap(), lResult);

            for (Uint64 lIndex = 0; lIndex < m_Data.MapCount(); ++lIndex)
            {
                if (lIndex == m_Data.PersistentMapIndex) { continue; }

                MountOne(m_Data.Maps[lIndex], lResult);
            }
        }

        // Log what was loaded (an empty world and a loaded one look the same otherwise).
        OPAAX_LOG(LogLevel, Info, "Level '{}' -> world '{}': {} map(s), {} entity(ies){}",
                  m_Data.Name.CStr(), m_World.GetName().CStr(),
                  lResult.MapsMounted, lResult.EntitiesCreated,
                  lResult.MapsFailed > 0 ? " (some maps FAILED — see above)" : "");

        return lResult;
    }

    bool Level::Mount(const OpaaxString& InAssetRelPath)
    {
        MountResult lResult;
        if (!MountOne(InAssetRelPath, lResult))
        {
            return false;
        }

        const MapId lMapId = m_Mounted.back().Id;

        OPAAX_LOG(LogLevel, Trace, "Mounted '{}' (map id '{}') into world '{}' — {} entity(ies)",
                  InAssetRelPath.CStr(), lMapId.IsValid() ? lMapId.CStr() : "(none)",
                  m_World.GetName().CStr(), lResult.EntitiesCreated);

        return true;
    }

    bool Level::Unmount(MapId InMapId)
    {
        Uint64 lFound = m_Mounted.size();
        for (Uint64 lIndex = 0; lIndex < m_Mounted.size(); ++lIndex)
        {
            if (m_Mounted[lIndex].Id == InMapId) { lFound = lIndex; break; }
        }

        if (lFound == m_Mounted.size())
        {
            OPAAX_LOG(LogLevel, Warn, "Unmount '{}' ignored — that map is not mounted",
                      InMapId.IsValid() ? InMapId.CStr() : "(none)");
            return false;
        }

        // Collect first, then destroy: destroying while iterating an entt view is unsafe.
        TDynArray<EntityID> lDoomed;
        m_World.Each<EntityMeta>([&lDoomed, InMapId](EntityID InId, const EntityMeta& InMeta)
        {
            if (InMeta.OwnerMap == InMapId) { lDoomed.emplace_back(InId); }
        });

        // Check each: destroying a parent also destroyed its children.
        for (const EntityID lId : lDoomed)
        {
            if (m_World.IsValid(lId)) { m_World.DestroyEntity(lId); }
        }

        OPAAX_LOG(LogLevel, Trace, "Unmounted '{}' from world '{}' — {} entity(ies) destroyed",
                  m_Mounted[lFound].AssetRelPath.CStr(), m_World.GetName().CStr(), lDoomed.size());

        m_Mounted.erase(m_Mounted.begin() + static_cast<std::ptrdiff_t>(lFound));
        return true;
    }

    // =============================================================================
    // Authoring
    // =============================================================================

    bool Level::AddMap(const OpaaxString& InAssetRelPath)
    {
        if (FindInManifest(InAssetRelPath) < m_Data.MapCount())
        {
            OPAAX_LOG(LogLevel, Warn, "'{}' is already in level '{}'",
                      InAssetRelPath.CStr(), m_Data.Name.CStr());
            return false;
        }

        // Mount first: a map that cannot be read must not be added to the level.
        if (!Mount(InAssetRelPath))
        {
            return false;
        }

        m_Data.Maps.emplace_back(InAssetRelPath);
        return true;
    }

    bool Level::RemoveMap(MapId InMapId)
    {
        // Refuse the persistent map (removing it would silently change which map is persistent).
        // Make another map persistent first.
        if (InMapId == GetPersistentMapId() && InMapId.IsValid())
        {
            OPAAX_LOG(LogLevel, Warn,
                      "'{}' is level '{}'s persistent map — set another one persistent before removing it",
                      InMapId, m_Data.Name.CStr());
            return false;
        }

        // The mount record links the map id to its manifest path.
        OpaaxString lAssetRelPath;
        for (const MountedMap& lMounted : m_Mounted)
        {
            if (lMounted.Id == InMapId) { lAssetRelPath = lMounted.AssetRelPath; break; }
        }

        if (!Unmount(InMapId))
        {
            return false;
        }

        EraseFromManifest(FindInManifest(lAssetRelPath));

        return true;
    }

    bool Level::RemoveMissingMap(const OpaaxString& InAssetRelPath)
    {
        // A mounted map has entities: use RemoveMap for it.
        if (IsMountedPath(InAssetRelPath))
        {
            OPAAX_LOG(LogLevel, Warn, "'{}' IS mounted — remove it by id so its entities go too",
                      InAssetRelPath.CStr());
            return false;
        }

        const Uint64 lIndex = FindInManifest(InAssetRelPath);
        if (lIndex >= m_Data.MapCount())
        {
            OPAAX_LOG(LogLevel, Warn, "'{}' is not in level '{}'s manifest",
                      InAssetRelPath.CStr(), m_Data.Name.CStr());
            return false;
        }

        // Same refusal as RemoveMap.
        if (lIndex == m_Data.PersistentMapIndex)
        {
            OPAAX_LOG(LogLevel, Warn,
                      "'{}' is level '{}'s persistent map — set another one persistent before removing it",
                      InAssetRelPath.CStr(), m_Data.Name.CStr());
            return false;
        }

        OPAAX_LOG(LogLevel, Info, "Dropped missing map '{}' from level '{}'",
                  InAssetRelPath.CStr(), m_Data.Name.CStr());

        EraseFromManifest(lIndex);
        return true;
    }

    void Level::EraseFromManifest(Uint64 InIndex)
    {
        if (InIndex >= m_Data.MapCount())
        {
            return;
        }

        m_Data.Maps.erase(m_Data.Maps.begin() + static_cast<std::ptrdiff_t>(InIndex));

        // Removing an entry before the persistent one shifts its index: fix it.
        if (m_Data.PersistentMapIndex > InIndex) { --m_Data.PersistentMapIndex; }
    }

    bool Level::SetPersistentMap(MapId InMapId)
    {
        for (const MountedMap& lMounted : m_Mounted)
        {
            if (lMounted.Id != InMapId) { continue; }

            const Uint64 lIndex = FindInManifest(lMounted.AssetRelPath);
            if (lIndex >= m_Data.MapCount())
            {
                // Mounted but not in the manifest (a standalone map): nothing to update.
                OPAAX_LOG(LogLevel, Warn, "'{}' is mounted but is not one of level '{}'s maps",
                          lMounted.AssetRelPath.CStr(), m_Data.Name.CStr());
                return false;
            }

            m_Data.PersistentMapIndex = lIndex;
            OPAAX_LOG(LogLevel, Info, "Level '{}': persistent map is now '{}'",
                      m_Data.Name.CStr(), lMounted.AssetRelPath.CStr());
            return true;
        }

        OPAAX_LOG(LogLevel, Warn, "SetPersistentMap '{}' ignored — that map is not mounted",
                  InMapId.IsValid() ? InMapId.CStr() : "(none)");
        return false;
    }

    // =============================================================================
    // Get
    // =============================================================================

    bool Level::IsMounted(MapId InMapId) const noexcept
    {
        if (!InMapId.IsValid())
        {
            return false;   // invalid means runtime-spawned
        }

        for (const MountedMap& lMounted : m_Mounted)
        {
            if (lMounted.Id == InMapId) { return true; }
        }

        return false;
    }

    MapId Level::GetPersistentMapId() const noexcept
    {
        if (m_Data.IsEmpty())
        {
            return MapId();
        }

        const OpaaxString& lPersistent = m_Data.PersistentMap();
        for (const MountedMap& lMounted : m_Mounted)
        {
            if (lMounted.AssetRelPath == lPersistent) { return lMounted.Id; }
        }

        return MapId();   // in the manifest but not mounted (failed to load)
    }

    void Level::OnWorldCleared() noexcept
    {
        m_Mounted.clear();
    }
}
