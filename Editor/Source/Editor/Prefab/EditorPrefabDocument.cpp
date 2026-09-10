#include "Editor/Prefab/EditorPrefabDocument.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/ResourceOperations.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Prefab/PrefabJson.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Prefab/ResourcePrefabResolver.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor
{
    namespace
    {
        // The world's entities as a prefab — the same transform Create Prefab from Selection uses,
        // so what the panel writes and what the Hierarchy writes cannot mean different things.
        //
        // FOLDED FIRST (P7): a nested placement lives in this world as an instance with a marker,
        // exactly like a level placement, and folds back to the RECORD the file stores. Save, the
        // baseline and the dirty check all come through here, so they cannot disagree.
        PrefabData CaptureAsPrefab(EditorContext& InContext, World& InWorld)
        {
            const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

            MapData lCaptured = MapSerializer::CaptureWorld(InWorld, lRegistry);

            ResourcePrefabResolver lResolver(InContext.Paths, InContext.Resources, lRegistry);
            PrefabFold::Fold(lCaptured, lResolver, lRegistry);

            return PrefabFactory::BuildPrefab(lCaptured, lRegistry);
        }
    }

    bool EditorPrefabDocument::Open(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        PrefabData lData;
        if (!PrefabFile::Load(InAbsPath, lData))
        {
            // The previous document stays open — a failed read must not half-replace it (**MP3**).
            return false;
        }

        if (m_World == nullptr)
        {
            // Created ONCE and kept for the session; see the header for why it is never destroyed.
            m_World = InContext.Worlds.CreateWorld(OpaaxString("Prefab Edit"), EWorldMode::Edit);

            if (m_World == nullptr)
            {
                OPAAX_LOG(LogEditorPrefabDocument, Error, "No world to edit prefabs in");
                return false;
            }
        }

        m_World->Clear();

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

        // The prefab's entities carry no map (**WM2**) and none is stamped: this world holds the
        // TEMPLATES, not a placement, so their guids are the file's own.
        //
        // ITS PLACEMENTS ARE EXPANDED ONE LEVEL (P7): a nested prefab shows here as an instance
        // with a marker, the way a level shows one — the resolver flattens anything deeper, so a
        // nested-nested prefab is one instance in this panel. Expand needs a map id to stamp
        // (BuildInstance refuses none), so a placeholder goes in and is cleared off every entity.
        MapData lAsMap;
        lAsMap.Id        = MapId("Prefab");
        lAsMap.Entities  = lData.Entities;
        lAsMap.Instances = lData.Instances;

        ResourcePrefabResolver lResolver(InContext.Paths, InContext.Resources, lRegistry);
        const Uint64 lExpanded = PrefabFold::Expand(lAsMap, lResolver, lRegistry);

        for (EntityData& lEntity : lAsMap.Entities) { lEntity.OwnerMap = MapId(); }

        const Uint64 lCreated = MapFactory::Instantiate(lAsMap, *m_World, lRegistry);

        m_AbsPath  = InAbsPath;
        m_Baseline = PrefabJson::Serialize(CaptureAsPrefab(InContext, *m_World));
        ++m_Generation;
        m_Undo.Clear();           // the steps name entities that were just replaced
        m_Selection.Clear();      // and so do the handles
        m_LastRevision = ~0ull;   // a new baseline — the cached answer is about the old one
        m_bDirty       = false;

        OPAAX_LOG(LogEditorPrefabDocument, Info, "Editing prefab '{}' — {} entity(ies), {} of {} nested placement(s)",
                  InAbsPath.CStr(), lCreated, lExpanded, lData.InstanceCount());

        return true;
    }

    bool EditorPrefabDocument::Save(EditorContext& InContext)
    {
        if (!IsOpen() || m_World == nullptr) { return false; }

        const PrefabData lData = CaptureAsPrefab(InContext, *m_World);

        // BEFORE THE WRITE, and that ordering is the whole correctness of the reconcile: a
        // listener that needs the OLD prefab has to read it while the OLD FILE is still on disk.
        // Announced after the write, every resolve returned the NEW template, the fold recorded the
        // author's own edit as an override, and expand cancelled it straight back out.
        ResourceOps::AboutToSave<PrefabResource>(InContext, m_AbsPath);

        if (!PrefabFile::Save(m_AbsPath, lData))
        {
            // Baseline untouched, so the document keeps reporting unsaved work rather than claiming
            // a file it never wrote — SaveMap's rule.
            return false;
        }

        m_Baseline     = PrefabJson::Serialize(lData);
        m_LastRevision = ~0ull;

        // THE POINT OF EDITING A PREFAB (P4): reload the resource and re-apply it to every placement
        // in the level, each keeping its own overrides.
        ResourceOps::SavedToDisk<PrefabResource>(InContext, m_AbsPath);

        return true;
    }

    void EditorPrefabDocument::Close()
    {
        m_AbsPath  = OpaaxString();
        m_Baseline = OpaaxString();
        ++m_Generation;
        m_Undo.Clear();
        m_Selection.Clear();
        m_LastRevision = ~0ull;
        m_bDirty       = false;

        if (m_World != nullptr) { m_World->Clear(); }
    }

    bool EditorPrefabDocument::IsDirty(EditorContext& InContext) const
    {
        if (!IsOpen() || m_World == nullptr) { return false; }

        // GATED ON THE WORLD'S REVISION, the level document's rule: the capture below is a full
        // serialize, and this is asked every frame the panel is drawn.
        const Uint64 lRevision = m_World->GetRevision();
        if (lRevision == m_LastRevision) { return m_bDirty; }
        m_LastRevision = lRevision;

        const bool lDirty = PrefabJson::Serialize(CaptureAsPrefab(InContext, *m_World)) != m_Baseline;

        // On the transition only — "did my edit register?" deserves an answer in the log ([[L12]]).
        if (lDirty != m_bDirty)
        {
            OPAAX_LOG(LogEditorPrefabDocument, Info, "Prefab '{}' {}", m_AbsPath.CStr(),
                      lDirty ? "has unsaved changes" : "matches its file again");
        }

        m_bDirty = lDirty;
        return lDirty;
    }
}
