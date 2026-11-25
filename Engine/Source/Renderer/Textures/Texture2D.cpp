#include "Renderer/Textures/Texture2D.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Core/Image/AlphaBleed.h"
#include "Core/IO/FileIO.h"
#include "Core/Memory/Memory.h"
#include "RHI/BlendState.h"

// stb_image implementation (the only one in the build).
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace Opaax
{
    Texture2D::Texture2D()  = default;
    Texture2D::~Texture2D() = default;

    bool Texture2D::LoadSource(const char* InAbsPath)
    {
        // Read the file ourselves: stbi_load(path) mishandles non-ASCII paths on MSVC.
        TDynArray<Uint8> lFileBytes;
        if (!FileIO::ReadAllBytes(OpaaxString(InAbsPath), lFileBytes))
        {
            OPAAX_LOG(LogTexture2D, Error, "cannot read '{}'", InAbsPath);
            return false;
        }

        // Thread-local flip (this may run on a worker). GL samples bottom-up, stb decodes top-down.
        stbi_set_flip_vertically_on_load_thread(1);

        Int32 lWidth = 0, lHeight = 0, lChannels = 0;
        stbi_uc* lPixels = stbi_load_from_memory(lFileBytes.data(),
                                                 static_cast<int>(lFileBytes.size()),
                                                 &lWidth, &lHeight, &lChannels, 0);
        if (lPixels == nullptr)
        {
            OPAAX_LOG(LogTexture2D, Error, "cannot decode '{}' — {}", InAbsPath, stbi_failure_reason());
            return false;
        }

        m_Width    = static_cast<Uint32>(lWidth);
        m_Height   = static_cast<Uint32>(lHeight);
        m_Channels = lChannels;
        m_Pixels.assign(lPixels, lPixels + (lWidth * lHeight * lChannels));
        stbi_image_free(lPixels);

        // The colour under transparent texels must not show at a sprite's blended edge.
        if (lChannels == 4)
        {
            BleedAlphaEdges(m_Pixels.data(), m_Width, m_Height);
        }

        return true;
    }

    void Texture2D::InitializeOnMainThread()
    {
        if (m_Pixels.empty() || m_Gpu != nullptr)
        {
            return;
        }

        TextureDesc lDesc;
        lDesc.Width      = m_Width;
        lDesc.Height     = m_Height;
        lDesc.Channels   = m_Channels;
        lDesc.bSmooth    = m_bSmooth;
        lDesc.bWritable  = m_bWritable;
        lDesc.bPaintable = m_bPaintable;

        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();
        m_Gpu = lEngine.CreateTexture(lDesc, m_Pixels.data());

        if (m_bPaintable && m_Gpu != nullptr)
        {
            m_PaintTarget = lEngine.CreateTextureTarget(*m_Gpu);
        }

        // Free the CPU copy: the GPU has the texture, or nothing can upload it.
        m_Pixels.clear();
        m_Pixels.shrink_to_fit();

        if (m_Gpu == nullptr)
        {
            OPAAX_LOG(LogTexture2D, Warn, "no device — '{}' ({}x{}) decoded but not uploaded",
                      GetName().CStr(), m_Width, m_Height);
        }
    }

    bool Texture2D::InitFromPixels(const Uint32 InWidth, const Uint32 InHeight, const Int32 InChannels,
                                   const Uint8* InPixels, const bool bInSmooth)
    {
        if (m_Gpu != nullptr || !m_Pixels.empty())
        {
            OPAAX_LOG(LogTexture2D, Error, "'{}' already has pixels: make a new texture instead", GetName().CStr());
            return false;
        }

        if (InChannels != 1 && InChannels != 4)
        {
            OPAAX_LOG(LogTexture2D, Error, "a texture made from pixels has 1 or 4 channels, not {}", InChannels);
            return false;
        }

        if (InWidth == 0 || InHeight == 0 || InPixels == nullptr)
        {
            OPAAX_LOG(LogTexture2D, Error, "a texture made from pixels needs a size ({}x{}) and its pixels",
                      InWidth, InHeight);
            return false;
        }

        m_Width     = InWidth;
        m_Height    = InHeight;
        m_Channels  = InChannels;
        m_bSmooth   = bInSmooth;
        m_bWritable = true;

        const Uint64 lBytes = RegionByteCount(TextureRegion::Whole(InWidth, InHeight), InChannels);
        m_Pixels.assign(InPixels, InPixels + lBytes);
        return true;
    }

    bool Texture2D::InitPaintable(const Uint32 InWidth, const Uint32 InHeight, const Uint8* InPixels)
    {
        if (m_Gpu != nullptr || !m_Pixels.empty())
        {
            OPAAX_LOG(LogTexture2D, Error, "'{}' already has pixels: make a new texture instead", GetName().CStr());
            return false;
        }

        if (InWidth == 0 || InHeight == 0)
        {
            OPAAX_LOG(LogTexture2D, Error, "a paintable texture needs a size ({}x{})", InWidth, InHeight);
            return false;
        }

        m_Width      = InWidth;
        m_Height     = InHeight;
        m_Channels   = 4;
        m_bSmooth    = true;
        m_bPaintable = true;

        // Always pixels to upload: transparent when none are given.
        const Uint64 lTexels = static_cast<Uint64>(InWidth) * InHeight;
        if (InPixels != nullptr)
        {
            m_Pixels.assign(InPixels, InPixels + lTexels * 4u);
            PremultiplyRGBA8(m_Pixels.data(), lTexels);
        }
        else
        {
            m_Pixels.assign(static_cast<size_t>(lTexels * 4u), 0u);
        }
        return true;
    }

    bool Texture2D::UpdatePixels(const TextureRegion& InRegion, const Uint8* InPixels)
    {
        if (m_bPaintable)
        {
            OPAAX_LOG(LogTexture2D, Warn, "'{}' is paintable: it changes through Paint and Clear, not its pixels",
                      GetName().CStr());
            return false;
        }

        if (!m_bWritable)
        {
            OPAAX_LOG(LogTexture2D, Warn, "'{}' was read from a file: only a texture made from pixels can change",
                      GetName().CStr());
            return false;
        }

        if (!IsRegionInside(InRegion, m_Width, m_Height) || InPixels == nullptr)
        {
            OPAAX_LOG(LogTexture2D, Warn, "'{}': the region {},{} {}x{} is not inside its {}x{} texels", GetName().CStr(),
                      InRegion.X, InRegion.Y, InRegion.Width, InRegion.Height, m_Width, m_Height);
            return false;
        }

        // Before the upload (or without a device), the pixels waiting for it take the change.
        if (m_Gpu == nullptr)
        {
            if (m_Pixels.empty())
            {
                return false;
            }

            const Uint64 lRow = static_cast<Uint64>(InRegion.Width) * m_Channels;
            for (Uint32 lY = 0; lY < InRegion.Height; ++lY)
            {
                const Uint64 lTo = (static_cast<Uint64>(InRegion.Y + lY) * m_Width + InRegion.X) * m_Channels;
                Memory::Copy(m_Pixels.data() + lTo, InPixels + lY * lRow, lRow);
            }
            return true;
        }

        return m_Gpu->Update(InRegion, InPixels);
    }

    Uint64 Texture2D::GetResourceSize() const
    {
        // From the dimensions: the upload frees the pixels. A paintable texture's mipmaps add a third.
        const Uint64 lBytes = static_cast<Uint64>(m_Width) * static_cast<Uint64>(m_Height) * static_cast<Uint64>(m_Channels);
        return sizeof(Texture2D) + (m_bPaintable ? lBytes + lBytes / 3u : lBytes);
    }
}
