#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    class Level;
    class World;
    class ComponentRegistry;
    class IPaths;
}

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorLevelDocument{"EditorLevelDocument"};

    // =============================================================================
    // EditorLevelDocument — the session's UNSAVED WORK, all of it: which `.opaaxlevel` is open, and
    //   one baseline per MOUNTED MAP.
    //
    //   SAVE LEVEL MEANS SAVE THE LEVEL — the manifest AND every map in it (**MP9**). A Level IS its
    //   maps, so writing the list while leaving the maps unwritten is a save that loses work.
    //
    //   CONTENT IS BATCHED; STRUCTURE IS NOT. Entity edits accumulate and wait for Save Level —
    //   that is what batching is for. Changing WHICH maps a level has (New/Add/Remove Map, Set as
    //   Persistent) is a deliberate one-off act, so SaveManifest writes it as it happens. Save Level
    //   still writes the manifest, and normally finds nothing to say.
    //
    //   THE BASELINES LIVE HERE AND NOWHERE ELSE. `EditorMapDocument` is a cursor — which map is
    //   focused — precisely because a second baseline for the same map would rebase on Save Map
    //   while this one did not, and the dirty marker would start lying ([[L30]]).
    //
    //   DIRTY IS DERIVED, NOT TRACKED (**MP5**), unchanged; what changed is how many things it is
    //   derived for. Records are RECONCILED, never rebuilt (TrackMounted): re-taking every baseline
    //   from the world after mounting one map would silently adopt every other map's unsaved edits
    //   as clean.
    // =============================================================================
    class EditorLevelDocument
    {
        // =============================================================================
        // Types
        // =============================================================================
    public:
        /** One mounted map's file and the CompareText it was last written or read as. */
        struct MapRecord
        {
            MapId       Id;
            OpaaxString AbsPath;

            /** CompareText form, never the file's — only ever compared against CompareText. */
            OpaaxString Baseline;

            /** Last answer from RefreshDirty. Cached because the UI asks per map, per frame. */
            bool        bDirty = false;
        };

        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorLevelDocument() = default;

        EditorLevelDocument(const EditorLevelDocument&)            = delete;
        EditorLevelDocument& operator=(const EditorLevelDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================

        // =============================================================================
        // Adoption
    public:
        /**
         * Open a level: point at its file, take the manifest baseline, and build a record per
         * mounted map. DISCARDS every existing record — this is a new world.
         *
         * @param InLevelAbsPath The `.opaaxlevel`, or EMPTY for a world whose maps belong to no
         *   manifest (a standalone map). Records are still built either way, which is what keeps
         *   Save Map working there.
         */
        void AdoptExisting(const OpaaxString& InLevelAbsPath, const Level& InLevel, const World& InWorld,
                           const ComponentRegistry& InRegistry, const IPaths& InPaths);

        /**
         * Reconcile the records against what is mounted NOW: a record for anything newly mounted,
         * no record for anything gone, and **every surviving record left exactly as it was**.
         *
         * Called after AddMap / RemoveMap. Re-taking the baselines instead would quietly adopt
         * every other map's unsaved edits as the new clean state.
         */
        void TrackMounted(const Level& InLevel, const World& InWorld,
                          const ComponentRegistry& InRegistry, const IPaths& InPaths);

        /** Forget everything — no level, no records. */
        void Clear();

        // End Adoption
        // =============================================================================

        // =============================================================================
        // Saving
    public:
        /**
         * Write every map that CHANGED, then the manifest if it changed (**MP9**).
         *
         * Clean maps are skipped rather than rewritten with identical bytes — a Save should not
         * touch the mtime of a file it had nothing to say about.
         *
         * @return false when anything that needed writing could not be written.
         */
        bool SaveAll(const World& InWorld, const ComponentRegistry& InRegistry, const Level& InLevel);

        /** Write ONE map and rebase its record. @return false when unknown, or the write failed. */
        bool SaveMap(MapId InMapId, const World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Write the MANIFEST now and rebase its baseline.
         *
         * The structural verbs — New Map, Add Map, Remove from Level, Set as Persistent — call this
         * as they run, so a level's membership is on disk the moment it changes. They are deliberate
         * one-off acts, unlike entity edits, which accumulate and are what batching into Save Level
         * is FOR; and New Map already half-committed by writing the map file, so leaving the other
         * half pending is the worst of both. Closing the editor after adding a map used to drop the
         * membership silently while the file stayed behind.
         *
         * A no-op when there is no `.opaaxlevel` (a standalone map's world has no manifest) or when
         * the text has not changed.
         */
        void SaveManifest(const Level& InLevel);

        /**
         * Write one map to a NEW path and re-point its record there (Save As). The MapId is
         * unchanged: renaming the FILE a map lives in does not rename the map its entities belong
         * to, and re-stamping every entity would be a much bigger action than the menu promises.
         */
        bool SaveMapAs(MapId InMapId, const OpaaxString& InAbsPath,
                       const World& InWorld, const ComponentRegistry& InRegistry);

        // End Saving
        // =============================================================================

        // =============================================================================
        // Get
    public:
        /** @return true when InMapId's world content no longer matches what was last written. */
        bool IsMapDirty(MapId InMapId, const World& InWorld, const ComponentRegistry& InRegistry) const;

        /** @return true when the manifest OR any mounted map would be written by SaveAll. */
        bool IsDirty(const World& InWorld, const ComponentRegistry& InRegistry, const Level& InLevel) const;

        /**
         * Recompute the cached answers below — ONE capture + serialize per mounted map.
         *
         * The caller owns the THROTTLE (**MP5**): this is the expensive form, and the Hierarchy
         * asks per map on every frame. Running it there would be N captures a frame, which is the
         * cost the derived check has always had to avoid.
         *
         * GATED on World::GetRevision(): the per-map captures are skipped entirely when the world
         * has not changed since the last call, so an idle editor costs one integer compare instead
         * of a full walk every 250ms. The gate lives here rather than with the throttle because it
         * guards the BASELINES — same split as the answers themselves. The manifest half is not
         * gated: `Set as Persistent` changes LevelData without touching an entity.
         */
        void RefreshDirty(const World& InWorld, const ComponentRegistry& InRegistry, const Level& InLevel);

        /** The last refreshed answer for one map. Pure — safe per row, per frame. */
        bool IsMapDirtyCached(MapId InMapId) const noexcept;

        /** The last refreshed answer for the LEVEL: the manifest, or any of its maps (**MP9**). */
        bool IsDirtyCached() const noexcept;

        bool               HasLevel() const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath()  const noexcept { return m_AbsPath; }

        /** Just the file name, for the menu bar — "Main.opaaxlevel". Empty when none is open. */
        OpaaxString FileName() const;

        const TDynArray<MapRecord>& GetRecords() const noexcept { return m_Maps; }

        // End Get
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /**
         * The world's content for ONE map as a COMPARISON TOKEN — the same json a save would write,
         * dumped without whitespace (MapJson::SerializeCompact).
         *
         * NOT the file's bytes, and it must not be written as them. The dirty check only ever asks
         * "same or not", and indentation is a third of the pass it pays per edit.
         */
        static OpaaxString CompareText(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId);

        MapRecord*       Find(MapId InMapId) noexcept;
        const MapRecord* Find(MapId InMapId) const noexcept;

        /** No capture has run yet — a real revision can never equal it, so the first check always runs. */
        static constexpr Uint64 k_RevisionNever = ~0ull;

        OpaaxString          m_AbsPath;
        OpaaxString          m_ManifestBaseline;
        TDynArray<MapRecord> m_Maps;
        bool                 m_bManifestDirty = false;   // cached, see RefreshDirty

        /** World revision the map records were last derived at. See RefreshDirty for the gate. */
        Uint64               m_LastRevision = k_RevisionNever;
    };
}
