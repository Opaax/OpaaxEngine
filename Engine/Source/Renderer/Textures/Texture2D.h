#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Resources/Asset/Asset.h"
#include "RHI/Texture.h"

namespace Opaax
{
    inline constexpr LogCategory LogTexture2D{"Texture2D"};

    // =============================================================================
    // Texture2D — an image file as an asset (.png, .jpg, .tga, .bmp), read from the file itself, or
    //   pixels made in code (InitFromPixels, through RuntimeTexture), which can then change.
    //   Decoded when it loads (any thread), handed to the GPU on the main thread, which then keeps the
    //   only copy. A field names one with TAssetRef<Texture2D>; the renderer draws a magenta square for
    //   one that did not load.
    // =============================================================================
    class Texture2D final : public Asset
    {
        OPAAX_OBJECT(Texture2D, Asset)
        OPAAX_ASSET_SOURCE(".png", ".jpg", ".jpeg", ".tga", ".bmp")

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        Texture2D();
        ~Texture2D() override;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads and decodes the image, bottom row first (as the GPU samples it). */
        bool LoadSource(const char* InAbsPath) override;

        /** Uploads the pixels, then frees them (without a device too: nothing could use them). */
        void InitializeOnMainThread() override;

        /** The image's bytes: the same before and after the upload. */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Made from pixels
        // =========================================================================
    public:
        /**
         * Gives a texture made in code its pixels (see RuntimeTexture, which also names it): uploaded by
         * InitializeOnMainThread, writable after. Only once, on a texture that has none.
         * @param InChannels 4 = RGBA8, or 1 = R8 (read as (1,1,1,value), always blended)
         * @param InPixels Width * Height * Channels bytes, bottom row first. Copied.
         * @param bInSmooth Read blended whatever the project's filter
         * @return False, with an error, for other channels, an empty size or no pixels
         */
        bool InitFromPixels(Uint32 InWidth, Uint32 InHeight, Int32 InChannels, const Uint8* InPixels, bool bInSmooth);

        /**
         * Replaces the pixels of InRegion in place. Before the upload, the pixels waiting for it change.
         * @param InPixels InRegion.Width * InRegion.Height * Channels bytes, bottom row first. Copied.
         * @return False, with a warning, for a texture read from a file or a region not inside it
         */
        bool UpdatePixels(const TextureRegion& InRegion, const Uint8* InPixels);

        /** True for a texture made from pixels: its pixels can be replaced. */
        bool IsWritable() const noexcept { return m_bWritable; }

        /** Read blended whatever the project's filter. */
        bool IsSmooth() const noexcept { return m_bSmooth; }

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The GPU texture, or null before the upload (or without a device). */
        ITexture2D* GetTexture() const noexcept { return m_Gpu.get(); }

        Uint32 GetWidth()  const noexcept { return m_Width; }
        Uint32 GetHeight() const noexcept { return m_Height; }

        /** 4 = RGBA8, 3 = RGB8, 1 = R8. */
        Int32 GetChannels() const noexcept { return m_Channels; }

        /** The decoded pixels, bottom row first. Empty once the GPU has them. */
        const TDynArray<Uint8>& GetPixels() const noexcept { return m_Pixels; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        TDynArray<Uint8>       m_Pixels;
        Uint32                 m_Width    = 0;
        Uint32                 m_Height   = 0;
        Int32                  m_Channels = 0;
        bool                   m_bSmooth   = false;
        bool                   m_bWritable = false;
        TUniquePtr<ITexture2D> m_Gpu;
    };
}
