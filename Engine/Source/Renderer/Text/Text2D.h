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
     * A face for layout: its metrics and its atlas texture. Borrowed for the draw.
     */
    struct FontFaceView
    {
        const FontFaceData* Data  = nullptr;
        ITexture2D*         Atlas = nullptr;

        bool IsValid() const noexcept { return Data != nullptr; }
    };

    // =============================================================================
    // Text2D — UTF-8 text as quads, in the normal sprite batch.
    //   World space, top-left anchored: the position is the top-left of the first line.
    //   One sprite per visible glyph (one atlas, one texture slot). A missing glyph draws a
    //   hollow box.
    // =============================================================================
    /**
     * One placed glyph (world units, Y-up, relative to the start position). Lets the same layout
     * feed another renderer (the editor's ImGui preview).
     */
    struct TextQuad
    {
        Vector2F Centre = { 0.f, 0.f };
        Vector2F Size   = { 0.f, 0.f };

        /**
         * Atlas UVs in world convention (UVMin is the bottom edge). Unused when bTofu.
         */
        Vector2F UVMin = { 0.f, 0.f };
        Vector2F UVMax = { 0.f, 0.f };

        /** The face has no glyph for this codepoint: draw a hollow box. */
        bool bTofu = false;
    };

    /** Called once per visible glyph, in reading order. */
    using FTextQuadSink = TFunction<void(const TextQuad&)>;

    namespace Text2D
    {
        /** Missing-glyph box border, as a fraction of Size. */
        constexpr float TOFU_THICKNESS_RATIO = 0.06f;

        /**
         * Lays InUtf8 out and passes every visible glyph to InSink (used by the editor preview).
         * @param InSink Called per glyph. Empty to only measure.
         * @return The extent (same as Measure)
         */
        OPAAX_API Vector2F Layout(const char* InUtf8, const Vector2F& InOrigin, const FontFaceView& InFace,
                                  const TextDrawParams& InParams, const FTextQuadSink& InSink);

        /**
         * Draws InUtf8 at InWorldPos. Call between BeginPass and EndPass.
         * '\n' breaks the line; '\t' advances four spaces. Invalid UTF-8 draws as boxes.
         * @param InRenderer The open batch
         * @param InUtf8 Null-terminated UTF-8. Null or empty draws nothing.
         * @param InWorldPos Top-left of the first line, world units
         * @param InFace Face and atlas. An invalid view draws nothing.
         * @param InParams Colour, size, line height, kerning, layer
         * @return The extent (same as Measure)
         */
        OPAAX_API Vector2F DrawString(Renderer2D& InRenderer, const char* InUtf8, const Vector2F& InWorldPos,
                                      const FontFaceView& InFace, const TextDrawParams& InParams = {});

        /**
         * The size DrawString would use, without drawing.
         * @return { widest line, line count * line advance }
         */
        OPAAX_API Vector2F Measure(const char* InUtf8, const FontFaceView& InFace,
                                   const TextDrawParams& InParams = {});

        /**
         * A rough size without a font (for picking and selection outlines). Errs on the large side.
         * @return { codepoints * average advance, lines * line height }
         */
        OPAAX_API Vector2F EstimateExtent(const char* InUtf8, const TextDrawParams& InParams = {});
    }
}
