#pragma once

#include "Assets/IAsset.hpp"
#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

#include <vector>

namespace Opaax
{
    class Texture2D;

    // =============================================================================
    // FontAsset
    //
    // TTF/OTF-baked single-page R8 glyph atlas. Loaded once, immutable post-ctor.
    // Mirrors Texture2D composition shape — IAsset surface (ID, type, state,
    // source path) lives on the asset; the GPU atlas is a composed Texture2D
    // built from the Step 1 raw-bytes R8 ctor.
    //
    // Bake range: ASCII printable [0x20..0x7E] (95 glyphs).
    // Bake pixel height: 32 px.
    // Atlas: starts 512x512 R8, grows by doubling up to 2048x2048; fails loudly
    // past the cap. Oversample default (2,2) for sharper anti-alias.
    //
    // Kerning is NOT in this step — Step 3 of M5 adds the kerning LUT.
    // =============================================================================
    /**
     * @class FontAsset
     *
     * Single-page R8 glyph atlas baked from a TTF/OTF file via stb_truetype.
     * Per-glyph metrics + scaled font-vertical metrics are pre-computed into
     * POD structs so consumers never touch stb_truetype types.
     */
    class OPAAX_API FontAsset final : public IAsset
    {
        // =============================================================================
        // Constants
        // =============================================================================
    public:
        static constexpr Uint32 FirstCodepoint = 0x20;                                    // space
        static constexpr Uint32 LastCodepoint  = 0x7E;                                    // tilde
        static constexpr Uint32 CodepointCount = LastCodepoint - FirstCodepoint + 1;      // 95
        static constexpr float  PixelHeight    = 32.f;                                    // D-g lock
        static constexpr Uint32 InitialAtlas   = 512u;
        static constexpr Uint32 MaxAtlas       = 2048u;

        // =============================================================================
        // POD types
        // =============================================================================
        /**
         * Per-glyph rendering data, pre-computed at bake from stbtt_packedchar.
         * UVMin/UVMax are normalized [0,1] atlas coordinates. QuadOffset/QuadSize
         * are pixel offsets from the layout cursor's pen position; XAdvance moves
         * the cursor to the next glyph.
         */
        struct GlyphMetrics
        {
            Vector2F UVMin      = { 0.f, 0.f };
            Vector2F UVMax      = { 0.f, 0.f };
            Vector2F QuadOffset = { 0.f, 0.f }; // (xoff,  yoff)  relative to pen
            Vector2F QuadSize   = { 0.f, 0.f }; // (xoff2-xoff, yoff2-yoff) in px
            float    XAdvance   = 0.f;          // px to advance the pen after draw
        };

        /**
         * Font-wide vertical metrics, scaled to PixelHeight at bake time.
         * LineAdvance is the precomputed sum used by \n breaks.
         */
        struct FontVMetrics
        {
            float Ascent      = 0.f;
            float Descent     = 0.f;
            float LineGap     = 0.f;
            float LineAdvance = 0.f; // Ascent - Descent + LineGap
        };

        /**
         * Compact kerning entry: signed pixel advance to add between First→Second
         * after a normal glyph advance. Sorted in m_Kerning by (First<<8)|Second
         * so GetKerning binary-searches in O(log N). Zero-advance pairs are
         * dropped at bake — a missing entry implies 0.
         */
        struct KerningPair
        {
            Uint8 First   = 0;
            Uint8 Second  = 0;
            float Advance = 0.f;
        };

        // =============================================================================
        // CTORS - DTOR
        // =============================================================================
    public:
        /**
         * Load + bake. State ends as Loaded on success or Failed if the file
         * cannot be read, stbtt_InitFont rejects it, or the atlas exceeds MaxAtlas.
         * @param InSourcePath Path the engine resolves at runtime (cwd-relative or
         *                     project-relative — matches Texture2D resolution).
         * @param InAssetID    Registry-stable logical ID.
         */
        FontAsset(const OpaaxString& InSourcePath, OpaaxStringID InAssetID);

        ~FontAsset() override;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        FontAsset(const FontAsset&)            = delete;
        FontAsset& operator=(const FontAsset&) = delete;

        // =============================================================================
        // Move - delete
        // =============================================================================
        FontAsset(FontAsset&&)                 = delete;
        FontAsset& operator=(FontAsset&&)      = delete;

        // =============================================================================
        // IAsset interface
        // =============================================================================
        //~ Begin IAsset interface
    public:
        OpaaxStringID      GetAssetID()    const override { return m_AssetID;       }
        AssetType          GetType()       const override { return AssetType::Font;  }
        EAssetState        GetState()      const override { return m_State;         }
        const OpaaxString& GetSourcePath() const override { return m_SourcePath;    }
        //~ End IAsset interface

        // =============================================================================
        // Public API
        // =============================================================================
    public:
        FORCEINLINE bool                IsLoaded()        const noexcept { return m_State == EAssetState::Loaded; }
        FORCEINLINE Uint32              GetAtlasSize()    const noexcept { return m_AtlasSize;    }
        FORCEINLINE const Texture2D*    GetAtlasTexture() const noexcept { return m_Atlas.get();  }
        FORCEINLINE const FontVMetrics& GetFontVMetrics() const noexcept { return m_VMetrics;     }

        /**
         * Per-glyph metrics for a codepoint in [0x20..0x7E]. Returns false for
         * codepoints outside the baked range (OutMetrics left unmodified).
         */
        bool GetGlyphMetrics(char InCodepoint, GlyphMetrics& OutMetrics) const noexcept;

        /**
         * Signed pixel advance to add between InFirst→InSecond beyond their normal
         * per-glyph XAdvance. Returns 0 if either codepoint is outside the baked
         * range, the font has no kerning data, or the specific pair is unkerned.
         * Negative values pull glyphs together (typical for 'AV', 'To', 'Wa').
         */
        float GetKerning(char InFirst, char InSecond) const noexcept;

        FORCEINLINE Uint32 GetKerningPairCount() const noexcept
        {
            return static_cast<Uint32>(m_Kerning.size());
        }

        /**
         * Draws the full atlas as a single quad at world InPosition with InSize.
         * Verify-gate path for M5 Step 2; removable at Step 6 close per OD-6.
         */
        void DebugDrawAtlas(const Vector2F& InPosition, const Vector2F& InSize) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID            m_AssetID    = {};
        OpaaxString              m_SourcePath;
        EAssetState              m_State      = EAssetState::Unloaded;
        UniquePtr<Texture2D>     m_Atlas;
        Uint32                   m_AtlasSize  = 0u;
        GlyphMetrics             m_Glyphs[CodepointCount] = {};
        FontVMetrics             m_VMetrics   = {};
        std::vector<KerningPair> m_Kerning;
    };

} // namespace Opaax
