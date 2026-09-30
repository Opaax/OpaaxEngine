#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Renderer/RenderLayer.h"

namespace Opaax
{
    /** Horizontal alignment in the box. */
    enum class ETextAlign : Uint8
    {
        Left,
        Center,
        Right
    };

    inline const char* ToString(const ETextAlign InAlign) noexcept
    {
        switch (InAlign)
        {
            case ETextAlign::Left:   return "Left";
            case ETextAlign::Center: return "Center";
            case ETextAlign::Right:  return "Right";
        }
        return "Left";
    }

    // =============================================================================
    // TextDrawParams — how to draw a string: colour, size, box, alignment, layer.
    // =============================================================================
    struct TextDrawParams
    {
        /** Multiplied with the glyphs. */
        Vector4F Color = { 1.f, 1.f, 1.f, 1.f };

        /**
         * Cap height in world units (absolute). Far above the bake height, edges get soft.
         */
        float Size = 32.f;

        /** Multiplies the line advance. */
        float LineHeightScale = 1.f;

        /** Off skips kerning. */
        bool bKerning = true;

        /**
         * Box width for alignment. 0 is a point: Center is centred on it, Right ends on it.
         */
        float      BoxWidth = 0.f;
        ETextAlign HAlign   = ETextAlign::Left;

        /** Wrap lines at BoxWidth (at the last space, or inside a word wider than the box). */
        bool bWrap = false;

        /** Layer and order in layer, for every glyph. */
        ERenderLayer Layer        = ERenderLayer::Default;
        Int16        OrderInLayer = 0;
    };
}

OPAAX_ENUM_VALUES(Opaax::ETextAlign, Left, Center, Right);
