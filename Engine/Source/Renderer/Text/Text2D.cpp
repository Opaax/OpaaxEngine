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
        constexpr float TOFU_THICKNESS_RATIO = 0.06f;

        /**
         * EstimateExtent's per-codepoint advance and per-line height, as fractions of Size.
         *
         * Both are ROUNDED UP from Roboto's real numbers (its average advance is nearer 0.55 of the
         * bake height and its line advance 1.17). Over-estimating is the whole contract: an extent
         * used for picking that comes up short makes the tail of a string unclickable.
         */
        constexpr float ESTIMATE_ADVANCE_RATIO = 0.62f;
        constexpr float ESTIMATE_LINE_RATIO    = 1.25f;

        /**
         * THE ONE WALK, for both entry points.
         *
         * Measure and DrawString differ by exactly one thing — whether a renderer is present — and
         * writing the layout twice is how the drawn string and the measured one start disagreeing.
         * Same argument Renderer2D::SubmitQuad makes for its two draw calls.
         *
         * @param InRenderer Null to measure only.
         * @return { widest line, total line-box height }.
         */
        Vector2F WalkText(Renderer2D* InRenderer, const char* InUtf8, const Vector2F& InWorldPos,
                          const FontFaceView& InFace, const TextDrawParams& InParams)
        {
            if (InUtf8 == nullptr || *InUtf8 == '\0' || !InFace.IsValid() || InFace.Data->PixelHeight <= 0.f)
            {
                return { 0.f, 0.f };
            }

            const FontFaceData& lFace  = *InFace.Data;
            const float         lScale = InParams.Size / lFace.PixelHeight;

            // The pen sits on the BASELINE, which is one ascent below the caller's top-left. Y is up
            // here, so "below" subtracts — the one sign the whole layout turns on.
            const float lLineStep = lFace.VMetrics.LineAdvance * InParams.LineHeightScale * lScale;

            float lPenX      = InWorldPos.x;
            float lBaselineY = InWorldPos.y - lFace.VMetrics.Ascent * lScale;

            const FontGlyph* lSpace        = lFace.FindGlyph(' ');
            const float      lSpaceAdvance = (lSpace != nullptr) ? lSpace->XAdvance * lScale
                                                                 : InParams.Size * FALLBACK_SPACE_RATIO;

            float  lWidestLine = 0.f;
            Uint32 lLineCount  = 1u;
            Uint32 lPrevious   = 0u;   // 0 = start of a line, so nothing kerns against it

            const char* lCursor = InUtf8;

            while (const Uint32 lCodepoint = Utf8::Decode(lCursor))
            {
                if (lCodepoint == '\n')
                {
                    lWidestLine = (lPenX - InWorldPos.x > lWidestLine) ? lPenX - InWorldPos.x : lWidestLine;
                    lPenX       = InWorldPos.x;
                    lBaselineY -= lLineStep;
                    lPrevious   = 0u;
                    ++lLineCount;
                    continue;
                }

                if (lCodepoint == '\t')
                {
                    lPenX    += lSpaceAdvance * TAB_SPACES;
                    lPrevious = 0u;   // no pair straddles a tab
                    continue;
                }

                if (InParams.bKerning && lPrevious != 0u)
                {
                    lPenX += lFace.GetKerning(lPrevious, lCodepoint) * lScale;
                }

                const FontGlyph* lGlyph = lFace.FindGlyph(lCodepoint);

                if (lGlyph == nullptr)
                {
                    // Tofu. Sits ON the baseline and is sized from Size rather than from the face,
                    // because the face is precisely what does not know this character.
                    if (InRenderer != nullptr)
                    {
                        const Vector2F lBox{ InParams.Size * TOFU_WIDTH_RATIO,
                                             InParams.Size * TOFU_HEIGHT_RATIO };

                        InRenderer->DrawQuadOutline({ lPenX + lBox.x * 0.5f, lBaselineY + lBox.y * 0.5f },
                                                    lBox, InParams.Color,
                                                    InParams.Size * TOFU_THICKNESS_RATIO, 0.f,
                                                    InParams.Layer, InParams.OrderInLayer);
                    }

                    lPenX    += InParams.Size * TOFU_ADVANCE_RATIO;
                    lPrevious = lCodepoint;
                    continue;
                }

                // A blank glyph (space) submits nothing but still advances — one quad per space saved
                // on every string in the frame. Atlas-less means the face is still uploading: lay the
                // line out, draw none of it, and the next frame draws it in the right place.
                const bool bDrawable = (InRenderer != nullptr) && (InFace.Atlas != nullptr)
                                    && (lGlyph->QuadSize.x > 0.f) && (lGlyph->QuadSize.y > 0.f);

                if (bDrawable)
                {
                    const Vector2F lSize{ lGlyph->QuadSize.x * lScale, lGlyph->QuadSize.y * lScale };

                    // QuadOffset is stb's, measured from the pen with Y going DOWN. This world's Y
                    // goes up, so the vertical term subtracts and the horizontal one adds.
                    const Vector2F lCentre{ lPenX      + (lGlyph->QuadOffset.x + lGlyph->QuadSize.x * 0.5f) * lScale,
                                            lBaselineY - (lGlyph->QuadOffset.y + lGlyph->QuadSize.y * 0.5f) * lScale };

                    InRenderer->DrawSprite(lCentre, lSize, *InFace.Atlas, InParams.Color, 0.f,
                                           InParams.Layer, InParams.OrderInLayer,
                                           lGlyph->UVMin, lGlyph->UVMax);
                }

                lPenX    += lGlyph->XAdvance * lScale;
                lPrevious = lCodepoint;
            }

            lWidestLine = (lPenX - InWorldPos.x > lWidestLine) ? lPenX - InWorldPos.x : lWidestLine;

            return { lWidestLine, static_cast<float>(lLineCount) * lLineStep };
        }
    }

    Vector2F DrawString(Renderer2D& InRenderer, const char* InUtf8, const Vector2F& InWorldPos,
                        const FontFaceView& InFace, const TextDrawParams& InParams)
    {
        return WalkText(&InRenderer, InUtf8, InWorldPos, InFace, InParams);
    }

    Vector2F Measure(const char* InUtf8, const FontFaceView& InFace, const TextDrawParams& InParams)
    {
        return WalkText(nullptr, InUtf8, { 0.f, 0.f }, InFace, InParams);
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
