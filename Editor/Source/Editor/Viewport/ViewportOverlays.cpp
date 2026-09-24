#include "Editor/Viewport/ViewportOverlays.h"

#include "Core/Maths/Bounds2D.h"
#include "Renderer/CameraView.h"
#include "Renderer/DebugDraw.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"    // the complete all-entities view, for the icon pass
#include "World/Entity/EntityQuery.h"   // the ONE entity-AABB rule — outline and icon
#include "World/World.h"

namespace Opaax::Editor::ViewportOverlays
{
    namespace
    {
        // Selection outline: orange because no Sandbox quad is; the padding pushes the border off
        // the quad's own edge so it reads as an outline rather than a repaint of its rim. WORLD
        // units, tuned against the Sandbox's 120x120 quads.
        constexpr Vector4F k_OutlineColor     = { 1.f, 0.6f, 0.1f, 1.f };
        constexpr Vector2F k_OutlinePadding   = { 6.f, 6.f };
        constexpr float    k_OutlineThickness = 3.f;

        // The icon for an entity that draws nothing. HALF-size in SCREEN pixels.
        constexpr float    k_IconHalfPx    = 9.f;
        constexpr Vector4F k_IconColor     = { 0.55f, 0.75f, 1.f, 1.f };
        constexpr float    k_IconThickness = 2.f;
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
            // An entity with an EXTENT is already visible — asking without an anchor is what
            // distinguishes the two, and it is one call rather than a list of component checks.
            Bounds2D lUnused;
            if (EntityQuery::TryGetBounds(Entity{ InId, &InWorld }, lUnused))
            {
                return;
            }

            Bounds2D lIcon;
            if (!EntityQuery::TryGetBounds(Entity{ InId, &InWorld }, lIcon, InAnchorHalfExtent))
            {
                return;   // no transform at all — not reachable through CreateEntity
            }

            InDraw.DrawBounds(lIcon, k_IconColor, k_IconThickness, ERenderLayer::Debug,
                              DebugChannels::Default, &InWorld);
            ++lDrawn;
        });

        return lDrawn;
    }
}
