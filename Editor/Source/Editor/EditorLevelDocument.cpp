#include "Editor/EditorLevelDocument.h"

#include <cstddef>   // std::ptrdiff_t — vector::erase takes a signed offset
#include <string>

#include "Application/Services/IPaths.h"
#include "Core/IO/FileIO.h"                      // the adopt-time round-trip stability check (MP6)
#include "World/Level.h"
#include "World/World.h"                         // GetRevision — the gate in RefreshDirty
#include "World/Serialization/LevelFile.h"
#include "World/Serialization/MapFile.h"
#include "World/Serialization/MapJson.h"
#include "World/Serialization/MapSerializer.h"

namespace Opaax::Editor
{
    OpaaxString EditorLevelDocument::CompareText(const World& InWorld, const ComponentRegistry& InRegistry,
                                                 MapId InMapId)
    {
        // CaptureMap, never CaptureWorld — "this map", not "the world". A runtime-spawned entity
        // carries an invalid OwnerMap and so can never match (WM2), which is what keeps bullets
        // out of an authored map without a special case, and what makes them unsaveable by
        // anything. The two are separate names precisely so this line cannot mean the other one
        // when InMapId happens to be invalid (**MP10**).
        //
        // The capture is a temporary, so SerializeCompact MOVES its payloads out rather than
        // deep-copying every component tree into the json.
        return MapJson::SerializeCompact(MapSerializer::CaptureMap(InWorld, InRegistry, InMapId));
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
        m_Maps.clear();        // a new world — nothing from the last one survives

        // ...and so does the gate. Revisions are per-World and start at zero, so a fresh world can
        // sit BELOW the value the last one reached and look unchanged forever.
        m_LastRevision = k_RevisionNever;

        // The manifest baseline comes from the LEVEL, not from re-reading the file: the engine
        // just built the world from it, so a freshly-opened editor is clean rather than dirty
        // because this build formats json differently from whoever hand-wrote the file.
        m_ManifestBaseline = LevelFile::Serialize(InLevel.GetData());

        TrackMounted(InLevel, InWorld, InRegistry, InPaths);

        OPAAX_LOG(LogEditorLevelDocument, Info, "Editing level '{}' — {} map(s) tracked",
                  HasLevel() ? m_AbsPath.CStr() : "(no level file)", m_Maps.size());
    }

    void EditorLevelDocument::TrackMounted(const Level& InLevel, const World& InWorld,
                                           const ComponentRegistry& InRegistry, const IPaths& InPaths)
    {
        const TDynArray<Level::MountedMap>& lMounted = InLevel.GetMountedMaps();

        // --- gone: drop the records of anything no longer mounted ----------------------------
        for (Uint64 lIndex = m_Maps.size(); lIndex > 0; --lIndex)
        {
            if (InLevel.IsMounted(m_Maps[lIndex - 1].Id)) { continue; }

            m_Maps.erase(m_Maps.begin() + static_cast<std::ptrdiff_t>(lIndex - 1));
        }

        // --- new: one record per mounted map that has none yet --------------------------------
        for (const Level::MountedMap& lMap : lMounted)
        {
            if (Find(lMap.Id) != nullptr)
            {
                continue;   // ALREADY TRACKED — leave its baseline alone (see the header)
            }

            MapRecord lRecord;
            lRecord.Id       = lMap.Id;
            lRecord.AbsPath  = InPaths.AssetToAbsolute(lMap.AssetRelPath);
            lRecord.Baseline = CompareText(InWorld, InRegistry, lMap.Id);

            // ROUND-TRIP STABILITY CHECK (MP6), now once per mounted map rather than once per
            // session. The milestone rests on world -> file -> world -> file being a fixed point:
            // when it is not, every Save rewrites the map with churn nobody asked for, and that is
            // completely silent otherwise — the map still loads and the world still looks right.
            //
            // The FILE form, not the baseline: this one question is about the bytes on disk, so it
            // is the only place that pays for a second, indented dump. Once per mount, not per check.
            const OpaaxString lOnDisk = FileIO::ReadAllText(lRecord.AbsPath);
            const OpaaxString lAsFile = MapJson::Serialize(MapSerializer::CaptureMap(InWorld, InRegistry, lMap.Id));

            if (lOnDisk.IsEmpty())
            {
                // No file yet (a brand-new map) — nothing to compare against.
            }
            else if (lOnDisk == lAsFile)
            {
                OPAAX_LOG(LogEditorLevelDocument, Info, "'{}' round trip is stable",
                          lMap.AssetRelPath.CStr());
            }
            else
            {
                OPAAX_LOG(LogEditorLevelDocument, Warn,
                          "'{}' re-serializes DIFFERENTLY from disk — the next Save will rewrite it "
                          "(formatting churn, or a component the registry no longer knows)",
                          lMap.AssetRelPath.CStr());
            }

            m_Maps.push_back(Move(lRecord));
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

        // ONE capture, used for both the file and the new baseline — walking the world twice for a
        // single Save would be the obvious version of this and pointlessly so.
        //
        // The two forms differ only in whitespace but each needs its own dump, so the capture is
        // turned into json twice and no more: SaveText takes the file text this already holds,
        // where Save(path, data) would have serialized the whole map a SECOND time.
        const MapData     lData = MapSerializer::CaptureMap(InWorld, InRegistry, InMapId);
        const OpaaxString lFileText = MapJson::Serialize(lData);
        const OpaaxString lText     = MapJson::SerializeCompact(lData);

        if (!MapFile::SaveText(lRecord->AbsPath, lFileText, lData.EntityCount()))
        {
            // Baseline deliberately UNTOUCHED: the document must keep reporting unsaved work
            // rather than claim to be clean against a file that was never written.
            OPAAX_LOG(LogEditorLevelDocument, Error, "Save FAILED for '{}' — still unsaved",
                      lRecord->AbsPath.CStr());
            return false;
        }

        // THE CACHED ANSWER REBASES WITH THE BASELINE — they are two halves of one fact. Leaving
        // bDirty for the next refresh used to mean a `*` lingering up to 250ms after a save; now
        // that the refresh is gated on the world's revision, and a save does not change the WORLD,
        // it would linger until the next unrelated edit.
        lRecord->Baseline = lText;
        lRecord->bDirty   = false;

        return true;
    }

    void EditorLevelDocument::SaveManifest(const Level& InLevel)
    {
        if (!HasLevel()) { return; }   // a standalone map's world has no manifest to write

        const OpaaxString lText = LevelFile::Serialize(InLevel.GetData());

        if (lText == m_ManifestBaseline) { return; }

        if (!LevelFile::Save(m_AbsPath, InLevel.GetData()))
        {
            // Baseline deliberately UNTOUCHED, exactly as SaveMap does: the document keeps
            // reporting unsaved structure rather than claiming a file that was never written.
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
            // Failing to write the NEW path must not leave the record pointing at it — the next
            // plain Save would then silently target a file that does not exist.
            lRecord->AbsPath = lPrevious;
            return false;
        }

        return true;
    }

    bool EditorLevelDocument::SaveAll(const World& InWorld, const ComponentRegistry& InRegistry,
                                      const Level& InLevel)
    {
        // SAVE LEVEL MEANS SAVE THE LEVEL (MP9): the maps first, then the manifest. Maps first so
        // a manifest that lists them is never written ahead of the content it names.
        Uint64 lWritten = 0;
        Uint64 lSkipped = 0;
        Uint64 lFailed  = 0;

        for (MapRecord& lRecord : m_Maps)
        {
            if (CompareText(InWorld, InRegistry, lRecord.Id) == lRecord.Baseline)
            {
                ++lSkipped;   // unchanged — do not touch a file this Save has nothing to say about
                continue;
            }

            if (SaveMap(lRecord.Id, InWorld, InRegistry)) { ++lWritten; }
            else                                          { ++lFailed;  }
        }

        bool lManifestWritten = false;

        if (HasLevel())
        {
            const OpaaxString lManifest = LevelFile::Serialize(InLevel.GetData());

            if (lManifest != m_ManifestBaseline)
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

        // Says what actually happened, not merely that nothing failed — "saved nothing because
        // nothing changed" and "saved nothing because it did not run" look identical otherwise
        // ([[L15]]), which is exactly the confusion that sent us looking for this command.
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
            return false;   // nothing to be dirty against
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
        // THE GATE. Every capture below is O(entities x component types) of json allocation plus a
        // full dump, and on the overwhelming majority of calls it re-derives an answer that cannot
        // have changed. The revision is a conservative over-approximation — it may move when nothing
        // really changed, which costs one wasted pass; it cannot fail to move when something did.
        const Uint64 lRevision = InWorld.GetRevision();

        if (lRevision != m_LastRevision)
        {
            m_LastRevision = lRevision;

            for (MapRecord& lRecord : m_Maps)
            {
                const bool lDirty = CompareText(InWorld, InRegistry, lRecord.Id) != lRecord.Baseline;

                // ON THE TRANSITION ONLY — twice per edit session, not per check. It is what makes
                // the `*` in the Hierarchy verifiable at all: the marker is a pixel, and "did my
                // edit register?" deserves an answer that survives into the log ([[L12]]).
                if (lDirty != lRecord.bDirty)
                {
                    OPAAX_LOG(LogEditorLevelDocument, Info, "Map '{}' {}", lRecord.Id,
                              lDirty ? "has unsaved changes" : "matches its file again");
                }

                lRecord.bDirty = lDirty;
            }
        }

        // UNGATED, and deliberately so: Set as Persistent rewrites LevelData without touching a
        // single entity, so the world's revision would not move for a change the manifest cares
        // about. It is a name and a few paths — the cost the gate exists for is not here.
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
