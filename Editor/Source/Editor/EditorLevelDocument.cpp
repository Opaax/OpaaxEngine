#include "Editor/EditorLevelDocument.h"

#include <cstddef>   // std::ptrdiff_t
#include <string>

#include "Application/Services/IPaths.h"
#include "Core/IO/FileIO.h"                      // round-trip check
#include "World/Level.h"
#include "World/World.h"                         // GetRevision
#include "World/Serialization/LevelFile.h"
#include "World/Serialization/MapFile.h"
#include "World/Serialization/MapJson.h"
#include "World/Serialization/MapSerializer.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/ResourcePrefabResolver.h"

namespace
{
    using namespace Opaax;

    /** Offset of the first difference, or the shorter length if one is a prefix of the other. */
    Uint32 FirstDifference(const OpaaxString& InA, const OpaaxString& InB) noexcept
    {
        const Uint32 lMin = InA.GetLength() < InB.GetLength() ? InA.GetLength() : InB.GetLength();

        for (Uint32 i = 0; i < lMin; ++i)
        {
            if (InA.CStr()[i] != InB.CStr()[i]) { return i; }
        }

        return lMin;
    }

    /** A short slice around InAt (to show which field differs). */
    OpaaxString Window(const OpaaxString& InText, Uint32 InAt)
    {
        constexpr Uint32 BEFORE = 40;
        constexpr Uint32 AFTER  = 80;

        const Uint32 lStart = InAt > BEFORE ? InAt - BEFORE : 0;
        return InText.SubString(lStart, BEFORE + AFTER);
    }
}

namespace Opaax::Editor
{
    MapData EditorLevelDocument::CaptureFolded(const World& InWorld, const ComponentRegistry& InRegistry,
                                               MapId InMapId) const
    {
        MapData lData = MapSerializer::CaptureMap(InWorld, InRegistry, InMapId);

        // Placements become records on every path that produces map text, so a baseline always matches
        // a written file.
        if (m_Paths != nullptr && m_Resources != nullptr)
        {
            ResourcePrefabResolver lResolver(*m_Paths, *m_Resources, InRegistry);
            PrefabFold::Fold(lData, lResolver, InRegistry);
        }

        return lData;
    }

    OpaaxString EditorLevelDocument::CompareText(const World& InWorld, const ComponentRegistry& InRegistry,
                                                 MapId InMapId) const
    {
        // CaptureMap, never CaptureWorld: runtime-spawned entities (invalid OwnerMap) never match, so they
        // are never saved. The capture is a temporary, so SerializeCompact moves its payloads.
        return MapJson::SerializeCompact(CaptureFolded(InWorld, InRegistry, InMapId));
    }

    EditorLevelDocument::MapRecord* EditorLevelDocument::Find(MapId InMapId) noexcept
    {
        for (MapRecord& lRecord : m_Maps)
        {
            if (lRecord.Id == InMapId) { return &lRecord; }
        }

        return nullptr;
    }

    const EditorLevelDocument::MapRecord* EditorLevelDocument::Find(MapId InMapId) const noexcept
    {
        for (const MapRecord& lRecord : m_Maps)
        {
            if (lRecord.Id == InMapId) { return &lRecord; }
        }

        return nullptr;
    }

    // =============================================================================
    // Adoption
    // =============================================================================

    void EditorLevelDocument::AdoptExisting(const OpaaxString& InLevelAbsPath, const Level& InLevel,
                                            const World& InWorld, const ComponentRegistry& InRegistry,
                                            const IPaths& InPaths)
    {
        m_AbsPath = InLevelAbsPath;
        m_Maps.clear();        // a new world

        // Reset the gate too: revisions are per world, so a new world may be below the last value.
        m_LastRevision = k_RevisionNever;

        // The manifest baseline comes from the Level (not the file), so a freshly opened level is clean
        // even if the file was formatted differently.
        m_ManifestBaseline = LevelFile::Serialize(InLevel.GetData());

        TrackMounted(InLevel, InWorld, InRegistry, InPaths);

        OPAAX_LOG(LogEditorLevelDocument, Info, "Editing level '{}' — {} map(s) tracked",
                  HasLevel() ? m_AbsPath.CStr() : "(no level file)", m_Maps.size());
    }

    void EditorLevelDocument::TrackMounted(const Level& InLevel, const World& InWorld,
                                           const ComponentRegistry& InRegistry, const IPaths& InPaths)
    {
        const TDynArray<Level::MountedMap>& lMounted = InLevel.GetMountedMaps();

        // --- drop the records of maps no longer mounted -----------------------------------------
        for (Uint64 lIndex = m_Maps.size(); lIndex > 0; --lIndex)
        {
            if (InLevel.IsMounted(m_Maps[lIndex - 1].Id)) { continue; }

            m_Maps.erase(m_Maps.begin() + static_cast<std::ptrdiff_t>(lIndex - 1));
        }

        // --- add a record for each newly mounted map ----------------------------------------------
        for (const Level::MountedMap& lMap : lMounted)
        {
            if (Find(lMap.Id) != nullptr)
            {
                continue;   // already tracked: keep its baseline
            }

            MapRecord lRecord;
            lRecord.Id       = lMap.Id;
            lRecord.AbsPath  = InPaths.AssetToAbsolute(lMap.AssetRelPath);
            lRecord.Baseline = CompareText(InWorld, InRegistry, lMap.Id);

            // Round-trip check, once per mounted map: world -> file -> world -> file must be stable, or every
            // Save rewrites the map with changes nobody made. Compares the file form (indented).
            const OpaaxString lOnDisk = FileIO::ReadAllText(lRecord.AbsPath);
            const OpaaxString lAsFile = MapJson::Serialize(CaptureFolded(InWorld, InRegistry, lMap.Id));

            // No file yet (new map): nothing to compare. A stable map logs nothing.
            if (!lOnDisk.IsEmpty() && lOnDisk != lAsFile)
            {
                // Show where they differ (first differing offset plus context), only on failure.
                const Uint32 lAt = FirstDifference(lOnDisk, lAsFile);

                OPAAX_LOG(LogEditorLevelDocument, Warn,
                          "'{}' re-serializes DIFFERENTLY from disk — saving it after an edit rewrites the "
                          "whole file (formatting churn, or a component the registry no longer knows); "
                          "File > Resave Level rewrites every map now.\n"
                          "  first difference at byte {} of {} (disk) / {} (rewrite)\n"
                          "  disk    ...{}...\n"
                          "  rewrite ...{}...",
                          lMap.AssetRelPath.CStr(), lAt, lOnDisk.GetLength(), lAsFile.GetLength(),
                          Window(lOnDisk, lAt).CStr(), Window(lAsFile, lAt).CStr());
            }

            m_Maps.emplace_back(Move(lRecord));
        }
    }

    void EditorLevelDocument::Clear()
    {
        m_AbsPath          = OpaaxString();
        m_ManifestBaseline = OpaaxString();
        m_LastRevision     = k_RevisionNever;
        m_Maps.clear();
    }

    // =============================================================================
    // Saving
    // =============================================================================

    bool EditorLevelDocument::SaveMap(MapId InMapId, const World& InWorld, const ComponentRegistry& InRegistry)
    {
        MapRecord* const lRecord = Find(InMapId);
        if (lRecord == nullptr)
        {
            OPAAX_LOG(LogEditorLevelDocument, Warn, "Save: '{}' is not a tracked map",
                      InMapId.IsValid() ? InMapId.CStr() : "(none)");
            return false;
        }

        // One capture for both the file and the new baseline (two dumps, one walk).
        const MapData     lData = CaptureFolded(InWorld, InRegistry, InMapId);
        const OpaaxString lFileText = MapJson::Serialize(lData);
        const OpaaxString lText     = MapJson::SerializeCompact(lData);

        if (!MapFile::SaveText(lRecord->AbsPath, lFileText, lData.EntityCount()))
        {
            // Keep the baseline: the document must still report unsaved work.
            OPAAX_LOG(LogEditorLevelDocument, Error, "Save FAILED for '{}' — still unsaved",
                      lRecord->AbsPath.CStr());
            return false;
        }

        // Update the cached answer with the baseline (the next refresh may not come until the next edit).
        lRecord->Baseline = lText;
        lRecord->bDirty   = false;

        return true;
    }

    void EditorLevelDocument::SaveManifest(const Level& InLevel)
    {
        if (!HasLevel()) { return; }   // standalone map: no manifest

        const OpaaxString lText = LevelFile::Serialize(InLevel.GetData());

        if (lText == m_ManifestBaseline) { return; }

        if (!LevelFile::Save(m_AbsPath, InLevel.GetData()))
        {
            // Keep the baseline, like SaveMap: the document must still report unsaved work.
            OPAAX_LOG(LogEditorLevelDocument, Error, "Manifest save FAILED for '{}' — still unsaved",
                      m_AbsPath.CStr());
            return;
        }

        m_ManifestBaseline = lText;
        m_bManifestDirty   = false;
    }

    bool EditorLevelDocument::SaveMapAs(MapId InMapId, const OpaaxString& InAbsPath,
                                        const World& InWorld, const ComponentRegistry& InRegistry)
    {
        MapRecord* const lRecord = Find(InMapId);
        if (lRecord == nullptr || InAbsPath.IsEmpty())
        {
            return false;
        }

        const OpaaxString lPrevious = lRecord->AbsPath;

        lRecord->AbsPath = InAbsPath;
        if (!SaveMap(InMapId, InWorld, InRegistry))
        {
            // Keep the old path if the new one could not be written.
            lRecord->AbsPath = lPrevious;
            return false;
        }

        return true;
    }

    bool EditorLevelDocument::SaveAll(const World& InWorld, const ComponentRegistry& InRegistry,
                                      const Level& InLevel, const bool bInEvenUnchanged)
    {
        // Maps first, then the manifest (never list content that is not written yet).
        Uint64 lWritten = 0;
        Uint64 lSkipped = 0;
        Uint64 lFailed  = 0;

        for (MapRecord& lRecord : m_Maps)
        {
            if (!bInEvenUnchanged && CompareText(InWorld, InRegistry, lRecord.Id) == lRecord.Baseline)
            {
                ++lSkipped;   // unchanged: do not touch the file
                continue;
            }

            if (SaveMap(lRecord.Id, InWorld, InRegistry)) { ++lWritten; }
            else                                          { ++lFailed;  }
        }

        bool lManifestWritten = false;

        if (HasLevel())
        {
            const OpaaxString lManifest = LevelFile::Serialize(InLevel.GetData());

            if (bInEvenUnchanged || lManifest != m_ManifestBaseline)
            {
                if (LevelFile::Save(m_AbsPath, InLevel.GetData()))
                {
                    m_ManifestBaseline = lManifest;
                    lManifestWritten   = true;
                }
                else
                {
                    ++lFailed;
                }
            }
        }

        // Say what happened ("nothing changed" vs "did not run").
        OPAAX_LOG(LogEditorLevelDocument, Info,
                  "Save Level '{}': {} map(s) written, {} unchanged, manifest {}{}",
                  HasLevel() ? FileName().CStr() : "(no level file)",
                  lWritten, lSkipped,
                  HasLevel() ? (lManifestWritten ? "written" : "unchanged") : "n/a",
                  lFailed > 0 ? " — SOME WRITES FAILED (see above)" : "");

        return lFailed == 0;
    }

    // =============================================================================
    // Get
    // =============================================================================

    bool EditorLevelDocument::IsMapDirty(MapId InMapId, const World& InWorld,
                                         const ComponentRegistry& InRegistry) const
    {
        const MapRecord* const lRecord = Find(InMapId);
        if (lRecord == nullptr)
        {
            return false;   // nothing to compare against
        }

        return CompareText(InWorld, InRegistry, InMapId) != lRecord->Baseline;
    }

    bool EditorLevelDocument::IsDirty(const World& InWorld, const ComponentRegistry& InRegistry,
                                      const Level& InLevel) const
    {
        if (HasLevel() && LevelFile::Serialize(InLevel.GetData()) != m_ManifestBaseline)
        {
            return true;
        }

        for (const MapRecord& lRecord : m_Maps)
        {
            if (CompareText(InWorld, InRegistry, lRecord.Id) != lRecord.Baseline) { return true; }
        }

        return false;
    }

    void EditorLevelDocument::RefreshDirty(const World& InWorld, const ComponentRegistry& InRegistry,
                                           const Level& InLevel)
    {
        // The gate: skip the captures when the world revision has not changed (it may move without a
        // real change, never the other way round).
        const Uint64 lRevision = InWorld.GetRevision();

        if (lRevision != m_LastRevision)
        {
            m_LastRevision = lRevision;

            for (MapRecord& lRecord : m_Maps)
            {
                const bool lDirty = CompareText(InWorld, InRegistry, lRecord.Id) != lRecord.Baseline;

                // Log only when the state changes (to confirm an edit was registered).
                if (lDirty != lRecord.bDirty)
                {
                    OPAAX_LOG(LogEditorLevelDocument, Info, "Map '{}' {}", lRecord.Id,
                              lDirty ? "has unsaved changes" : "matches its file again");
                }

                lRecord.bDirty = lDirty;
            }
        }

        // Not gated: Set as Persistent changes LevelData without touching an entity.
        const bool lManifestDirty = HasLevel() && LevelFile::Serialize(InLevel.GetData()) != m_ManifestBaseline;

        if (lManifestDirty != m_bManifestDirty)
        {
            OPAAX_LOG(LogEditorLevelDocument, Info, "Level manifest '{}' {}", FileName().CStr(),
                      lManifestDirty ? "has unsaved changes" : "matches its file again");
        }

        m_bManifestDirty = lManifestDirty;
    }

    bool EditorLevelDocument::IsMapDirtyCached(MapId InMapId) const noexcept
    {
        const MapRecord* const lRecord = Find(InMapId);
        return lRecord != nullptr && lRecord->bDirty;
    }

    bool EditorLevelDocument::IsDirtyCached() const noexcept
    {
        if (m_bManifestDirty) { return true; }

        for (const MapRecord& lRecord : m_Maps)
        {
            if (lRecord.bDirty) { return true; }
        }

        return false;
    }

    OpaaxString EditorLevelDocument::FileName() const
    {
        if (!HasLevel()) { return OpaaxString(); }

        const std::string lPath(m_AbsPath.CStr());
        const size_t      lSlash = lPath.find_last_of("/\\");

        return OpaaxString(lSlash == std::string::npos ? lPath.c_str() : lPath.c_str() + lSlash + 1);
    }
}
