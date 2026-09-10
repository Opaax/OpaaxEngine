#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"
#include "World/Entity/EntityTypes.h"          // MapId
#include "World/Serialization/LevelFile.h"     // LevelData, held by value

namespace Opaax
{
    class World;
    class ComponentRegistry;
    class ResourceManager;
    class ResourceFormatRegistry;
    class IResourceHold;   // complete only in Level.cpp, where every MountedMap is destroyed
    class IPaths;
    struct MapData;

    inline constexpr LogCategory LogLevel{"Level"};

    // =============================================================================
    // Level — WHICH maps a World has in it, and the only thing that puts them there or takes
    //   them out (**WM1**: World owns the registry, a Level composes Maps).
    //
    //   ONE PER WORLD, owned by it (World::SetLevel), so "which level is loaded" is a question
    //   the world answers and nothing above has to track. Its four references are the parameter
    //   list the deleted LevelLoader threaded through every call — held once here, which is what
    //   lets Mount(path) take nothing else. The level knows which maps.
    //
    //   MOUNT ORDER IS THE LEVEL'S: the persistent map goes first (**WM1a**) because every other
    //   map composes on top of it.
    //
    //   UNMOUNTING IS A FILTER, NOT A TEARDOWN, and that is WM2 paying off: a Map is a partition
    //   of the world's one registry, so removing one is "destroy the entities whose OwnerMap is
    //   this" — no store to drop, no refcount to release. The MapResource ref is dropped the
    //   moment the entities exist; holding it would be a refcount with no consumer.
    //
    //   A CLONE COPIES THIS STATE AND DOES NOT RE-MOUNT (WorldManager::CloneWorld). The clone's
    //   entities arrive in the unfiltered snapshot (**WM6**), so mounting again would duplicate
    //   every one of them and CreateEntityWithGuid would refuse the lot (**WM3**).
    // =============================================================================
    class OPAAX_API Level
    {
        // =========================================================================
        // Types
        // =========================================================================
    public:
        /** One map that is IN the world right now. The id is what Unmount filters on. */
        struct MountedMap
        {
            MapId       Id;
            OpaaxString AssetRelPath;

            /**
             * What this map's entities must have RESIDENT (⑦-C P5b, **PF11**) — every hard field
             * they name, acquired at mount and released with this record. The map is the holder
             * because it lives exactly as long as the entities do: the prefab RESOURCE an instance
             * came from is released the moment it is instantiated, so a chain hung off that
             * payload would not outlive the gun it was meant to serve.
             */
            TDynArray<TUniquePtr<IResourceHold>> HardRefs;
        };

        /**
         * What a mount pass actually did. Returned rather than logged-and-forgotten so a caller
         * can tell "the level was empty" from "every map in it failed" — an empty world looks
         * identical either way, which is precisely the confusion FailFast exists to prevent.
         */
        struct MountResult
        {
            Uint64 MapsMounted     = 0;
            Uint64 MapsFailed      = 0;
            Uint64 EntitiesCreated = 0;

            /** Nothing went wrong AND something arrived. An empty level is not a success. */
            bool IsValid() const noexcept { return MapsFailed == 0 && MapsMounted > 0; }
        };

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /**
         * @param InWorld      The world this level mounts into. Owns this Level.
         * @param InComponents Decides which components can be rebuilt; an unknown one is skipped
         *                     with a warning by MapFactory, exactly as for a PIE clone.
         * @param InPaths      Resolves the manifest's asset-relative map paths.
         * @param InResources  Maps are loaded THROUGH the manager (**WM4**), never MapFile::Load.
         * @param InFormats    Turns a hard field's type id back into a typed load (P5b).
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
        /** Adopt the manifest. Mounts NOTHING — opening is the caller's next call. */
        void SetData(LevelData InData);

        /**
         * Copy another level's manifest AND its mounted list WITHOUT mounting anything.
         *
         * The PIE clone path, and the reason it exists rather than a second MountAll: the clone's
         * entities came from the snapshot, so re-mounting would duplicate every one of them. The
         * source keeps its hard-reference holds (P5b); a clone lives inside the source's lifetime.
         */
        void AdoptMountedFrom(const Level& InSource);

        // End Manifest
        // =========================================================================

        // =========================================================================
        // Mounting
    public:
        /**
         * Mount every map the manifest names, PERSISTENT FIRST (**WM1a**).
         *
         * A map that fails is COUNTED AND SKIPPED rather than abandoning the rest: one missing
         * file in a ten-map level should cost that map, not the level.
         */
        MountResult MountAll();

        /**
         * Mount one map, WITHOUT adding it to the manifest — a map being edited on its own, in a
         * world whose level has no file (see AddMap for the authoring verb).
         *
         * @param InAssetRelPath ASSET-RELATIVE ("Maps/Main.opaaxmap").
         * @return false when the file could not be read, or when that map is already mounted.
         */
        bool Mount(const OpaaxString& InAssetRelPath);

        /**
         * Destroy every entity InMapId authored, and forget the mount.
         *
         * @return false when that map is not mounted.
         */
        bool Unmount(MapId InMapId);

        // End Mounting
        // =========================================================================

        // =========================================================================
        // Authoring
    public:
        /** Mount InAssetRelPath AND append it to the manifest. @return false if the mount failed. */
        bool AddMap(const OpaaxString& InAssetRelPath);

        /**
         * Unmount InMapId and drop it from the manifest.
         *
         * REFUSES THE PERSISTENT MAP: removing it would silently re-point persistence at whatever
         * ended up first, which is a bigger decision than the caller asked for. Set another map
         * persistent first.
         */
        bool RemoveMap(MapId InMapId);

        /**
         * Drop a manifest entry that is NOT mounted — a map whose file is missing, renamed or moved.
         *
         * Its OWN verb because such an entry cannot be named any other way: a MapId comes from the
         * file's entities (**MP10**) and there is no file, so `RemoveMap` has nothing to look up and
         * the Hierarchy — which lists MOUNTED maps — has no row to hang a menu on. Without this the
         * only repair is hand-editing the `.opaaxlevel`, and the level never opens cleanly again.
         *
         * REFUSES a path that IS mounted: that one has entities in the world and must go through
         * `RemoveMap`, which unmounts them.
         *
         * @param InAssetRelPath As the manifest spells it ("Maps/Sprites.opaaxmap").
         */
        bool RemoveMissingMap(const OpaaxString& InAssetRelPath);

        /** @return false when InMapId names no map of this level. */
        bool SetPersistentMap(MapId InMapId);

        // End Authoring
        // =========================================================================

        // =========================================================================
        // Get
    public:
        const LevelData& GetData() const noexcept { return m_Data; }

        /** Every map currently in the world, in mount order. */
        const TDynArray<MountedMap>& GetMountedMaps() const noexcept { return m_Mounted; }

        bool IsMounted(MapId InMapId) const noexcept;

        /** The always-mounted map's id, invalid when the level has no maps or none are mounted. */
        MapId GetPersistentMapId() const noexcept;

        /** Forget every mount without destroying anything — World::Clear already emptied us. */
        void OnWorldCleared() noexcept;

        // End Get
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        /**
         * Load + instantiate one map, appending its record to m_Mounted. Every failure is logged
         * and counted into OutResult.
         */
        bool MountOne(const OpaaxString& InAssetRelPath, MountResult& OutResult);

        /** Index into m_Data.Maps, or MapCount() when InAssetRelPath is not in the manifest. */
        Uint64 FindInManifest(const OpaaxString& InAssetRelPath) const noexcept;

        /** Mounted by PATH — the check that still works for a map with no entities to claim it. */
        bool IsMountedPath(const OpaaxString& InAssetRelPath) const noexcept;

        /**
         * Drop manifest entry InIndex and keep PersistentMapIndex pointing at the same map.
         * Out-of-range is a no-op. Shared by both removal verbs so the index fix-up cannot drift.
         */
        void EraseFromManifest(Uint64 InIndex);

        /**
         * Acquire every hard reference InData's entities name and hand the holds to OutMounted,
         * logging the count — the success branch — and warning per reference that did not load.
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
