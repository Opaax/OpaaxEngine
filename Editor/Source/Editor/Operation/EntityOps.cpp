#include "Editor/Operation/EntityOps.h"

#include "Editor/Camera/EditorCamera.h"
#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Operation/MapOperations.h"
#include "Editor/PIE/PlayInEditor.h"   // IsEdit — the focus rule is about which camera owns the view

#include "Application/Services/ILogger.h"
#include "Core/Maths/Bounds2D.h"
#include "World/Components/TransformComponent.h"   // I17 — the one position a drag writes
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityQuery.h"
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
        InContext.Selection.Clear();

        for (const EntityID lId : lIds)
        {
            lWorld->DestroyEntity(lId);
        }

        lWorld->MarkChanged();

        OPAAX_LOG(LogEntityOps, Info, "Deleted {} entity(ies)", static_cast<Uint64>(lIds.size()));
    }

    void EntityOps::TranslateSelected(EditorContext& InContext, const Vector2F& InWorldDelta)
    {
        if (InWorldDelta.x == 0.f && InWorldDelta.y == 0.f) { return; }

        if (!InContext.Selection.HasSelection()) { return; }

        if (!MapOps::CanEdit(InContext, "Move Entity")) { return; }

        World* const lWorld = InContext.Selection.GetWorld();
        if (lWorld == nullptr) { return; }

        // NOT logged per call: a drag lands one of these every frame it is held. The panel says so
        // once, the way it does for the outline and the icons (L15 without the flood).
        bool lMoved = false;

        for (const EntityID lId : InContext.Selection.Ids())
        {
            Entity lEntity{ lId, lWorld };

            // Every entity has one (I17), so a miss means the handle went stale between the measure
            // and this call — skip it rather than emplacing a transform nobody asked for.
            if (TransformComponent* lTransform = lEntity.TryGet<TransformComponent>())
            {
                lTransform->Position += InWorldDelta;
                lMoved = true;
            }
        }

        if (lMoved) { lWorld->MarkChanged(); }
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
