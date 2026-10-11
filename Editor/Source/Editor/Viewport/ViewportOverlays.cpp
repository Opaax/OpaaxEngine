#include "Editor/Viewport/ViewportOverlays.h"

#include <algorithm>

#include "Core/Maths/Bounds2D.h"
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

        /** The light's colour at full brightness (black, which lights nothing, shows white). */
        Vector4F GizmoColor(const Light2DComponent& InLight)
        {
            const LinearColor& lColor = InLight.Color;
            const float        lPeak  = std::max(lColor.r, std::max(lColor.g, lColor.b));
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
                                   const float InAnchorHalfExtent)
    {
        Uint64 lDrawn = 0;

        for (const EntityID lId : InIds)
        {
            Bounds2D lBounds;
            if (!EntityQuery::TryGetBounds(Entity{ lId, &InWorld }, lBounds, InAnchorHalfExtent))
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

    Uint64 EnqueueEntityIcons(DebugDraw& InDraw, World& InWorld, const float InAnchorHalfExtent)
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
            if (!EntityQuery::TryGetBounds(Entity{ InId, &InWorld }, lIcon, InAnchorHalfExtent))
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
                              const float InAnchorHalfExtent)
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

            const TransformComponent lPose = EntityHierarchy::WorldTransform(lEntity);
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
}
