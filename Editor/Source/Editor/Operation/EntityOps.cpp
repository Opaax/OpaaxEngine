#include "Editor/Operation/EntityOps.h"

#include "Editor/Camera/EditorCamera.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Operation/MapOperations.h"
#include "Editor/PIE/PlayInEditor.h"   // IsEdit
#include "Editor/Undo/ComponentUndoables.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/EntityUndoables.h"

#include <cmath>                 // atan2
#include <glm/geometric.hpp>     // length
#include <glm/mat2x2.hpp>        // the delta's linear part
#include <glm/matrix.hpp>        // transpose

#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"
#include "Core/Maths/Bounds2D.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "Core/Maths/Maths.h"    // RadiansToDegrees
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"   // drags move the world pose
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityQuery.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include "Application/Services/IPaths.h"         // the marker stores an asset-relative path
#include "Engine/Subsystems/Resources/ResourceManager.h"  // before PrefabResource (completes LoadContext)
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabFile.h"
#include "Editor/Operation/ResourceOperations.h"
#include "World/Prefab/ResourcePrefabResolver.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Serialization/MapFactory.h"

using namespace Opaax;

namespace
{
    constexpr LogCategory LogEntityOps{"EntityOps"};

    using namespace Opaax;

    bool NameTaken(World& InWorld, const OpaaxString& InName)
    {
        bool lTaken = false;

        InWorld.Each<EntityMeta>([&](EntityID, const EntityMeta& InMeta)
        {
            if (InMeta.Name == InName) { lTaken = true; }
        });

        return lTaken;
    }

    /**
     * A world-space linear delta in the gizmo's frame: Rot(-a) * InLinear * Rot(a) (transpose is the
     * inverse of a rotation). A scale delta R*S*R^-1 becomes a clean S for every entity; using each
     * entity's own rotation made multi-selection scaling drift. Zero returns InLinear unchanged.
     */
    glm::mat2 ToGizmoFrame(const glm::mat2& InLinear, const float InFrameRad)
    {
        if (InFrameRad == 0.f) { return InLinear; }

        const float lCos = std::cos(InFrameRad);
        const float lSin = std::sin(InFrameRad);

        const glm::mat2 lRotation{ Vector2F{ lCos, lSin }, Vector2F{ -lSin, lCos } };

        return glm::transpose(lRotation) * InLinear * lRotation;
    }

    /**
     * InBase, then "InBase 1", "InBase 2", ... (like Unity), only when needed.
     */
    OpaaxString MakeUniqueName(World& InWorld, const OpaaxString& InBase)
    {
        if (!NameTaken(InWorld, InBase)) { return InBase; }

        for (Uint32 lIndex = 1; ; ++lIndex)
        {
            OpaaxString lCandidate = InBase + OpaaxString(" ") + OpaaxString::FromUInt(lIndex);

            if (!NameTaken(InWorld, lCandidate)) { return lCandidate; }
        }
    }
}

namespace Opaax::Editor
{
    Entity EntityOps::Create(EditorContext& InContext, MapId InOwnerMap, const OpaaxString& InName)
    {
        if (!MapOps::CanEdit(InContext, "Create Entity")) { return Entity{}; }

        World* const lWorld = InContext.Worlds.GetActiveWorld();

        // No map: the entity could never be saved. Refuse with an error.
        if (!InOwnerMap.IsValid())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Create Entity refused — no map to author into. Open or focus a map first.");
            return Entity{};
        }

        const OpaaxString lName = MakeUniqueName(*lWorld, InName);

        Entity lEntity = lWorld->CreateEntity(lName, InOwnerMap);
        if (!lEntity.IsValid()) { return Entity{}; }

        InContext.Selection.Select(lEntity);
        lWorld->MarkChanged();

        // The action records its own undo step, captured after the fact (redo brings it back with the same guid).
        InContext.Undo.Record(EntityCreate{
            MapSerializer::CaptureEntities(*lWorld, InContext.Engine.GetRegistries().Components(),
                                           { lEntity.GetHandle() }) });

        OPAAX_LOG(LogEntityOps, Info, "Created entity '{}' in map '{}'",
                  lName.CStr(), InOwnerMap.ToString().CStr());

        return lEntity;
    }

    void EntityOps::ParentPlaced(World& InWorld, const TDynArray<EntityID>& InHandles, const EntityID InParent)
    {
        Entity lParent{ InParent, &InWorld };
        if (!lParent.IsValid()) { return; }

        TDynArray<EntityID> lRoots;
        EntityHierarchy::TopmostOf(InWorld, InHandles, lRoots);

        // Not keeping the world pose: the prefab's authored root pose becomes the local under the parent.
        for (const EntityID lRoot : lRoots)
        {
            EntityHierarchy::SetParent(Entity{ lRoot, &InWorld }, lParent, /*bKeepWorld*/false);
        }
    }

    Uint64 EntityOps::InstantiatePrefab(EditorContext& InContext, const OpaaxString& InAbsPath,
                                        MapId InOwnerMap, const Vector2F* InAtWorld, const EntityID InParent)
    {
        if (!MapOps::CanEdit(InContext, "Instantiate Prefab")) { return 0; }

        World* const lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return 0; }

        // Without a map these entities could never be saved.
        if (!InOwnerMap.IsValid())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Instantiate Prefab refused — no map to author into. Open or focus a map first.");
            return 0;
        }

        // The marker stores an asset-relative path. Empty means the file is outside every asset root:
        // refuse (no map could reference it).
        const OpaaxString lAssetPath = InContext.Paths.AbsoluteToAsset(InAbsPath);
        if (lAssetPath.IsEmpty())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Instantiate Prefab refused — '{}' is outside the project's and the engine's "
                      "asset trees, so no map could reference it", InAbsPath.CStr());
            return 0;
        }

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

        // Through the resolver: the prefab comes flattened and loaded once. FailFast: a bad file gives
        // null, not an empty prefab.
        ResourcePrefabResolver  lResolver(InContext.Paths, InContext.Resources, lRegistry);
        const PrefabData* const lPrefab = lResolver.Resolve(lAssetPath);

        if (lPrefab == nullptr)
        {
            OPAAX_LOG(LogEntityOps, Warn, "Instantiate Prefab refused — '{}' did not load",
                      InAbsPath.CStr());
            return 0;
        }

        const MapData lInstance = PrefabFactory::BuildInstance(*lPrefab, lAssetPath, Guid::New(),
                                                               InOwnerMap, lRegistry);
        if (lInstance.IsEmpty())
        {
            return 0;   // PrefabFactory logged why
        }

        const TDynArray<EntityID> lHandles = PlaceInstance(*lWorld, lInstance, InAtWorld, lRegistry);
        if (lHandles.empty())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Instantiate Prefab created nothing from '{}'",
                      lAssetPath.CStr());
            return 0;
        }

        // Parented under the drop row before the capture, so the link is in the undo step.
        ParentPlaced(*lWorld, lHandles, InParent);

        // The whole instance is selected.
        InContext.Selection.Replace(lWorld, lHandles);

        // Captured after the fact, so it records what was actually created.
        InContext.Undo.Record(PrefabInstantiate{
            MapSerializer::CaptureEntities(*lWorld, lRegistry, lHandles) });

        OPAAX_LOG(LogEntityOps, Info, "Instantiated {} entity(ies) from '{}' into map '{}'",
                  lHandles.size(), lAssetPath.CStr(), InOwnerMap.ToString().CStr());

        return lHandles.size();
    }

    TDynArray<EntityID> EntityOps::PlaceInstance(World& InWorld, const MapData& InInstance,
                                                 const Vector2F* InAtWorld, const ComponentRegistry& InRegistry)
    {
        TDynArray<EntityID> lHandles;

        if (MapFactory::Instantiate(InInstance, InWorld, InRegistry) == 0) { return lHandles; }

        // MapFactory only returns a count; the derived guids find what it created, in order.
        lHandles.reserve(InInstance.Entities.size());
        for (const EntityData& lEntity : InInstance.Entities)
        {
            if (Entity lFound = InWorld.FindByGuid(lEntity.Id); lFound.IsValid())
            {
                lHandles.emplace_back(lFound.GetHandle());
            }
        }

        // Moved before the caller captures, so a drop is one undo step. The roots move (children follow);
        // the first root is the anchor.
        if (InAtWorld != nullptr && !lHandles.empty())
        {
            TDynArray<EntityID> lRoots;
            EntityHierarchy::TopmostOf(InWorld, lHandles, lRoots);

            if (!lRoots.empty())
            {
                const Vector2F lDelta =
                    *InAtWorld - EntityHierarchy::WorldTransform(Entity{ lRoots.front(), &InWorld }).Position;

                for (const EntityID lRoot : lRoots)
                {
                    Entity             lEntity{ lRoot, &InWorld };
                    TransformComponent lWorldXf = EntityHierarchy::WorldTransform(lEntity);
                    lWorldXf.Position += lDelta;
                    EntityHierarchy::SetWorldTransform(lEntity, lWorldXf);
                }
            }
        }

        InWorld.MarkChanged();

        return lHandles;
    }

    bool EntityOps::CreatePrefabFromSelection(EditorContext& InContext, const OpaaxString& InAbsPath)
    {
        if (!MapOps::CanEdit(InContext, "Create Prefab")) { return false; }

        World* const lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return false; }

        if (InContext.Selection.Count() == 0)
        {
            OPAAX_LOG(LogEntityOps, Warn, "Create Prefab refused — nothing is selected.");
            return false;
        }

        // Check before writing: a prefab no map could reference would be a stray file.
        const OpaaxString lAssetPath = InContext.Paths.AbsoluteToAsset(InAbsPath);
        if (lAssetPath.IsEmpty())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Create Prefab refused — '{}' is outside the project's and the engine's asset "
                      "trees, so no map could reference it", InAbsPath.CStr());
            return false;
        }

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

        // The subtree, not only the clicked rows (children come along). Captured before the swap.
        TDynArray<EntityID> lIds;
        EntityHierarchy::CollectSubtree(*lWorld, InContext.Selection.Ids(), lIds);

        MapData lOriginals = MapSerializer::CaptureEntities(*lWorld, lRegistry, lIds);
        if (lOriginals.IsEmpty())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Create Prefab refused — the selection captured nothing.");
            return false;
        }

        // An entity whose parent is outside the set becomes a prefab root, stored at its world pose; the
        // instance's root is hung back under that parent below, so nothing moves.
        struct OutsideLink { Guid Template; Guid Parent; };
        TDynArray<OutsideLink> lOutside;

        MapData lForPrefab = lOriginals;
        if (const IComponentEntry* lTransformEntry = lRegistry.FindByTypeId(entt::type_hash<TransformComponent>::value()))
        {
            for (EntityData& lEntity : lForPrefab.Entities)
            {
                if (!lEntity.Parent.IsValid()) { continue; }

                bool lInSet = false;
                for (const EntityData& lOther : lForPrefab.Entities)
                {
                    if (lOther.Id == lEntity.Parent) { lInSet = true; break; }
                }
                if (lInSet) { continue; }

                lOutside.emplace_back(OutsideLink{ lEntity.Id, lEntity.Parent });
                lEntity.Parent = Guid{};

                const TransformComponent lWorldXf = EntityHierarchy::WorldTransform(lWorld->FindByGuid(lEntity.Id));
                for (ComponentData& lComponent : lEntity.Components)
                {
                    if (lComponent.TypeName == lTransformEntry->GetName()) { lComponent.Payload = lWorldXf; }
                }
            }
        }

        // The originals' map, not the focused one. A runtime-only selection falls back to the focused map.
        const MapId lOwnerMap = lOriginals.OwnerId().IsValid() ? lOriginals.OwnerId()
                                                               : InContext.MapDocument.GetMapId();
        if (!lOwnerMap.IsValid())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Create Prefab refused — the selection belongs to no map and none is focused.");
            return false;
        }

        const PrefabData lPrefab = PrefabFactory::BuildPrefab(lForPrefab, lRegistry);

        // Before the write (see EditorPrefabDocument::Save): matters when overwriting a placed prefab.
        ResourceOps::AboutToSave<PrefabResource>(InContext, InAbsPath);

        if (!PrefabFile::Save(InAbsPath, lPrefab))
        {
            return false;   // PrefabFile logged it; nothing changed
        }

        // Usually does nothing (a new prefab has no instances); when overwriting a placed prefab, every
        // instance rebuilds against the new file, keeping its overrides.
        ResourceOps::SavedToDisk<PrefabResource>(InContext, InAbsPath);

        const Guid lInstanceId = Guid::New();
        MapData    lInstance   = PrefabFactory::BuildInstance(lPrefab, lAssetPath, lInstanceId, lOwnerMap,
                                                              lRegistry);
        if (lInstance.IsEmpty())
        {
            // The file is written and the originals are untouched (recoverable). PrefabFactory logged why.
            return false;
        }

        // The swap. Destroy first, so the selection ends up on the instance.
        InContext.Selection.Clear();
        for (const EntityData& lEntity : lOriginals.Entities)
        {
            if (Entity lFound = lWorld->FindByGuid(lEntity.Id); lFound.IsValid())
            {
                lWorld->DestroyEntity(lFound.GetHandle());
            }
        }

        const Uint64 lCount = MapFactory::Instantiate(lInstance, *lWorld, lRegistry);

        // The roots go back under the originals' parents (saved as one override on the record).
        for (const OutsideLink& lLink : lOutside)
        {
            Entity lRoot   = lWorld->FindByGuid(Guid::Derive(lInstanceId, lLink.Template));
            Entity lParent = lWorld->FindByGuid(lLink.Parent);

            if (lRoot.IsValid() && lParent.IsValid()) { EntityHierarchy::SetParent(lRoot, lParent); }
        }

        TDynArray<EntityID> lPlaced;
        lPlaced.reserve(lInstance.Entities.size());
        for (const EntityData& lEntity : lInstance.Entities)
        {
            if (Entity lFound = lWorld->FindByGuid(lEntity.Id); lFound.IsValid())
            {
                lPlaced.emplace_back(lFound.GetHandle());
            }
        }

        InContext.Selection.Replace(lWorld, lPlaced);
        lWorld->MarkChanged();

        // One undo step for the gesture. The instance is captured after re-parenting.
        InContext.Undo.Record(PrefabCreateFromSelection{
            Move(lOriginals), MapSerializer::CaptureEntities(*lWorld, lRegistry, lPlaced) });

        OPAAX_LOG(LogEntityOps, Info,
                  "Created prefab '{}' from {} entity(ies) and replaced them with an instance of {}",
                  lAssetPath.CStr(), lPrefab.EntityCount(), lCount);

        return true;
    }

    Uint64 EntityOps::RevertToPrefab(EditorContext& InContext, bool bInWholeInstance)
    {
        if (!MapOps::CanEdit(InContext, "Revert to Prefab")) { return 0; }

        World* const lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return 0; }

        // The placements the selection touches, deduplicated (one revert per instance).
        TDynArray<Guid> lInstanceIds;
        TDynArray<Guid> lSelectedTargets;

        for (const EntityID lId : InContext.Selection.Ids())
        {
            Entity lEntity{ lId, lWorld };
            if (!lEntity.IsValid() || !lEntity.Has<PrefabInstanceComponent>()) { continue; }

            const PrefabInstanceComponent& lMarker = lEntity.Get<PrefabInstanceComponent>();
            if (!lMarker.IsLinked()) { continue; }

            lSelectedTargets.emplace_back(lEntity.GetGuid());

            bool lKnown = false;
            for (const Guid& lSeen : lInstanceIds)
            {
                if (lSeen == lMarker.InstanceId) { lKnown = true; break; }
            }

            if (!lKnown) { lInstanceIds.emplace_back(lMarker.InstanceId); }
        }

        if (lInstanceIds.empty())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Revert to Prefab — nothing in the selection came from a prefab.");
            return 0;
        }

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();
        ResourcePrefabResolver   lResolver(InContext.Paths, InContext.Resources, lRegistry);

        // One MapData of every entity to restore, built from the templates (that is what makes it a revert).
        MapData             lRestore;
        MapData             lCreated;   // pieces the revert brings back (undo destroys them)
        TDynArray<EntityID> lHandles;
        TDynArray<EntityID> lOrphans;   // pieces no longer in the prefab (removed)

        for (const Guid& lInstanceId : lInstanceIds)
        {
            // The path and map come from any entity of the instance (all share them).
            OpaaxString lPrefabPath;
            MapId       lOwnerMap;

            lWorld->Each<PrefabInstanceComponent>([&lPrefabPath, &lInstanceId](const PrefabInstanceComponent& InMarker)
            {
                if (InMarker.InstanceId == lInstanceId && lPrefabPath.IsEmpty())
                {
                    lPrefabPath = InMarker.Prefab.Path;
                }
            });

            const PrefabData* lPrefab = lResolver.Resolve(lPrefabPath);
            if (lPrefab == nullptr)
            {
                OPAAX_LOG(LogEntityOps, Warn,
                          "Revert to Prefab skipped one placement — '{}' could not be resolved",
                          lPrefabPath.CStr());
                continue;
            }

            // Built once from the prefab's guids; the map is set afterwards (BuildInstance needs a valid one,
            // so a placeholder goes in first).
            MapData lPristine = PrefabFactory::BuildInstance(*lPrefab, lPrefabPath, lInstanceId,
                                                             MapId("Pending"), lRegistry);

            // Read from a live entity, not the focused map (the placement may be in another map).
            for (const EntityData& lProbe : lPristine.Entities)
            {
                if (Entity lLive = lWorld->FindByGuid(lProbe.Id); lLive.IsValid())
                {
                    lOwnerMap = lLive.Get<EntityMeta>().OwnerMap;
                    break;
                }
            }

            if (!lOwnerMap.IsValid()) { continue; }   // no entity of this placement in the world

            // Entities whose template is no longer in the prefab are removed. A selection-only revert only
            // removes selected ones.
            lWorld->Each<PrefabInstanceComponent>([&](EntityID InId, const PrefabInstanceComponent& InMarker)
            {
                if (InMarker.InstanceId != lInstanceId) { return; }

                const Guid lGuid = Entity{ InId, lWorld }.GetGuid();

                for (const EntityData& lNamed : lPristine.Entities)
                {
                    if (lNamed.Id == lGuid) { return; }
                }

                if (!bInWholeInstance)
                {
                    bool lSelected = false;
                    for (const Guid& lTarget : lSelectedTargets)
                    {
                        if (lTarget == lGuid) { lSelected = true; break; }
                    }
                    if (!lSelected) { return; }
                }

                lOrphans.emplace_back(InId);
            });

            for (EntityData& lEntity : lPristine.Entities)
            {
                lEntity.OwnerMap = lOwnerMap;

                Entity lLive = lWorld->FindByGuid(lEntity.Id);

                if (!bInWholeInstance)
                {
                    // Selection only: a deleted entity cannot be selected (use the whole-instance revert).
                    if (!lLive.IsValid()) { continue; }

                    bool lSelected = false;
                    for (const Guid& lTarget : lSelectedTargets)
                    {
                        if (lTarget == lEntity.Id) { lSelected = true; break; }
                    }

                    if (!lSelected) { continue; }
                }

                if (lLive.IsValid())
                {
                    lHandles.emplace_back(lLive.GetHandle());
                }
                else
                {
                    // A deleted piece: MapFactory::Restore recreates it with its guid. Kept so the undo step knows
                    // what to destroy.
                    lCreated.Entities.emplace_back(lEntity);
                }

                lRestore.Entities.emplace_back(Move(lEntity));
            }
        }

        if (lRestore.Entities.empty() && lOrphans.empty())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Revert to Prefab — nothing to revert.");
            return 0;
        }

        // Before, captured while the overrides are still there (orphans included, so undo restores
        // them). What redo must destroy again is captured separately.
        for (const EntityID lOrphan : lOrphans) { lHandles.emplace_back(lOrphan); }

        MapData lBefore    = MapSerializer::CaptureEntities(*lWorld, lRegistry, lHandles);
        MapData lDestroyed = MapSerializer::CaptureEntities(*lWorld, lRegistry, lOrphans);

        const Uint64 lReverted = MapFactory::Restore(lRestore, *lWorld, lRegistry);

        // Removed from the selection before they are destroyed.
        for (const EntityID lOrphan : lOrphans)
        {
            if (Entity lE{ lOrphan, lWorld }; InContext.Selection.Contains(lE)) { InContext.Selection.Toggle(lE); }
        }
        const Uint64 lRemoved = DestroyEntities(*lWorld, lOrphans);

        // Handles resolved again after the restore (recreated entities did not exist before).
        TDynArray<EntityID> lAfterHandles;
        lAfterHandles.reserve(lRestore.Entities.size());
        for (const EntityData& lEntity : lRestore.Entities)
        {
            if (Entity lLive = lWorld->FindByGuid(lEntity.Id); lLive.IsValid())
            {
                lAfterHandles.emplace_back(lLive.GetHandle());
            }
        }

        lWorld->MarkChanged();

        // Read before the move.
        const Uint64 lRecreated = lCreated.EntityCount();

        InContext.Undo.Record(PrefabRevert{
            Move(lBefore),
            MapSerializer::CaptureEntities(*lWorld, lRegistry, lAfterHandles),
            Move(lCreated),
            Move(lDestroyed) });

        OPAAX_LOG(LogEntityOps, Info,
                  "Reverted {} entity(ies) across {} placement(s) to their prefab ({} recreated, {} removed)",
                  lReverted, lInstanceIds.size(), lRecreated, lRemoved);

        return lReverted + lRemoved;
    }

    void EntityOps::Rename(EditorContext& InContext, Entity InEntity, const OpaaxString& InName)
    {
        if (!InEntity.IsValid() || InName.IsEmpty()) { return; }

        if (!MapOps::CanEdit(InContext, "Rename Entity")) { return; }

        EntityMeta& lMeta = InEntity.Get<EntityMeta>();
        if (lMeta.Name == InName) { return; }   // unchanged: not an edit

        OPAAX_LOG(LogEntityOps, Info, "Renamed '{}' -> '{}'", lMeta.Name.CStr(), InName.CStr());

        // Two strings and a guid: the whole step.
        InContext.Undo.Record(EntityRename{ InEntity.GetGuid(), lMeta.Name, InName });

        lMeta.Name = InName;

        // EntityMeta is written directly, so mark the world changed.
        if (World* lWorld = InEntity.GetWorld()) { lWorld->MarkChanged(); }
    }

    bool EntityOps::Reparent(EditorContext& InContext, const EUndoWorld InScope, const EntityID InChild,
                             const EntityID InParent, const MapId InToMap)
    {
        // The prefab world is always Edit; only the level checks for Play.
        if (InScope == EUndoWorld::Active && !MapOps::CanEdit(InContext, "Reparent")) { return false; }

        World* const lWorld = UndoWorld(InContext, InScope);
        if (lWorld == nullptr) { return false; }

        Entity lChild{ InChild, lWorld };
        Entity lParent{ InParent, lWorld };
        if (!lChild.IsValid()) { return false; }

        // Before, for the whole subtree (a cross-map drop may change their map).
        TDynArray<EntityID> lSubtree;
        EntityHierarchy::CollectSubtree(*lWorld, { InChild }, lSubtree);

        EntityReparent lStep;
        lStep.Scope = InScope;
        lStep.Entries.reserve(lSubtree.size());

        for (const EntityID lId : lSubtree)
        {
            Entity            lEntity{ lId, lWorld };
            const EntityMeta& lMeta = lEntity.Get<EntityMeta>();

            EntityReparent::Entry lEntry;
            lEntry.Id          = lMeta.Id;
            lEntry.ParentBefore = lMeta.Parent;
            lEntry.MapBefore    = lMeta.OwnerMap;
            lEntry.LocalBefore  = lEntity.Get<TransformComponent>();
            lStep.Entries.emplace_back(Move(lEntry));
        }

        if (!EntityHierarchy::SetParent(lChild, lParent)) { return false; }   // refused (it logged why)

        // A drop on a map header: to root, in that map. A parent brings its map by itself.
        if (!lParent.IsValid() && InToMap.IsValid())
        {
            for (const EntityID lId : lSubtree)
            {
                EntityMeta& lMeta = lWorld->GetRegistry().get<EntityMeta>(lId);
                if (lMeta.OwnerMap.IsValid()) { lMeta.OwnerMap = InToMap; }
            }
        }

        bool lChanged = false;
        for (EntityReparent::Entry& lEntry : lStep.Entries)
        {
            Entity lEntity = lWorld->FindByGuid(lEntry.Id);
            if (!lEntity.IsValid()) { continue; }

            const EntityMeta& lMeta = lEntity.Get<EntityMeta>();
            lEntry.ParentAfter = lMeta.Parent;
            lEntry.MapAfter    = lMeta.OwnerMap;
            lEntry.LocalAfter  = lEntity.Get<TransformComponent>();

            lChanged = lChanged || lEntry.ParentAfter != lEntry.ParentBefore || lEntry.MapAfter != lEntry.MapBefore;
        }

        if (!lChanged) { return false; }   // already there: nothing to record

        lWorld->MarkChanged();
        UndoStack(InContext, InScope).Record(Move(lStep));

        return true;
    }

    void EntityOps::DetachSelected(EditorContext& InContext, const EUndoWorld InScope)
    {
        World* const lWorld = UndoWorld(InContext, InScope);
        if (lWorld == nullptr) { return; }

        // A copy: Reparent does not touch the selection, but it costs nothing.
        const TDynArray<EntityID> lIds = UndoSelection(InContext, InScope).Ids();

        Uint64 lDetached = 0;
        for (const EntityID lId : lIds)
        {
            if (!EntityHierarchy::GetParent(Entity{ lId, lWorld }).IsValid()) { continue; }
            if (Reparent(InContext, InScope, lId, ENTITY_NONE)) { ++lDetached; }
        }

        OPAAX_LOG(LogEntityOps, Info, "Detached {} entity(ies) to root", lDetached);
    }

    void EntityOps::DestroySelected(EditorContext& InContext)
    {
        if (!InContext.Selection.HasSelection())
        {
            return;   // nothing selected (normal)
        }

        if (!MapOps::CanEdit(InContext, "Delete Entity")) { return; }

        World* const lWorld = InContext.Selection.GetWorld();
        if (lWorld == nullptr) { return; }

        // Copy the handles first (the selection is cleared and the entities destroyed). With the subtree:
        // the world would cascade anyway, and undo must restore all of it.
        TDynArray<EntityID> lIds;
        EntityHierarchy::CollectSubtree(*lWorld, InContext.Selection.Ids(), lIds);

        // Captured before, unlike other actions: once destroyed nothing can say what they were.
        EntityDelete lStep{ MapSerializer::CaptureEntities(
            *lWorld, InContext.Engine.GetRegistries().Components(), lIds), EUndoWorld::Active };

        InContext.Selection.Clear();

        const Uint64 lCount = DestroyEntities(*lWorld, lIds);

        InContext.Undo.Record(Move(lStep));

        OPAAX_LOG(LogEntityOps, Info, "Deleted {} entity(ies)", lCount);
    }

    Uint64 EntityOps::DestroyEntities(World& InWorld, const TDynArray<EntityID>& InEntities)
    {
        Uint64 lCount = 0;

        for (const EntityID lId : InEntities)
        {
            if (!InWorld.IsValid(lId)) { continue; }

            InWorld.DestroyEntity(lId);
            ++lCount;
        }

        if (lCount > 0) { InWorld.MarkChanged(); }

        return lCount;
    }

    bool EntityOps::AddComponent(EditorContext& InContext, Entity InEntity, const OpaaxStringID InTypeName)
    {
        if (!InEntity.IsValid() || !MapOps::CanEdit(InContext, "Add Component")) { return false; }

        const ComponentRegistry& lTypes = InContext.Engine.GetRegistries().Components();
        const IComponentEntry*   lEntry = lTypes.FindByName(InTypeName);

        if (lEntry == nullptr)
        {
            OPAAX_LOG(LogEntityOps, Warn, "Add Component refused — '{}' is not a registered type", InTypeName);
            return false;
        }

        EntityRegistry& lEntities = InEntity.GetWorld()->GetRegistry();

        if (lEntry->Has(lEntities, InEntity.GetHandle())) { return false; }

        lEntry->Add(lEntities, InEntity.GetHandle());

        // Explicit: adding a component is a content change.
        InEntity.GetWorld()->MarkChanged();

        // No payload: Add default-constructs, so redo has nothing to restore.
        InContext.Undo.Record(ComponentAdd{ InEntity.GetGuid(), InTypeName });

        OPAAX_LOG(LogEntityOps, Info, "Added component '{}' to entity '{}'",
                  InTypeName, InEntity.Get<EntityMeta>().Name.CStr());

        return true;
    }

    bool EntityOps::RemoveComponent(EditorContext& InContext, Entity InEntity, const OpaaxStringID InTypeName)
    {
        if (!InEntity.IsValid() || !MapOps::CanEdit(InContext, "Remove Component")) { return false; }

        const ComponentRegistry& lTypes = InContext.Engine.GetRegistries().Components();
        const IComponentEntry*   lEntry = lTypes.FindByName(InTypeName);

        if (lEntry == nullptr)
        {
            OPAAX_LOG(LogEntityOps, Warn, "Remove Component refused — '{}' is not a registered type", InTypeName);
            return false;
        }

        EntityRegistry& lEntities = InEntity.GetWorld()->GetRegistry();

        if (!lEntry->Has(lEntities, InEntity.GetHandle())) { return false; }

        // Read while it exists, so undo restores its values (not the defaults).
        nlohmann::json lData = lEntry->Save(lEntities, InEntity.GetHandle());

        // Essential types are refused by the entry itself.
        if (!lEntry->Remove(lEntities, InEntity.GetHandle())) { return false; }

        InEntity.GetWorld()->MarkChanged();

        InContext.Undo.Record(ComponentRemove{ InEntity.GetGuid(), InTypeName, Move(lData) });

        OPAAX_LOG(LogEntityOps, Info, "Removed component '{}' from entity '{}'",
                  InTypeName, InEntity.Get<EntityMeta>().Name.CStr());

        return true;
    }

    void EntityOps::TransformSelected(EditorContext& InContext, const TransformDelta& InDelta)
    {
        if (!InContext.Selection.HasSelection()) { return; }

        if (!MapOps::CanEdit(InContext, "Transform Entity")) { return; }

        World* const lWorld = InContext.Selection.GetWorld();
        if (lWorld == nullptr) { return; }

        // The level's version. The world-agnostic part is TransformEntities, also used by the prefab viewport.
        TransformEntities(*lWorld, InContext.Selection.Ids(), InDelta);
    }

    bool EntityOps::TransformEntities(World& InWorld, const TDynArray<EntityID>& InEntities,
                                      const TransformDelta& InDelta)
    {
        // The delta's linear part holds rotation and scale; positions go through the whole matrix below.
        // Converted once, in the gizmo's frame (the same for every entity).
        const glm::mat2 lWorldLinear{ Vector2F{ InDelta.Matrix[0][0], InDelta.Matrix[0][1] },
                                      Vector2F{ InDelta.Matrix[1][0], InDelta.Matrix[1][1] } };

        const glm::mat2 lLinear = ToGizmoFrame(lWorldLinear, InDelta.FrameRad);

        const float    lDeltaDegrees = Maths::RadiansToDegrees(std::atan2(lLinear[0][1], lLinear[0][0]));
        const Vector2F lDeltaScale{ glm::length(lLinear[0]), glm::length(lLinear[1]) };

        // Not logged per call (one per frame during a drag).
        bool lChanged = false;

        // A selected child of a selected parent moves once, with its parent.
        TDynArray<EntityID> lTopmost;
        EntityHierarchy::TopmostOf(InWorld, InEntities, lTopmost);

        for (const EntityID lId : lTopmost)
        {
            Entity lEntity{ lId, &InWorld };

            // Every entity has one, so a miss means a stale handle: skip it.
            if (lEntity.TryGet<TransformComponent>() == nullptr) { continue; }

            // The delta is world-space: applied to the world pose, stored as local.
            TransformComponent lWorldXf = EntityHierarchy::WorldTransform(lEntity);

            // Positions go through the matrix, so rotate and scale orbit the shared pivot. Individual
            // Origins skips this step (rotation and scale still apply, the position stays).
            const Vector4F lMoved =
                InDelta.Origin == ETransformOrigin::Individual
                    ? Vector4F(lWorldXf.Position.x, lWorldXf.Position.y, 0.f, 1.f)
                    : InDelta.Matrix * Vector4F(lWorldXf.Position.x, lWorldXf.Position.y, 0.f, 1.f);

            lWorldXf.Position  = { lMoved.x, lMoved.y };
            lWorldXf.Rotation += lDeltaDegrees;
            lWorldXf.Scale    *= lDeltaScale;

            EntityHierarchy::SetWorldTransform(lEntity, lWorldXf);

            lChanged = true;
        }

        if (lChanged) { InWorld.MarkChanged(); }

        return lChanged;
    }

    void EntityOps::FocusSelected(EditorContext& InContext)
    {
        if (!InContext.Selection.HasSelection())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Focus Selected — nothing is selected");
            return;
        }

        // Not CanEdit (focusing is not an edit): a Play world is framed by its CameraComponent, so moving
        // the editor camera would do nothing.
        if (!InContext.PIE.IsEdit())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Focus Selected ignored — a Play world is framed by its own camera");
            return;
        }

        if (!InContext.Viewport.IsValid())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Focus Selected — the viewport has not been measured yet");
            return;
        }

        World* const lWorld = InContext.Selection.GetWorld();
        if (lWorld == nullptr) { return; }

        // The anchor size makes an entity with nothing to draw framable (same helper as the viewport icon).
        // A fixed world size: the camera has not moved yet, so there is no pixel scale.
        Bounds2D lBounds;
        if (!EntityQuery::TryGetBounds(*lWorld, InContext.Selection.Ids(), lBounds, 25.f))
        {
            OPAAX_LOG(LogEntityOps, Warn, "Focus Selected — nothing selected has a position");
            return;
        }

        InContext.Camera.FocusOn(lBounds, InContext.Viewport.GetSizePx());
    }
}
