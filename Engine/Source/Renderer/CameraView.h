#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // CameraView — WHERE a world is being looked at from, in world units. The authored
    //   half of RenderView: a producer writes one onto the World (World::SetCameraView),
    //   and RendererManager turns it into matrices against the render target's pixels.
    //
    //   OrthoSize is the vertical HALF-EXTENT in world units and width follows the target's
    //   aspect, so resizing SCALES the view instead of revealing more world — a 4K player
    //   must not see four times the playfield. The default is the frame the engine drew
    //   before a camera existed: centred, 600 units tall.
    // =============================================================================
    struct CameraView
    {
        Vector2F Position  = { 0.f, 0.f };
        float    OrthoSize = 300.f;
    };

    // =============================================================================
    // The two questions a view answers. Defined OUT OF LINE so <glm/gtc/matrix_transform.hpp>
    //   stays out of every TU that includes World.h, and exported because the EDITOR calls
    //   ScreenToWorld from the exe (I6 — the tell that has produced LNK2019 three times here).
    // =============================================================================

    /**
     * Combined Proj * View for InView filling a target of InWidth x InHeight pixels.
     * Y-up, centred on InView.Position. A zero dimension yields identity — there is
     * nothing to frame.
     */
    OPAAX_API Matrix44F MakeViewProjection(const CameraView& InView, Uint32 InWidth, Uint32 InHeight);

    /**
     * Viewport-local pixels (origin TOP-LEFT, Y growing down) -> world units.
     *
     * The inverse of MakeViewProjection's framing and the ONE rule for it: zoom-at-cursor
     * needs it now, click-select and gizmo placement need the same answer later.
     * Returns InView.Position for a degenerate viewport.
     */
    OPAAX_API Vector2F ScreenToWorld(const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InLocalPx);
}
