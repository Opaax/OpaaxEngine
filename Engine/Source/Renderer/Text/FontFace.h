#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Renderer/Text/FontFaceData.h"
#include "Resources/Asset/Asset.h"
#include "RHI/Texture.h"

namespace Opaax
{
    inline constexpr LogCategory LogFontFace{"FontFace"};

    // =============================================================================
    // FontFace — one font file (.ttf, .otf) as an asset, read from the file itself: a baked glyph atlas
    //   and its metrics. Baked when it loads (any thread), the atlas handed to the GPU on the main
    //   thread. One file is one face; choosing between faces is FontFamily's job. Text whose face did
    //   not load draws every character as a box.
    // =============================================================================
    class FontFace final : public Asset
    {
        OPAAX_OBJECT(FontFace, Asset)
        OPAAX_ASSET_SOURCE(".ttf", ".otf")

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        FontFace();
        ~FontFace() override;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads the font and bakes its glyph atlas. */
        bool LoadSource(const char* InAbsPath) override;

        /** Uploads the atlas (R8), then frees its pixels (without a device too: nothing could use them). */
        void InitializeOnMainThread() override;

        /** The atlas's bytes and the glyph tables: the same before and after the upload. */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The glyph table, the metrics and the kerning pairs. */
        const FontFaceData& GetData() const noexcept { return m_Data; }

        /** The atlas texture, or null before the upload (or without a device). */
        ITexture2D* GetAtlas() const noexcept { return m_Gpu.get(); }

        /** The baked coverage (R8), top row first. Empty once the GPU has it. */
        const TDynArray<Uint8>& GetPixels() const noexcept { return m_Pixels; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        FontFaceData           m_Data;
        TDynArray<Uint8>       m_Pixels;
        TUniquePtr<ITexture2D> m_Gpu;
    };
}
