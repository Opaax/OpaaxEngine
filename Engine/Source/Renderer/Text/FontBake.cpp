#include "Renderer/Text/FontBake.h"

#include <algorithm>

#include "Renderer/Text/FontFaceData.h"

// stb_truetype — the ONE implementation in the build, and it lives here for the same reason
// STB_IMAGE_IMPLEMENTATION lives in TextureResource.cpp: rasterising is CPU work, above the RHI,
// that every backend shares. Nothing else in the engine includes this header.
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace Opaax::FontBake
{
    namespace
    {
        /** Padding between packed glyphs, in atlas texels — one is enough to stop bilinear bleed. */
        constexpr Int32 GLYPH_PADDING = 1;

        /**
         * The offset table (12 bytes) plus one table record (16). Below this there is not even a
         * header to read, and stb_truetype does not check.
         */
        constexpr Uint64 MIN_FONT_BYTES = 28u;

        /**
         * Every codepoint in the scan window that this font actually has a glyph for.
         *
         * The probe is a cmap lookup per codepoint — ~13k of them across the default window, a few
         * milliseconds — and it is what removes the "which range do I bake?" question entirely. A
         * fontsource subset answers its own subset; a full face answers everything it carries.
         */
        TDynArray<Int32> GatherCoverage(const stbtt_fontinfo& InInfo, const BakeParams& InParams)
        {
            TDynArray<Int32> lCodepoints;

            for (Uint32 lCodepoint = InParams.FirstCodepoint; lCodepoint <= InParams.LastCodepoint; ++lCodepoint)
            {
                if (stbtt_FindGlyphIndex(&InInfo, static_cast<int>(lCodepoint)) != 0)
                {
                    lCodepoints.emplace_back(static_cast<Int32>(lCodepoint));
                }
            }

            return lCodepoints;
        }

        /**
         * One packing attempt at InAtlasSize. Answers false when the glyphs did not all fit, which is
         * the caller's signal to double and retry.
         */
        bool TryPack(const Uint8* InTtfBytes, const BakeParams& InParams, const TDynArray<Int32>& InCodepoints,
                     Uint32 InAtlasSize, TDynArray<Uint8>& OutPixels, TDynArray<stbtt_packedchar>& OutPacked)
        {
            OutPixels.assign(static_cast<Uint64>(InAtlasSize) * InAtlasSize, Uint8{ 0 });
            OutPacked.assign(InCodepoints.size(), stbtt_packedchar{});

            stbtt_pack_context lPack{};
            if (stbtt_PackBegin(&lPack, OutPixels.data(), static_cast<int>(InAtlasSize), static_cast<int>(InAtlasSize),
                                0, GLYPH_PADDING, nullptr) == 0)
            {
                return false;
            }

            stbtt_PackSetOversampling(&lPack, InParams.Oversample, InParams.Oversample);

            stbtt_pack_range lRange{};
            lRange.font_size                       = InParams.PixelHeight;
            lRange.first_unicode_codepoint_in_range = 0;   // 0 => use the explicit list below
            lRange.array_of_unicode_codepoints     = const_cast<int*>(InCodepoints.data());
            lRange.num_chars                       = static_cast<int>(InCodepoints.size());
            lRange.chardata_for_range              = OutPacked.data();

            const bool bPacked = stbtt_PackFontRanges(&lPack, InTtfBytes, 0, &lRange, 1) != 0;
            stbtt_PackEnd(&lPack);

            return bPacked;
        }

        /**
         * The packed rectangles as FontGlyphs.
         *
         * THE V COORDINATES ARE SWAPPED HERE, once, and this is the correction M5 got wrong twice.
         * stb_truetype fills the atlas top-down (row 0 is the top), while Renderer2D's quad samples
         * bottom-up — so the quad's UVMin (its bottom edge) must read the glyph's LAST row. Doing it
         * at bake keeps the layout walker's maths straight and every draw free of a per-glyph fix-up.
         */
        void FillGlyphs(const TDynArray<Int32>& InCodepoints, const TDynArray<stbtt_packedchar>& InPacked,
                        const float InAtlasSize, FontFaceData& OutData)
        {
            for (Uint64 lIndex = 0; lIndex < InCodepoints.size(); ++lIndex)
            {
                const stbtt_packedchar& lPacked = InPacked[lIndex];

                FontGlyph lGlyph;
                lGlyph.UVMin      = { static_cast<float>(lPacked.x0) / InAtlasSize,
                                      static_cast<float>(lPacked.y1) / InAtlasSize };
                lGlyph.UVMax      = { static_cast<float>(lPacked.x1) / InAtlasSize,
                                      static_cast<float>(lPacked.y0) / InAtlasSize };
                lGlyph.QuadOffset = { lPacked.xoff, lPacked.yoff };
                lGlyph.QuadSize   = { lPacked.xoff2 - lPacked.xoff, lPacked.yoff2 - lPacked.yoff };
                lGlyph.XAdvance   = lPacked.xadvance;

                OutData.Glyphs.emplace(static_cast<Uint32>(InCodepoints[lIndex]), lGlyph);
            }
        }

        /** The N-squared kerning walk, already gated on the covered count by the caller. */
        void FillKerning(const stbtt_fontinfo& InInfo, const TDynArray<Int32>& InCodepoints,
                         const float InScale, FontFaceData& OutData)
        {
            for (const Int32 lFirst : InCodepoints)
            {
                for (const Int32 lSecond : InCodepoints)
                {
                    const Int32 lRaw = stbtt_GetCodepointKernAdvance(&InInfo, lFirst, lSecond);
                    if (lRaw == 0)
                    {
                        continue;   // an absent pair means zero, so storing zeros would only cost memory
                    }

                    OutData.Kerning.emplace_back(FontKerningPair{ static_cast<Uint32>(lFirst),
                                                                  static_cast<Uint32>(lSecond),
                                                                  static_cast<float>(lRaw) * InScale });
                }
            }

            std::sort(OutData.Kerning.begin(), OutData.Kerning.end(),
                      [](const FontKerningPair& InLeft, const FontKerningPair& InRight)
                      {
                          return FontFaceData::PackKey(InLeft.First,  InLeft.Second)
                               < FontFaceData::PackKey(InRight.First, InRight.Second);
                      });

            OutData.Kerning.shrink_to_fit();
        }
    }

    bool Bake(const Uint8* InTtfBytes, const Uint64 InByteCount, const BakeParams& InParams,
              FontFaceData& OutData, TDynArray<Uint8>& OutPixels)
    {
        if (InTtfBytes == nullptr || InByteCount < MIN_FONT_BYTES)
        {
            OPAAX_LOG(LogFontBake, Error, "{} byte(s) is not a font header", InByteCount);
            return false;
        }

        // CHECKED BEFORE stbtt_InitFont, and that order is the whole point: the offset lookup is what
        // reads the file's magic, and it answers -1 for anything that is not a font. Handing that -1
        // straight to InitFont — the obvious one-liner — indexes before the buffer and SEGFAULTS. A
        // `.ttf` that is really a PNG is an ordinary authoring mistake, and it must reach the
        // placeholder policy rather than take the process down.
        const Int32 lOffset = stbtt_GetFontOffsetForIndex(InTtfBytes, 0);
        if (lOffset < 0)
        {
            OPAAX_LOG(LogFontBake, Error, "not a font — no recognised magic in {} bytes", InByteCount);
            return false;
        }

        stbtt_fontinfo lInfo{};
        if (stbtt_InitFont(&lInfo, InTtfBytes, lOffset) == 0)
        {
            OPAAX_LOG(LogFontBake, Error, "not a font the parser accepts ({} bytes)", InByteCount);
            return false;
        }

        const TDynArray<Int32> lCodepoints = GatherCoverage(lInfo, InParams);
        if (lCodepoints.empty())
        {
            OPAAX_LOG(LogFontBake, Error, "no glyph in [{:#x}..{:#x}] — nothing to bake",
                      InParams.FirstCodepoint, InParams.LastCodepoint);
            return false;
        }

        // Grow by doubling. Written into LOCALS and only published at the end, so a failed bake leaves
        // OutData and OutPixels exactly as the caller had them.
        TDynArray<Uint8>             lPixels;
        TDynArray<stbtt_packedchar>  lPacked;
        Uint32                       lAtlasSize = InParams.InitialAtlasSize;

        while (!TryPack(InTtfBytes, InParams, lCodepoints, lAtlasSize, lPixels, lPacked))
        {
            if (lAtlasSize >= InParams.MaxAtlasSize)
            {
                OPAAX_LOG(LogFontBake, Error, "{} glyph(s) do not fit {}x{}, the cap — bake refused",
                          lCodepoints.size(), lAtlasSize, lAtlasSize);
                return false;
            }

            lAtlasSize *= 2u;
            OPAAX_LOG(LogFontBake, Warn, "{} glyph(s) overflowed the atlas, retrying at {}x{}",
                      lCodepoints.size(), lAtlasSize, lAtlasSize);
        }

        const float lScale = stbtt_ScaleForPixelHeight(&lInfo, InParams.PixelHeight);

        Int32 lAscent = 0, lDescent = 0, lLineGap = 0;
        stbtt_GetFontVMetrics(&lInfo, &lAscent, &lDescent, &lLineGap);

        FontFaceData lData;
        lData.AtlasWidth          = lAtlasSize;
        lData.AtlasHeight         = lAtlasSize;
        lData.PixelHeight         = InParams.PixelHeight;
        lData.VMetrics.Ascent     = static_cast<float>(lAscent)  * lScale;
        lData.VMetrics.Descent    = static_cast<float>(lDescent) * lScale;
        lData.VMetrics.LineGap    = static_cast<float>(lLineGap) * lScale;
        lData.VMetrics.LineAdvance = lData.VMetrics.Ascent - lData.VMetrics.Descent + lData.VMetrics.LineGap;

        FillGlyphs(lCodepoints, lPacked, static_cast<float>(lAtlasSize), lData);

        if (lCodepoints.size() <= static_cast<Uint64>(InParams.MaxKerningGlyphs))
        {
            FillKerning(lInfo, lCodepoints, lScale, lData);
        }
        else
        {
            OPAAX_LOG(LogFontBake, Warn, "{} glyph(s) is past the {} kerning limit — face baked UNKERNED",
                      lCodepoints.size(), InParams.MaxKerningGlyphs);
        }

        OutData   = Move(lData);
        OutPixels = Move(lPixels);
        return true;
    }
}
