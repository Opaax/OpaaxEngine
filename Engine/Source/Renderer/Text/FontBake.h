#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Renderer/Text/FontFaceData.h"   // DEFAULT_FONT_PIXEL_HEIGHT — the bake's default is its

namespace Opaax
{
    inline constexpr LogCategory LogFontBake{"FontBake"};

    // =============================================================================
    // FontBake — TTF/OTF bytes in, a glyph atlas out. The one place stb_truetype is spoken to.
    //
    //   NO FILE IO and no GPU: it takes bytes and answers pixels, which is what makes the whole
    //   rasterisation path unit-testable with no device and lets `FontFaceResource::Load` run it on
    //   a worker thread (the CResource two-phase split's entire point).
    //
    //   IT BAKES WHAT THE FILE HAS. There is no "which range?" parameter to get wrong: the coverage
    //   scan asks the font itself which codepoints it carries, so a fontsource subset bakes exactly
    //   its subset and a full unsubsetted face bakes everything it has. The only bound is the scan
    //   window, and its far end is the CJK line the project has not crossed yet.
    // =============================================================================
    namespace FontBake
    {
        /**
         * What one bake is asked for. Every number is here rather than inline in the body so a
         * caller can say why it wants something different, and so a test can shrink the atlas.
         */
        struct BakeParams
        {
            /** Rasterisation height. A draw at another size scales the baked metrics. */
            float PixelHeight = DEFAULT_FONT_PIXEL_HEIGHT;

            /**
             * Supersampling factor. (2,2) costs 4x the atlas footprint per glyph and buys visibly
             * cleaner edges at small sizes — the setting M5 landed on and verified.
             */
            Uint32 Oversample = 2u;

            /** First codepoint probed. Below space there is nothing to draw. */
            Uint32 FirstCodepoint = 0x0020u;

            /**
             * Last codepoint probed — and the project's CJK line, drawn deliberately.
             *
             * 0x33FF is the last code point before CJK Unified Ideographs Extension A. Chinese and
             * Japanese need a DYNAMIC atlas (thousands of glyphs, rasterised on demand), not a bigger
             * window: raising this number would not make them work, it would make a 2048x2048 atlas
             * overflow. When that day comes the change is inside this file and the resource, and
             * FontFaceData's codepoint-keyed lookup does not move.
             */
            Uint32 LastCodepoint = 0x33FFu;

            Uint32 InitialAtlasSize = 512u;
            Uint32 MaxAtlasSize     = 2048u;

            /**
             * Above this many glyphs the kerning pass is SKIPPED, loudly.
             *
             * The pass is N-squared over the covered set, and Roboto's `symbols` and `math` subsets
             * carry a thousand glyphs that no one kerns. The real text subsets (latin ~200, greek
             * ~130, cyrillic ~150) sit well under it.
             */
            Uint32 MaxKerningGlyphs = 512u;
        };

        /**
         * Rasterise InTtfBytes into a single-page R8 coverage atlas.
         *
         * The atlas starts at InParams.InitialAtlasSize and DOUBLES until the glyphs fit, then fails
         * loudly past MaxAtlasSize rather than silently dropping the tail — a face missing half its
         * glyphs reads as a broken font, not as a full atlas.
         *
         * @param InTtfBytes Whole font file. Borrowed; nothing is retained past the call.
         * @param InByteCount Its length.
         * @param InParams Bake settings.
         * @param OutData Glyph table, vertical metrics, kerning and the atlas dimensions. Left
         *   UNTOUCHED on failure, so a reload that fails cannot half-replace a working face.
         * @param OutPixels The atlas, one byte of coverage per texel, AtlasWidth * AtlasHeight long.
         *   Also left untouched on failure.
         * @return false when the bytes are not a font the parser accepts, when the file carries no
         *   glyph in the scan window, or when the atlas would exceed the cap.
         */
        OPAAX_API bool Bake(const Uint8* InTtfBytes, Uint64 InByteCount, const BakeParams& InParams,
                            FontFaceData& OutData, TDynArray<Uint8>& OutPixels);
    }
}
