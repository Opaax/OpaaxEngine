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
    // Texture2D — an image file as an asset (.png, .jpg, .tga, .bmp), read from the file itself.
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
        TUniquePtr<ITexture2D> m_Gpu;
    };
}
