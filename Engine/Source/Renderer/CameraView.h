#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // CameraView — where a world is viewed from, in world units. Set on the World
    //   (World::SetCameraView); RendererManager turns it into matrices for the target.
    //   OrthoSize is the vertical half-extent; width follows the target's aspect, so resizing
    //   scales the view instead of showing more world. Default: centred, 600 units tall.
    // =============================================================================
    struct CameraView
    {
        Vector2F Position  = { 0.f, 0.f };
        float    OrthoSize = 300.f;
    };

    // =============================================================================
    // Defined in the .cpp (keeps glm headers out of World.h) and exported (used by the editor).
    // =============================================================================

    /**
     * The view matrix: a translation by -Position.
     */
    OPAAX_API Matrix44F MakeView(const CameraView& InView);

    /**
     * The projection: Y-up ortho, OrthoSize tall, width from the target's aspect.
     * A zero dimension gives identity.
     */
    OPAAX_API Matrix44F MakeProjection(const CameraView& InView, Uint32 InWidth, Uint32 InHeight);

    /**
     * Projection * View for InView on a target of InWidth x InHeight pixels.
     * Y-up, centred on InView.Position. A zero dimension gives identity.
     */
    OPAAX_API Matrix44F MakeViewProjection(const CameraView& InView, Uint32 InWidth, Uint32 InHeight);

    /**
     * Viewport pixels (origin top-left, Y down) -> world units.
     * Returns InView.Position for an empty viewport.
     */
    OPAAX_API Vector2F ScreenToWorld(const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InLocalPx);

    /**
     * World units -> viewport pixels (origin top-left, Y down). Inverse of ScreenToWorld.
     * Returns the viewport centre for an empty viewport.
     */
    OPAAX_API Vector2F WorldToScreen(const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InWorld);

    /**
     * World units per viewport pixel: (2 * OrthoSize) / height. 1 for a zero height.
     */
    OPAAX_API float WorldPerPixel(const CameraView& InView, float InViewportHeightPx);
}
