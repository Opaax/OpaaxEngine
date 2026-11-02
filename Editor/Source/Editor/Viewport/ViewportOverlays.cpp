#include "Editor/Viewport/ViewportOverlays.h"

#include "Core/Maths/Maths.h"

#include "Core/Maths/Bounds2D.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraConfinerStage.h"
#include "Renderer/Camera/CameraFollowStage.h"
#include "Renderer/Camera/CameraGroupStage.h"
#include "Renderer/Camera/CameraZoneComponent.h"
#include "Renderer/Camera/CameraZoneStage.h"
#include "Renderer/CameraView.h"
#include "Renderer/Components/Light2DComponent.h"
#include "Renderer/DebugDraw.h"
#include "Renderer/Lighting/Light2DGizmo.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"   // the light's world pose
#include "World/Entity/EntityMeta.h"    // the all-entities view, for icons
#include "World/Entity/EntityQuery.h"   // entity bounds for outline and icon
#include "World/World.h"

namespace Opaax::Editor::ViewportOverlays
{
    namespace
    {
        // Selection outline color and padding (world units), so the border sits just outside the quad.
        constexpr Vector4F k_OutlineColor     = { 1.f, 0.6f, 0.1f, 1.f };
        constexpr Vector2F k_OutlinePadding   = { 6.f, 6.f };
        constexpr float    k_OutlineThickness = 3.f;

        // Icon for an entity that draws nothing. Half-size in screen pixels.
        constexpr float    k_IconHalfPx    = 9.f;
        constexpr Vector4F k_IconColor     = { 0.55f, 0.75f, 1.f, 1.f };
        constexpr float    k_IconThickness = 2.f;

        // A light's reach, in screen pixels: line thickness, and a global light's arrow length.
        constexpr float k_LightThicknessPx = 1.5f;
        constexpr float k_LightArrowPx     = 60.f;

        // A rigged camera's areas: line thickness in screen pixels, and a colour each.
        constexpr float    k_RigThicknessPx = 1.5f;
        constexpr Vector4F k_DeadZoneColor  = { 1.f, 0.9f, 0.2f, 0.9f };
        constexpr Vector4F k_BoundsColor    = { 1.f, 0.35f, 0.35f, 0.9f };
        constexpr Vector4F k_GroupColor     = { 0.35f, 1.f, 0.5f, 0.9f };
        constexpr Vector4F k_ZoneColor      = { 0.45f, 0.7f, 1.f, 0.9f };

        TransformComponent PoseOf(const Entity InEntity, const TOptional<float> InAlpha)
        {
            return InAlpha.has_value() ? EntityHierarchy::DisplayTransform(InEntity, *InAlpha)
                                       : EntityHierarchy::WorldTransform(InEntity);
        }

        /** The first entity of InWorld named InName, or an invalid one. */
        Entity FindNamed(World& InWorld, const OpaaxString& InName)
        {
            EntityID lFound = ENTITY_NONE;
            InWorld.Each<EntityMeta>([&](const EntityID InId, const EntityMeta& InMeta)
            {
                if (lFound == ENTITY_NONE && InMeta.Name == InName)
                {
                    lFound = InId;
                }
            });
            return Entity(lFound, &InWorld);
        }

        /**
         * InBounds outlined; an axis of no extent is free, so only its two walls are drawn, as tall as
         * InReach each side of InAround.
         */
        void DrawArea(DebugDraw& InDraw, World& InWorld, const Bounds2D& InBounds, const Vector2F InAround,
                      const Vector2F InReach, const Vector4F& InColor, const float InThickness)
        {
            const Vector2F lMin = InBounds.Min();
            const Vector2F lMax = InBounds.Max();
            if (InBounds.HalfExtent.x > 0.f && InBounds.HalfExtent.y > 0.f)
            {
                InDraw.DrawBounds(InBounds, InColor, InThickness, ERenderLayer::Debug, DebugChannels::Default, &InWorld);
            }
            else if (InBounds.HalfExtent.x > 0.f)
            {
                for (const float lX : { lMin.x, lMax.x })
                {
                    InDraw.DrawLine({ lX, InAround.y - InReach.y }, { lX, InAround.y + InReach.y }, InColor, InThickness,
                                    ERenderLayer::Debug, DebugChannels::Default, &InWorld);
                }
            }
            else if (InBounds.HalfExtent.y > 0.f)
            {
                for (const float lY : { lMin.y, lMax.y })
                {
                    InDraw.DrawLine({ InAround.x - InReach.x, lY }, { InAround.x + InReach.x, lY }, InColor, InThickness,
                                    ERenderLayer::Debug, DebugChannels::Default, &InWorld);
                }
            }
        }

        /** The light's colour at full brightness (black, which lights nothing, shows white). */
        Vector4F GizmoColor(const Light2DComponent& InLight)
        {
            const LinearColor& lColor = InLight.Color;
            const float        lPeak  = Maths::Max(lColor.r, Maths::Max(lColor.g, lColor.b));
            const float        lAlpha = InLight.bEnabled ? 0.9f : 0.35f;

            if (lPeak < 0.05f)
            {
                return Vector4F{ 1.f, 1.f, 1.f, lAlpha };
            }
            return Vector4F{ lColor.r / lPeak, lColor.g / lPeak, lColor.b / lPeak, lAlpha };
        }
    }

    float AnchorHalfExtent(const CameraView& InView, const Vector2F& InViewportPx)
    {
        return k_IconHalfPx * WorldPerPixel(InView, InViewportPx.y);
    }

    Uint64 EnqueueSelectionOutline(DebugDraw& InDraw, World& InWorld, const TDynArray<EntityID>& InIds,
                                   const float InAnchorHalfExtent, const TOptional<float> InAlpha)
    {
        Uint64 lDrawn = 0;

        for (const EntityID lId : InIds)
        {
            Bounds2D lBounds;
            if (!EntityQuery::TryGetBounds(Entity{ lId, &InWorld }, lBounds, InAnchorHalfExtent, InAlpha))
            {
                continue;
            }

            InDraw.DrawBounds(Bounds2D::FromCenterSize(lBounds.Center, lBounds.Size() + k_OutlinePadding),
                              k_OutlineColor, k_OutlineThickness, ERenderLayer::Debug,
                              DebugChannels::Default, &InWorld);
            ++lDrawn;
        }

        return lDrawn;
    }

    Uint64 EnqueueEntityIcons(DebugDraw& InDraw, World& InWorld, const float InAnchorHalfExtent,
                              const TOptional<float> InAlpha)
    {
        Uint64 lDrawn = 0;

        InWorld.Each<EntityMeta>([&](EntityID InId, const EntityMeta&)
        {
            // An entity with an extent is already visible.
            Bounds2D lUnused;
            if (EntityQuery::TryGetBounds(Entity{ InId, &InWorld }, lUnused))
            {
                return;
            }

            Bounds2D lIcon;
            if (!EntityQuery::TryGetBounds(Entity{ InId, &InWorld }, lIcon, InAnchorHalfExtent, InAlpha))
            {
                return;   // no transform at all
            }

            InDraw.DrawBounds(lIcon, k_IconColor, k_IconThickness, ERenderLayer::Debug,
                              DebugChannels::Default, &InWorld);
            ++lDrawn;
        });

        return lDrawn;
    }

    Uint64 EnqueueLightGizmos(DebugDraw& InDraw, World& InWorld, const TDynArray<EntityID>& InIds,
                              const float InAnchorHalfExtent, const TOptional<float> InAlpha)
    {
        // The anchor is k_IconHalfPx pixels: the gizmo keeps its on-screen size at any zoom.
        const float lPixel = InAnchorHalfExtent / k_IconHalfPx;

        TDynArray<GizmoLine2D> lLines;
        Uint64                 lDrawn = 0;

        for (const EntityID lId : InIds)
        {
            Entity                        lEntity{ lId, &InWorld };
            const Light2DComponent* const lLight = lEntity.TryGet<Light2DComponent>();
            if (lLight == nullptr)
            {
                continue;
            }

            const TransformComponent lPose = InAlpha.has_value() ? EntityHierarchy::DisplayTransform(lEntity, *InAlpha)
                                                                 : EntityHierarchy::WorldTransform(lEntity);
            BuildLight2DGizmo(*lLight, lPose.Position, lPose.Rotation, k_LightArrowPx * lPixel, lLines);

            const Vector4F lColor = GizmoColor(*lLight);
            for (const GizmoLine2D& lLine : lLines)
            {
                InDraw.DrawLine(lLine.Start, lLine.End, lColor, k_LightThicknessPx * lPixel, ERenderLayer::Debug,
                                DebugChannels::Default, &InWorld);
            }
            ++lDrawn;
        }

        return lDrawn;
    }

    Uint64 EnqueueCameraRigGizmos(DebugDraw& InDraw, World& InWorld, const TDynArray<EntityID>& InIds,
                                  const float InAnchorHalfExtent, const TOptional<float> InAlpha)
    {
        // The anchor is k_IconHalfPx pixels: the lines keep their on-screen width at any zoom.
        const float lThickness = k_RigThicknessPx * InAnchorHalfExtent / k_IconHalfPx;
        const float lAspect    = InWorld.GetViewAspect();

        Uint64 lDrawn = 0;
        bool   bZones = false;

        for (const EntityID lId : InIds)
        {
            Entity lEntity{ lId, &InWorld };

            // A zone selected: its own rectangle.
            if (const CameraZoneComponent* lZone = lEntity.TryGet<CameraZoneComponent>())
            {
                InDraw.DrawBounds(CameraZoneBounds(*lZone, PoseOf(lEntity, InAlpha)), k_ZoneColor, lThickness,
                                  ERenderLayer::Debug, DebugChannels::Default, &InWorld);
                ++lDrawn;
            }

            const CameraComponent* lCamera = lEntity.TryGet<CameraComponent>();
            if (lCamera == nullptr)
            {
                continue;
            }

            const TransformComponent lPose = PoseOf(lEntity, InAlpha);
            const Vector2F           lHalfView{ lCamera->OrthoSize * lAspect, lCamera->OrthoSize };

            if (const CameraFollowStage* lFollow = lEntity.TryGet<CameraFollowStage>())
            {
                InDraw.DrawBounds(CameraDeadZoneBounds(lPose.Position, lFollow->DeadZone, lHalfView), k_DeadZoneColor,
                                  lThickness, ERenderLayer::Debug, DebugChannels::Default, &InWorld);
                ++lDrawn;
            }

            if (const CameraConfinerStage* lConfiner = lEntity.TryGet<CameraConfinerStage>())
            {
                const Entity lOn = lConfiner->BoundsEntity.IsEmpty() ? Entity() : FindNamed(InWorld, lConfiner->BoundsEntity);
                if (lConfiner->BoundsEntity.IsEmpty() || lOn.IsValid())
                {
                    const TransformComponent lPlace = lOn.IsValid() ? PoseOf(lOn, InAlpha) : TransformComponent();
                    DrawArea(InDraw, InWorld,
                             CameraConfinerBounds(lConfiner->Center, lConfiner->Size, lOn.IsValid() ? &lPlace : nullptr),
                             lPose.Position, lHalfView * 2.f, k_BoundsColor, lThickness);
                    ++lDrawn;
                }
            }

            if (const CameraGroupStage* lGroup = lEntity.TryGet<CameraGroupStage>())
            {
                TOptional<Bounds2D> lTargets;
                for (const OpaaxString& lName : lGroup->Targets)
                {
                    const Entity lTarget = FindNamed(InWorld, lName);
                    if (!lTarget.IsValid())
                    {
                        continue;
                    }
                    const Bounds2D lPoint{ PoseOf(lTarget, InAlpha).Position, { 0.f, 0.f } };
                    if (lTargets.has_value())
                    {
                        lTargets->Encapsulate(lPoint);
                    }
                    else
                    {
                        lTargets = lPoint;
                    }
                }
                if (lTargets.has_value())
                {
                    lTargets->HalfExtent += Vector2F{ lGroup->Padding, lGroup->Padding };
                    InDraw.DrawBounds(*lTargets, k_GroupColor, lThickness, ERenderLayer::Debug, DebugChannels::Default,
                                      &InWorld);
                    ++lDrawn;
                }
            }

            bZones = bZones || lEntity.Has<CameraZoneStage>();
        }

        // A camera with a zone stage: every zone it may enter.
        if (bZones)
        {
            InWorld.Each<TransformComponent, CameraZoneComponent>(
                [&](const EntityID InId, TransformComponent&, CameraZoneComponent& InZone)
                {
                    InDraw.DrawBounds(CameraZoneBounds(InZone, PoseOf(Entity(InId, &InWorld), InAlpha)), k_ZoneColor,
                                      lThickness, ERenderLayer::Debug, DebugChannels::Default, &InWorld);
                    ++lDrawn;
                });
        }

        return lDrawn;
    }
}
