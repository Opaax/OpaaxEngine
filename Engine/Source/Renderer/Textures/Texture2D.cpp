#include "Renderer/Textures/Texture2D.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Core/IO/FileIO.h"

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

        return true;
    }

    void Texture2D::InitializeOnMainThread()
    {
        if (m_Pixels.empty() || m_Gpu != nullptr)
        {
            return;
        }

        m_Gpu = OpaaxApplication::GetAppService<IEngine>().CreateTexture(m_Pixels.data(), m_Width, m_Height, m_Channels);

        // Free the CPU copy: the GPU has the texture, or nothing can upload it.
        m_Pixels.clear();
        m_Pixels.shrink_to_fit();

        if (m_Gpu == nullptr)
        {
            OPAAX_LOG(LogTexture2D, Warn, "no device — '{}' ({}x{}) decoded but not uploaded",
                      GetName().CStr(), m_Width, m_Height);
        }
    }

    Uint64 Texture2D::GetResourceSize() const
    {
        // From the dimensions: the upload frees the pixels.
        return sizeof(Texture2D)
             + static_cast<Uint64>(m_Width) * static_cast<Uint64>(m_Height) * static_cast<Uint64>(m_Channels);
    }
}
