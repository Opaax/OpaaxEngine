#pragma once

#include <algorithm>

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // FontFaceData — one baked font face: glyph atlas positions, advances and kerning, by codepoint.
    //   Plain data (no GPU handle); FontFaceView pairs it with the atlas texture.
    // =============================================================================

    /**
     * Default bake height.
     */
    inline constexpr float DEFAULT_FONT_PIXEL_HEIGHT = 32.f;

    /**
     * One glyph: its atlas rectangle and how the pen moves. Distances in bake pixels.
     */
    struct FontGlyph
    {
        /**
         * Atlas UVs, V already flipped at bake.
         */
        Vector2F UVMin = { 0.f, 0.f };
        Vector2F UVMax = { 0.f, 0.f };

        /** Top-left of the glyph box relative to the pen (x right, y down). */
        Vector2F QuadOffset = { 0.f, 0.f };

        /** Box size. Zero for a blank glyph (space). */
        Vector2F QuadSize = { 0.f, 0.f };

        /** Pen advance after this glyph, in pixels. */
        float XAdvance = 0.f;
    };

    /** Vertical metrics, at bake height. */
    struct FontVMetrics
    {
        float Ascent      = 0.f;   // above the baseline, positive
        float Descent     = 0.f;   // below the baseline, negative
        float LineGap     = 0.f;
        float LineAdvance = 0.f;   // Ascent - Descent + LineGap
    };

    /**
     * Extra spacing between two codepoints (negative pulls them together: 'AV', 'To').
     * Absent pairs are 0.
     */
    struct FontKerningPair
    {
        Uint32 First   = 0u;
        Uint32 Second  = 0u;
        float  Advance = 0.f;
    };

    struct FontFaceData
    {
        // =========================================================================
        // Members
        // =========================================================================
    public:
        /** Every codepoint in the font, and its glyph. */
        TUnorderedMap<Uint32, FontGlyph> Glyphs;

        /** Sorted by (First, Second) for binary search. */
        TDynArray<FontKerningPair> Kerning;

        FontVMetrics VMetrics;

        Uint32 AtlasWidth  = 0u;
        Uint32 AtlasHeight = 0u;

        /** Bake height. A draw divides by it to get its scale. */
        float PixelHeight = 0.f;

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /**
         * The glyph for InCodepoint, or nullptr (the caller decides what to draw, e.g. a box).
         */
        const FontGlyph* FindGlyph(const Uint32 InCodepoint) const noexcept
        {
            const auto lFound = Glyphs.find(InCodepoint);
            return (lFound != Glyphs.end()) ? &lFound->second : nullptr;
        }

        /** Kerning between two codepoints, in bake pixels. 0 if none. */
        float GetKerning(const Uint32 InFirst, const Uint32 InSecond) const noexcept
        {
            if (Kerning.empty())
            {
                return 0.f;
            }

            const auto lLower = std::lower_bound(Kerning.begin(), Kerning.end(), PackKey(InFirst, InSecond),
                                                 [](const FontKerningPair& InPair, const Uint64 InKey)
                                                 {
                                                     return PackKey(InPair.First, InPair.Second) < InKey;
                                                 });

            return (lLower != Kerning.end() && lLower->First == InFirst && lLower->Second == InSecond)
                       ? lLower->Advance
                       : 0.f;
        }

        Uint32 GlyphCount() const noexcept { return static_cast<Uint32>(Glyphs.size()); }

        /** No glyphs (failed bake or placeholder): everything draws as boxes. */
        bool IsEmpty() const noexcept { return Glyphs.empty(); }

        /**
         * Ordering shared by the bake's sort and the lookup's search.
         */
        static constexpr Uint64 PackKey(const Uint32 InFirst, const Uint32 InSecond) noexcept
        {
            return (static_cast<Uint64>(InFirst) << 32) | static_cast<Uint64>(InSecond);
        }

        /**
         * A face with no glyphs but valid metrics, so text lays out normally as a row of boxes.
         * Used for missing fonts and missing scripts. (A default FontFaceData has zero metrics.)
         */
        static FontFaceData Tofu() noexcept
        {
            constexpr float ASCENT_RATIO  =  0.8f;
            constexpr float DESCENT_RATIO = -0.2f;

            FontFaceData lFace;
            lFace.PixelHeight          = DEFAULT_FONT_PIXEL_HEIGHT;
            lFace.VMetrics.Ascent      = DEFAULT_FONT_PIXEL_HEIGHT * ASCENT_RATIO;
            lFace.VMetrics.Descent     = DEFAULT_FONT_PIXEL_HEIGHT * DESCENT_RATIO;
            lFace.VMetrics.LineAdvance = DEFAULT_FONT_PIXEL_HEIGHT;

            return lFace;
        }
    };
}
