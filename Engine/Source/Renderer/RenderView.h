#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"

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
    //   class; the host composes the matrices and hands over a snapshot. One BeginScene
    //   per view, so split-screen / editor viewport / minimap are just more views.
    //
    //   ViewProjection is the combined matrix (Proj * View) — ICamera already exposes it,
    //   and the batcher only needs the product. A separate View/Proj split lands when a
    //   consumer actually needs the two apart.
    // =============================================================================
    struct RenderView
    {
        Matrix44F ViewProjection = Matrix44F(1.f);
        Viewport  Viewport;
    };
}
