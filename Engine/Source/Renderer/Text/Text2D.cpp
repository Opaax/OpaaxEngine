#include "Text2D.h"

#include "Renderer/Renderer2D.h"
#include "Renderer/Text/FontAsset.h"
#include "Renderer/Texture2D.h"

namespace Opaax::Text2D
{
    // =============================================================================
    // Local helpers
    // =============================================================================
    namespace
    {
        // OD-4 lock: '\t' renders as 4 spaces — cheap, predictable, no tab-stop state.
        constexpr Uint32 sTabSpaceWidth = 4u;
        // OD-3 lock: out-of-range codepoints render '?' as fallback.
        constexpr char   sFallbackGlyph = '?';

        // Resolve glyph metrics, falling back to '?' if the codepoint is outside
        // the baked [0x20..0x7E] range. Returns false only if '?' is also missing
        // (would mean a malformed font; logged once at FontAsset::ctor).
        bool ResolveGlyph(const FontAsset& InFont, char InCp, FontAsset::GlyphMetrics& OutG)
        {
            if (InFont.GetGlyphMetrics(InCp, OutG))
            {
                return true;
            }
            return InFont.GetGlyphMetrics(sFallbackGlyph, OutG);
        }
    }

    // =============================================================================
    // DrawString
    // =============================================================================
    void DrawString(const char*       InText,
                    const Vector2F&   InWorldPos,
                    const FontAsset&  InFont,
                    const DrawParams& InParams)
    {
        if (!InText || !*InText || !InFont.IsLoaded())
        {
            return;
        }
        const Texture2D* lAtlasPtr = InFont.GetAtlasTexture();
        if (!lAtlasPtr)
        {
            return;
        }
        // Renderer2D::DrawSprite mutates only its own bind-slot state, not the texture.
        Texture2D& lAtlas = const_cast<Texture2D&>(*lAtlasPtr);

        const FontAsset::FontVMetrics& lV = InFont.GetFontVMetrics();
        const float lScale      = InParams.Scale;
        const float lLineDownPx = lV.LineAdvance * InParams.LineHeightScale * lScale;

        // Y-UP world: user passes the text's top-left. Baseline sits BELOW the top
        // by Ascent*Scale (stb's Ascent is the distance from baseline to the highest
        // glyph extent; in y-up world that distance goes upward, so baseline =
        // top - Ascent).
        const float lStartX     = InWorldPos.x;
        const float lBaselineY0 = InWorldPos.y - lV.Ascent * lScale;

        FontAsset::GlyphMetrics lSpace = {};
        const bool              lSpaceOk = InFont.GetGlyphMetrics(' ', lSpace);

        float lCursorX = lStartX;
        float lCursorY = lBaselineY0;
        char  lPrev    = 0;

        for (const char* lP = InText; *lP; ++lP)
        {
            const char lC = *lP;

            if (lC == '\n')
            {
                lCursorX = lStartX;
                lCursorY -= lLineDownPx; // y-up: new line moves the baseline downward
                lPrev    = 0;
                continue;
            }
            if (lC == '\t')
            {
                if (lSpaceOk)
                {
                    lCursorX += static_cast<float>(sTabSpaceWidth) * lSpace.XAdvance * lScale;
                }
                lPrev = 0;
                continue;
            }

            FontAsset::GlyphMetrics lG = {};
            if (!ResolveGlyph(InFont, lC, lG))
            {
                lPrev = 0;
                continue;
            }

            // Apply kerning before fixing this glyph's draw position. GetKerning
            // returns 0 for prev=0 (start of line / after tab) since 0 is outside
            // the baked range — no special-case branch needed beyond the toggle.
            if (InParams.EnableKerning && lPrev != 0)
            {
                lCursorX += InFont.GetKerning(lPrev, lC) * lScale;
            }

            // Y-UP quad geometry. QuadOffset.y is stb's yoff (offset from pen to
            // bbox top in stb's y-down — typically negative for ascending glyphs).
            // top_y    = cursor_y - QuadOffset.y * Scale    (subtracting a negative goes up)
            // bottom_y = top_y - QuadSize.y * Scale         (y-up: down = subtract)
            // center_y = top_y - QuadSize.y * Scale * 0.5
            //
            // Equivalent (LearnOpenGL §60 reference-glyph trick) but cleaner —
            // uses the documented font Ascent instead of a top-touching probe glyph.
            const float lQuadW   = lG.QuadSize.x * lScale;
            const float lQuadH   = lG.QuadSize.y * lScale;
            const float lTopY    = lCursorY - lG.QuadOffset.y * lScale;
            const float lCenterX = lCursorX + lG.QuadOffset.x * lScale + lQuadW * 0.5f;
            const float lCenterY = lTopY - lQuadH * 0.5f;

            // UVMin/UVMax are V-swapped at bake (Step 2) — they map cleanly to
            // Renderer2D's bottom-up vertex layout. No per-draw UV correction.
            Renderer2D::DrawSprite({ lCenterX, lCenterY }, { lQuadW, lQuadH }, lAtlas,
                                   lG.UVMin, lG.UVMax, InParams.Color, 0.f);

            lCursorX += lG.XAdvance * lScale;
            lPrev = lC;
        }
    }

    // =============================================================================
    // Measure
    // =============================================================================
    Vector2F Measure(const char*       InText,
                     const FontAsset&  InFont,
                     const DrawParams& InParams)
    {
        if (!InText || !*InText || !InFont.IsLoaded())
        {
            return { 0.f, 0.f };
        }

        const FontAsset::FontVMetrics& lV = InFont.GetFontVMetrics();
        const float lScale      = InParams.Scale;
        const float lLineDownPx = lV.LineAdvance * InParams.LineHeightScale * lScale;

        FontAsset::GlyphMetrics lSpace = {};
        const bool              lSpaceOk = InFont.GetGlyphMetrics(' ', lSpace);

        float  lLineWidth = 0.f;
        float  lMaxWidth  = 0.f;
        Uint32 lLineCount = 1u;
        char   lPrev      = 0;

        for (const char* lP = InText; *lP; ++lP)
        {
            const char lC = *lP;

            if (lC == '\n')
            {
                if (lLineWidth > lMaxWidth) { lMaxWidth = lLineWidth; }
                lLineWidth = 0.f;
                ++lLineCount;
                lPrev = 0;
                continue;
            }
            if (lC == '\t')
            {
                if (lSpaceOk)
                {
                    lLineWidth += static_cast<float>(sTabSpaceWidth) * lSpace.XAdvance * lScale;
                }
                lPrev = 0;
                continue;
            }

            FontAsset::GlyphMetrics lG = {};
            if (!ResolveGlyph(InFont, lC, lG))
            {
                lPrev = 0;
                continue;
            }

            if (InParams.EnableKerning && lPrev != 0)
            {
                lLineWidth += InFont.GetKerning(lPrev, lC) * lScale;
            }
            lLineWidth += lG.XAdvance * lScale;
            lPrev = lC;
        }
        if (lLineWidth > lMaxWidth) { lMaxWidth = lLineWidth; }

        // Total height = N_lines * LineAdvance — over-estimates by ~LineGap on the
        // last line but matches "where the next line would start if appended" which
        // is what layout callers (future Draw Debug API) actually need.
        const float lTotalHeight = static_cast<float>(lLineCount) * lLineDownPx;
        return { lMaxWidth, lTotalHeight };
    }

} // namespace Opaax::Text2D
