#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    class ITexture2D;

    // =============================================================================
    // UIQuad — one rectangle a widget wants drawn, canvas units. The RETAINED half of the UI:
    //   a widget rebuilds its quads only when its content or rect changed, and the canvas submits
    //   them every frame. A null texture is a solid colour.
    // =============================================================================
    struct UIQuad
    {
        Bounds2D    Bounds;
        Vector4F    Color   = { 1.f, 1.f, 1.f, 1.f };
        ITexture2D* Texture = nullptr;   // borrowed; the owning resource outlives the frame it draws in
        Vector2F    UVMin   = { 0.f, 0.f };
        Vector2F    UVMax   = { 1.f, 1.f };
    };
}
