#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // Viewport — pixel rect the scene renders into (x, y from bottom-left).
    // =============================================================================
    struct Viewport
    {
        Uint32 X      = 0;
        Uint32 Y      = 0;
        Uint32 Width  = 0;
        Uint32 Height = 0;
    };

    // =============================================================================
    // RenderView — the per-frame camera contract (POD). The renderer has NO camera
    //   class; the host composes the matrices and hands over a snapshot. One BeginPass
    //   per view, so split-screen / editor viewport / minimap are just more views.
    //
    //   What the host composes it FROM is CameraView (Renderer/CameraView.h): a position
    //   and an OrthoSize in world units, which RendererManager turns into matrices against
    //   the target's pixels. ViewProjection is the combined product because the batcher
    //   only needs that; a View/Proj split lands when a consumer needs the two apart.
    // =============================================================================
    struct RenderView
    {
        Matrix44F ViewProjection = Matrix44F(1.f);
        Viewport  Viewport;
    };
}
