#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class DebugDraw;
    class World;
    struct CameraView;
}

// =============================================================================
// ViewportOverlays — editor visuals drawn over a world: the selection outline, the icon for
//   entities that draw nothing and the reach of selected lights. Queued into DebugDraw tagged with
//   their world, so other worlds' passes skip them. Shared by the level viewport and the prefab panel.
// =============================================================================
namespace Opaax::Editor::ViewportOverlays
{
    /**
     * The icon's half-size in world units for a view at this size. Used for both drawing and hit
     * testing, so what you see is what you click. Constant on screen at any zoom.
     */
    float AnchorHalfExtent(const CameraView& InView, const Vector2F& InViewportPx);

    /** An outline around every entity of InIds that has bounds. @return How many were drawn. */
    Uint64 EnqueueSelectionOutline(DebugDraw& InDraw, World& InWorld, const TDynArray<EntityID>& InIds,
                                   float InAnchorHalfExtent);

    /**
     * A small box on every entity of InWorld that draws nothing, so it stays visible and clickable.
     * The caller decides whether the world should be decorated. @return How many were drawn.
     */
    Uint64 EnqueueEntityIcons(DebugDraw& InDraw, World& InWorld, float InAnchorHalfExtent);

    /**
     * Where each light of InIds reaches (Light2DGizmo), in the light's own colour, dimmed when it
     * is off. @return How many were drawn.
     */
    Uint64 EnqueueLightGizmos(DebugDraw& InDraw, World& InWorld, const TDynArray<EntityID>& InIds,
                              float InAnchorHalfExtent);
}
