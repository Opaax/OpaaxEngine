#pragma once

#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // LinearColor — RGBA as four linear floats.
    //   A distinct type (not an alias) so the editor shows a colour picker for it.
    //   Derives from Vector4F: .r/.g/.b/.a and .x/.y/.z/.w both work.
    //   Saved as a Vector4F ({x,y,z,w}).
    // =============================================================================
    struct LinearColor : Vector4F
    {
        LinearColor() : Vector4F(1.f, 1.f, 1.f, 1.f) {}

        LinearColor(const Vector4F& InValue) : Vector4F(InValue) {}
        LinearColor(const float InR, const float InG, const float InB, const float InA = 1.f)
            : Vector4F(InR, InG, InB, InA)
        {
        }
    };
}
