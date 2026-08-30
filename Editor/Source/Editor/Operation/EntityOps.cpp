#include "Editor/Operation/EntityOps.h"

#include "Editor/Camera/EditorCamera.h"
#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Operation/MapOperations.h"
#include "Editor/PIE/PlayInEditor.h"   // IsEdit — the focus rule is about which camera owns the view

#include <cmath>                 // atan2 — the delta's turn, read off its own basis
#include <glm/geometric.hpp>     // length — and its stretch
#include <glm/mat2x2.hpp>        // the delta's LINEAR part, conjugated into an entity's own frame
#include <glm/matrix.hpp>        // transpose — a rotation's inverse

#include "Application/Services/ILogger.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/Maths.h"    // RadiansToDegrees — the transform authors degrees
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
    /**
     * A world-space linear delta expressed in the frame of an entity turned by InDegrees:
     * `Rot(-θ)·InLinear·Rot(θ)`, using transpose for the inverse since a rotation is orthonormal.
     *
     * The unrotated case returns InLinear untouched — not merely as an optimisation, but so the
     * overwhelmingly common path is bit-identical to what shipped before this existed.
     */
    glm::mat2 ToEntityFrame(const glm::mat2& InLinear, const float InDegrees)
    {
        if (InDegrees == 0.f) { return InLinear; }

        const float lRad = Maths::DegreesToRadians(InDegrees);
        const float lCos = std::cos(lRad);
        const float lSin = std::sin(lRad);

        const glm::mat2 lRotation{ Vector2F{ lCos, lSin }, Vector2F{ -lSin, lCos } };

        return glm::transpose(lRotation) * InLinear * lRotation;
    }

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

    void EntityOps::TransformSelected(EditorContext& InContext, const Matrix44F& InDelta,
                                      const ETransformOrigin InOrigin)
    {
        if (!InContext.Selection.HasSelection()) { return; }

        if (!MapOps::CanEdit(InContext, "Transform Entity")) { return; }

        World* const lWorld = InContext.Selection.GetWorld();
        if (lWorld == nullptr) { return; }

        // The delta's LINEAR part carries the rotation and the scale; the translation is handled by
        // running each position through the whole matrix below.
        const glm::mat2 lLinear{ Vector2F{ InDelta[0][0], InDelta[0][1] },
                                 Vector2F{ InDelta[1][0], InDelta[1][1] } };

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
            const Vector4F lMoved = InOrigin == ETransformOrigin::Individual
                                        ? Vector4F(lTransform->Position.x, lTransform->Position.y, 0.f, 1.f)
                                        : InDelta * Vector4F(lTransform->Position.x, lTransform->Position.y, 0.f, 1.f);

            // ROTATION AND SCALE ARE READ IN THE ENTITY'S OWN FRAME, and for scale that is the whole
            // difference between right and wrong. A Local scale of an entity turned by R arrives here
            // as `R·S·R⁻¹` — reading world-axis lengths off that mixes the axes and reports a
            // rotation nobody asked for (45° and 2x reads as ~18° and 1.58x). Conjugating back by R
            // recovers S exactly. Translation and rotation deltas are unaffected: the first has an
            // identity linear part, and 2D rotations commute.
            const glm::mat2 lLocal = ToEntityFrame(lLinear, lTransform->Rotation);

            lTransform->Position = { lMoved.x, lMoved.y };
            lTransform->Rotation += Maths::RadiansToDegrees(std::atan2(lLocal[0][1], lLocal[0][0]));
            lTransform->Scale    *= Vector2F{ glm::length(lLocal[0]), glm::length(lLocal[1]) };

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
