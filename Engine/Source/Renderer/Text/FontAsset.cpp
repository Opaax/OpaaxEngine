#include "FontAsset.h"

#include "Core/Log/OpaaxLog.h"
#include "Renderer/Texture2D.h"

#include <algorithm>
#include <fstream>
#include <vector>

// stb_truetype — implementation defined once here. The two stb implementations
// (stb_image in OpenGLTexture2D.cpp, stb_truetype here) are independent and do
// not collide. Future Text2D.cpp only needs the header, not the implementation.
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace Opaax
{
    // =============================================================================
    // Local helpers
    // =============================================================================
    namespace
    {
        //(2,2) oversample default. Sharper anti-alias at ~4x atlas footprint.
        constexpr Uint32 sOversampleH = 2u;
        constexpr Uint32 sOversampleV = 2u;

        FORCEINLINE Uint32 PackKerningKey(Uint8 InFirst, Uint8 InSecond) noexcept
        {
            return (static_cast<Uint32>(InFirst) << 8) | static_cast<Uint32>(InSecond);
        }

        std::vector<unsigned char> ReadFileBytes(const OpaaxString& InPath)
        {
            std::ifstream lFile(InPath.CStr(), std::ios::binary | std::ios::ate);
            if (!lFile)
            {
                return {};
            }

            const std::streamsize lSize = lFile.tellg();
            if (lSize <= 0)
            {
                return {};
            }

            lFile.seekg(0, std::ios::beg);
            std::vector<unsigned char> lBytes(static_cast<size_t>(lSize));
            if (!lFile.read(reinterpret_cast<char*>(lBytes.data()), lSize))
            {
                return {};
            }
            return lBytes;
        }
    }

    // =============================================================================
    // CTORS - DTOR
    // =============================================================================
    FontAsset::FontAsset(const OpaaxString& InSourcePath, OpaaxStringID InAssetID)
        : m_AssetID(InAssetID)
        , m_SourcePath(InSourcePath)
        , m_State(EAssetState::Loading)
    {
        const TDynArray<unsigned char> lFontBytes = ReadFileBytes(m_SourcePath);
        if (lFontBytes.empty())
        {
            OPAAX_CORE_ERROR("FontAsset: failed to read TTF bytes from '{}'", m_SourcePath.CStr());
            m_State = EAssetState::Failed;
            return;
        }

        // The TTF byte
        // buffer must remain alive across stbtt_PackFontRange + future kern queries;
        // lFontBytes is a local std::vector so it dies at ctor end.
        stbtt_fontinfo lFontInfo = {};
        if (!stbtt_InitFont(&lFontInfo, lFontBytes.data(), 0))
        {
            OPAAX_CORE_ERROR("FontAsset: stbtt_InitFont rejected '{}'", m_SourcePath.CStr());
            m_State = EAssetState::Failed;
            return;
        }

        // Scale font-VMetrics to the bake pixel height. Layout consumers use these
        // for baseline offset (Ascent) and \n line advance.
        const float lScale = stbtt_ScaleForPixelHeight(&lFontInfo, PixelHeight);
        Int32 lUnscaledAscent = 0, lUnscaledDescent = 0, lUnscaledLineGap = 0;
        stbtt_GetFontVMetrics(&lFontInfo, &lUnscaledAscent, &lUnscaledDescent, &lUnscaledLineGap);

        m_VMetrics.Ascent      = static_cast<float>(lUnscaledAscent)  * lScale;
        m_VMetrics.Descent     = static_cast<float>(lUnscaledDescent) * lScale;
        m_VMetrics.LineGap     = static_cast<float>(lUnscaledLineGap) * lScale;
        m_VMetrics.LineAdvance = m_VMetrics.Ascent - m_VMetrics.Descent + m_VMetrics.LineGap;

        // Atlas bake — grow-by-double up to MaxAtlas. PackFontRange returns 0 on
        // out-of-room failure (the only path that should retry); other failures
        // are unrecoverable and fall through to the post-loop error.
        Uint32 lSize = InitialAtlas;
        std::vector<unsigned char> lAtlasBytes;
        stbtt_packedchar lPacked[CodepointCount] = {};
        bool lPackOk = false;

        while (lSize <= MaxAtlas)
        {
            lAtlasBytes.assign(static_cast<size_t>(lSize) * lSize, 0);

            stbtt_pack_context lPack = {};
            if (!stbtt_PackBegin(&lPack, lAtlasBytes.data(),
                                 static_cast<int>(lSize), static_cast<int>(lSize),
                                 0 /*stride = width*/, 1 /*padding*/, nullptr))
            {
                OPAAX_CORE_ERROR("FontAsset: stbtt_PackBegin failed at size {}x{} for '{}'",
                                 lSize, lSize, m_SourcePath.CStr());
                m_State = EAssetState::Failed;
                return;
            }

            stbtt_PackSetOversampling(&lPack, sOversampleH, sOversampleV);

            const int lResult = stbtt_PackFontRange(&lPack, lFontBytes.data(), 0,
                                                    PixelHeight,
                                                    static_cast<int>(FirstCodepoint),
                                                    static_cast<int>(CodepointCount),
                                                    lPacked);
            stbtt_PackEnd(&lPack);

            if (lResult)
            {
                lPackOk = true;
                break;
            }

            const Uint32 lNext = lSize * 2u;
            OPAAX_CORE_WARN("FontAsset: atlas {}x{} too small for '{}' — retrying at {}x{}",
                            lSize, lSize, m_SourcePath.CStr(), lNext, lNext);
            lSize = lNext;
        }

        if (!lPackOk)
        {
            OPAAX_CORE_ERROR("FontAsset: atlas exceeded MaxAtlas {}x{} for '{}' — fail-loud per D-h",
                             MaxAtlas, MaxAtlas, m_SourcePath.CStr());
            m_State = EAssetState::Failed;
            return;
        }

        m_AtlasSize = lSize;

        // stbtt_packedchar -> POD GlyphMetrics. UVs normalized to [0,1].
        // V is swapped here (UVMin.y = y1/H, UVMax.y = y0/H) because stb_truetype
        // writes the atlas top-down (row 0 = top of original), while Renderer2D's
        // vertex layout — matching stb_image's globally-set vertical-flip-on-load
        // — assumes bottom-up data (UV.y=0 at world bottom = original bottom).
        // Inverting V at bake-time keeps Step 4's layout walker math straight.
        const float lInvSize = 1.f / static_cast<float>(lSize);
        for (Uint32 i = 0; i < CodepointCount; ++i)
        {
            const stbtt_packedchar& lP = lPacked[i];
            GlyphMetrics&           lOut = m_Glyphs[i];

            lOut.UVMin      = Vector2F(static_cast<float>(lP.x0) * lInvSize, static_cast<float>(lP.y1) * lInvSize);
            lOut.UVMax      = Vector2F(static_cast<float>(lP.x1) * lInvSize, static_cast<float>(lP.y0) * lInvSize);
            lOut.QuadOffset = Vector2F(lP.xoff,  lP.yoff);
            lOut.QuadSize   = Vector2F(lP.xoff2 - lP.xoff, lP.yoff2 - lP.yoff);
            lOut.XAdvance   = lP.xadvance;
        }

        // GPU upload via Step 1 R8 path (Texture2D composes OpenGLTexture2D with
        // GL_R8 + GL_RED + swizzle). lAtlasBytes owns the buffer until the ctor
        // returns; safe per the raw-bytes ctor contract.
        m_Atlas = MakeUnique<Texture2D>(lAtlasBytes.data(), lSize, lSize, 1);
        if (!m_Atlas || !m_Atlas->IsLoaded())
        {
            OPAAX_CORE_ERROR("FontAsset: GPU upload failed for '{}'", m_SourcePath.CStr());
            m_State = EAssetState::Failed;
            return;
        }

        // Kerning LUT — 95×95 codepoint matrix via stbtt_GetCodepointKernAdvance
        // against the ctor-scoped lFontInfo. Returned values are unscaled font-units;
        // multiply by lScale to match the per-glyph metrics' pixel space. Zero-advance
        // pairs are dropped (a missing key in GetKerning implies 0). Sorted by packed
        // (First<<8)|Second key so std::lower_bound walks it in O(log N) at draw time.
        m_Kerning.reserve(CodepointCount); // conservative; ~1 entry per first-glyph on average for Roboto-class fonts
        for (Uint32 lA = FirstCodepoint; lA <= LastCodepoint; ++lA)
        {
            for (Uint32 lB = FirstCodepoint; lB <= LastCodepoint; ++lB)
            {
                const int lRaw = stbtt_GetCodepointKernAdvance(&lFontInfo,
                                                               static_cast<int>(lA),
                                                               static_cast<int>(lB));
                if (lRaw == 0)
                {
                    continue;
                }
                m_Kerning.push_back({
                    static_cast<Uint8>(lA),
                    static_cast<Uint8>(lB),
                    static_cast<float>(lRaw) * lScale
                });
            }
        }
        std::sort(m_Kerning.begin(), m_Kerning.end(),
                  [](const KerningPair& InA, const KerningPair& InB) noexcept
                  {
                      return PackKerningKey(InA.First, InA.Second)
                           < PackKerningKey(InB.First, InB.Second);
                  });
        m_Kerning.shrink_to_fit();

        m_State = EAssetState::Loaded;
        OPAAX_CORE_INFO("FontAsset loaded '{}' ({} glyphs, atlas {}x{}, kern pairs {}, ascent {:.1f} / descent {:.1f} / lineGap {:.1f})",
                        m_SourcePath.CStr(),
                        CodepointCount, m_AtlasSize, m_AtlasSize, m_Kerning.size(),
                        m_VMetrics.Ascent, m_VMetrics.Descent, m_VMetrics.LineGap);
    }

    // Defined here so UniquePtr<Texture2D>'s deleter sees the complete type.
    FontAsset::~FontAsset() = default;

    // =============================================================================
    // Public API
    // =============================================================================
    bool FontAsset::GetGlyphMetrics(char InCodepoint, GlyphMetrics& OutMetrics) const noexcept
    {
        const Uint32 lCp = static_cast<Uint32>(static_cast<unsigned char>(InCodepoint));
        if (lCp < FirstCodepoint || lCp > LastCodepoint)
        {
            return false;
        }
        OutMetrics = m_Glyphs[lCp - FirstCodepoint];
        return true;
    }

    float FontAsset::GetKerning(char InFirst, char InSecond) const noexcept
    {
        if (m_Kerning.empty())
        {
            return 0.f;
        }
        const Uint32 lFirstCp  = static_cast<Uint32>(static_cast<unsigned char>(InFirst));
        const Uint32 lSecondCp = static_cast<Uint32>(static_cast<unsigned char>(InSecond));
        if (lFirstCp  < FirstCodepoint || lFirstCp  > LastCodepoint) { return 0.f; }
        if (lSecondCp < FirstCodepoint || lSecondCp > LastCodepoint) { return 0.f; }

        const Uint32 lKey = PackKerningKey(static_cast<Uint8>(lFirstCp),
                                           static_cast<Uint8>(lSecondCp));

        const auto lIt = std::lower_bound(m_Kerning.begin(), m_Kerning.end(), lKey,
            [](const KerningPair& InP, Uint32 InTarget) noexcept
            {
                return PackKerningKey(InP.First, InP.Second) < InTarget;
            });

        if (lIt == m_Kerning.end())
        {
            return 0.f;
        }
        return (PackKerningKey(lIt->First, lIt->Second) == lKey) ? lIt->Advance : 0.f;
    }

} // namespace Opaax
