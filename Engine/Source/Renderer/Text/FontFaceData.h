#pragma once

#include <algorithm>

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // FontFaceData — one baked typeface as DATA: where every glyph sits in the atlas, how far the
    //   pen moves, and how two of them tighten against each other.
    //
    //   KEYED BY CODEPOINT, not by an index into an ASCII table. That is the whole difference from
    //   the retired M5 shape (a 95-slot array indexed by `char - 0x20`), and it is what lets a Greek
    //   or Cyrillic face exist at all. A face holds whatever its file holds — sparse, so the map is
    //   the honest container.
    //
    //   UNDER Renderer/, not under Resources/: this is what the layout walker reads, and the walker
    //   is portable. `FontFaceResource` composes it the same way `TextureResource` composes an
    //   `ITexture2D` — Resources depends on Renderer, never the other way.
    //
    //   NO GPU HANDLE HERE. The atlas texture is the resource's; pairing the two is `FontFaceView`'s
    //   job, once per draw. Which is what keeps this struct testable with no device.
    //
    //   EVERY MEMBER IS INLINE: the struct carries no OPAAX_API, so a member defined in the DLL's
    //   .cpp would be unresolvable from the exe (**I6**).
    // =============================================================================

    /**
     * The height a face is rasterised at unless something says otherwise, and the one every face
     * with no file behind it reports.
     *
     * Here rather than on FontBake::BakeParams because it is a property of BAKED DATA — what
     * PixelHeight means — and because the tofu face below needs it without reaching for the baker.
     */
    inline constexpr float DEFAULT_FONT_PIXEL_HEIGHT = 32.f;

    /**
     * One glyph's rectangle in the atlas, and what it does to the pen. All distances are in the
     * face's BAKE pixels — a draw at another size scales them.
     */
    struct FontGlyph
    {
        /**
         * Normalised atlas coordinates, V ALREADY FLIPPED at bake. stb_truetype packs top-down and
         * Renderer2D samples bottom-up, so correcting once here keeps the layout walker's maths
         * straight and every draw free of a per-glyph fix-up.
         */
        Vector2F UVMin = { 0.f, 0.f };
        Vector2F UVMax = { 0.f, 0.f };

        /** Top-left of the glyph's box relative to the pen (x right, y DOWN — stb's convention). */
        Vector2F QuadOffset = { 0.f, 0.f };

        /** The box's width and height. Zero for a blank glyph like space, which draws nothing. */
        Vector2F QuadSize = { 0.f, 0.f };

        /** How far the pen moves after this glyph. Already PIXELS — NOT FreeType's 1/64th fixed-point. */
        float XAdvance = 0.f;
    };

    /** The face's vertical rhythm, scaled to the bake height. */
    struct FontVMetrics
    {
        float Ascent      = 0.f;   // above the baseline, positive
        float Descent     = 0.f;   // below it, NEGATIVE
        float LineGap     = 0.f;
        float LineAdvance = 0.f;   // Ascent - Descent + LineGap, precomputed for '\n'
    };

    /**
     * A signed pixel nudge applied BETWEEN two codepoints, on top of the first one's XAdvance.
     * Negative pulls them together — 'AV', 'To', 'Wa'. Zero-advance pairs are dropped at bake, so
     * a pair that is absent means 0.
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
        /** Every codepoint the source file actually carried, and where it landed. */
        TUnorderedMap<Uint32, FontGlyph> Glyphs;

        /** SORTED by (First, Second) — GetKerning binary-searches it. */
        TDynArray<FontKerningPair> Kerning;

        FontVMetrics VMetrics;

        Uint32 AtlasWidth  = 0u;
        Uint32 AtlasHeight = 0u;

        /** The pixel height everything above was baked at. A draw divides by it to get its scale. */
        float PixelHeight = 0.f;

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /**
         * The glyph for InCodepoint, or nullptr when this face does not have it.
         *
         * NULLPTR RATHER THAN A SUBSTITUTE, deliberately: choosing what a missing glyph looks like is
         * the caller's decision (Text2D draws a tofu box), and a face that quietly answered '?' would
         * make "does this face cover this text?" unanswerable.
         */
        const FontGlyph* FindGlyph(const Uint32 InCodepoint) const noexcept
        {
            const auto lFound = Glyphs.find(InCodepoint);
            return (lFound != Glyphs.end()) ? &lFound->second : nullptr;
        }

        /** The nudge between two codepoints, in bake pixels. 0 when the pair is unkerned or absent. */
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

        /** No glyphs at all — a face that failed to bake, or the placeholder. Everything draws tofu. */
        bool IsEmpty() const noexcept { return Glyphs.empty(); }

        /**
         * The one ordering both the bake's sort and the lookup's search use. Shared so they cannot
         * disagree, which is the only way a sorted-array lookup goes wrong.
         */
        static constexpr Uint64 PackKey(const Uint32 InFirst, const Uint32 InSecond) noexcept
        {
            return (static_cast<Uint64>(InFirst) << 32) | static_cast<Uint64>(InSecond);
        }

        /**
         * A face with NO glyphs but usable metrics — so a string drawn with it lays out normally and
         * comes out as a row of tofu boxes.
         *
         * The ONE definition of that, because three things want it and they must agree: a `.ttf`
         * that failed to load, a family with nothing in the requested script, and any future "I have
         * no face for you" answer. A zero PixelHeight divides by zero the moment a draw scales it,
         * and a zero LineAdvance stacks every line on top of itself — which is why an empty
         * value-initialised FontFaceData is NOT the same thing.
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
