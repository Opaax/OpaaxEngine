#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Renderer/Text/FontFaceData.h"   // DEFAULT_FONT_PIXEL_HEIGHT

namespace Opaax
{
    inline constexpr LogCategory LogFontBake{"FontBake"};

    // =============================================================================
    // FontBake — font file bytes to a glyph atlas (stb_truetype). No file IO, no GPU
    //   (runs on a worker in FontFaceResource::Load). Bakes every glyph the font has in the
    //   scan range.
    // =============================================================================
    namespace FontBake
    {
        /**
         * Bake settings.
         */
        struct BakeParams
        {
            /** Rasterization height. Other draw sizes scale the metrics. */
            float PixelHeight = DEFAULT_FONT_PIXEL_HEIGHT;

            /**
             * Oversampling. (2,2) costs 4x the atlas space and gives cleaner small text.
             */
            Uint32 Oversample = 2u;

            /** First codepoint scanned. */
            Uint32 FirstCodepoint = 0x0020u;

            /**
             * Last codepoint scanned: 0x33FF, just before CJK. CJK needs a dynamic atlas, not a bigger range.
             */
            Uint32 LastCodepoint = 0x33FFu;

            Uint32 InitialAtlasSize = 512u;
            Uint32 MaxAtlasSize     = 2048u;

            /**
             * Above this many glyphs, kerning is skipped (with a warning). Text subsets are well below it.
             */
            Uint32 MaxKerningGlyphs = 512u;
        };

        /**
         * Rasterizes InTtfBytes into an R8 atlas. The atlas doubles until everything fits, and fails
         * past MaxAtlasSize.
         * @param InTtfBytes Whole font file (not kept)
         * @param InByteCount Its size
         * @param InParams Bake settings
         * @param OutData Glyphs, metrics, kerning and atlas size. Untouched on failure.
         * @param OutPixels The atlas (AtlasWidth * AtlasHeight bytes). Untouched on failure.
         * @return False if the bytes are not a font, have no glyph in range, or the atlas is too big
         */
        bool Bake(const Uint8* InTtfBytes, Uint64 InByteCount, const BakeParams& InParams,
                            FontFaceData& OutData, TDynArray<Uint8>& OutPixels);
    }
}
