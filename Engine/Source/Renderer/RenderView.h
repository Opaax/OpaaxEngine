#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // Viewport — pixel rect to render into (x, y from bottom-left).
    // =============================================================================
    struct Viewport
    {
        Uint32 X      = 0;
        Uint32 Y      = 0;
        Uint32 Width  = 0;
        Uint32 Height = 0;
    };

    // =============================================================================
    // RenderView — the camera for one pass (plain data). The host builds the matrix from a
    //   CameraView; one BeginPass per view (split-screen, editor viewport, minimap, ...).
    // =============================================================================
    struct RenderView
    {
        Matrix44F ViewProjection = Matrix44F(1.f);
        Viewport  Viewport;
    };
}
