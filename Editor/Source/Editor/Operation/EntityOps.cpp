#include "Editor/Operation/EntityOps.h"

#include "Editor/Camera/EditorCamera.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Operation/MapOperations.h"
#include "Editor/PIE/PlayInEditor.h"   // IsEdit — the focus rule is about which camera owns the view
#include "Editor/Undo/ComponentUndoables.h"   // ⑤ — the steps these verbs record
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Undo/EntityUndoables.h"

#include <cmath>                 // atan2 — the delta's turn, read off its own basis
#include <glm/geometric.hpp>     // length — and its stretch
#include <glm/mat2x2.hpp>        // the delta's LINEAR part, conjugated into an entity's own frame
#include <glm/matrix.hpp>        // transpose — a rotation's inverse

#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"
#include "Core/Maths/Bounds2D.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "Core/Maths/Maths.h"    // RadiansToDegrees — the transform authors degrees
#include "World/Components/TransformComponent.h"   // I17 — the one position a drag writes
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"   // §HR — a drag moves the WORLD pose of the topmost selected
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityQuery.h"
#include "World/Serialization/MapSerializer.h"   // ⑤ — what a create/delete step carries
#include "World/World.h"
#include "World/WorldManager.h"

#include "Application/Services/IPaths.h"         // ⑦-C — the marker stores an ASSET-relative path
#include "Engine/Subsystems/Resources/ResourceManager.h"  // before PrefabResource — completes LoadContext
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
     * A world-space linear delta re-expressed in the frame it was BUILT in:
     * `Rot(-θ)·InLinear·Rot(θ)`, transpose standing in for the inverse since a rotation is
     * orthonormal.
     *
     * THE FRAME IS THE GIZMO'S, NOT THE ENTITY'S, and getting that wrong is what made a
     * multi-selection scale drift. A scale delta arrives as `R·S·R⁻¹` where R is the pose the
     * gizmo was seated with; conjugating by R recovers a clean diagonal S for EVERY entity.
     * Conjugating by each entity's own rotation only cancels for the one entity whose rotation
     * happens to match the gizmo — every other one gets a non-diagonal matrix, whose `atan2`
     * reports a turn nobody asked for.
     *
     * Zero returns InLinear untouched, so the unrotated case is bit-identical to what shipped.
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
     * InBase, then "InBase 1", "InBase 2"... — Unity's shape, and it only appends when it has to.
     * Linear per attempt, which is nothing at authoring scale and needs no counter to keep in sync
     * with entities that have been deleted or renamed.
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

        // An invalid map is the WM2 state this verb exists to make unreachable — refuse loudly
        // rather than quietly authoring something no Save can ever write.
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

        // THE VERB RECORDS ITS OWN STEP (⑤). Captured AFTER the fact, which is the only moment the
        // entity exists to be captured — and it is what lets redo bring it back on the same Guid.
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

        // NOT keeping the world pose: the prefab's authored root pose is what its local becomes,
        // so a turret authored at the origin sits ON its new parent rather than staying where the
        // prefab happened to be authored.
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

        // Create's rule (**WM2**): without a map these entities would read as runtime-spawned and
        // no Save could ever write them.
        if (!InOwnerMap.IsValid())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Instantiate Prefab refused — no map to author into. Open or focus a map first.");
            return 0;
        }

        // What the MARKER stores (**MP8**). Empty means the file is under neither asset root, so no
        // map could name it — refuse rather than write a path that resolves on this machine only.
        const OpaaxString lAssetPath = InContext.Paths.AbsoluteToAsset(InAbsPath);
        if (lAssetPath.IsEmpty())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Instantiate Prefab refused — '{}' is outside the project's and the engine's "
                      "asset trees, so no map could reference it", InAbsPath.CStr());
            return 0;
        }

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

        // Through the RESOLVER, never the raw resource (P7): it hands the prefab back FLATTENED —
        // its nested placements expanded — and goes through the ResourceManager underneath, which
        // is what dedups a level placing many instances of one prefab. FailFast makes a missing or
        // malformed file a null rather than an empty prefab that instantiates nothing and reports
        // success.
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
            return 0;   // PrefabFactory logged which refusal it was
        }

        const TDynArray<EntityID> lHandles = PlaceInstance(*lWorld, lInstance, InAtWorld, lRegistry);
        if (lHandles.empty())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Instantiate Prefab created nothing from '{}'",
                      lAssetPath.CStr());
            return 0;
        }

        // Under the row it was dropped on (§HR), BEFORE the capture so the link is in the step.
        ParentPlaced(*lWorld, lHandles, InParent);

        // The WHOLE instance is selected — **K10**.
        InContext.Selection.Replace(lWorld, lHandles);

        // Captured AFTER the fact, exactly as Create does — it records what the world actually got,
        // not what was asked for, so an entity Instantiate refused is not in the step either.
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

        // MapFactory answers only a count; the instance's derived guids are the way back to what it
        // created, in the instance's order.
        lHandles.reserve(InInstance.Entities.size());
        for (const EntityData& lEntity : InInstance.Entities)
        {
            if (Entity lFound = InWorld.FindByGuid(lEntity.Id); lFound.IsValid())
            {
                lHandles.emplace_back(lFound.GetHandle());
            }
        }

        // MOVED BEFORE THE CALLER CAPTURES, so a drop is ONE undo step rather than a place followed
        // by a move. The instance's ROOTS move and their children ride along (§HR); the first root
        // is the anchor and the rest keep their relative offsets.
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

        // BEFORE writing anything: a prefab no map could reference is worse than no prefab, and the
        // author would have a stray file to clean up (**MP8**).
        const OpaaxString lAssetPath = InContext.Paths.AbsoluteToAsset(InAbsPath);
        if (lAssetPath.IsEmpty())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Create Prefab refused — '{}' is outside the project's and the engine's asset "
                      "trees, so no map could reference it", InAbsPath.CStr());
            return false;
        }

        const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();

        // THE SUBTREE, not the rows clicked (§HR): a prefab made of a parent takes its children,
        // as Unity's does. Captured BEFORE the swap — nothing else can recover the originals.
        TDynArray<EntityID> lIds;
        EntityHierarchy::CollectSubtree(*lWorld, InContext.Selection.Ids(), lIds);

        MapData lOriginals = MapSerializer::CaptureEntities(*lWorld, lRegistry, lIds);
        if (lOriginals.IsEmpty())
        {
            OPAAX_LOG(LogEntityOps, Warn, "Create Prefab refused — the selection captured nothing.");
            return false;
        }

        // An entity parented OUTSIDE the set becomes one of the prefab's roots. Its local is
        // relative to a parent the file will not hold, so the prefab gets its WORLD pose instead —
        // only this caller has the world to ask — and the instance's root is hung back under that
        // parent below, so nothing moves and nothing changes place in the tree.
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

        // The ORIGINALS' map, not the focused one: cutting a prefab out of map A while B is focused
        // must not move the result to B. A purely runtime-spawned selection falls back.
        const MapId lOwnerMap = lOriginals.OwnerId().IsValid() ? lOriginals.OwnerId()
                                                               : InContext.MapDocument.GetMapId();
        if (!lOwnerMap.IsValid())
        {
            OPAAX_LOG(LogEntityOps, Warn,
                      "Create Prefab refused — the selection belongs to no map and none is focused.");
            return false;
        }

        const PrefabData lPrefab = PrefabFactory::BuildPrefab(lForPrefab, lRegistry);

        // Before the write — see EditorPrefabDocument::Save. Matters when this OVERWRITES a
        // prefab that is already placed.
        ResourceOps::AboutToSave<PrefabResource>(InContext, InAbsPath);

        if (!PrefabFile::Save(InAbsPath, lPrefab))
        {
            return false;   // PrefabFile logged it; nothing has been touched
        }

        // ⑦-C P4. Usually a no-op — a NEW prefab has no instances to update. It matters when this
        // OVERWRITES a prefab that is already placed: every existing instance of that path then
        // rebuilds against what was just written, keeping its own overrides. Announced here rather
        // than left to the caller for the reason the other eight Save ops go through this seam: a
        // write that does not announce is a write half the editor never hears about.
        ResourceOps::SavedToDisk<PrefabResource>(InContext, InAbsPath);

        const Guid lInstanceId = Guid::New();
        MapData    lInstance   = PrefabFactory::BuildInstance(lPrefab, lAssetPath, lInstanceId, lOwnerMap,
                                                              lRegistry);
        if (lInstance.IsEmpty())
        {
            // The file is written and the originals are untouched — a recoverable state, which is
            // why the write goes first. PrefabFactory logged the refusal.
            return false;
        }

        // THE SWAP. Destroy first so the derived guids cannot meet their own templates: an instance
        // built from these very entities carries DIFFERENT ids (Guid::Derive), so a collision is not
        // actually possible — but destroying first is also what makes the selection end up on the
        // instance rather than on entities that are about to go.
        InContext.Selection.Clear();
        for (const EntityData& lEntity : lOriginals.Entities)
        {
            if (Entity lFound = lWorld->FindByGuid(lEntity.Id); lFound.IsValid())
            {
                lWorld->DestroyEntity(lFound.GetHandle());
            }
        }

        const Uint64 lCount = MapFactory::Instantiate(lInstance, *lWorld, lRegistry);

        // The roots go back under the parents the originals had — a placement's root parent is
        // scene state, not a prefab property, so it lands as ONE override on this record at save.
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

        // ONE step for one gesture — see PrefabCreateFromSelection for why this is not two. The
        // instance is captured AFTER the re-parenting so redo puts it back where it hangs.
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

        // Which PLACEMENTS the selection touches. Deduped, because a multi-entity selection inside
        // one instance must revert that instance once, not once per entity.
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

        // One MapData naming every entity to restore, built from the TEMPLATES — which is what
        // makes this a revert rather than a re-save of what is already there.
        MapData             lRestore;
        MapData             lCreated;   // pieces the revert brings BACK — what undo must destroy
        TDynArray<EntityID> lHandles;
        TDynArray<EntityID> lOrphans;   // pieces whose TEMPLATE the prefab no longer has — taken away

        for (const Guid& lInstanceId : lInstanceIds)
        {
            // The path and the map come off any entity of the instance; every entity of one
            // placement carries the same pair.
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

            // Built ONCE, against the prefab's own guids; the map is stamped afterwards because it
            // is not knowable until a live entity has been found. `BuildInstance` refuses an
            // invalid map, so a placeholder goes in and the real answer replaces it below.
            MapData lPristine = PrefabFactory::BuildInstance(*lPrefab, lPrefabPath, lInstanceId,
                                                             MapId("Pending"), lRegistry);

            // READ FROM A LIVE ENTITY, never assumed to be the focused map: reverting a placement
            // that lives in an unfocused map must not re-stamp it into the cursor's map.
            for (const EntityData& lProbe : lPristine.Entities)
            {
                if (Entity lLive = lWorld->FindByGuid(lProbe.Id); lLive.IsValid())
                {
                    lOwnerMap = lLive.Get<EntityMeta>().OwnerMap;
                    break;
                }
            }

            if (!lOwnerMap.IsValid()) { continue; }   // no entity of this placement is in the world

            // A PREFAB CAN REMOVE A PIECE (L87's other half): an entity of this placement whose
            // template is no longer in the prefab is named by nothing above, and "be this again"
            // means it goes. Selection-only reverts take only the selected ones.
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
                    // Selection-only: a DELETED entity cannot be selected, so it is not a target
                    // here. Bringing deleted pieces back is what the whole-instance entry is for.
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
                    // A DELETED piece of the placement. MapFactory::Restore recreates it on its own
                    // Guid — that is what "be this again" means — and SKIPPING it here was why
                    // deleting one half of a turret could never be undone by a revert.
                    //
                    // Kept so the undo step knows what to destroy: nothing else can tell that this
                    // entity did not exist beforehand, and Restore leaves absent entities alone.
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

        // BEFORE, captured while the overrides are still on the entities — nothing else can
        // recover them once Restore has run. The orphans are in it too, which is what lets undo
        // bring them back; what redo must destroy again is captured on its own.
        for (const EntityID lOrphan : lOrphans) { lHandles.emplace_back(lOrphan); }

        MapData lBefore    = MapSerializer::CaptureEntities(*lWorld, lRegistry, lHandles);
        MapData lDestroyed = MapSerializer::CaptureEntities(*lWorld, lRegistry, lOrphans);

        const Uint64 lReverted = MapFactory::Restore(lRestore, *lWorld, lRegistry);

        // Out of the selection before they go, so no destroyed handle lingers there.
        for (const EntityID lOrphan : lOrphans)
        {
            if (Entity lE{ lOrphan, lWorld }; InContext.Selection.Contains(lE)) { InContext.Selection.Toggle(lE); }
        }
        const Uint64 lRemoved = DestroyEntities(*lWorld, lOrphans);

        // Handles re-resolved AFTER the restore: a recreated entity did not exist when the list
        // above was built, and an After that omitted it would make redo silently drop it again.
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

        // Read BEFORE the move — a moved-from container is not required to still hold anything.
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
        if (lMeta.Name == InName) { return; }   // committing an untouched field is not an edit

        OPAAX_LOG(LogEntityOps, Info, "Renamed '{}' -> '{}'", lMeta.Name.CStr(), InName.CStr());

        // Two strings and a Guid: the whole step, and the reason a rename serializes nothing (⑤).
        InContext.Undo.Record(EntityRename{ InEntity.GetGuid(), lMeta.Name, InName });

        lMeta.Name = InName;

        // EntityMeta is written straight through, so nothing else observes it — the same reason
        // the Inspector's drawers need World::MarkChanged.
        if (World* lWorld = InEntity.GetWorld()) { lWorld->MarkChanged(); }
    }

    bool EntityOps::Reparent(EditorContext& InContext, const EUndoWorld InScope, const EntityID InChild,
                             const EntityID InParent, const MapId InToMap)
    {
        // The prefab world is Edit whatever the level is doing (PF10); only the level gates on PIE.
        if (InScope == EUndoWorld::Active && !MapOps::CanEdit(InContext, "Reparent")) { return false; }

        World* const lWorld = UndoWorld(InContext, InScope);
        if (lWorld == nullptr) { return false; }

        Entity lChild{ InChild, lWorld };
        Entity lParent{ InParent, lWorld };
        if (!lChild.IsValid()) { return false; }

        // BEFORE, for the whole subtree: every entity's map may follow a cross-map drop.
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

        if (!EntityHierarchy::SetParent(lChild, lParent)) { return false; }   // refused, and it said why

        // A drop on a map HEADER: to root, in THAT map. A parent pulls its map on its own.
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

        if (!lChanged) { return false; }   // already there — SetParent said nothing, nothing to record

        lWorld->MarkChanged();
        UndoStack(InContext, InScope).Record(Move(lStep));

        return true;
    }

    void EntityOps::DetachSelected(EditorContext& InContext, const EUndoWorld InScope)
    {
        World* const lWorld = UndoWorld(InContext, InScope);
        if (lWorld == nullptr) { return; }

        // A copy: Reparent does not touch the selection, but the rule costs nothing.
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
            return;   // nothing to do, and nothing worth a log line — Delete on empty is ordinary
        }

        if (!MapOps::CanEdit(InContext, "Delete Entity")) { return; }

        World* const lWorld = InContext.Selection.GetWorld();
        if (lWorld == nullptr) { return; }

        // COPY the handles before touching anything: the selection is about to be cleared and the
        // entities destroyed, and iterating the live list while doing either is the shape that made
        // the Hierarchy's Remove from Level assert. WITH THE SUBTREE (§HR): the world would cascade
        // anyway, and the step must hold what the cascade takes or undo brings back half of it.
        TDynArray<EntityID> lIds;
        EntityHierarchy::CollectSubtree(*lWorld, InContext.Selection.Ids(), lIds);

        // BEFORE the fact, unlike every other verb here: once these are destroyed nothing else in
        // the editor can say what they were (⑤).
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

        // Explicit, not left to the Inspector's "any item active" check: emplacing a component IS a
        // content change whether or not a popup item still counts as active.
        InEntity.GetWorld()->MarkChanged();

        // No payload — Add default-constructs, so there is nothing for a redo to restore (⑤).
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

        // Read while it still exists: without its values, undo would bring the type back at its
        // defaults, which reads as data loss rather than as an undo (⑤).
        nlohmann::json lData = lEntry->Save(lEntities, InEntity.GetHandle());

        // Refuses an essential type on its own — the guarantee lives in the entry, not in every
        // caller remembering it.
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

        // THE LEVEL'S surface, named explicitly. Everything below this line is world-agnostic and
        // lives in TransformEntities, so the prefab viewport drives the SAME verb with its own world
        // and its own selection rather than a second copy of the gizmo maths (**MP7**).
        TransformEntities(*lWorld, InContext.Selection.Ids(), InDelta);
    }

    bool EntityOps::TransformEntities(World& InWorld, const TDynArray<EntityID>& InEntities,
                                      const TransformDelta& InDelta)
    {
        // The delta's LINEAR part carries the rotation and the scale; the translation is handled by
        // running each position through the whole matrix below.
        //
        // CONJUGATED ONCE, HERE, in the gizmo's own frame — the answer is the same for every entity,
        // which is both why it is out of the loop and why it is correct. Doing it per entity was the
        // bug: only the entity matching the gizmo's pose came back clean.
        const glm::mat2 lWorldLinear{ Vector2F{ InDelta.Matrix[0][0], InDelta.Matrix[0][1] },
                                      Vector2F{ InDelta.Matrix[1][0], InDelta.Matrix[1][1] } };

        const glm::mat2 lLinear = ToGizmoFrame(lWorldLinear, InDelta.FrameRad);

        const float    lDeltaDegrees = Maths::RadiansToDegrees(std::atan2(lLinear[0][1], lLinear[0][0]));
        const Vector2F lDeltaScale{ glm::length(lLinear[0]), glm::length(lLinear[1]) };

        // NOT logged per call: a drag lands one of these every frame it is held. The panel says so
        // once, the way it does for the outline and the icons (L15 without the flood).
        bool lChanged = false;

        // A selected child of a selected parent rides along — moved once, through its parent (§HR).
        TDynArray<EntityID> lTopmost;
        EntityHierarchy::TopmostOf(InWorld, InEntities, lTopmost);

        for (const EntityID lId : lTopmost)
        {
            Entity lEntity{ lId, &InWorld };

            // Every entity has one (I17), so a miss means the handle went stale between the measure
            // and this call — skip it rather than emplacing a transform nobody asked for.
            if (lEntity.TryGet<TransformComponent>() == nullptr) { continue; }

            // The delta is world-space, so it is applied to the WORLD pose and the verb stores the
            // local that lands there. For a root the two are the same three fields.
            TransformComponent lWorldXf = EntityHierarchy::WorldTransform(lEntity);

            // The POSITION goes through the matrix rather than being offset by hand, which is what
            // makes a rotate or a scale orbit the shared pivot instead of spinning each entity where
            // it stands. For one entity the pivot IS its origin, so this reduces to no movement.
            //
            // INDIVIDUAL ORIGINS IS EXACTLY THE ABSENCE OF THIS STEP: an entity that is its own
            // pivot cannot be moved by turning about itself, so the delta's rotation and scale still
            // land below while the position is left alone. Nothing else differs between the modes.
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

        // Not CanEdit: focusing is not an edit. The rule is about WHICH CAMERA owns the view — a
        // Play world is framed by its CameraComponent, so moving the editor camera would be silent.
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

        // The anchor keeps an entity with nothing to draw framable — same question the viewport's
        // icon answers, and asked of the same helper so the two cannot disagree about where it is.
        // A fixed world size is right here: this runs before the camera has moved, so there is no
        // meaningful pixel scale to convert from yet.
        Bounds2D lBounds;
        if (!EntityQuery::TryGetBounds(*lWorld, InContext.Selection.Ids(), lBounds, 25.f))
        {
            OPAAX_LOG(LogEntityOps, Warn, "Focus Selected — nothing selected has a position");
            return;
        }

        InContext.Camera.FocusOn(lBounds, InContext.Viewport.GetSizePx());
    }
}
