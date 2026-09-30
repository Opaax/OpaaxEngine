#include "Editor/Prefab/EditorPrefabDocument.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/EntityOps.h"           // PlaceInstance
#include "Editor/Operation/ResourceOperations.h"
#include "Editor/Undo/EntityUndoables.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
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
        // The world's entities as a prefab (same transform as Create Prefab from Selection). Nested
        // placements are folded back to the records the file stores. Save, the baseline and the dirty
        // check all use this.
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
            // The previous document stays open: a failed read must not half-replace it.
            return false;
        }

        if (m_World == nullptr)
        {
            // Created once and kept for the session (see the header).
            m_World = InContext.Worlds.CreateWorld(OpaaxString("Prefab Edit"), EWorldMode::Edit);

            if (m_World == nullptr)
            {
                OPAAX_LOG(LogEditorPrefabDocument, Error, "No world to edit prefabs in");
                return false;
            }
        }

        m_World->Clear();

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

        // The prefab's entities have no map: this world holds the templates, with the file's own guids.
        // Nested placements are expanded one level (deeper ones are flattened by the resolver).
        // BuildInstance needs a map id, so a placeholder is used and cleared afterwards.
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

        // Before the write: listeners that need the old prefab must read it while the old file exists.
        ResourceOps::AboutToSave<PrefabResource>(InContext, m_AbsPath);

        if (!PrefabFile::Save(m_AbsPath, lData))
        {
            // Baseline untouched, so the document still reports unsaved work.
            return false;
        }

        m_Baseline     = PrefabJson::Serialize(lData);
        m_LastRevision = ~0ull;

        // Reload the resource and reapply it to every placement in the level, keeping their overrides.
        ResourceOps::SavedToDisk<PrefabResource>(InContext, m_AbsPath);

        return true;
    }

    bool EditorPrefabDocument::SaveAsVariant(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        if (!IsOpen() || m_World == nullptr) { return false; }

        // Compared asset-relative (as the record stores it): absolute paths may spell one file two ways.
        const OpaaxString lBase    = InContext.Paths.AbsoluteToAsset(m_AbsPath);
        const OpaaxString lVariant = InContext.Paths.AbsoluteToAsset(InAbsPath);

        if (lVariant.IsEmpty())
        {
            OPAAX_LOG(LogEditorPrefabDocument, Warn,
                      "Save As Variant refused — '{}' is outside the project's and the engine's asset "
                      "trees, so no map could reference it", InAbsPath.CStr());
            return false;
        }

        if (lVariant == lBase)
        {
            OPAAX_LOG(LogEditorPrefabDocument, Error,
                      "Save As Variant refused — '{}' is the base itself; a prefab cannot be its own variant",
                      lBase.CStr());
            return false;
        }

        // The unsaved edits become the variant's overrides; the base file is left as it is.
        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();
        ResourcePrefabResolver   lResolver(InContext.Paths, InContext.Resources, lRegistry);

        const PrefabData lData = PrefabFactory::BuildVariant(
            MapSerializer::CaptureWorld(*m_World, lRegistry), lBase, lResolver, lRegistry);
        if (lData.Instances.empty()) { return false; }   // the base did not resolve (logged)

        // Same bracket as Save, so a variant written over an existing prefab reaches its placements.
        ResourceOps::AboutToSave<PrefabResource>(InContext, InAbsPath);

        if (!PrefabFile::Save(InAbsPath, lData)) { return false; }

        ResourceOps::SavedToDisk<PrefabResource>(InContext, InAbsPath);

        Uint64 lOverrides = 0;
        for (const PrefabInstanceRecord& lRecord : lData.Instances)
        {
            if (lRecord.Prefab == lBase) { lOverrides = lRecord.Overrides.size(); break; }
        }

        OPAAX_LOG(LogEditorPrefabDocument, Info, "Wrote '{}' as a variant of '{}' — {} override(s), {} own entity(ies)",
                  lVariant.CStr(), lBase.CStr(), lOverrides, lData.EntityCount());

        return Open(InContext, InAbsPath);
    }

    Uint64 EditorPrefabDocument::Place(EditorContext& InContext, const OpaaxString& InAssetPath,
                                       const Vector2F* InAtWorld, const EntityID InParent)
    {
        if (!IsOpen() || m_World == nullptr) { return 0; }

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();
        ResourcePrefabResolver   lResolver(InContext.Paths, InContext.Resources, lRegistry);

        const OpaaxString lOwnPath = InContext.Paths.AbsoluteToAsset(m_AbsPath);

        if (InAssetPath == lOwnPath || lResolver.Places(InAssetPath, lOwnPath))
        {
            OPAAX_LOG(LogEditorPrefabDocument, Error,
                      "Refused to place '{}' into '{}' — the prefab would place itself",
                      InAssetPath.CStr(), lOwnPath.CStr());
            return 0;
        }

        const PrefabData* const lPrefab = lResolver.Resolve(InAssetPath);
        if (lPrefab == nullptr)
        {
            OPAAX_LOG(LogEditorPrefabDocument, Warn, "Place refused — '{}' did not load", InAssetPath.CStr());
            return 0;
        }

        // As in Open: a placeholder map for BuildInstance, then cleared. The marker stays (Save folds by it).
        MapData lInstance = PrefabFactory::BuildInstance(*lPrefab, InAssetPath, Guid::New(),
                                                         MapId("Prefab"), lRegistry);
        for (EntityData& lEntity : lInstance.Entities) { lEntity.OwnerMap = MapId(); }

        const TDynArray<EntityID> lHandles = EntityOps::PlaceInstance(*m_World, lInstance, InAtWorld, lRegistry);
        if (lHandles.empty()) { return 0; }

        // Under the row it was dropped on, before the capture so the link is in the undo step.
        EntityOps::ParentPlaced(*m_World, lHandles, InParent);

        m_Selection.Replace(m_World, lHandles);

        m_Undo.Record(PrefabInstantiate{ MapSerializer::CaptureEntities(*m_World, lRegistry, lHandles),
                                         EUndoWorld::Prefab });

        OPAAX_LOG(LogEditorPrefabDocument, Info, "Placed '{}' into '{}' — {} entity(ies)",
                  InAssetPath.CStr(), lOwnPath.CStr(), lHandles.size());

        return lHandles.size();
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

        // Gated on the world's revision: the capture is a full serialize, and this runs every frame.
        const Uint64 lRevision = m_World->GetRevision();
        if (lRevision == m_LastRevision) { return m_bDirty; }
        m_LastRevision = lRevision;

        const bool lDirty = PrefabJson::Serialize(CaptureAsPrefab(InContext, *m_World)) != m_Baseline;

        // Logged on change only.
        if (lDirty != m_bDirty)
        {
            OPAAX_LOG(LogEditorPrefabDocument, Info, "Prefab '{}' {}", m_AbsPath.CStr(),
                      lDirty ? "has unsaved changes" : "matches its file again");
        }

        m_bDirty = lDirty;
        return lDirty;
    }
}
