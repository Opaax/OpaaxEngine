#pragma once

#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    /** The bounds of a whole texture: a quad drawn with them reads its texture freely. */
    inline constexpr Vector4F WHOLE_TEXTURE_UV{ 0.f, 0.f, 1.f, 1.f };

    /**
     * The part of a texture between two corners, as (min u, min v, max u, max v), whichever way
     * the corners are given (a flipped sprite has its max before its min). A sheet frame's: a
     * blended read of a quad drawn from the frame stays inside it.
     */
    inline Vector4F MakeUVBounds(const Vector2F& InUVMin, const Vector2F& InUVMax) noexcept
    {
        return Vector4F{ InUVMin.x < InUVMax.x ? InUVMin.x : InUVMax.x,
                         InUVMin.y < InUVMax.y ? InUVMin.y : InUVMax.y,
                         InUVMin.x < InUVMax.x ? InUVMax.x : InUVMin.x,
                         InUVMin.y < InUVMax.y ? InUVMax.y : InUVMin.y };
    }
}
