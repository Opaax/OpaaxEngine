#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/RenderLayer.h"

namespace Opaax
{
    // =============================================================================
    // TextDrawParams — everything about a string that is not the string, the font or where it goes.
    //
    //   Deliberately small. Anchor, word-wrap, rotation, outline and letter-spacing are absent
    //   because nothing asks for them: alignment belongs to a UI pass that does not exist yet, and
    //   a rotated label wants the transform that already rotates every other component.
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

        /** The band the glyphs draw in, and the tie-break inside it. Every glyph shares both. */
        ERenderLayer Layer        = ERenderLayer::Default;
        Int16        OrderInLayer = 0;
    };
}
