#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Renderer/Text/TextDrawParams.h"

namespace Opaax
{
    class ITexture2D;
    class Renderer2D;
    struct FontFaceData;

    /**
     * A face as the layout walker needs it: the metrics, and the atlas they index into.
     *
     * The RenderView idiom — a per-draw snapshot of two things that are owned elsewhere. It exists
     * because the two halves live in different layers ON PURPOSE: FontFaceData is portable data
     * under Renderer/, the GPU atlas belongs to a resource, and pairing them at the call site is
     * what keeps Text2D from ever naming FontFaceResource.
     *
     * BORROWED for the draw. Both must outlive the batch flush, which the caller's ResourceRef
     * guarantees for the frame it draws in.
     */
    struct FontFaceView
    {
        const FontFaceData* Data  = nullptr;
        ITexture2D*         Atlas = nullptr;

        bool IsValid() const noexcept { return Data != nullptr; }
    };

    // =============================================================================
    // Text2D — a UTF-8 string as quads, through the batch every sprite already uses.
    //
    //   WORLD SPACE, TOP-LEFT ANCHORED: the position is the top-left of the first line's box, and
    //   the walker advances right and down (down = smaller Y, this world being Y-up). Screen-space
    //   text — a stats overlay pinned to a corner — is a second ortho pass, i.e. multi-view, and is
    //   deliberately not faked here.
    //
    //   ONE DrawSprite PER VISIBLE GLYPH, all on one atlas, so a whole string costs one texture slot
    //   and sorts with everything else. Blank glyphs (space) submit nothing.
    //
    //   A CODEPOINT THE FACE LACKS DRAWS A TOFU BOX — a hollow rectangle, the typographic convention
    //   for "this font cannot show that". Loud rather than silent, and free: it is one
    //   DrawQuadOutline, the same call an editor selection uses. There is deliberately no fallback
    //   to a sibling face: choosing the wrong script is an authoring mistake and it should look like
    //   one.
    // =============================================================================
    namespace Text2D
    {
        /**
         * Draw InUtf8 at InWorldPos. Call between InRenderer's BeginPass and EndPass.
         *
         * '\n' breaks the line; '\t' advances four spaces and stops kerning across itself. Malformed
         * UTF-8 degrades to U+FFFD, which no face has, so it draws as tofu like any other miss.
         *
         * @param InRenderer The open batch.
         * @param InUtf8 NUL-terminated UTF-8. Null or empty draws nothing.
         * @param InWorldPos Top-left of the first line's box, world units.
         * @param InFace The face and its atlas. An invalid view draws nothing.
         * @param InParams Colour, size, line height, kerning, band.
         * @return The extent the string occupied — Measure's answer, so a caller that draws and then
         *   needs the size does not walk the string twice.
         */
        OPAAX_API Vector2F DrawString(Renderer2D& InRenderer, const char* InUtf8, const Vector2F& InWorldPos,
                                      const FontFaceView& InFace, const TextDrawParams& InParams = {});

        /**
         * What DrawString would occupy, without drawing.
         *
         * @return { widest line, line count * scaled line advance }. The height is the LINE BOX, not
         *   a tight bounding box — which is what a caller centring or stacking text actually wants.
         */
        OPAAX_API Vector2F Measure(const char* InUtf8, const FontFaceView& InFace,
                                   const TextDrawParams& InParams = {});
    }
}
