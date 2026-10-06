#include "TextureResource.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Core/IO/FileIO.h"

// stb_image implementation (the only one in the build).
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace Opaax
{
    namespace
    {
        // 2x2 RGBA placeholder.
        constexpr Uint32 PLACEHOLDER_EXTENT   = 2;
        constexpr Int32  PLACEHOLDER_CHANNELS = 4;
        constexpr Uint8  PLACEHOLDER_RGBA[4]  = { 255, 0, 255, 255 };
    }

    std::optional<TextureResource> TextureResource::Load(const char* InPath, LoadContext& /*InCtx*/)
    {
        // Read the file ourselves: stbi_load(path) mishandles non-ASCII paths on MSVC.
        TDynArray<Uint8> lFileBytes;
        if (!FileIO::ReadAllBytes(OpaaxString(InPath), lFileBytes))
        {
            OPAAX_LOG(LogTextureResource, Error, "cannot read '{}'", InPath);
            return std::nullopt;
        }

        // Thread-local flip (Load may run on a worker). GL samples bottom-up, stb decodes top-down.
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

        // Free the CPU copy: the GPU has the texture.
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
        // From the dimensions: Initialize clears Pixels.
        return sizeof(TextureResource)
             + static_cast<Uint64>(Width) * static_cast<Uint64>(Height) * static_cast<Uint64>(Channels);
    }
}
