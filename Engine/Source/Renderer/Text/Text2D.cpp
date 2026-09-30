#include "Renderer/Text/Text2D.h"

#include "Core/String/OpaaxUtf8.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Text/FontFaceData.h"

namespace Opaax::Text2D
{
    namespace
    {
        /** How many spaces a '\t' advances. */
        constexpr Uint32 TAB_SPACES = 4u;

        /** Space advance when the face has no space glyph, as a fraction of Size. */
        constexpr float FALLBACK_SPACE_RATIO = 0.5f;

        /**
         * Box size for a missing codepoint, as fractions of Size (about a capital letter).
         */
        constexpr float TOFU_WIDTH_RATIO     = 0.55f;
        constexpr float TOFU_HEIGHT_RATIO    = 0.70f;
        constexpr float TOFU_ADVANCE_RATIO   = 0.65f;

        /**
         * EstimateExtent's advance and line height, as fractions of Size. Rounded up on purpose
         * (for picking, too big is better than too small).
         */
        constexpr float ESTIMATE_ADVANCE_RATIO = 0.62f;
        constexpr float ESTIMATE_LINE_RATIO    = 1.25f;

        /** The face as used by the layout: scale and space advance resolved once per string. */
        struct WalkMetrics
        {
            const FontFaceData&   Face;
            const TextDrawParams& Params;
            float                 Scale;
            float                 SpaceAdvance;
        };

        /** One codepoint's step: kerning before, advance after, the glyph. */
        struct PenStep
        {
            float            Kern    = 0.f;
            float            Advance = 0.f;
            const FontGlyph* Glyph   = nullptr;
            bool             bTab    = false;
        };

        /**
         * The advance rule, shared by line measuring and placing (so they always agree).
         * InOutPrevious is 0 at a line start and after a tab.
         */
        PenStep StepPen(const WalkMetrics& InM, const Uint32 InCodepoint, Uint32& InOutPrevious)
        {
            PenStep lStep;

            if (InCodepoint == '\t')
            {
                lStep.Advance  = InM.SpaceAdvance * TAB_SPACES;
                lStep.bTab     = true;
                InOutPrevious  = 0u;
                return lStep;
            }

            if (InM.Params.bKerning && InOutPrevious != 0u)
            {
                lStep.Kern = InM.Face.GetKerning(InOutPrevious, InCodepoint) * InM.Scale;
            }

            lStep.Glyph   = InM.Face.FindGlyph(InCodepoint);
            lStep.Advance = (lStep.Glyph != nullptr) ? lStep.Glyph->XAdvance * InM.Scale
                                                     : InM.Params.Size * TOFU_ADVANCE_RATIO;
            InOutPrevious = InCodepoint;
            return lStep;
        }

        /** Where a line ends, where the next starts, and the line's width. */
        struct LineSpan
        {
            const char* End   = nullptr;   // exclusive
            const char* Next  = nullptr;   // first byte of the next line
            float       Width = 0.f;
            bool        bLast = false;     // end of the string
        };

        /**
         * Finds the end of the line starting at InStart: '\n', the end of the string, or (when
         * wrapping) the last space before the box edge, else the glyph that would cross it.
         */
        LineSpan ScanLine(const WalkMetrics& InM, const char* InStart)
        {
            const bool lWrap = InM.Params.bWrap && InM.Params.BoxWidth > 0.f;

            const char* lCursor   = InStart;
            Uint32      lPrevious = 0u;
            float       lWidth    = 0.f;

            const char* lBreakEnd   = nullptr;   // line end if it breaks at the last space
            const char* lBreakNext  = nullptr;
            float       lBreakWidth = 0.f;

            while (true)
            {
                const char*  lBefore    = lCursor;
                const Uint32 lCodepoint = Utf8::Decode(lCursor);

                if (lCodepoint == 0u)   { return { lBefore, lBefore, lWidth, true }; }
                if (lCodepoint == '\n') { return { lBefore, lCursor, lWidth, false }; }

                const PenStep lStep    = StepPen(InM, lCodepoint, lPrevious);
                const float   lAfter   = lWidth + lStep.Kern + lStep.Advance;
                const bool    lCrosses = lWrap && lAfter > InM.Params.BoxWidth && lWidth > 0.f;

                if (lCodepoint == ' ' || lCodepoint == '\t')
                {
                    // A space crossing the edge is the break (not drawn).
                    if (lCrosses) { return { lBefore, lCursor, lWidth, false }; }

                    lBreakEnd   = lBefore;
                    lBreakNext  = lCursor;
                    lBreakWidth = lWidth;
                }
                else if (lCrosses)
                {
                    // Back to the last space; a word wider than the box breaks before this glyph.
                    // A first glyph wider than the box is placed anyway.
                    if (lBreakEnd != nullptr) { return { lBreakEnd, lBreakNext, lBreakWidth, false }; }
                    return { lBefore, lBefore, lWidth, false };
                }

                lWidth = lAfter;
            }
        }

        /** Places one line's glyphs, pen at InPenX on InBaselineY. */
        void EmitLine(const WalkMetrics& InM, const FTextQuadSink& InSink, const char* InBegin, const char* InEnd,
                      float InPenX, const float InBaselineY)
        {
            const char* lCursor   = InBegin;
            Uint32      lPrevious = 0u;

            while (lCursor < InEnd)
            {
                const Uint32  lCodepoint = Utf8::Decode(lCursor);
                const PenStep lStep      = StepPen(InM, lCodepoint, lPrevious);

                InPenX += lStep.Kern;

                if (lStep.bTab)
                {
                    InPenX += lStep.Advance;
                    continue;
                }

                if (lStep.Glyph == nullptr)
                {
                    // Box for a missing glyph: on the baseline, sized from Size.
                    TextQuad lQuad;
                    lQuad.Size   = { InM.Params.Size * TOFU_WIDTH_RATIO, InM.Params.Size * TOFU_HEIGHT_RATIO };
                    lQuad.Centre = { InPenX + lQuad.Size.x * 0.5f, InBaselineY + lQuad.Size.y * 0.5f };
                    lQuad.bTofu  = true;

                    InSink(lQuad);
                }
                else if (lStep.Glyph->QuadSize.x > 0.f && lStep.Glyph->QuadSize.y > 0.f)
                {
                    // A blank glyph (space) draws nothing but still advances.
                    const FontGlyph& lGlyph = *lStep.Glyph;

                    TextQuad lQuad;
                    lQuad.Size   = { lGlyph.QuadSize.x * InM.Scale, lGlyph.QuadSize.y * InM.Scale };

                    // QuadOffset has Y down; the world has Y up.
                    lQuad.Centre = { InPenX      + (lGlyph.QuadOffset.x + lGlyph.QuadSize.x * 0.5f) * InM.Scale,
                                     InBaselineY - (lGlyph.QuadOffset.y + lGlyph.QuadSize.y * 0.5f) * InM.Scale };
                    lQuad.UVMin  = lGlyph.UVMin;
                    lQuad.UVMax  = lGlyph.UVMax;

                    InSink(lQuad);
                }

                InPenX += lStep.Advance;
            }
        }

        /**
         * The layout walk shared by Measure, DrawString and the editor preview.
         * Per line: find its end and width, then place it (alignment needs the width first).
         * @param InSink Empty to only measure
         * @return { widest line, total height }
         */
        Vector2F WalkText(const FTextQuadSink& InSink, const char* InUtf8, const Vector2F& InWorldPos,
                          const FontFaceView& InFace, const TextDrawParams& InParams)
        {
            if (InUtf8 == nullptr || *InUtf8 == '\0' || !InFace.IsValid() || InFace.Data->PixelHeight <= 0.f)
            {
                return { 0.f, 0.f };
            }

            const FontFaceData& lFace  = *InFace.Data;
            const float         lScale = InParams.Size / lFace.PixelHeight;

            const FontGlyph* lSpace = lFace.FindGlyph(' ');
            const WalkMetrics lM{ lFace, InParams, lScale,
                                  (lSpace != nullptr) ? lSpace->XAdvance * lScale : InParams.Size * FALLBACK_SPACE_RATIO };

            // The pen starts on the baseline, one ascent below the top (Y up, so subtract).
            const float lLineStep  = lFace.VMetrics.LineAdvance * InParams.LineHeightScale * lScale;
            float       lBaselineY = InWorldPos.y - lFace.VMetrics.Ascent * lScale;

            const float lAlign = InParams.HAlign == ETextAlign::Center ? 0.5f
                               : InParams.HAlign == ETextAlign::Right  ? 1.f : 0.f;

            float  lWidestLine = 0.f;
            Uint32 lLineCount  = 0u;

            const char* lLineStart = InUtf8;

            while (true)
            {
                const LineSpan lSpan = ScanLine(lM, lLineStart);

                lWidestLine = (lSpan.Width > lWidestLine) ? lSpan.Width : lWidestLine;
                ++lLineCount;

                if (InSink)
                {
                    EmitLine(lM, InSink, lLineStart, lSpan.End,
                             InWorldPos.x + (InParams.BoxWidth - lSpan.Width) * lAlign, lBaselineY);
                }

                if (lSpan.bLast) { break; }

                lBaselineY -= lLineStep;
                lLineStart  = lSpan.Next;
            }

            return { lWidestLine, static_cast<float>(lLineCount) * lLineStep };
        }
    }

    Vector2F Layout(const char* InUtf8, const Vector2F& InOrigin, const FontFaceView& InFace,
                    const TextDrawParams& InParams, const FTextQuadSink& InSink)
    {
        return WalkText(InSink, InUtf8, InOrigin, InFace, InParams);
    }

    Vector2F DrawString(Renderer2D& InRenderer, const char* InUtf8, const Vector2F& InWorldPos,
                        const FontFaceView& InFace, const TextDrawParams& InParams)
    {
        // No atlas yet (still uploading): lay out, draw nothing. Missing-glyph boxes still draw.
        ITexture2D* lAtlas = InFace.Atlas;

        return WalkText([&InRenderer, &InParams, lAtlas](const TextQuad& InQuad)
                        {
                            if (InQuad.bTofu)
                            {
                                InRenderer.DrawQuadOutline(InQuad.Centre, InQuad.Size, InParams.Color,
                                                           InParams.Size * TOFU_THICKNESS_RATIO, 0.f,
                                                           InParams.Layer, InParams.OrderInLayer);
                                return;
                            }

                            if (lAtlas != nullptr)
                            {
                                InRenderer.DrawSprite(InQuad.Centre, InQuad.Size, *lAtlas, InParams.Color,
                                                      0.f, InParams.Layer, InParams.OrderInLayer,
                                                      InQuad.UVMin, InQuad.UVMax);
                            }
                        },
                        InUtf8, InWorldPos, InFace, InParams);
    }

    Vector2F Measure(const char* InUtf8, const FontFaceView& InFace, const TextDrawParams& InParams)
    {
        return WalkText({}, InUtf8, { 0.f, 0.f }, InFace, InParams);
    }

    Vector2F EstimateExtent(const char* InUtf8, const TextDrawParams& InParams)
    {
        if (InUtf8 == nullptr || *InUtf8 == '\0')
        {
            return { 0.f, 0.f };
        }

        Uint32 lWidestLine = 0u;
        Uint32 lThisLine   = 0u;
        Uint32 lLineCount  = 1u;

        const char* lCursor = InUtf8;

        while (const Uint32 lCodepoint = Utf8::Decode(lCursor))
        {
            if (lCodepoint == '\n')
            {
                lWidestLine = (lThisLine > lWidestLine) ? lThisLine : lWidestLine;
                lThisLine   = 0u;
                ++lLineCount;
                continue;
            }

            lThisLine += (lCodepoint == '\t') ? TAB_SPACES : 1u;
        }

        lWidestLine = (lThisLine > lWidestLine) ? lThisLine : lWidestLine;

        return { static_cast<float>(lWidestLine) * InParams.Size * ESTIMATE_ADVANCE_RATIO,
                 static_cast<float>(lLineCount)  * InParams.Size * ESTIMATE_LINE_RATIO
                                                 * InParams.LineHeightScale };
    }
}
