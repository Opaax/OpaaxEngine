#include "World/Level.h"

#include <cstddef>   // std::ptrdiff_t — vector::erase takes a signed offset

#include "Application/Services/IPaths.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"   // before the resources — completes LoadContext
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapResource.hpp"
#include "World/World.h"

namespace Opaax
{
    Level::Level(World& InWorld, const ComponentRegistry& InComponents,
                 const IPaths& InPaths, ResourceManager& InResources) noexcept
        : m_World(InWorld), m_Components(InComponents), m_Paths(InPaths), m_Resources(InResources)
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
        // NOT a mount. The entities this describes are already in the world — they came from the
        // snapshot MapSerializer::Capture took (WM6). Mounting them again would re-read every map
        // and CreateEntityWithGuid would refuse every entity as a live duplicate (WM3), leaving a
        // wall of warnings and a clone identical to the one this line already produced.
        m_Data    = InSource.m_Data;
        m_Mounted = InSource.m_Mounted;
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
        // Checked by PATH as well as by id: path catches the same file twice, id catches two files
        // claiming one map. Both are needed — the id no longer goes invalid for an empty map
        // (**MP10**), but two different paths can still name one map.
        if (IsMountedPath(InAssetRelPath))
        {
            OPAAX_LOG(LogLevel, Warn, "Map '{}' is already mounted — skipped", InAssetRelPath.CStr());
            ++OutResult.MapsFailed;
            return false;
        }

        const OpaaxString lAbsPath = m_Paths.AssetToAbsolute(InAssetRelPath);

        // Through the ResourceManager, not MapFile::Load (WM4): it gives MapResource a real caller
        // on every boot rather than only in its own test, and two worlds opening the same map then
        // parse it once. The Ref is scoped to this call — once the entities exist the parsed
        // MapData has no further consumer, and holding it would be a refcount with no purpose.
        const ResourceRef<MapResource> lRef = m_Resources.Load<MapResource>(lAbsPath.CStr());
        const MapResource* const       lMap = lRef.Get();

        if (lMap == nullptr)
        {
            // FailFast: a missing map resolves to null rather than to an empty placeholder, which
            // is the whole reason the policy is what it is.
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

        m_Mounted.emplace_back(lMapId, InAssetRelPath);
        return true;
    }

    Level::MountResult Level::MountAll()
    {
        MountResult lResult;

        // PERSISTENT FIRST (WM1a). Mount order is the LEVEL's, not the manifest array's: every
        // other map composes on top of the persistent one, so it has to already be there.
        if (!m_Data.IsEmpty())
        {
            MountOne(m_Data.PersistentMap(), lResult);

            for (Uint64 lIndex = 0; lIndex < m_Data.MapCount(); ++lIndex)
            {
                if (lIndex == m_Data.PersistentMapIndex) { continue; }

                MountOne(m_Data.Maps[lIndex], lResult);
            }
        }

        // The SUCCESS branch says what actually arrived, not merely that nothing failed — an empty
        // world and a loaded one look identical in a log that only reports errors ([[L15]]).
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

        OPAAX_LOG(LogLevel, Info, "Mounted '{}' (map id '{}') into world '{}' — {} entity(ies)",
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

        // COLLECT, then destroy. A map is a PARTITION of the world's one registry (WM2), so this
        // is a filter and never a store to tear down — but destroying entities while iterating an
        // entt view is not safe, so the handles are gathered first.
        TDynArray<EntityID> lDoomed;
        m_World.Each<EntityMeta>([&lDoomed, InMapId](EntityID InId, const EntityMeta& InMeta)
        {
            if (InMeta.OwnerMap == InMapId) { lDoomed.emplace_back(InId); }
        });

        for (const EntityID lId : lDoomed) { m_World.DestroyEntity(lId); }

        OPAAX_LOG(LogLevel, Info, "Unmounted '{}' from world '{}' — {} entity(ies) destroyed",
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

        // Mounted BEFORE the manifest grows: a map that cannot be read must not be written into
        // the level's data, or the next Save would persist an entry that never worked.
        if (!Mount(InAssetRelPath))
        {
            return false;
        }

        m_Data.Maps.emplace_back(InAssetRelPath);
        return true;
    }

    bool Level::RemoveMap(MapId InMapId)
    {
        // The PERSISTENT map is refused. Removing it would silently re-point persistence at
        // whatever ended up first in the list — a bigger decision than "remove this map", and one
        // the author never made. SetPersistentMap first, then remove.
        if (InMapId == GetPersistentMapId() && InMapId.IsValid())
        {
            OPAAX_LOG(LogLevel, Warn,
                      "'{}' is level '{}'s persistent map — set another one persistent before removing it",
                      InMapId, m_Data.Name.CStr());
            return false;
        }

        // The manifest entry is found through the MOUNT record: the level knows a map by its id
        // and the manifest by its path, and this is the one place the two meet.
        OpaaxString lAssetRelPath;
        for (const MountedMap& lMounted : m_Mounted)
        {
            if (lMounted.Id == InMapId) { lAssetRelPath = lMounted.AssetRelPath; break; }
        }

        if (!Unmount(InMapId))
        {
            return false;
        }

        const Uint64 lIndex = FindInManifest(lAssetRelPath);
        if (lIndex < m_Data.MapCount())
        {
            m_Data.Maps.erase(m_Data.Maps.begin() + static_cast<std::ptrdiff_t>(lIndex));

            // The persistent INDEX is a position, so anything removed ahead of it shifts it. Left
            // alone it would silently start naming the next map along.
            if (m_Data.PersistentMapIndex > lIndex) { --m_Data.PersistentMapIndex; }
        }

        return true;
    }

    bool Level::SetPersistentMap(MapId InMapId)
    {
        for (const MountedMap& lMounted : m_Mounted)
        {
            if (lMounted.Id != InMapId) { continue; }

            const Uint64 lIndex = FindInManifest(lMounted.AssetRelPath);
            if (lIndex >= m_Data.MapCount())
            {
                // Mounted but not in the manifest — a standalone map (Level::Mount). Persistence
                // is a manifest statement, so there is nothing to write it into.
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
            return false;   // an invalid id means "runtime-spawned" (WM2), never a mounted map
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

        return MapId();   // named by the manifest but not mounted (it failed to load)
    }

    void Level::OnWorldCleared() noexcept
    {
        m_Mounted.clear();
    }
}
