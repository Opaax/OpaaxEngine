#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    class ITexture2D;

    // =============================================================================
    // UIQuad — one rectangle to draw, in canvas units. Rebuilt only when the widget changes;
    //   submitted every frame. A null texture is a solid colour.
    // =============================================================================
    struct UIQuad
    {
        Bounds2D    Bounds;
        Vector4F    Color   = { 1.f, 1.f, 1.f, 1.f };
        ITexture2D* Texture = nullptr;   // borrowed
        Vector2F    UVMin   = { 0.f, 0.f };
        Vector2F    UVMax   = { 1.f, 1.f };
        /** > 0 draws a hollow box with this border width (missing-glyph box). Untextured. */
        float       Outline = 0.f;
    };
}
