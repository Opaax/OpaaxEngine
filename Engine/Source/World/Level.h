#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"
#include "World/Entity/EntityTypes.h"
#include "World/Serialization/LevelFile.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;
    class ResourceManager;
    class ResourceFormatRegistry;
    class IResourceHold;   // complete in Level.cpp
    class IPaths;
    struct MapData;

    inline constexpr LogCategory LogLevel{"Level"};

    // =============================================================================
    // Level — which maps a World contains, and the only thing that mounts or unmounts them.
    //   One per world, owned by it (World::SetLevel).
    //   The persistent map is mounted first; the others go on top of it.
    //   Unmounting destroys the entities of that map (a map is a subset of the world's registry).
    //   A clone copies this state without mounting again (WorldManager::CloneWorld).
    // =============================================================================
    class OPAAX_API Level
    {
        // =========================================================================
        // Types
        // =========================================================================
    public:
        /** One mounted map. Unmount uses the id. */
        struct MountedMap
        {
            MapId       Id;
            OpaaxString AssetRelPath;

            /**
             * Resources the map's entities need loaded (hard references), kept for as long as the map is mounted.
             */
            TDynArray<TUniquePtr<IResourceHold>> HardRefs;
        };

        /**
         * What a mount pass did, so "empty level" and "every map failed" can be told apart.
         */
        struct MountResult
        {
            Uint64 MapsMounted     = 0;
            Uint64 MapsFailed      = 0;
            Uint64 EntitiesCreated = 0;

            /** No failure and something was loaded. An empty level is not a success. */
            bool IsValid() const noexcept { return MapsFailed == 0 && MapsMounted > 0; }
        };

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /**
         * @param InWorld      The world to mount into (owns this Level)
         * @param InComponents Component types (unknown ones are skipped with a warning)
         * @param InPaths      Resolves the manifest's asset-relative paths
         * @param InResources  Loads the maps
         * @param InFormats    Loads hard resource references by type id
         */
        Level(World& InWorld, const ComponentRegistry& InComponents,
              const IPaths& InPaths, ResourceManager& InResources,
              const ResourceFormatRegistry& InFormats) noexcept;

        ~Level();

        // =========================================================================
        // Copy - Move Delete
        // =========================================================================
        Level(const Level&)            = delete;
        Level& operator=(const Level&) = delete;
        Level(Level&&)                 = delete;
        Level& operator=(Level&&)      = delete;

        // =========================================================================
        // Functions
        // =========================================================================

        // =========================================================================
        // Manifest
    public:
        /** Sets the manifest. Mounts nothing. */
        void SetData(LevelData InData);

        /**
         * Copies another level's manifest and mounted list without mounting anything (Play copy).
         */
        void AdoptMountedFrom(const Level& InSource);

        // End Manifest
        // =========================================================================

        // =========================================================================
        // Mounting
    public:
        /**
         * Mounts every map in the manifest, persistent first. A failing map is skipped and counted.
         */
        MountResult MountAll();

        /**
         * Mounts one map without adding it to the manifest (editing a single map).
         * @param InAssetRelPath Asset-relative ("Maps/Main.opaaxmap")
         * @return False if the file could not be read or the map is already mounted
         */
        bool Mount(const OpaaxString& InAssetRelPath);

        /**
         * Destroys every entity of InMapId and forgets the mount.
         * @return False if that map is not mounted
         */
        bool Unmount(MapId InMapId);

        // End Mounting
        // =========================================================================

        // =========================================================================
        // Authoring
    public:
        /** Mounts InAssetRelPath and adds it to the manifest. @return False if the mount failed. */
        bool AddMap(const OpaaxString& InAssetRelPath);

        /**
         * Unmounts InMapId and removes it from the manifest.
         * Refuses the persistent map: make another map persistent first.
         */
        bool RemoveMap(MapId InMapId);

        /**
         * Removes a manifest entry that is not mounted (a missing, renamed or moved file).
         * Refuses a mounted path (use RemoveMap).
         * @param InAssetRelPath As written in the manifest ("Maps/Sprites.opaaxmap")
         */
        bool RemoveMissingMap(const OpaaxString& InAssetRelPath);

        /** @return False if InMapId is not a map of this level */
        bool SetPersistentMap(MapId InMapId);

        // End Authoring
        // =========================================================================

        // =========================================================================
        // Get
    public:
        const LevelData& GetData() const noexcept { return m_Data; }

        /** Every mounted map, in mount order. */
        const TDynArray<MountedMap>& GetMountedMaps() const noexcept { return m_Mounted; }

        bool IsMounted(MapId InMapId) const noexcept;

        /** The persistent map's id; invalid if the level has no mounted maps. */
        MapId GetPersistentMapId() const noexcept;

        /** Forgets every mount without destroying anything (after World::Clear). */
        void OnWorldCleared() noexcept;

        // End Get
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        /**
         * Loads and instantiates one map. Failures are logged and counted in OutResult.
         */
        bool MountOne(const OpaaxString& InAssetRelPath, MountResult& OutResult);

        /** Index in m_Data.Maps, or MapCount() if InAssetRelPath is not in the manifest. */
        Uint64 FindInManifest(const OpaaxString& InAssetRelPath) const noexcept;

        /** Whether InAssetRelPath is mounted (works for a map with no entities). */
        bool IsMountedPath(const OpaaxString& InAssetRelPath) const noexcept;

        /**
         * Removes manifest entry InIndex and keeps PersistentMapIndex on the same map.
         */
        void EraseFromManifest(Uint64 InIndex);

        /**
         * Loads every hard reference of InData's entities into OutMounted, logs the count,
         * and warns for each one that failed.
         */
        void HoldHardReferences(const MapData& InData, MountedMap& OutMounted);

        World&                        m_World;
        const ComponentRegistry&      m_Components;
        const IPaths&                 m_Paths;
        ResourceManager&              m_Resources;
        const ResourceFormatRegistry& m_Formats;

        LevelData             m_Data;
        TDynArray<MountedMap> m_Mounted;
    };
}
