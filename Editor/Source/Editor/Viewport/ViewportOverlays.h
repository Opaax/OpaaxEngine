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
// ViewportOverlays — the authoring furniture drawn OVER a world: the selection outline and the
//   icon for an entity that renders nothing. Queued into DebugDraw tagged with the world they
//   annotate, so a pass of another world never sees them (P8, the DebugDraw source rule).
//
//   Shared by the level viewport and the prefab panel for the gestures' reason: one look, one
//   hit-test size, two surfaces. Each caller keeps its own one-shot log on the returned count —
//   "queued for zero" and "queued for one" must stay distinguishable per surface ([[L15]]).
// =============================================================================
namespace Opaax::Editor::ViewportOverlays
{
    /**
     * The icon's half-size in WORLD units for a view at this size — ONE value, used by the draw
     * and by the hit test, which is what makes what-you-see-what-you-click true by construction
     * (**SEL4**). Sized in screen pixels, so it holds its apparent size at any zoom.
     */
    float AnchorHalfExtent(const CameraView& InView, const Vector2F& InViewportPx);

    /** An outline around every entity of InIds that has bounds. @return How many were drawn. */
    Uint64 EnqueueSelectionOutline(DebugDraw& InDraw, World& InWorld, const TDynArray<EntityID>& InIds,
                                   float InAnchorHalfExtent);

    /**
     * A small box at every entity of InWorld that draws NOTHING, so it stays visible and clickable
     * — Unreal's billboard, Godot's origin grab-area. The caller decides whether the world is one
     * that should be decorated at all. @return How many were drawn.
     */
    Uint64 EnqueueEntityIcons(DebugDraw& InDraw, World& InWorld, float InAnchorHalfExtent);
}
