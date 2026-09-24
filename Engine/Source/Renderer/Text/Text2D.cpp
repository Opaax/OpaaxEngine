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

        /** The space advance used when the face has no space glyph at all, as a fraction of Size. */
        constexpr float FALLBACK_SPACE_RATIO = 0.5f;

        /**
         * The tofu box a missing codepoint draws, as fractions of Size. Roughly a capital's
         * proportions, so a row of them reads as text that could not be shown rather than as debris.
         */
        constexpr float TOFU_WIDTH_RATIO     = 0.55f;
        constexpr float TOFU_HEIGHT_RATIO    = 0.70f;
        constexpr float TOFU_ADVANCE_RATIO   = 0.65f;

        /**
         * EstimateExtent's per-codepoint advance and per-line height, as fractions of Size.
         *
         * Both are ROUNDED UP from Roboto's real numbers (its average advance is nearer 0.55 of the
         * bake height and its line advance 1.17). Over-estimating is the whole contract: an extent
         * used for picking that comes up short makes the tail of a string unclickable.
         */
        constexpr float ESTIMATE_ADVANCE_RATIO = 0.62f;
        constexpr float ESTIMATE_LINE_RATIO    = 1.25f;

        /** The face as the walk reads it — scale and the space advance resolved once per string. */
        struct WalkMetrics
        {
            const FontFaceData&   Face;
            const TextDrawParams& Params;
            float                 Scale;
            float                 SpaceAdvance;
        };

        /** One codepoint's pen movement: the kerning BEFORE it, the advance AFTER it, the glyph. */
        struct PenStep
        {
            float            Kern    = 0.f;
            float            Advance = 0.f;
            const FontGlyph* Glyph   = nullptr;
            bool             bTab    = false;
        };

        /**
         * THE ONE ADVANCE RULE — the scan that finds a line's end and the emit that places it both
         * step through here, which is what keeps a wrapped or aligned line the width it was measured.
         * InOutPrevious is 0 at a line start (nothing kerns against it) and after a tab.
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

        /** Where a line stops, where the next one starts, and how wide the stopped one is. */
        struct LineSpan
        {
            const char* End   = nullptr;   // exclusive
            const char* Next  = nullptr;   // the next line's first byte
            float       Width = 0.f;
            bool        bLast = false;     // the string ended here
        };

        /**
         * Find the end of the line starting at InStart: '\n', the terminator, or — wrapping — the
         * last space before the box's edge (consumed), else the glyph that would cross it.
         */
        LineSpan ScanLine(const WalkMetrics& InM, const char* InStart)
        {
            const bool lWrap = InM.Params.bWrap && InM.Params.BoxWidth > 0.f;

            const char* lCursor   = InStart;
            Uint32      lPrevious = 0u;
            float       lWidth    = 0.f;

            const char* lBreakEnd   = nullptr;   // the line's end if it breaks at the last space seen
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
                    // A blank crossing the edge IS the break — nothing of it is drawn.
                    if (lCrosses) { return { lBefore, lCursor, lWidth, false }; }

                    lBreakEnd   = lBefore;
                    lBreakNext  = lCursor;
                    lBreakWidth = lWidth;
                }
                else if (lCrosses)
                {
                    // Back to the last blank; a word wider than the box breaks before this glyph. A
                    // first glyph wider than the box is placed anyway (lWidth > 0 above).
                    if (lBreakEnd != nullptr) { return { lBreakEnd, lBreakNext, lBreakWidth, false }; }
                    return { lBefore, lBefore, lWidth, false };
                }

                lWidth = lAfter;
            }
        }

        /** Place one line's glyphs, pen starting at InPenX on InBaselineY. */
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
                    // Tofu. Sits ON the baseline and is sized from Size rather than from the face,
                    // because the face is precisely what does not know this character.
                    TextQuad lQuad;
                    lQuad.Size   = { InM.Params.Size * TOFU_WIDTH_RATIO, InM.Params.Size * TOFU_HEIGHT_RATIO };
                    lQuad.Centre = { InPenX + lQuad.Size.x * 0.5f, InBaselineY + lQuad.Size.y * 0.5f };
                    lQuad.bTofu  = true;

                    InSink(lQuad);
                }
                else if (lStep.Glyph->QuadSize.x > 0.f && lStep.Glyph->QuadSize.y > 0.f)
                {
                    // A blank glyph (space) emits nothing but still advances.
                    const FontGlyph& lGlyph = *lStep.Glyph;

                    TextQuad lQuad;
                    lQuad.Size   = { lGlyph.QuadSize.x * InM.Scale, lGlyph.QuadSize.y * InM.Scale };

                    // QuadOffset is stb's, measured from the pen with Y going DOWN. This world's Y
                    // goes up, so the vertical term subtracts and the horizontal one adds.
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
         * THE ONE WALK, for every entry point.
         *
         * Measure, DrawString and the editor's preview differ by exactly one thing — what they do
         * with each placed glyph — and writing the layout more than once is how they start
         * disagreeing. Same argument Renderer2D::SubmitQuad makes for its two draw calls.
         *
         * Per line: SCAN for its end and width, then EMIT it — alignment needs the width before
         * the first glyph lands, and wrapping needs the break before the glyph that crosses.
         *
         * @param InSink Empty to measure only.
         * @return { widest line, total line-box height }.
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

            // The pen sits on the BASELINE, which is one ascent below the caller's top-left. Y is up
            // here, so "below" subtracts — the one sign the whole layout turns on.
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
        // NO ATLAS means the face is still uploading: lay the line out, draw none of it, and the
        // next frame draws it in the right place. The tofu boxes still go through, because a face
        // with no glyphs has nothing to wait for.
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
