#include "TextureResource.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Core/IO/FileIO.h"

// stb_image — the ONE implementation in the build. It lives here, above the RHI, because decoding
// is CPU work every backend shares: a device only ever receives pixels (F2a).
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace Opaax
{
    namespace
    {
        // A placeholder has to be visible, not merely present. 2x2 so it tiles and filters into a
        // flat block at any size, RGBA so it needs no format branch.
        constexpr Uint32 PLACEHOLDER_EXTENT   = 2;
        constexpr Int32  PLACEHOLDER_CHANNELS = 4;
        constexpr Uint8  PLACEHOLDER_RGBA[4]  = { 255, 0, 255, 255 };
    }

    std::optional<TextureResource> TextureResource::Load(const char* InPath, LoadContext& /*InCtx*/)
    {
        // Read the bytes ourselves rather than calling stbi_load(path): the narrow CRT entry point
        // it uses decodes the path with the ANSI code page on MSVC, which resolves a DIFFERENT FILE
        // than the caller named, silently (I7). FileIO already converts.
        TDynArray<Uint8> lFileBytes;
        if (!FileIO::ReadAllBytes(OpaaxString(InPath), lFileBytes))
        {
            OPAAX_LOG(LogTextureResource, Error, "cannot read '{}'", InPath);
            return std::nullopt;
        }

        // Thread-local flip: Load may run on a worker, and the global setter is shared state.
        // GL samples bottom-up, stb decodes top-down.
        stbi_set_flip_vertically_on_load_thread(1);

        Int32 lWidth = 0, lHeight = 0, lChannels = 0;
        stbi_uc* lPixels = stbi_load_from_memory(lFileBytes.data(),
                                                 static_cast<int>(lFileBytes.size()),
                                                 &lWidth, &lHeight, &lChannels, 0);
        if (lPixels == nullptr)
        {
            OPAAX_LOG(LogTextureResource, Error, "cannot decode '{}' — {}", InPath, stbi_failure_reason());
            return std::nullopt;
        }

        TextureResource lResource;
        lResource.Width    = static_cast<Uint32>(lWidth);
        lResource.Height   = static_cast<Uint32>(lHeight);
        lResource.Channels = lChannels;
        lResource.Pixels.assign(lPixels, lPixels + (lWidth * lHeight * lChannels));
        stbi_image_free(lPixels);

        return lResource;
    }

    void TextureResource::Initialize()
    {
        if (Pixels.empty() || Gpu != nullptr)
        {
            return;
        }

        Gpu = OpaaxApplication::GetAppService<IEngine>().CreateTexture(Pixels.data(), Width, Height, Channels);

        // The GPU owns the only copy now. Keeping a CPU mirror would double every texture's cost for
        // a reader nothing has: a sprite samples, it does not read back.
        Pixels.clear();
        Pixels.shrink_to_fit();

        if (Gpu == nullptr)
        {
            OPAAX_LOG(LogTextureResource, Warn, "no device — {}x{} decoded but not uploaded", Width, Height);
        }
    }

    TextureResource TextureResource::Placeholder()
    {
        TextureResource lResource;
        lResource.Width    = PLACEHOLDER_EXTENT;
        lResource.Height   = PLACEHOLDER_EXTENT;
        lResource.Channels = PLACEHOLDER_CHANNELS;

        lResource.Pixels.reserve(PLACEHOLDER_EXTENT * PLACEHOLDER_EXTENT * PLACEHOLDER_CHANNELS);
        for (Uint32 lTexel = 0; lTexel < PLACEHOLDER_EXTENT * PLACEHOLDER_EXTENT; ++lTexel)
        {
            lResource.Pixels.insert(lResource.Pixels.end(), std::begin(PLACEHOLDER_RGBA), std::end(PLACEHOLDER_RGBA));
        }

        return lResource;
    }

    Uint64 TextureResource::ByteSize() const noexcept
    {
        // Computed from the DIMENSIONS, not from Pixels.size(): Initialize empties that vector, and a
        // payload's accounting must not change because the pump happened to run.
        return sizeof(TextureResource)
             + static_cast<Uint64>(Width) * static_cast<Uint64>(Height) * static_cast<Uint64>(Channels);
    }
}
