#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    class Level;
    class World;
    class ComponentRegistry;
    class IPaths;
    class ResourceManager;
    struct MapData;
}

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorLevelDocument{"EditorLevelDocument"};

    // =============================================================================
    // EditorLevelDocument — the session's unsaved work: the open .opaaxlevel and one baseline per
    //   mounted map.
    //   Save Level writes the manifest and every changed map. Entity edits wait for Save Level;
    //   structural changes (New/Add/Remove Map, Set as Persistent) write the manifest immediately.
    //   All map baselines live here (EditorMapDocument is only a cursor). Dirty state is derived by
    //   comparing text. Records are reconciled when maps are mounted/unmounted, never rebuilt
    //   (rebuilding would adopt other maps' unsaved edits as clean).
    // =============================================================================
    class EditorLevelDocument
    {
        // =============================================================================
        // Types
        // =============================================================================
    public:
        /** One mounted map: its file, and its text as last written or read. */
        struct MapRecord
        {
            MapId       Id;
            OpaaxString AbsPath;

            /** Compact comparison text (not the file format). */
            OpaaxString Baseline;

            /** Last answer from RefreshDirty (the UI asks per map, per frame). */
            bool        bDirty = false;
        };

        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorLevelDocument() = default;

        EditorLevelDocument(const EditorLevelDocument&)            = delete;
        EditorLevelDocument& operator=(const EditorLevelDocument&) = delete;

        /**
         * Sets what prefab paths are resolved with. Called once by EditorService.
         * Until then the document does not fold prefab placements (only tests construct it that way).
         */
        void BindPrefabSources(const IPaths& InPaths, ResourceManager& InResources) noexcept
        {
            m_Paths     = &InPaths;
            m_Resources = &InResources;
        }

        // =============================================================================
        // Functions
        // =============================================================================

        // =============================================================================
        // Adoption
    public:
        /**
         * Opens a level: its file, the manifest baseline, and a record per mounted map.
         * Discards every existing record.
         * @param InLevelAbsPath The .opaaxlevel, or empty for a standalone map (records are still built,
         *   so Save Map works)
         */
        void AdoptExisting(const OpaaxString& InLevelAbsPath, const Level& InLevel, const World& InWorld,
                           const ComponentRegistry& InRegistry, const IPaths& InPaths);

        /**
         * Reconciles the records with what is mounted now: adds new maps, drops removed ones, and leaves
         * every other record as it was. Called after AddMap / RemoveMap.
         */
        void TrackMounted(const Level& InLevel, const World& InWorld,
                          const ComponentRegistry& InRegistry, const IPaths& InPaths);

        /** Forgets everything: no level, no records. */
        void Clear();

        // End Adoption
        // =============================================================================

        // =============================================================================
        // Saving
    public:
        /**
         * Writes every changed map, then the manifest if it changed. Unchanged files are not touched.
         * @return False if anything could not be written
         */
        bool SaveAll(const World& InWorld, const ComponentRegistry& InRegistry, const Level& InLevel);

        /** Writes one map and updates its baseline. @return False if unknown or the write failed. */
        bool SaveMap(MapId InMapId, const World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Writes the manifest now and updates its baseline. Called by the structural verbs (New Map,
         * Add Map, Remove from Level, Set as Persistent) so membership is saved as it changes.
         * Does nothing without a .opaaxlevel or when the text is unchanged.
         */
        void SaveManifest(const Level& InLevel);

        /**
         * Writes one map to a new path and points its record there (Save As). The MapId is unchanged.
         */
        bool SaveMapAs(MapId InMapId, const OpaaxString& InAbsPath,
                       const World& InWorld, const ComponentRegistry& InRegistry);

        // End Saving
        // =============================================================================

        // =============================================================================
        // Get
    public:
        /** @return True if InMapId's content no longer matches what was last written */
        bool IsMapDirty(MapId InMapId, const World& InWorld, const ComponentRegistry& InRegistry) const;

        /** @return True if the manifest or any mounted map would be written by SaveAll */
        bool IsDirty(const World& InWorld, const ComponentRegistry& InRegistry, const Level& InLevel) const;

        /**
         * Recomputes the cached answers below (one capture + serialize per mounted map). Expensive:
         * the caller throttles it. Skipped when the world revision has not changed. The manifest part
         * is always checked (Set as Persistent changes it without touching an entity).
         */
        void RefreshDirty(const World& InWorld, const ComponentRegistry& InRegistry, const Level& InLevel);

        /** The last computed answer for one map. Cheap. */
        bool IsMapDirtyCached(MapId InMapId) const noexcept;

        /** The last computed answer for the level: the manifest, or any of its maps. */
        bool IsDirtyCached() const noexcept;

        bool               HasLevel() const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath()  const noexcept { return m_AbsPath; }

        /** The file name, for the menu bar ("Main.opaaxlevel"). Empty when none is open. */
        OpaaxString FileName() const;

        // End Get
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /**
         * One map's content as a comparison token: the save JSON without whitespace
         * (MapJson::SerializeCompact). Not the file format.
         */
        OpaaxString CompareText(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId) const;

        /**
         * One map's content with its prefab placements folded into records. Used by the dirty check,
         * the round-trip check and Save, so the baseline always matches the written form.
         */
        MapData CaptureFolded(const World& InWorld, const ComponentRegistry& InRegistry,
                              MapId InMapId) const;

        MapRecord*       Find(MapId InMapId) noexcept;
        const MapRecord* Find(MapId InMapId) const noexcept;

        /** No capture yet: never equal to a real revision, so the first check always runs. */
        static constexpr Uint64 k_RevisionNever = ~0ull;

        /**
         * What prefab paths are resolved with. Bound once by EditorService. Null means no folding (tests).
         */
        const IPaths*        m_Paths     = nullptr;
        ResourceManager*     m_Resources = nullptr;

        OpaaxString          m_AbsPath;
        OpaaxString          m_ManifestBaseline;
        TDynArray<MapRecord> m_Maps;
        bool                 m_bManifestDirty = false;   // cached, see RefreshDirty

        /** World revision the map records were last computed at (see RefreshDirty). */
        Uint64               m_LastRevision = k_RevisionNever;
    };
}
