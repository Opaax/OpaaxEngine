#include "Editor/Operation/EntityOps.h"

#include "Editor/Camera/EditorCamera.h"
#include "Editor/EditorContext.h"
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
#include "Application/Services/ILogger.h"
#include "Core/Maths/Bounds2D.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "Core/Maths/Maths.h"    // RadiansToDegrees — the transform authors degrees
#include "World/Components/TransformComponent.h"   // I17 — the one position a drag writes
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityQuery.h"
#include "World/Serialization/MapSerializer.h"   // ⑤ — what a create/delete step carries
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

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
        // the Hierarchy's Remove from Level assert.
        const TDynArray<EntityID> lIds = InContext.Selection.Ids();

        // BEFORE the fact, unlike every other verb here: once these are destroyed nothing else in
        // the editor can say what they were (⑤).
        EntityDelete lStep{ MapSerializer::CaptureEntities(
            *lWorld, InContext.Engine.GetRegistries().Components(), lIds) };

        InContext.Selection.Clear();

        for (const EntityID lId : lIds)
        {
            lWorld->DestroyEntity(lId);
        }

        lWorld->MarkChanged();

        InContext.Undo.Record(Move(lStep));

        OPAAX_LOG(LogEntityOps, Info, "Deleted {} entity(ies)", static_cast<Uint64>(lIds.size()));
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

        for (const EntityID lId : InContext.Selection.Ids())
        {
            Entity lEntity{ lId, lWorld };

            // Every entity has one (I17), so a miss means the handle went stale between the measure
            // and this call — skip it rather than emplacing a transform nobody asked for.
            TransformComponent* lTransform = lEntity.TryGet<TransformComponent>();
            if (lTransform == nullptr) { continue; }

            // The POSITION goes through the matrix rather than being offset by hand, which is what
            // makes a rotate or a scale orbit the shared pivot instead of spinning each entity where
            // it stands. For one entity the pivot IS its origin, so this reduces to no movement.
            //
            // INDIVIDUAL ORIGINS IS EXACTLY THE ABSENCE OF THIS STEP: an entity that is its own
            // pivot cannot be moved by turning about itself, so the delta's rotation and scale still
            // land below while the position is left alone. Nothing else differs between the modes.
            const Vector4F lMoved =
                InDelta.Origin == ETransformOrigin::Individual
                    ? Vector4F(lTransform->Position.x, lTransform->Position.y, 0.f, 1.f)
                    : InDelta.Matrix * Vector4F(lTransform->Position.x, lTransform->Position.y, 0.f, 1.f);

            lTransform->Position = { lMoved.x, lMoved.y };
            lTransform->Rotation += lDeltaDegrees;
            lTransform->Scale    *= lDeltaScale;

            lChanged = true;
        }

        if (lChanged) { lWorld->MarkChanged(); }
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
