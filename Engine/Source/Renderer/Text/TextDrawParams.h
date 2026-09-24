#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Renderer/RenderLayer.h"

namespace Opaax
{
    /** Where a line sits inside its box. */
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
    // TextDrawParams — everything about a string that is not the string, the font or where it goes.
    //
    //   Deliberately small. Rotation, outline and letter-spacing are absent because nothing asks
    //   for them; a rotated label wants the transform that already rotates every other component.
    //   The BOX arrived with the UI (U2): a rect to wrap in and align against.
    // =============================================================================
    struct TextDrawParams
    {
        /** Multiplied into the glyph's coverage. White draws the face's own anti-aliasing unchanged. */
        Vector4F Color = { 1.f, 1.f, 1.f, 1.f };

        /**
         * Cap-to-cap height in WORLD UNITS — an absolute size, not a multiplier on the bake.
         *
         * Absolute because that is what an author means ("18 tall"), and because the bake height is
         * an implementation detail they should never have to divide by. Scaling far above the bake
         * softens the edges; that is the cost of a bitmap atlas and the reason SDF exists.
         */
        float Size = 32.f;

        /** Multiplies the face's natural line advance on '\n'. */
        float LineHeightScale = 1.f;

        /** Off skips the pair lookup per glyph — visible on 'AV', 'To', 'Wa'. */
        bool bKerning = true;

        /**
         * The box lines align in, from the origin rightward. 0 is a POINT: Center straddles the
         * origin, Right ends on it — what a label pinned to a point wants.
         */
        float      BoxWidth = 0.f;
        ETextAlign HAlign   = ETextAlign::Left;

        /** Break lines at BoxWidth — after the last space, or inside a word wider than the box. */
        bool bWrap = false;

        /** The band the glyphs draw in, and the tie-break inside it. Every glyph shares both. */
        ERenderLayer Layer        = ERenderLayer::Default;
        Int16        OrderInLayer = 0;
    };
}

OPAAX_ENUM_VALUES(Opaax::ETextAlign, Left, Center, Right);
